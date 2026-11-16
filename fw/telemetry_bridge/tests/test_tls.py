# SPDX-License-Identifier: GPL-3.0-only

import asyncio
import contextlib
import ssl
import struct
import subprocess
import sys
import tempfile
import unittest
from io import StringIO
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"host"))
import collector

class TlsTests(unittest.IsolatedAsyncioTestCase):
    async def test_pinned_ipv6_stream_and_control(self):
        with tempfile.TemporaryDirectory() as directory:
            cert=Path(directory)/"cert.pem"; key=Path(directory)/"key.pem"
            subprocess.run(["openssl","req","-x509","-newkey","ec","-pkeyopt","ec_paramgen_curve:P-256",
              "-nodes","-days","1","-keyout",str(key),"-out",str(cert),"-subj","/CN=robot-telemetry.local",
              "-addext","subjectAltName=DNS:robot-telemetry.local"],check=True,capture_output=True)
            ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); ctx.load_cert_chain(str(cert),str(key))
            payload=bytes.fromhex(subprocess.check_output([str(Path(__file__).with_name("wire_test"))],text=True).split()[1])
            result=[]
            async def server(reader, writer):
                head=await reader.readexactly(3)
                body=await reader.readexactly(struct.unpack("<H",head[:2])[0]-1)
                result.append((head[2],body))
                writer.write(collector.frame(3,b"\x08\x01\x10\x02\x18\x00"))
                wire=collector.frame(1,payload)
                writer.write(wire[:1]); await writer.drain()
                writer.write(wire[1:]); await writer.drain()
                await asyncio.sleep(.05)
                writer.close(); await writer.wait_closed()
            listener=await asyncio.start_server(server,"::1",0,ssl=ctx)
            try:
                port=listener.sockets[0].getsockname()[1]
                out=StringIO(); sink=collector.Sink(out,{},display=False)
                commands=asyncio.Queue(); await commands.put(collector.frame(2,collector.encode_control(1,2)))
                await asyncio.wait_for(collector.collect_tls("::1",port,str(cert),sink,commands),5)
                self.assertEqual(result[0][0],2)
                self.assertEqual(collector.decode_ack(b"\x08\x01\x10\x02\x18\x00")["status"],0)
                self.assertIn('"boot_id":77',out.getvalue())
            finally:
                listener.close(); await listener.wait_closed()

if __name__ == "__main__": unittest.main()
