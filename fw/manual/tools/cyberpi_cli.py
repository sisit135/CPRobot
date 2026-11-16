#!/usr/bin/env python3
"""CyberPi USB serial client, derived from the mBlock CyberPi/meoser JS sources.

No mLink, Bluetooth, network access, firmware flashing, or raw-REPL dependency.
See docs/adr_usb_serial.md for protocol evidence and hardware validation limits.
"""
import argparse
import codecs
import json
import math
from pathlib import Path
import re
import struct
import sys
import time
import zipfile

DEFAULT_STUB = Path(__file__).resolve().parents[1] / "LoaderStub.mblock"
FILE_PREFIX = b"\x01\x00\x5e"


class ProtocolError(RuntimeError):
    pass


def frame(payload):
    if not 0 < len(payload) <= 65535:
        raise ValueError("payload length outside 1..65535")
    length = struct.pack("<H", len(payload))
    return bytes((0xf3, (0xf3 + sum(length)) & 255)) + length + payload + bytes((sum(payload) & 255, 0xf4))


def file_checksum(data):
    lanes = bytearray(4)
    for index, value in enumerate(data):
        lanes[index % 4] ^= value
    return bytes(lanes)


def upload_frames(path, data, block_size=80):
    if not 1 <= block_size <= 1024:
        raise ValueError("block size outside 1..1024")
    if not re.fullmatch(r"/flash/[^\x00-\x1f\\]+\.py", path) or ".." in path.split("/"):
        raise ValueError("remote path must be a .py file under /flash/")
    encoded_path = path.encode("utf-8")
    if len(encoded_path) > 255 or len(data) > 0xffffffff:
        raise ValueError("file path/content too large")
    header = b"\x00" + struct.pack("<I", len(data)) + file_checksum(data) + encoded_path
    yield frame(FILE_PREFIX + b"\x01" + struct.pack("<H", len(header)) + header)
    for offset in range(0, len(data), block_size):
        body = struct.pack("<I", offset) + data[offset:offset + block_size]
        yield frame(FILE_PREFIX + b"\x02" + struct.pack("<H", len(body)) + body)


def execute_payload(code, request_id=0):
    data = code.encode("utf-8")
    if not 0 <= request_id <= 65535 or len(data) > 65529:
        raise ValueError("execution request too large")
    # CyberPi __createFrame(4, index, source, null, true).
    return b"\x28\x04" + struct.pack("<HH", request_id, len(data)) + data


def project_path(version, name):
    match = re.fullmatch(r"44\.01\.(\d{3})", version)
    if not match:
        raise ProtocolError("unsupported firmware %r; expected 44.01.xxx" % version)
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,80}", name):
        raise ValueError("project name must be 1..80 ASCII letters, digits, _ or -")
    return "/flash/main.py" if int(match[1]) < 6 else "/flash/_xx_%s.py" % name


def stub_info(path):
    with zipfile.ZipFile(path) as archive:
        project = json.loads(archive.read("project.json"))
        state = json.loads(archive.read("mscratch.json"))
    targets = [target for target in project["targets"] if target.get("deviceId") == "cyberpi"]
    if len(targets) != 1:
        raise ValueError("loader stub must contain one CyberPi target")
    target = targets[0]
    return {"device": "cyberpi", "extensions": target.get("loadedExtIds", []),
            "editor": target.get("editorMode"),
            "cyberpi_version": state.get("extensions", {}).get("cyberpi", {}).get("version")}


class StreamDecoder:
    """Split length-delimited F3/F4 frames from interleaved raw tracing bytes."""
    def __init__(self, max_payload=8192):
        self.buffer = bytearray()
        self.max_payload = max_payload
        self.bad_frames = 0

    def feed(self, data):
        self.buffer.extend(data)
        events = []
        while self.buffer:
            start = self.buffer.find(0xf3)
            if start < 0:
                events.append(("text", bytes(self.buffer)))
                self.buffer.clear()
                break
            if start:
                events.append(("text", bytes(self.buffer[:start])))
                del self.buffer[:start]
            if len(self.buffer) < 4:
                break
            size = struct.unpack_from("<H", self.buffer, 2)[0]
            if (self.buffer[1] != (0xf3 + self.buffer[2] + self.buffer[3]) & 255
                    or not 0 < size <= self.max_payload):
                events.append(("text", bytes(self.buffer[:1])))
                del self.buffer[:1]
                self.bad_frames += 1
                continue
            if len(self.buffer) < size + 6:
                break
            packet = bytes(self.buffer[:size + 6])
            if packet[-1] != 0xf4 or sum(packet[4:-2]) & 255 != packet[-2]:
                self.bad_frames += 1
                del self.buffer[:1]
                continue
            del self.buffer[:size + 6]
            events.append(("frame", packet[4:-2]))
        return events

    def finish(self):
        pending = bytes(self.buffer)
        self.buffer.clear()
        return pending


