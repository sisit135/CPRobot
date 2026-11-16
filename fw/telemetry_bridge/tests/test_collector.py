# SPDX-License-Identifier: GPL-3.0-only

import json
import subprocess
import tempfile
import unittest
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "host"))
import collector

class ProtocolTests(unittest.TestCase):
    def test_c_to_python_and_chunks(self):
        binary = Path(__file__).with_name("wire_test")
        n, hexed = subprocess.check_output([str(binary)], text=True).split()
        payload = bytes.fromhex(hexed)
        self.assertEqual(len(payload), int(n))
        log = collector.decode_ekf(payload)
        self.assertEqual((log["grid_x"], log["grid_y"]), (-1, 2))
        self.assertEqual(log["sequence"], 9)
        f = collector.frame(1, payload)
        parsed = collector.FrameParser()
        self.assertEqual(parsed.feed(f[:1]), [])
        self.assertEqual(parsed.feed(f[1:]), [(1,payload)])
        chunks = collector.BleChunks()
        pieces = [bytes((0xb7, 19, i, 2)) + f[i*30:(i+1)*30] for i in range(2)]
        self.assertEqual(chunks.feed(pieces[1]), [])
        self.assertEqual(chunks.feed(pieces[0]), [(1,payload)])
    def test_sink_gap_and_control(self):
        from io import StringIO
        file = StringIO(); sink=collector.Sink(file,{})
        payload=bytes.fromhex(subprocess.check_output([str(Path(__file__).with_name("wire_test"))],text=True).split()[1])
        sink.emit(payload,"tls"); sink.emit(payload,"ble")
        first,second=map(json.loads,file.getvalue().splitlines())
        self.assertEqual(first["sequence_gap"],0)
        self.assertTrue(second["late_or_duplicate"])
        self.assertEqual(collector.FrameParser().feed(collector.frame(2,collector.encode_control(3,2)))[0][0],2)
        with self.assertRaises(ValueError): collector.FrameParser().feed(b"\xff\xff")

if __name__ == "__main__": unittest.main()
