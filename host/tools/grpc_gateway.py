#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Optional loopback HTTP/2 gRPC gateway to the existing framed ESP TLS transport.

RPC input is not retried. A lost connection leaves mutations unconfirmed.
Run generate_python.py first, and use server.crt (public certificate only).
"""
import argparse, concurrent.futures, pathlib, socket, ssl, struct, sys, threading
import grpc
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent/'generated'))
import remote_config_pb2 as pb
import remote_config_pb2_grpc as rpc
import telemetry_pb2 as telemetry

class Gateway(rpc.RemoteConfigServicer, rpc.PlatformServiceServicer):
    def __init__(self,host,certificate):
        context=ssl.create_default_context(cafile=certificate)
        context.check_hostname=False # Explicitly pinned CA/leaf; link-local address varies.
        self.socket=context.wrap_socket(socket.create_connection((host,7443),timeout=5),server_hostname=None)
        self.socket.settimeout(8);self.lock=threading.Lock();self.sequence=0;self.failed=False
    def exact(self,n):
        out=bytearray()
        while len(out)<n:
            b=self.socket.recv(n-len(out))
            if not b:raise EOFError('Telemetry link closed')
            out.extend(b)
        return bytes(out)
    def call(self,req,context,operation):
        if req.operation!=operation:context.abort(grpc.StatusCode.INVALID_ARGUMENT,'RPC method/operation mismatch')
        with self.lock:
            if self.failed:context.abort(grpc.StatusCode.UNAVAILABLE,'Reconnect gateway; previous mutation may be unconfirmed')
            self.sequence=(self.sequence%0x7ffffffe)+1
            wire=pb.RemoteRequest();wire.CopyFrom(req);wire.request_id=self.sequence
            payload=wire.SerializeToString()
            try:
                self.socket.sendall(struct.pack('<HB',len(payload)+1,4)+payload)
                while True:
                    size,=struct.unpack('<H',self.exact(2))
                    if not 2<=size<=241:raise ValueError('Invalid frame size')
                    data=self.exact(size)
                    if data[0]==5:
                        reply=pb.RemoteReply.FromString(data[1:])
                        if reply.request_id==self.sequence:
                            reply.request_id=req.request_id
                            return reply
            except Exception as e:
                self.failed=True;self.socket.close();context.abort(grpc.StatusCode.UNAVAILABLE,f'{e}; operation unconfirmed, no retry')
    def GetSchema(self,r,c):return self.call(r,c,pb.GET_SCHEMA)
    def GetSource(self,r,c):return self.call(r,c,pb.GET_SOURCE)
    def ListFields(self,r,c):return self.call(r,c,pb.LIST_FIELDS)
    def Read(self,r,c):return self.call(r,c,pb.READ)
    def WriteIfChanged(self,r,c):return self.call(r,c,pb.WRITE)
    def CalibrateGyro(self,r,c):return self.call(r,c,pb.CALIBRATE_GYRO)
    def CalibrateColor(self,request,context):
        if request.color_id_mapped>3:
            context.abort(grpc.StatusCode.INVALID_ARGUMENT,'color_id_mapped must be 0..3')
        with self.lock:
            if self.failed:context.abort(grpc.StatusCode.UNAVAILABLE,'Reconnect gateway; calibration may be unconfirmed')
            self.sequence=(self.sequence%0x7ffffffe)+1
            wire=pb.CalibrateColorRequest(request_id=self.sequence,color_id_mapped=request.color_id_mapped)
            try:
                payload=wire.SerializeToString()
                self.socket.sendall(struct.pack('<HB',len(payload)+1,8)+payload)
                while True:
                    size,=struct.unpack('<H',self.exact(2))
                    if not 2<=size<=241:raise ValueError('Invalid frame size')
                    data=self.exact(size)
                    if data[0]==3:
                        ack=telemetry.ControlAck.FromString(data[1:])
                        if ack.request_id==self.sequence:
                            ack.request_id=request.request_id
                            return ack
            except Exception as error:
                self.failed=True;self.socket.close()
                context.abort(grpc.StatusCode.UNAVAILABLE,f'{error}; calibration unconfirmed, no retry')

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--host',required=True);ap.add_argument('--cert',required=True);ap.add_argument('--port',type=int,default=50051);args=ap.parse_args()
    gateway=Gateway(args.host,args.cert)
    server=grpc.server(concurrent.futures.ThreadPoolExecutor(max_workers=2));rpc.add_RemoteConfigServicer_to_server(gateway,server);rpc.add_PlatformServiceServicer_to_server(gateway,server)
    server.add_insecure_port(f'127.0.0.1:{args.port}');server.start()
    print(f'gRPC listening on loopback 127.0.0.1:{args.port}');server.wait_for_termination()
