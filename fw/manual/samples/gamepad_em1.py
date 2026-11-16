"""Minimal EM1 gamepad test, based on PerfecXX/mBot2 basic_movement.py.

Hold R2 and move Ly; release R2, Select or CyberPi B to stop.
Only EM1 is commanded to move; EM2 remains at zero. Limit: +/-20 RPM.
"""
import cyberpi, gamepad, mbot2, time

mbot2.drive_speed(0, 0)
mbot2.motor_set(0, "m1")
last_rpm = None
last_draw = time.ticks_ms()
try:
    while True:
        rpm = 0
        if (gamepad.is_online() and not cyberpi.controller.is_press("b")
                and not gamepad.is_key_pressed("Select")
                and gamepad.is_key_pressed("R2")):
            ly = gamepad.get_joystick("Ly")
            if abs(ly) > 10:
                rpm = int(max(-20, min(20, ly * 0.2)))
        if rpm != last_rpm:
            mbot2.drive_speed(rpm, 0)
            last_rpm = rpm
            print("EM1_RPM", rpm)
        now = time.ticks_ms()
        if time.ticks_diff(now, last_draw) >= 250:
            cyberpi.display.show_label("EM1: %d RPM\nHold R2 + Ly\nSelect/B: stop" % rpm, 16, 0, 0, index=0)
            last_draw = now
        time.sleep(0.02)
finally:
    mbot2.drive_speed(0, 0)
    mbot2.motor_set(0, "m1")
