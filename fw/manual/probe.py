"""Read-only CyberPi upload-mode capability probe; does NOT actuate any output.

Run with wheels raised and shield outputs already off. It cannot stop a previous
program's latched motor output. No claim that symbol presence proves semantics.
"""
import sys
import time
import cyberpi
import mbot2
import gamepad


def screen_print(*values):
    cyberpi.console.println(" ".join(str(value) for value in values))


# cyberpi.console.clear()

screen_print("runtime:", sys.version)
screen_print("implementation:", sys.implementation)
screen_print("gamepad attributes:", dir(gamepad))
screen_print("mbot2 attributes:", dir(mbot2))
try:
    import _thread
    lock = _thread.allocate_lock()
    lock.acquire()
    lock.release()
    screen_print("_thread lock: OK; start_new_thread:", callable(getattr(_thread, "start_new_thread", None)))
except Exception as exc:
    screen_print("_thread unavailable:", exc)

screen_print("Required link status: gamepad.is_online; no guessed fallback")
for _ in range(20):
    try:
        link = getattr(gamepad, "is_online", None)
        screen_print("link:", link() if callable(link) else "MISSING")
        screen_print("axes:", [(name, gamepad.get_joystick(name)) for name in ("Lx", "Ly", "Rx", "Ry")])
        screen_print("keys:", [(name, gamepad.is_key_pressed(name)) for name in
                        ("Start", "Select", "R2", "L1", "L2", "Up", "Down", "N1", "N2", "N3", "N4")])
        screen_print("encoder RPM EM1/EM2:", mbot2.EM_get_speed("EM1"), mbot2.EM_get_speed("EM2"))
    except Exception as exc:
        screen_print("probe:", exc)
    time.sleep(0.25)
