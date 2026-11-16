// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote.proto;
import org.junit.Test;
import static org.junit.Assert.*;
import java.io.IOException;
public class EnvelopeTest {
 @Test public void preservesCorrelationAndPayload()throws Exception {
  var value=new Envelope(123,"test.EchoCommand",new byte[]{8,42});
  var decoded=Envelope.decode(Frames.decode(Frames.encode(9,value.encode())).payload());
  assertEquals(123,decoded.requestId());assertEquals(value.type(),decoded.type());assertArrayEquals(value.payload(),decoded.payload());
 }
 @Test public void rejectsOversizeAndMalformedMessages() {
  assertThrows(IOException.class,()->new Envelope(1,"test.Command",new byte[238]).encode());
  assertThrows(IOException.class,()->Envelope.decode(new byte[]{18,10,65}));
  assertThrows(IOException.class,()->Envelope.decode(new byte[]{8,1}));
 }
}
