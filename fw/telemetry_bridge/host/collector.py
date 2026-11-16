#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only

"""Collect the same length-prefixed EkfLog protobuf over TLS or BLE NUS."""
from __future__ import annotations
import argparse
import asyncio
import json
import contextlib
import getpass
import secrets
import ssl
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  # central -> ESP
NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # ESP -> central
MAX_PROTO = 240
NAMES = {1:"boot_id",2:"monotonic_us",3:"sequence",4:"x_m",5:"y_m",6:"yaw_rad",7:"gyro_bias_rps",8:"sigma_x_m",9:"sigma_y_m",10:"sigma_yaw_rad",11:"grid_x",12:"grid_y",13:"tracking",14:"grid_confident",15:"queue_dropped"}

def varint(data: bytes, pos: int) -> tuple[int, int]:
    value = 0
    for shift in range(0, 70, 7):
        if pos >= len(data):
            raise ValueError("truncated varint")
        b = data[pos]; pos += 1
        value |= (b & 127) << shift
        if not b & 128:
            return value, pos
    raise ValueError("oversized varint")

def decode_ekf(data: bytes) -> dict:
    """Decode starter EkfLog while skipping unknown protobuf fields."""
    out = {}
    pos = 0
    while pos < len(data):
        key, pos = varint(data, pos)
        field, wire = key >> 3, key & 7
        if field == 0:
            raise ValueError("zero field number")
        if wire == 0:
            value, pos = varint(data, pos)
            if field in (11, 12):
                value = (value >> 1) ^ -(value & 1)
            if field in (13, 14):
                value = bool(value)
        elif wire == 5:
            if pos + 4 > len(data): raise ValueError("truncated fixed32")
            value = struct.unpack_from("<f", data, pos)[0]; pos += 4
        elif wire == 1:
            if pos + 8 > len(data): raise ValueError("truncated fixed64")
            value = None; pos += 8
        elif wire == 2:
            size, pos = varint(data, pos)
            if pos + size > len(data): raise ValueError("truncated bytes")
            value = None; pos += size
        else:
            raise ValueError("unsupported wire type")
        if field in NAMES:
            if (field in range(4,11) and wire != 5) or (field not in range(4,11) and wire != 0):
                raise ValueError("unexpected wire type")
            out[NAMES[field]] = value
    for mandatory in ("boot_id", "monotonic_us", "sequence"):
        if mandatory not in out: raise ValueError(f"missing {mandatory}")
    return out

def frame(kind: int, payload: bytes) -> bytes:
    if not 1 <= len(payload) <= MAX_PROTO: raise ValueError("protobuf too large")
    return struct.pack("<H", len(payload) + 1) + bytes((kind,)) + payload

def enc_varint(value: int) -> bytes:
    out = bytearray()
    while value > 127:
        out.append((value & 127) | 128); value >>= 7
    out.append(value)
    return bytes(out)

def encode_control(request_id: int, operation: int) -> bytes:
    return bytes((8,)) + enc_varint(request_id) + bytes((16,)) + enc_varint(operation)

class FrameParser:
    """Incremental length+kind+protobuf stream parser for TCP and BLE reassembly."""
    def __init__(self): self.buffer = bytearray()
    def feed(self, chunk: bytes) -> list[tuple[int, bytes]]:
        self.buffer.extend(chunk)
        result = []
        while len(self.buffer) >= 2:
            n = self.buffer[0] | self.buffer[1] << 8
            if not 2 <= n <= MAX_PROTO + 1: raise ValueError("invalid frame length")
            if len(self.buffer) < n + 2: break
            result.append((self.buffer[2], bytes(self.buffer[3:n+2])))
            del self.buffer[:n+2]
        return result

class BleChunks:
    """Reassemble independent NUS notifications; reject incomplete prior frame."""
    def __init__(self): self.frame_id = None; self.count = 0; self.parts = {}
    def feed(self, chunk: bytes) -> list[tuple[int, bytes]]:
        if len(chunk) < 5 or chunk[0] != 0xb7: return []
        fid, index, count = chunk[1:4]
        if count == 0 or index >= count: return []
        if fid != self.frame_id or count != self.count:
            self.frame_id, self.count, self.parts = fid, count, {}
        self.parts[index] = bytes(chunk[4:])
        if len(self.parts) != count: return []
        data = b"".join(self.parts[i] for i in range(count))
        self.frame_id, self.count, self.parts = None, 0, {}
        return FrameParser().feed(data)

