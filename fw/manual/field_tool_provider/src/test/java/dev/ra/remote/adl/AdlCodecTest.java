// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote.adl;
import dev.ra.remote.proto.DynamicProto;
import java.util.Map;
import org.junit.Test;
import static org.junit.Assert.*;

public class AdlCodecTest {
    @Test public void generatedCodecMatchesFirmwareGoldenPacket() throws Exception {
        DynamicProto schema=new DynamicProto(AdlCodec.descriptor());
        assertEquals(0x29b1,AdlCodec.crc("123456789"));
        assertEquals("1|42|2|-60|60|1|d814",AdlCodec.encode(42,"adl.v1.DriveRequest",
            schema.build("adl.v1.DriveRequest",Map.of("left_rpm",-60,"right_rpm",60,"deadman",true))));
        assertArrayEquals(new int[]{42,2,0,1},AdlCodec.reply("1|42|2|0|1|e845"));
        assertEquals(7,schema.names().stream().filter(n->n.endsWith("Request")).count());
    }
    @Test public void invalidPacketsAndOutOfBoundsInputAreRejected() throws Exception {
        DynamicProto schema=new DynamicProto(AdlCodec.descriptor());
        assertThrows(IllegalArgumentException.class,()->AdlCodec.encode(42,"adl.v1.DriveRequest",
            schema.build("adl.v1.DriveRequest",Map.of("left_rpm",61))));
        assertThrows(IllegalArgumentException.class,()->AdlCodec.reply("1|42|2|0|1|0000"));
        assertThrows(IllegalArgumentException.class,()->AdlCodec.reply(AdlCodec.seal("2|42|2|0|1")));
        assertThrows(IllegalArgumentException.class,()->AdlCodec.reply(AdlCodec.seal("1|42|99|0|1")));
        assertThrows(IllegalArgumentException.class,()->AdlCodec.opcode("adl.v1.DriveCommand"));
    }
}