class TraceSink:
    def __init__(self, output=None, show_frames=False, raw=False):
        self.output = output
        self.show_frames = show_frames
        self.raw = raw
        self.decoder = codecs.getincrementaldecoder("utf-8")("replace")

    def received(self, data):
        # Binary capture preserves exactly what the serial device sent.
        if self.output is not None:
            self.output.write(data)
            self.output.flush()
        if self.raw:
            self.text(data)

    def text(self, data):
        text = self.decoder.decode(data)
        if text:
            sys.stdout.write(text)
            sys.stdout.flush()

    def packet(self, payload):
        if self.show_frames:
            print("[RX frame] " + payload.hex(" "), file=sys.stderr)

    def finish(self):
        sys.stdout.write(self.decoder.decode(b"", final=True))
        sys.stdout.flush()


class Client:
    def __init__(self, serial_port, trace, timeout=3.0, clock=time.monotonic):
        self.port = serial_port
        self.trace = trace
        self.timeout = timeout
        self.clock = clock
        self.decoder = StreamDecoder()

    def send(self, payload):
        packet = frame(payload)
        if self.port.write(packet) != len(packet):
            raise ProtocolError("short serial write")
        self.port.flush()

    def receive(self):
        data = self.port.read(max(1, min(getattr(self.port, "in_waiting", 0), 4096)))
        self.trace.received(data)
        packets = []
        for kind, value in self.decoder.feed(data):
            if kind == "text":
                if not self.trace.raw:
                    self.trace.text(value)
            else:
                self.trace.packet(value)
                packets.append(value)
        return packets

    def request(self, payload, predicate, timeout=None):
        self.send(payload)
        deadline = self.clock() + (self.timeout if timeout is None else timeout)
        while self.clock() < deadline:
            for packet in self.receive():
                if predicate(packet):
                    return packet
        raise TimeoutError("no matching CyberPi response; device state is unconfirmed")

    def version(self):
        # Opening the CH340 port can restart CyberPi. Its protocol becomes ready
        # after the early boot text; retransmit only this read-only query.
        deadline = self.clock() + max(45, self.timeout)
        while self.clock() < deadline:
            try:
                packet = self.request(b"\x06", lambda p: p[:1] == b"\x06" and len(p) > 1,
                                      min(2, deadline - self.clock()))
                return packet[1:].rstrip(b"\x00").decode("ascii")
            except TimeoutError:
                pass
        raise TimeoutError("CyberPi did not answer its version query within the startup window")

    def mode(self):
        packet = self.request(b"\x0d\x80", lambda p: len(p) >= 3 and p[0] == 13)
        if packet[2] not in (0, 1):
            raise ProtocolError("unknown device mode %d" % packet[2])
        return packet[2]

    def set_mode(self, mode):
        self.send(bytes((13, 0, mode)))
        time.sleep(0.2)  # CyberPi extension also settles after a mode change.
        if self.mode() != mode:
            raise ProtocolError("device did not enter requested mode")

    def upload(self, path, data, progress=None):
        for index, packet in enumerate(upload_frames(path, data)):
            # JS retries the header twice and sends each body only once. A timed-out
            # body has ambiguous admission; never silently replay or report success.
            attempts = 2 if index == 0 else 1
            for attempt in range(attempts):
                try:
                    reply = self.request(packet[4:-2],
                        lambda p: len(p) >= 6 and p[:4] == FILE_PREFIX + b"\xf0")
                    break
                except TimeoutError:
                    if attempt == attempts - 1:
                        raise
            status = reply[5]  # meoser checks byte 9 of the complete outer frame.
            if status:
                raise ProtocolError("file transfer rejected at block %d: status 0x%02x" % (index, status))
            if progress:
                progress(min(index * 80, len(data)), len(data))

    def monitor(self, duration=0):
        deadline = self.clock() + duration if duration else None
        while deadline is None or self.clock() < deadline:
            self.receive()

    def finish(self):
        pending = self.decoder.finish()
        if not self.trace.raw:
            self.trace.text(pending)
        self.trace.finish()


def open_serial(port, baud):
    try:
        import serial
    except ImportError as exc:
        raise RuntimeError("pyserial is required; see docs/building.md#adr-usb-serial-cli") from exc
    # Set inactive modem lines before opening; do not pulse the ESP reset/boot pins.
    connection = serial.Serial(port=None, baudrate=baud, timeout=0.05, write_timeout=3)
    connection.dtr = False
    connection.rts = False
    connection.port = port
    connection.open()
    return connection