def decode_ack(data: bytes) -> dict:
    pos = 0; result = {}
    while pos < len(data):
        tag, pos = varint(data, pos)
        val, pos = varint(data, pos)
        if tag in (8, 16, 24): result[{8:"request_id",16:"operation",24:"status"}[tag]] = val
    return result

@dataclass
class Sink:
    file: object
    last: dict
    display: bool = True
    transport: str = ""
    def emit(self, packet: bytes, transport: str):
        log = decode_ekf(packet)
        key = log["boot_id"]
        prev = self.last.get(key)
        seq = log["sequence"]
        log["transport"] = transport
        log["sequence_gap"] = max(0, seq - prev - 1) if prev is not None else 0
        log["late_or_duplicate"] = prev is not None and seq <= prev
        if prev is None or seq > prev: self.last[key] = seq
        line = json.dumps(log, separators=(",", ":"), allow_nan=False)
        if self.file is not sys.stdout:
            self.file.write(line + "\n"); self.file.flush()
        if self.display:
            print(line)

def dispatch(kind: int, packet: bytes, sink: Sink):
    if kind == 1: sink.emit(packet, "tls" if sink.transport == "tls" else "ble")
    elif kind == 3:
        ack = decode_ack(packet)
        label = "ACK" if ack.get("status") == 0 else "NACK"
        print(label, ack, file=sys.stderr)

async def collect_tls(address: str, port: int, cert: str, sink: Sink, commands: asyncio.Queue | None = None):
    ctx = ssl.create_default_context(cafile=cert)
    reader, writer = await asyncio.open_connection(address, port, ssl=ctx, server_hostname="robot-telemetry.local")
    sink.transport = "tls"
    parser = FrameParser()
    try:
        while True:
            read = asyncio.create_task(reader.read(1024))
            send = asyncio.create_task(commands.get()) if commands else None
            pending = {read} | ({send} if send else set())
            done, rest = await asyncio.wait(pending, return_when=asyncio.FIRST_COMPLETED)
            for task in rest: task.cancel()
            if read in done:
                chunk = read.result()
                if not chunk: break
                for kind, packet in parser.feed(chunk): dispatch(kind, packet, sink)
            if send and send in done:
                writer.write(send.result()); await writer.drain()
    finally:
        writer.close(); await writer.wait_closed()

async def collect_ble(address: str, sink: Sink, seconds: float = 30.0, commands: asyncio.Queue | None = None):
    try: from bleak import BleakClient
    except ImportError as exc: raise RuntimeError("Install bleak for BLE: pip install bleak") from exc
    parser = BleChunks(); sink.transport = "ble"
    queue: asyncio.Queue[bytes] = asyncio.Queue(maxsize=128)
    async with BleakClient(address, timeout=15) as client:
        def notify(_handle, data):
            try: queue.put_nowait(bytes(data))
            except asyncio.QueueFull: pass
        await client.start_notify(NUS_TX, notify)
        deadline = asyncio.get_running_loop().time() + seconds
        while client.is_connected:
            remaining = deadline - asyncio.get_running_loop().time()
            if remaining <= 0: break
            recv = asyncio.create_task(queue.get())
            send = asyncio.create_task(commands.get()) if commands else None
            pending = {recv} | ({send} if send else set())
            done, rest = await asyncio.wait(pending, timeout=min(remaining, 2), return_when=asyncio.FIRST_COMPLETED)
            for task in rest: task.cancel()
            if recv in done:
                for kind, packet in parser.feed(recv.result()): dispatch(kind, packet, sink)
            if send and send in done:
                await client.write_gatt_char(NUS_RX, bytes((2,)) + send.result(), response=True)

