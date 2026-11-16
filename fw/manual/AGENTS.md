
### Manual uPy FW ref
The python script is presumably running on a MicroPython-base system.

- Follow working [PerfecXX/mBot2 samples](https://github.com/PerfecXX/mBot2) and
  the references below for vendor API usage. Use upload-mode/on-device APIs.
- Use the sample's `mbot2.drive_speed(left_rpm, right_rpm)` for onboard encoder
  PID speed regulation, currently capped at ±100 RPM. The top-level
  `PID_BYPASS = False` selects this mode; setting it to `True` before upload
  selects the sample's `drive_power(left_percent, right_percent)` instead,
  bounded by `MAX_DRIVE_POWER` (default 100%). Keep boot/stop commands on the
  selected API and keep RPM and percent limits distinct. RWD wiring is E1/EM1 =
  left rear, E2/EM2 = right rear, in that argument order. Speed targets are RPM;
  motor power is regulated by the vendor firmware. The optional host slew ramp,
  catapult and field-tool runtime remain commented out. Do not
  invent method names or infer on-device support from the PC online-mode package.
  If a required API is absent from the samples, verify it against official
  extension/docs and the actual firmware before integrating it.
- Local driving requires neutral-stick Start arming, then stick movement directly;
  no R2/deadman hold is used. Select/CyberPi B still stops and disarms.

- https://docs.micropython.org/en/v1.16/library/index.html
- https://d4iqe7beda780.cloudfront.net/resources/static/main/pdf/mb10132_python_instruction_manual.pdf Offical API surface
- https://github.com/zakandrewking/rocky/blob/main/apps/cyberpi/docs/cyberos-api-surface.md
- https://www.yuque.com/makeblock-help-center-en/mcode/cyberpi-api - uPython mode ONLY!!!
