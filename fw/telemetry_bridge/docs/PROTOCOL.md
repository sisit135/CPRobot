# Telemetry wire specification

Current initialization, connection ownership, and operator workflows are maintained
in [field-tool architecture](../../../docs/field_tool_architecture.md). THS flash
lifecycle is maintained in [THS firmware architecture](../../../docs/fw_ths_architecture.md#remote-configuration-and-storage).
The STM32 remote service is required in every standard firmware profile.
`proto/flash_settings.json` generates the field catalog and `FlashSettings`
descriptor; field numbers equal stable settings keys. `FieldSpec` includes
factory values and source-page IDs as well as type/bounds metadata.

## Framing

| Medium | Bytes |
|---|---|
| TLS / reassembled NUS | LE16 length including kind, uint8 kind, serialized protobuf |
| NUS notifications | `b7,frame_id,index,count` then stream-frame fragment |
| Encrypted NUS command write | kind byte (2, 4, 8, or 9), complete stream frame |
| SPI legacy EKF | `a6,51,lenLE16,EkfLog,CRC16`, padded to 256 |
| SPI typed request/reply | `a6,54,lenLE16,kind,protobuf,CRC16`, padded to 256 |
| SPI tail at offset 246 | existing 10-byte Control ACK (current STM32 adapter does not emit pairing-window events) |

Kinds: 9/10/11 generic command/response/sample Envelopes; 1 EkfLog, 2 Control, 3 ControlAck, 4 RemoteRequest, 5 RemoteReply,
6 LinkInfo (BLE out-of-band only), 7 LogRecord, 8 CalibrateColorRequest.
Typed SPI protobuf is limited to 238 bytes. Color calibration is transported
as kind 8 but forwarded over SPI in the existing control header (operation 3,
status byte=color slot 0..3); its result returns as kind 3 ControlAck.
The old Control header and log fields are preserved. No application retry,
ordering layer or IP-dependent protobuf envelope is introduced.

## Remote result status

| Status | Meaning |
|---|---|
| OK = 0 | Value durably committed, or already equal (no flash write) |
| INVALID = 1 | Nonfinite/out-of-range/nonintegral value or invalid final reference |
| CONFLICT = 2 | Expected revision differs; fetch before trying again |
| BUSY = 3 | Navigation has started, or bridge has an outstanding config request |
| IO_ERROR = 4 | Persistence failed; fetch to inspect actual state |
| NOT_FOUND = 5 | Key/page/source is not exposed |
| NOT_READY = 6 | Store or fresh stationary gyro samples unavailable |

GATT write completion or a TCP send is not the config ACK. Transport loss or
reply timeout is **unconfirmed**, not success and not proof that no write ran.
Malformed transport/protobuf bytes can be rejected at the transport boundary;
only a valid, correlated RemoteReply establishes an application ACK/NACK.
