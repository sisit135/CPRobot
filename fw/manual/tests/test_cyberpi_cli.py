"""Wire contracts from executing the actual mBlock meoser JavaScript composer."""
import contextlib
import importlib.util
import io
import json
import os
import select
import subprocess
import tempfile
import threading
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('cyberpi_cli', ROOT / 'tools/cyberpi_cli.py')
cli = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cli)


class Trace:
    raw = False
    def __init__(self): self.texts = []; self.frames = []; self.bytes = bytearray()
    def received(self, data): self.bytes.extend(data)
    def text(self, data): self.texts.append(data)
    def packet(self, data): self.frames.append(data)
    def finish(self): pass


class Serial:
    in_waiting = 0
    def __init__(self, respond):
        self.respond = respond
        self.pending = bytearray()
        self.sent = []
        self.closed = False
        self.now = 0
    def write(self, packet):
        self.sent.append(packet)
        self.pending.extend(self.respond(packet))
        return len(packet)
    def flush(self): pass
    def read(self, count):
        self.now += .05
        data = bytes(self.pending[:count])
        del self.pending[:count]
        return data
    def close(self): self.closed = True


def ack(status=0):
    return cli.frame(b'\x01\x00\x5e\xf0\x01' + bytes((status,)))


class ProtocolTest(unittest.TestCase):
    def test_matches_executed_official_js_composer(self):
        vectors = json.loads((ROOT / 'tests/uploader_golden.json').read_text())
        for vector in vectors:
            with self.subTest(size=len(vector['data']) // 2):
                actual = list(cli.upload_frames(vector['path'], bytes.fromhex(vector['data'])))
                self.assertEqual(vector['frames'], [packet.hex() for packet in actual])

    def test_utf8_file_header_and_source_length(self):
        data = 'print("สวัสดี")\n'.encode()
        packets = list(cli.upload_frames('/flash/_xx_ADR.py', data))
        self.assertEqual(len(data), struct.unpack_from('<I', packets[0], 11)[0])
        self.assertEqual(data, packets[1][14:-2])

    def test_frame_handles_all_byte_values_and_split_reads(self):
        payload = bytes(range(256))
        wire = b'trace\r\n' + cli.frame(payload) + b'next\n'
        decoder = cli.StreamDecoder()
        events = []
        for byte in wire: events.extend(decoder.feed(bytes((byte,))))
        self.assertEqual([payload], [v for kind, v in events if kind == 'frame'])
        self.assertEqual(b'trace\r\nnext\n', b''.join(v for kind, v in events if kind == 'text'))
        self.assertEqual(b'', decoder.finish())

    def test_bad_checksum_and_oversized_header_resynchronize(self):
        corrupt = bytearray(cli.frame(b'\x06garbled'))
        corrupt[-2] ^= 1
        events = cli.StreamDecoder(max_payload=256).feed(bytes(corrupt) + cli.frame(b'x' * 257) + cli.frame(b'\x06good'))
        self.assertEqual([b'\x06good'], [v for kind, v in events if kind == 'frame'])

    def test_mode_version_and_firmware_path_selection(self):
        def respond(packet):
            payload = packet[4:-2]
            if payload == b'\x06': return cli.frame(b'\x0644.01.016')
            if payload == b'\x0d\x80': return cli.frame(b'\x0d\x80\x00')
            return b''
        port = Serial(respond)
        client = cli.Client(port, Trace(), clock=lambda: port.now)
        self.assertEqual('44.01.016', client.version())
        client.set_mode(0)
        self.assertEqual('/flash/main.py', cli.project_path('44.01.005', 'ADR'))
        self.assertEqual('/flash/_xx_ADR.py', cli.project_path('44.01.006', 'ADR'))
        for version in ('1.0.0', '44.02.016', 'nonsense'):
            with self.assertRaises(cli.ProtocolError): cli.project_path(version, 'ADR')

    def test_version_waits_through_hardware_boot_and_retries_only_read_query(self):
        port = Serial(lambda packet: cli.frame(b'\x0644.01.016') if port.now >= 9 else b'')
        client = cli.Client(port, Trace(), clock=lambda: port.now)
        self.assertEqual('44.01.016', client.version())
        self.assertGreater(len(port.sent), 1)
        self.assertTrue(all(packet == cli.frame(b'\x06') for packet in port.sent))

    def test_upload_waits_for_each_ack_and_keeps_traces(self):
        port = Serial(lambda packet: b'custom trace\n' + ack())
        trace = Trace()
        client = cli.Client(port, trace, clock=lambda: port.now)
        data = b'x' * 161
        client.upload('/flash/_xx_ADR.py', data)
        self.assertEqual(4, len(port.sent))
        self.assertEqual(list(cli.upload_frames('/flash/_xx_ADR.py', data)), port.sent)
        self.assertEqual(b'custom trace\n' * 4, b''.join(trace.texts))

    def test_upload_rejects_error_and_never_sends_body(self):
        port = Serial(lambda packet: ack(0xf0))
        client = cli.Client(port, Trace(), clock=lambda: port.now)
        with self.assertRaisesRegex(cli.ProtocolError, 'status 0xf0'):
            client.upload('/flash/main.py', b'x')
        self.assertEqual(1, len(port.sent))

    def test_timeout_does_not_replay_body_or_claim_success(self):
        count = 0
        def respond(packet):
            nonlocal count
            count += 1
            return ack() if count == 1 else b''
        port = Serial(respond)
        client = cli.Client(port, Trace(), timeout=.2, clock=lambda: port.now)
        with self.assertRaises(TimeoutError): client.upload('/flash/main.py', b'x' * 161)
        self.assertEqual(2, len(port.sent))

    def test_header_timeout_retries_only_twice(self):
        port = Serial(lambda packet: b'')
        client = cli.Client(port, Trace(), timeout=.2, clock=lambda: port.now)
        with self.assertRaises(TimeoutError): client.upload('/flash/main.py', b'x')
        self.assertEqual(2, len(port.sent))

    def test_monitor_is_passive_and_captures_exact_wire_bytes(self):
        trace = Trace()
        port = Serial(lambda packet: b'')
        wire = b'[ADL] startup\n' + cli.frame(b'\x08\xc0') + 'อักษร\n'.encode()
        port.pending.extend(wire)
        client = cli.Client(port, trace, clock=lambda: port.now)
        client.monitor(10)
        self.assertEqual([], port.sent)
        self.assertEqual(wire, bytes(trace.bytes))
        self.assertEqual(b'[ADL] startup\n' + 'อักษร\n'.encode(), b''.join(trace.texts))

    def test_exec_uses_ide_command_four_and_utf8_length(self):
        code = 'print("é")'
        self.assertEqual(b'\x28\x04' + struct.pack('<HH', 42, len(code.encode())) + code.encode(),
                         cli.execute_payload(code, 42))

    def test_loader_stub_dependencies_are_read_without_network_or_serial(self):
        with patch.object(cli, 'open_serial', side_effect=AssertionError('serial should be unused')):
            info = cli.stub_info(cli.DEFAULT_STUB)
            with contextlib.redirect_stdout(io.StringIO()): self.assertEqual(0, cli.main(['stub-info']))
        self.assertEqual('1.1.46', info['cyberpi_version'])
        self.assertEqual(['mbot2', 'firefly_bluetoothcontroller'], info['extensions'])

    def test_trace_sink_utf8_fragmentation_and_binary_capture(self):
        wire = 'trace: é\n'.encode()
        capture, output = io.BytesIO(), io.StringIO()
        trace = cli.TraceSink(capture)
        with contextlib.redirect_stdout(output):
            for byte in wire:
                part = bytes((byte,))
                trace.received(part)
                trace.text(part)
            trace.finish()
        self.assertEqual(wire, capture.getvalue())
        self.assertEqual(wire.decode(), output.getvalue())


@unittest.skipUnless(os.name == 'posix' and importlib.util.find_spec('serial'),
                     'POSIX and pyserial required for serial-driver integration')
class SerialDriverTest(unittest.TestCase):
    def test_cli_upload_and_monitor_through_real_pyserial(self):
        master, slave = os.openpty()
        stopped = threading.Event()
        packets, failures = [], []
        def device():
            decoder = cli.StreamDecoder()
            try:
                while not stopped.is_set():
                    ready, _, _ = select.select([master], [], [], .05)
                    if not ready: continue
                    for kind, payload in decoder.feed(os.read(master, 4096)):
                        if kind != 'frame': continue
                        packets.append(payload)
                        if payload == b'\x06': reply = cli.frame(b'\x0644.01.016')
                        elif payload == b'\x0d\x80': reply = cli.frame(b'\x0d\x80\x00')
                        elif payload[:3] == cli.FILE_PREFIX:
                            reply = ack()
                            if payload[3] == 2: reply += b'[ADL boot] serial simulator ready\r\n'
                        else: continue
                        os.write(master, reply)
            except Exception as exc:
                failures.append(exc)
        worker = threading.Thread(target=device, daemon=True)
        worker.start()
        try:
            with tempfile.TemporaryDirectory() as temp:
                source = Path(temp) / 'program.py'
                capture = Path(temp) / 'capture.bin'
                source.write_text('print("hello from USB")\n')
                result = subprocess.run([sys.executable, '-B', str(ROOT / 'tools/cyberpi_cli.py'),
                    '-p', os.ttyname(slave), '--capture', str(capture), 'upload', str(source),
                    '--monitor', '--duration', '.15'], capture_output=True, timeout=5)
                self.assertEqual(0, result.returncode, result.stderr.decode())
                self.assertIn(b'Transfer acknowledged', result.stderr)
                self.assertIn(b'serial simulator ready', result.stdout)
                self.assertIn(b'[ADL boot]', capture.read_bytes())
                self.assertEqual([b'\x06', b'\x0d\x00\x00', b'\x0d\x80'], packets[:3])
                sent = [cli.frame(payload) for payload in packets[3:]]
                self.assertEqual(list(cli.upload_frames('/flash/_xx_ADR.py', source.read_bytes())), sent)
        finally:
            stopped.set()
            worker.join(1)
            os.close(master)
            os.close(slave)
        self.assertEqual([], failures)


if __name__ == '__main__': unittest.main()
