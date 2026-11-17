# ADR CyberPi USB serial tooling

[`cyberpi_cli.py`](../fw/manual/tools/cyberpi_cli.py) communicates directly with
the CyberPi USB serial port at 115200 baud. It implements the mBlock application
protocol, without mLink, Bluetooth, a browser, raw REPL, or firmware flashing.
Installation, validation, and commands are consolidated in
[building](building.md#adr-usb-serial-cli).

## Commands and ownership

| Command | Behavior |
| --- | --- |
| `ports` | List local serial ports with USB identifiers |
| `stub-info` | Read the loader project and its declared dependencies, offline |
| `info` | Query firmware version and current upload/live mode |
| `mode upload` / `mode live` | Explicitly change mode, then query it back |
| `upload FILE` | Validate UTF-8 Python source, query firmware, enter upload mode, send a file and await each acknowledgement |
| `exec --code CODE` / `exec --file FILE` | Send the IDE's command-4 Python execution request and collect output; it does not automatically switch mode |
| `monitor` | Passively collect serial text without transmitting protocol commands |

Specify `--port` explicitly for device commands. Close the IDE/mLink serial
connection first so only one process owns the port. The client sets DTR/RTS
inactive before opening and does not deliberately pulse reset or boot pins.
The tested CH340 connection nevertheless restarts the board when opened.
Device commands retry only the read-only version query for up to 45 seconds
while boot completes; Python execution and file bodies are never replayed. Port discovery
alone never opens a device. `monitor` sends no bytes and does not alter REPL,
parser, Bluetooth, or mode configuration. `exec` changes device state according
to the supplied Python; the CLI does not await an execution result, so successful transmission alone
is not execution confirmation. The tested device emits a `28 01` result frame
after a short command 4, alongside printed output. Longer execution requests
were not confirmed during this session; use file upload for complete programs.

`--capture FILE` appends the exact incoming bytes, including protocol frames, to
a binary capture. The default display separates valid F3/F4 frames from text and
decodes text incrementally as UTF-8. `--show-frames` prints frame payloads to
stderr; `monitor --raw` displays all bytes. Unsupported/corrupt protocol traffic
can appear as replacement characters; retain a capture when diagnosing it.

Upload defaults follow the CyberPi extension: firmware `44.01.000`–`005` uses
`/flash/main.py`, while `44.01.006` and later uses
`/flash/_xx_<project-name>.py`. The default project name is `ADR`. Other firmware
families are rejected. A transfer overwrites that project file. The client
reports transfer acknowledgement, not proof that a program started or completed.
The tested firmware soft-reboots and starts the transferred project after its
final data block. The CLI sends no separate reset/start command. Confirm
execution through application tracing or the device UI. A data-block timeout has ambiguous device
admission; it fails without retrying that block or reporting completion.

## Loader dependencies

[`LoaderStub.mblock`](../fw/manual/LoaderStub.mblock) is a ZIP containing
`project.json`, `mscratch.json`, and visual/audio assets. Its CyberPi target
selects Python and declares `mbot2` and `firefly_bluetoothcontroller`; the state
records CyberPi extension `1.1.46`. It contains no Python source or library
payload. The official dependency extensions both declare
`targets: [{name: "cyberpi", options: {upload: {}}}]`: they add no upload
middleware. Their Python generators import `mbot2` and `gamepad`, respectively.

These declarations describe IDE capabilities. They do not install missing
MicroPython modules on the board. ADR's single uploaded file imports `cyberpi`,
`mbot2`, and `gamepad` itself. `stub-info` exposes the manifest so dependency drift
is reviewable without treating an mBlock project as a firmware image.

## Recovered wire format

Source observations below were made on 2026-11-13. Integers are little endian.
`sum8` is the byte sum modulo 256.

```text
F3 | sum8(F3, length_lo, length_hi) | payload_length:u16 |
payload | sum8(payload) | F4
```

Payloads are length-delimited; F3/F4 inside source bytes are ordinary data.

| Operation | Payload |
| --- | --- |
| Firmware version query | `06`; response starts `06` followed by version text |
| Mode query | `0D 80`; response starts `0D`, followed by a command byte and mode byte |
| Mode set | `0D 00 <mode>`; 0 upload, 1 live |
| Python execution | `28 04 <request-id:u16> <UTF-8-byte-count:u16> <UTF-8-source>` |
| File header | `01 00 5E 01 <body-size:u16> 00 <file-size:u32> <checksum:4> <UTF-8-path>` |
| File data | `01 00 5E 02 <body-size:u16> <offset:u32> <up-to-80-file-bytes>` |

The file checksum is four independent XOR lanes: byte `i` contributes to lane
`i % 4`. The length advertised by the header is the original file length.
The official JS checksum helper mutates its input by appending zero bytes;
its composer captures the original length before that mutation and transfers
only the original bytes. The Python implementation avoids that mutation while
producing identical wire bytes, including partial final blocks.

The JS uploader waits for file-protocol responses and checks complete-frame
byte 9 (payload byte 5): 0 succeeds, 1 indicates upload failure, F0 indicates
checksum failure, and any other nonzero status fails. The client also checks
the file response prefix `01 00 5E F0`, length, outer checksums, and terminator.
The byte between that prefix and status remains opaque. The default reply
wait is three seconds. The header may be attempted twice after timeout;
each file data block is sent once. There is no sequence number in the recovered
acknowledgement handling, so concurrent uploaders are unsupported.

## Serial tracing

The CyberPi extension's `onRead` handles both `readText(false)` and `readRaw()`.
It sends text to the IDE log and raw bytes to `meoser.doReceived`. Meoser's
fallback log decoder consumes CR/LF text. This establishes that serial text and
application frames share the connection; it does not require disabling the
USB UART's default protocol parser.

ADR retains its `[ADL boot]` print markers and now emits bounded runtime records
as `[ADL <seconds>] <level> <topic>: <message>` through built-in `print` when
`LOG_SERIAL_ENABLED` is true. The main-thread `KernelLog.pump` writes serial
records independently of LCD availability, outside the logging/hardware locks.
Worker threads still only enqueue logs. The existing bounded queue, flush
interval, message truncation and dropped-record count also apply to serial
tracing. Output is best-effort; a blocking native stdout call can delay input
sampling and trigger existing freshness stops. Physical USB routing and timing
remain device qualification items. `cyberpi.console.println` targets the LCD;
use `print` for custom host tracing.

## Source evidence and verification

The [IDE index](https://ide.mblock.cc/) loads
[`index.15dfc4b8.js`](https://ide.mblock.cc/js/index.15dfc4b8.js) and
[`common.cd4114e2.js`](https://ide.mblock.cc/js/common.cd4114e2.js).
The common bundle uses Web Serial `requestPort`, opens the port at the device
extension's baud rate and reads/writes raw bytes. The index bundle delegates
code uploads to the device's custom driver.

The [official CyberPi metadata](https://extservice.makeblock.com/api/publish/ext/list?ap=mblockweb&v=latest&ids=cyberpi)
resolved extension 1.1.46 to
[CyberPi index.js](https://mblock-extend.oss-cn-shenzhen.aliyuncs.com/extlist/prod/extract/1991329599799169024/119c0854-4dab-46b0-9234-339448ef1a30/cyberpi/index.js).
Its upload driver selects the firmware-dependent path and calls
`uploader.uploadContent`; `__createFrame`, `R` commands and `Y` mode/version
methods establish the other operations. `onRead` establishes the serial text
path. The dependency
[metadata](https://extservice.makeblock.com/api/publish/ext/list?ap=mblockweb&v=latest&ids=mbot2,firefly_bluetoothcontroller)
resolved mBot2 0.0.13 and Bluetooth controller 0.0.3.

The shared [meoser.js](https://res-cdn.makeblock.com/mblock/static/exts/common/meoser.js)
contains webpack module 67 (file composer and outer framing), 13 (checksums and
integer conversions), 65 (reply/status handling), 120 (80-byte uploader), and
118 (stream parser). Module numbers refer only to the captured revision.
The client is independently written Python; vendor bundles are not checked in.

| Captured source | SHA-256 |
| --- | --- |
| CyberPi index.js | `8b6729e041fddd46e1c949ee2399287fa3b7cd54252226b22e0450e8fc8e9cfa` |
| meoser.js | `7fb25c12b525e4aefea123c46d671badbb42c9a7083ef54e99964034c297cfeb` |
| mBot2 index.js | `2b228aa25b5d223ecbcbfaa5695cc7b1b734c0974a78539872c7e482e67c7a99` |
| Bluetooth controller index.js | `93bd853fa285b6b88e797a4bc3fc057436cc934728b389d751ceb99f3815cfff` |
| LoaderStub.mblock | `8a37156ec7d002e5347502caf22ccddb4b0ca420e28c6694ef77b68c60ff3cd6` |

Host tests compare every upload byte with fixtures generated by executing the
actual JS composer, plus a pyserial/subprocess upload against a POSIX pseudo-terminal simulator,
fragmented streams, embedded delimiters, UTF-8 tracing,
corrupt frames, ACK errors, timeouts, and passive monitoring. Fake-device replies
establish client behavior, not firmware behavior. Physical USB verification on
2026-11-13 confirmed a CH340 `1A86:7523` device at `/dev/ttyUSB0`, firmware
`44.01.016`, upload-mode replies, file acknowledgements, automatic soft reboot,
and print tracing from an uploaded non-actuating script. The actual ADR boot
trace additionally established that CyberOS runs the project as module `main1`,
so the application entry guard now accepts MicroPython `mainN` slots.
See the hardware boot record (internal record).
A subsequent [gamepad API retest](adr_gamepad_api.md) confirmed the controller
connected via `gamepad.is_online()` and corrected ADR's link method. Gamepad
input-to-actuator end-to-end testing and physical movement remain deferred.