async def interactive_commands(queue: asyncio.Queue, sink: Sink):
    """Linux terminal input; never blocks the telemetry event loop."""
    loop = asyncio.get_running_loop()
    incoming: asyncio.Queue[str] = asyncio.Queue()
    loop.add_reader(sys.stdin.fileno(), lambda: incoming.put_nowait(sys.stdin.readline()))
    request_id = secrets.randbelow(0x7fffffff) + 1
    print("commands: start-loc | halt | log on | log off | status | quit", file=sys.stderr)
    try:
        while True:
            line = (await incoming.get()).strip().lower()
            if line == "quit" or not line: break
            if line in ("log", "log on", "log off"):
                sink.display = line != "log off"
                print(f"live log {'on' if sink.display else 'off'}", file=sys.stderr)
                continue
            if line == "status":
                print(f"transport={sink.transport or 'disconnected'} streams={len(sink.last)}", file=sys.stderr)
                continue
            op = {"start-loc":1,"halt":2}.get(line)
            if not op:
                print("unknown command", file=sys.stderr); continue
            request_id = (request_id % 0xffffffff) + 1
            await queue.put(frame(2, encode_control(request_id, op)))
    finally:
        loop.remove_reader(sys.stdin.fileno())

async def provision(address: str, ssid: str, password: str):
    try: from bleak import BleakClient
    except ImportError as exc: raise RuntimeError("Install bleak for BLE: pip install bleak") from exc
    ssid_b, password_b = ssid.encode(), password.encode()
    if not 1 <= len(ssid_b) <= 32 or len(password_b) > 63:
        raise ValueError("SSID must be 1..32 bytes and password 0..63 bytes")
    # Encrypted GATT write: opcode, length-prefixed SSID and WPA password.
    packet = bytes((0xf0, len(ssid_b))) + ssid_b + bytes((len(password_b),)) + password_b
    async with BleakClient(address) as client:
        # A paired, encrypted link is enforced by the ESP GATT characteristic.
        await client.write_gatt_char(NUS_RX, packet, response=True)

async def main(args):
    if args.command == "provision":
        await provision(args.ble, args.ssid, args.password or getpass.getpass("Wi-Fi password: "))
        return
    commands = asyncio.Queue(maxsize=8) if args.command == "shell" else None
    output = open(args.output, "a", buffering=1) if args.output != "-" else contextlib.nullcontext(sys.stdout)
    with output as file:
        sink = Sink(file, {})
        sink.transport = ""
        async def links():
            while True:
                try:
                    if args.mode in ("auto", "tls") and args.address and args.cert:
                        await collect_tls(args.address, args.port, args.cert, sink, commands)
                    elif args.mode == "tls": raise ValueError("--address and --cert required")
                    if args.mode in ("auto", "ble") and args.ble:
                        await collect_ble(args.ble, sink, args.ble_window, commands)
                    elif args.mode == "ble": raise ValueError("--ble required")
                except (OSError, ssl.SSLError, asyncio.TimeoutError, RuntimeError) as exc:
                    print(f"transport disconnected: {exc}", file=sys.stderr)
                if args.mode != "auto": break
                await asyncio.sleep(2)
        if args.mode == "auto" and not (args.ble or (args.address and args.cert)):
            raise ValueError("--ble or both --address and --cert required")
        if commands:
            ui=asyncio.create_task(interactive_commands(commands,sink))
            net=asyncio.create_task(links())
            done,pending=await asyncio.wait({ui,net},return_when=asyncio.FIRST_COMPLETED)
            for task in pending: task.cancel()
            await asyncio.gather(*pending,return_exceptions=True)
            for task in done: task.result()
        else: await links()

def cli():
    p = argparse.ArgumentParser(description="AXI EKF telemetry collector (JSONL)")
    sub = p.add_subparsers(dest="command", required=True)
    for name in ("collect", "shell"):
        collect = sub.add_parser(name)
        collect.add_argument("--mode", choices=("auto", "tls", "ble"), default="auto")
        collect.add_argument("--address", help="ESP IPv6 link-local with Linux zone, e.g. fe80::abcd%%wlan0")
        collect.add_argument("--port", type=int, default=7443)
        collect.add_argument("--cert", help="pinned self-signed PEM certificate")
        collect.add_argument("--ble", help="BLE address or device identifier")
        collect.add_argument("--ble-window", type=float, default=30)
        collect.add_argument("--output", default="-", help="append JSONL; - for stdout")
    provision_cmd = sub.add_parser("provision")
    provision_cmd.add_argument("--ble", required=True)
    provision_cmd.add_argument("--ssid", required=True)
    provision_cmd.add_argument("--password", help="omit to enter without shell history")
    args = p.parse_args()
    try: asyncio.run(main(args))
    except KeyboardInterrupt: pass

if __name__ == "__main__": cli()
