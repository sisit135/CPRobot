// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote.proto;
import org.junit.Test;
import static org.junit.Assert.*;
import com.google.protobuf.*;
import java.nio.file.*;
import java.io.*;
import java.util.*;
public class ProtocolTest {
 private DynamicProto registry()throws Exception{return new DynamicProto(Files.readAllBytes(Path.of("../app/src/main/assets/robot.desc")));}
 @Test public void dynamicRoundtrip()throws Exception{
  var r=registry();assertTrue(r.names().contains("axi.telemetry.v1.EkfLog"));
  byte[] bytes=r.json("axi.remote.v1.RemoteRequest","{\"requestId\":7,\"operation\":\"WRITE\",\"key\":1,\"value\":0.25,\"expectedRevision\":3}");
  var m=r.parse("axi.remote.v1.RemoteRequest",bytes);assertEquals(7,DynamicProto.number(m,"request_id"));assertEquals(.25f,DynamicProto.decimal(m,"value"),0);
  r.requireBootstrapCompatible(registry());
 }
 @Test public void outOfOrderImportsResolve()throws Exception{
  var dep=DescriptorProtos.FileDescriptorProto.newBuilder().setName("dep.proto").setSyntax("proto3").setPackage("test").addMessageType(DescriptorProtos.DescriptorProto.newBuilder().setName("Child"));
  var outer=DescriptorProtos.FileDescriptorProto.newBuilder().setName("outer.proto").setSyntax("proto3").setPackage("test").addDependency("dep.proto").addMessageType(DescriptorProtos.DescriptorProto.newBuilder().setName("Parent"));
  var r=new DynamicProto(DescriptorProtos.FileDescriptorSet.newBuilder().addFile(outer).addFile(dep).build().toByteArray());assertEquals(2,r.names().size());
 }
 @Test public void missingImportFails()throws Exception{
  var f=DescriptorProtos.FileDescriptorProto.newBuilder().setName("a.proto").addDependency("missing.proto");
  assertThrows(IllegalArgumentException.class,()->new DynamicProto(DescriptorProtos.FileDescriptorSet.newBuilder().addFile(f).build().toByteArray()));
  assertThrows(IllegalArgumentException.class,()->new DynamicProto(new byte[65537]));
 }
 @Test public void streamAndNusChunks()throws Exception{
  var r=registry();byte[] p=r.json("axi.telemetry.v1.Control","{\"requestId\":1,\"operation\":2}");
  byte[] frame=Frames.encode(2,p);var decoded=Frames.read(new ByteArrayInputStream(frame));assertEquals(2,decoded.kind());assertArrayEquals(p,decoded.payload());
  var assembler=new Frames.Chunks();byte[] a=new byte[8],b=new byte[frame.length];a[0]=b[0]=(byte)0xb7;a[1]=b[1]=9;a[2]=0;b[2]=1;a[3]=b[3]=2;System.arraycopy(frame,0,a,4,4);System.arraycopy(frame,4,b,4,frame.length-4);
  assertNull(assembler.accept(a));assertArrayEquals(p,assembler.accept(b).payload());
  assertNull(assembler.accept(a));b[1]=10;assertThrows(IOException.class,()->assembler.accept(b));
  assertThrows(IOException.class,()->Frames.decode(new byte[]{0,0,1}));
  assertThrows(IOException.class,()->Frames.read(new ByteArrayInputStream(new byte[]{(byte)255,127})));
 }
}
