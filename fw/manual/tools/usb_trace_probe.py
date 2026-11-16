"""Non-actuating upload/trace probe for the CyberPi USB CLI."""
import time
import cyberpi
import mbot2
import gamepad

print("ADR_USB_UPLOAD_TRACE_OK")
print("ADR_USB_MODULES", bool(cyberpi), bool(mbot2), bool(gamepad))
for index in range(3):
    print("ADR_USB_CUSTOM_TRACE", index)
    time.sleep(0.1)
print("ADR_USB_PROBE_DONE")
