# ADR milestones

| Subsystem | Status | Accepted | Scope |
| --- | --- | --- | --- |
| Drivetrain | **PASS** | 2026-11-13 | Gamepad-controlled RWD; E1/EM1 left rear, E2/EM2 right rear |

Drivetrain marked PASS at the user's request, following the earlier confirmation
that gamepad commands and EM1 drive were operational and integration into ADR.

The current configuration supports onboard PID or direct power through the
top-level `PID_BYPASS` setting. Neutral-stick Start arms; no deadman hold is
required. Select/CyberPi B stops and disarms.

The latest firmware changes passed 26 host checks. No additional hardware run
was performed for this milestone entry. Earlier device evidence and its tested
scope are recorded in the native drive validation (internal record).
See [ADR architecture](../../docs/fw_adr_architecture.md) for current operation.
