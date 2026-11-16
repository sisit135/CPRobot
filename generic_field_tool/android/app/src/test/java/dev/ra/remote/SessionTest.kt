// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote
import com.google.protobuf.ByteString
import com.google.protobuf.DynamicMessage
import dev.ra.remote.proto.DynamicProto
import dev.ra.remote.proto.Frames
import dev.ra.remote.proto.Envelope
import com.google.protobuf.DescriptorProtos
import kotlinx.coroutines.*
import kotlinx.coroutines.channels.Channel
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import java.io.File
class SessionTest {
 private class Fake:Link {
  override val label="Fake connected link"
  val descriptor=File("src/main/assets/robot.desc").readBytes()
  val registry=DynamicProto(descriptor)
  val fields=mutableListOf<ConfigField>()
  val values=mutableMapOf<Int,Float>()
  var revision=1;var writes=0;var failKey=-1;var busy=false;var dropWriteAck=false
  var genericWrites=0;var wrongResponseType=false;var dropGenericAck=false
  val replies=Channel<Frames.Frame>(Channel.UNLIMITED)
  init {
   fields+=ConfigField(1,"sigma_position_rw","EKF",0,.001f,2f,"localization/config.hpp",.12f,factoryDefault=.12f,sourceId=1)
   fields+=ConfigField(2,"sigma_gyro_rw","EKF",0,.00001f,1f,"localization/config.hpp",.035f)
   for(base in listOf(100,110,120,130))for(i in 0..6)fields+=ConfigField(base+i,"ref${base}_$i","Color references",if(i in 1..3)0 else 1,0f,if(i in 5..6)5f else if(i==4)2f else 1f,"localization/color_tracker.hpp",if(i in 1..3)1f/3 else 0f)
   fields.forEach{values[it.key]=it.value}
  }
  override suspend fun open()=Unit
  override suspend fun read()=replies.receive()
  override fun close(){replies.close()}
  override suspend fun send(kind:Int,bytes:ByteArray){
   if(kind==9){
    genericWrites++;if(dropGenericAck)return;val envelope=Envelope.decode(bytes)
    replies.send(Frames.Frame(10,Envelope(envelope.requestId()+1,"test.EchoResponse",envelope.payload()).encode()))
    replies.send(Frames.Frame(10,Envelope(envelope.requestId(),if(wrongResponseType)"test.OtherResponse"else "test.EchoResponse",envelope.payload()).encode()))
    return
   }
   val req=registry.parse(REQUEST,bytes);val id=DynamicProto.number(req,"request_id");val op=DynamicProto.number(req,"operation");val key=DynamicProto.number(req,"key");val offset=DynamicProto.number(req,"offset")
   val out=mutableMapOf<String,Any>("request_id" to id,"status" to 0,"revision" to revision,"provisioned" to true)
   when(op){
    1->{val n=minOf(128,descriptor.size-offset);out["schema_chunk"]=ByteString.copyFrom(descriptor,offset,n);out["total"]=descriptor.size;out["offset"]=offset}
    2->{val f=fields[offset];out["field"]=registry.build("axi.remote.v1.FieldSpec",mapOf("key" to f.key,"name" to f.name,"group" to f.group,"type" to f.type,"minimum" to f.minimum,"maximum" to f.maximum,"source" to f.source,"default_value" to (f.factoryDefault?:0f),"source_id" to f.sourceId));out["total"]=fields.size;out["offset"]=offset}
    3->out["value"]=values.getValue(key)
    4->{val value=DynamicProto.decimal(req,"value");when{busy->out["status"]=3;DynamicProto.number(req,"expected_revision")!=revision->out["status"]=2;key==failKey->out["status"]=4;else->{if(values[key]!=value){values[key]=value;revision++;writes++};out["revision"]=revision;out["value"]=value}}}
   }
   if(op==4&&dropWriteAck)return
   replies.send(Frames.Frame(5,registry.build(REPLY,out).toByteArray()))
  }
 }
 private suspend fun session(block:suspend(RobotSession,Fake)->Unit){
  val fake=Fake();val scope=CoroutineScope(SupervisorJob()+Dispatchers.IO);val s=RobotSession(fake,fake.registry,scope,{_,_->},{})
  try{s.start{};block(s,fake)}finally{s.close();scope.cancel()}
 }
 @Test fun fetchedFactoryDefaultAndSourceStayIndependentOfSavedValue()=runBlocking{session{s,f->val first=s.fetch{_,_->}.fields.first();assertEquals(.12f,first.factoryDefault!!,0f);assertEquals(1,first.sourceId);s.write(first,.2f,f.revision);val next=s.fetch{_,_->}.fields.first();assertEquals(.2f,next.value,0f);assertEquals(.12f,next.factoryDefault!!,0f);assertEquals(1,next.sourceId)}}
 @Test fun exportRestoreUnchangedNeverWrites()=runBlocking{session{s,f->val c=s.fetch{_,_->};val export=s.export(c);s.restore(export){_,_,_->};assertEquals(0,f.writes);assertEquals(1,f.revision)}}
 @Test fun sequentialChangedOnlyAndVerified()=runBlocking{session{s,f->val c=s.fetch{_,_->};val json=JSONObject(s.export(c));json.getJSONArray("values").getJSONObject(0).put("value",.2);json.getJSONArray("values").getJSONObject(1).put("value",.08);val restored=s.restore(json.toString()){_,_,_->};assertEquals(2,f.writes);assertEquals(3,restored.revision);assertEquals(.2f,restored.fields[0].value,0f)}}
 @Test fun preflightRejectsInvalidBeforeWrites()=runBlocking{session{s,f->val json=JSONObject(s.export(s.fetch{_,_->}));json.getJSONArray("values").getJSONObject(0).put("value",.2);json.getJSONArray("values").getJSONObject(1).put("value",500);assertTrue(runCatching{s.restore(json.toString()){_,_,_->}}.isFailure);assertEquals(0,f.writes)}}
 @Test fun nackAndPartialRestoreAreVisible()=runBlocking{session{s,f->val c=s.fetch{_,_->};f.busy=true;assertTrue(runCatching{s.write(c.fields[0],.2f,c.revision)}.exceptionOrNull()?.message?.contains("busy")==true);f.busy=false;f.failKey=2;val json=JSONObject(s.export(c));json.getJSONArray("values").getJSONObject(0).put("value",.2);json.getJSONArray("values").getJSONObject(1).put("value",.08);val error=runCatching{s.restore(json.toString()){_,_,_->}}.exceptionOrNull();assertTrue(error?.message?.contains("Applied keys remain")==true);assertEquals(1,f.writes);assertEquals(.2f,f.values.getValue(1),0f);assertEquals(.035f,f.values.getValue(2),0f)}}
 @Test fun lostAckIsUnconfirmedAndNeverRetried()=runBlocking{session{s,f->val c=s.fetch{_,_->};f.dropWriteAck=true;val error=runCatching{s.write(c.fields[0],.2f,c.revision)}.exceptionOrNull();assertTrue(error?.message?.contains("unconfirmed")==true);assertEquals(1,f.writes)}}
 @Test fun externalRevisionConflictStopsBeforeMutation()=runBlocking{session{s,f->val c=s.fetch{_,_->};f.revision++;assertTrue(runCatching{s.write(c.fields[0],.2f,c.revision)}.isFailure);assertEquals(0,f.writes)}}
 private fun commandSchema():DynamicProto {
  val file=DescriptorProtos.FileDescriptorProto.newBuilder().setName("commands.proto").setSyntax("proto3").setPackage("test")
  for(name in listOf("EchoCommand","EchoRequest","EchoResponse"))file.addMessageType(DescriptorProtos.DescriptorProto.newBuilder().setName(name).addField(DescriptorProtos.FieldDescriptorProto.newBuilder().setName("text").setNumber(1).setType(DescriptorProtos.FieldDescriptorProto.Type.TYPE_STRING)))
  return DynamicProto(DescriptorProtos.FileDescriptorSet.newBuilder().addFile(file).build().toByteArray())
 }
 @Test fun genericCommandCorrelatesResponseAndIgnoresWrongId()=runBlocking{session{s,f->s.projectSchema=commandSchema();val result=s.sendCommand("test.EchoCommand","{\"text\":\"hello\"}");assertTrue(result.contains("test.EchoResponse"));assertTrue(result.contains("hello"));assertEquals(1,f.genericWrites)}}
 @Test fun genericRequestUsesSuppliedSchemaAndMatchingCorrelatedResponse()=runBlocking{session{s,f->s.projectSchema=commandSchema();val result=s.sendCommand("test.EchoRequest","{\"text\":\"hud\"}");assertTrue(result.contains("test.EchoResponse"));assertTrue(result.contains("hud"));assertEquals(1,f.genericWrites)}}
 @Test fun genericCommandRejectsWrongResponseTypeWithoutReplay()=runBlocking{session{s,f->s.projectSchema=commandSchema();f.wrongResponseType=true;assertTrue(runCatching{s.sendCommand("test.EchoCommand","{}")} .exceptionOrNull()?.message?.contains("Unexpected response type")==true);assertEquals(1,f.genericWrites)}}

 @Test fun genericDisconnectIsUnconfirmedAndNeverReplayed()=runBlocking{session{s,f->
  s.projectSchema=commandSchema();f.dropGenericAck=true
  val result=async{runCatching{s.sendCommand("test.EchoCommand","{}")}}
  while(f.genericWrites==0)yield()
  s.close();assertTrue(result.await().exceptionOrNull()?.message?.contains("unconfirmed")==true);assertEquals(1,f.genericWrites)
 }}
 @Test fun genericInvalidOrOversizedDraftNeverSends()=runBlocking{session{s,f->
  s.projectSchema=commandSchema()
  assertTrue(runCatching{s.sendCommand("test.EchoCommand","{\"unknown\":1}")}.isFailure)
  assertTrue(runCatching{s.sendCommand("test.EchoCommand",JSONObject().put("text","x".repeat(240)).toString())}.isFailure)
  assertEquals(0,f.genericWrites)
  assertTrue(s.sendCommand("test.EchoCommand","{}").contains("EchoResponse"));assertEquals(1,f.genericWrites)
 }}

}