def parser():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("-p", "--port", help="USB serial device, e.g. /dev/ttyUSB0 or COM3")
    cli.add_argument("--baud", type=int, default=115200)
    cli.add_argument("--timeout", type=float, default=3)
    cli.add_argument("--capture", type=Path, help="save exact incoming serial bytes")
    cli.add_argument("--show-frames", action="store_true", help="also show decoded protocol frames on stderr")
    commands = cli.add_subparsers(dest="command", required=True)
    commands.add_parser("ports", help="list serial ports and USB identifiers")
    stub = commands.add_parser("stub-info", help="inspect the mBlock loader dependency manifest offline")
    stub.add_argument("stub", type=Path, nargs="?", default=DEFAULT_STUB)
    commands.add_parser("info", help="query firmware version and mode")
    mode = commands.add_parser("mode", help="explicitly change execution mode")
    mode.add_argument("mode", choices=("upload", "live"))
    upload = commands.add_parser("upload", help="upload a UTF-8 Python file using the IDE file protocol")
    upload.add_argument("file", type=Path)
    upload.add_argument("--project-name", default="ADR")
    upload.add_argument("--monitor", action="store_true", help="continue reading serial tracing after transfer")
    upload.add_argument("--duration", type=float, default=0, help="monitor seconds; 0 means until Ctrl-C")
    monitor = commands.add_parser("monitor", help="passively read serial tracing without transmitting commands")
    monitor.add_argument("--duration", type=float, default=0)
    monitor.add_argument("--raw", action="store_true", help="display all incoming bytes, including protocol frames")
    execute = commands.add_parser("exec", help="execute Python with the IDE online command protocol")
    source = execute.add_mutually_exclusive_group(required=True)
    source.add_argument("--code")
    source.add_argument("--file", type=Path)
    execute.add_argument("--duration", type=float, default=2, help="seconds to collect output after sending")
    return cli


def main(argv=None):
    cli = parser()
    args = cli.parse_args(argv)
    if (not math.isfinite(args.timeout) or args.timeout <= 0 or args.baud <= 0
            or not math.isfinite(getattr(args, "duration", 0)) or getattr(args, "duration", 0) < 0):
        cli.error("baud/timeout must be positive and duration must be nonnegative")
    if args.command == "stub-info":
        print(json.dumps(stub_info(args.stub), indent=2))
        return 0
    if args.command == "ports":
        try:
            from serial.tools import list_ports
        except ImportError as exc:
            raise RuntimeError("install pyserial; see docs/building.md#adr-usb-serial-cli") from exc
        for port in list_ports.comports():
            print("%s\t%s\t%s" % (port.device, port.description, port.hwid))
        return 0
    if not args.port:
        cli.error("--port is required for device commands")
    # Read/validate source before opening a port or changing the running device.
    data = None
    if args.command == "upload":
        data = args.file.read_bytes()
        data.decode("utf-8")
        if not data:
            raise ValueError("empty Python file")
        compile(data, str(args.file), "exec")
        project_path("44.01.016", args.project_name)
    if args.command == "exec":
        code = args.code if args.code is not None else args.file.read_text(encoding="utf-8")
        payload = execute_payload(code)
    capture = args.capture.open("ab") if args.capture else None
    port = None
    client = None
    try:
        port = open_serial(args.port, args.baud)
        client = Client(port, TraceSink(capture, args.show_frames, getattr(args, "raw", False)), args.timeout)
        version = client.version() if args.command != "monitor" else None
        if args.command == "info":
            print(json.dumps({"firmware": version, "mode": "upload" if client.mode() == 0 else "live"}))
        elif args.command == "mode":
            client.set_mode(0 if args.mode == "upload" else 1)
            print("Mode: " + args.mode, file=sys.stderr)
        elif args.command == "upload":
            path = project_path(version, args.project_name)
            # Enter upload mode as the IDE does; this is an explicit upload action.
            client.set_mode(0)
            client.upload(path, data)
            print("Transfer acknowledged: %d bytes -> %s" % (len(data), path), file=sys.stderr)
            if args.monitor:
                client.monitor(args.duration)
        elif args.command == "exec":
            client.send(payload)
            print("Python command sent; inspect output for completion/errors", file=sys.stderr)
            client.monitor(args.duration)
        else:
            client.monitor(args.duration)
    finally:
        try:
            if client is not None:
                client.finish()
        finally:
            try:
                if port is not None:
                    port.close()
            finally:
                if capture is not None:
                    capture.close()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
    except (OSError, RuntimeError, ValueError, SyntaxError, zipfile.BadZipFile) as exc:
        print("cyberpi: " + str(exc), file=sys.stderr)
        raise SystemExit(1)
