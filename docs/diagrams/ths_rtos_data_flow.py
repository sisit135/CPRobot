#!/usr/bin/env python3
"""Generate the THS RTOS SVG using only the Python standard library.

Edit render() for nodes, labels and routes; edit Svg for shared symbols/styles.
Default output is beside this script, independent of the working directory.
See README.md and ../building.md for editing and validation instructions.
"""

import argparse
import sys
from html import escape
from pathlib import Path

SVG_HEADER = """<svg xmlns="http://www.w3.org/2000/svg" width="1880" height="1780" viewBox="0 0 1880 1780" role="img" aria-labelledby="title description">
<title id="title">THS RTOS task ownership and I/O data flow</title>
<desc id="description">STM32F411CE FreeRTOS data flow. Sensor HAL to loc_sensor, then BootstrapFrame one-slot mailbox to feature_nav and path_exec. feature_nav publishes FeatureSnapshot to path_exec; path_exec sends a one-slot stop-target back. SensorEvent FIFO holds 64 events after fusion starts; loc_fusion publishes PoseSnapshot to loc_logger and, only in explicit EKF mode, path_exec. path_exec publishes WheelCommand to motor_pwm; MotorSnapshot feeds back to path_exec. motor_pwm writes DRV8833 through the motor HAL. loc_key and ekf_spi issue control requests to navigation. bup_task and path_exec submit deferred logs to a bounded ring drained by ekf_spi. ekf_spi exchanges SPI data with an external ESP32-C6 bridge. Queue compartment drawings are symbolic; printed capacities are authoritative. Source: docs/fw_ths_architecture.md, RTOS task ownership and data flow, and linked task implementations.</desc>
<defs>
<marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path d="M 0 0 L 10 5 L 0 10 Z" fill="#36566d"/></marker>
<marker id="control" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M 0 0 L 10 5 L 0 10 Z" fill="#9a6031"/></marker>
<marker id="back" viewBox="0 0 10 10" refX="1" refY="5" markerWidth="8" markerHeight="8" orient="auto"><path d="M 10 0 L 0 5 L 10 10 Z" fill="#36566d"/></marker>
</defs>
<rect width="1880" height="1780" fill="white"/>
"""


class Svg:
    """Small renderer for tasks, hardware, open-ended queue symbols and arrows."""

    def __init__(self):
        self.parts = []

    def add(self, s):
        self.parts.append(s)

    def text(
        self,
        x,
        y,
        s,
        size=18,
        fill="#20374b",
        anchor="start",
        weight="400",
        family="Arial, Helvetica, sans-serif",
    ):
        self.add(
            f'<text x="{x}" y="{y}" font-family="{family}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" font-weight="{weight}">{escape(s)}</text>'
        )

    def line(self, d, color="#36566d", dash=False, start=False):
        """SVG route: M x y = move; H x / V y = horizontal / vertical segments.

        White underlay separates crossings. dash marks control/conditional flows;
        start adds a reverse arrowhead for a bidirectional connection.
        """
        self.add(
            f'<path d="{d}" fill="none" stroke="white" stroke-width="8" stroke-linejoin="round"/>'
        )
        self.add(
            f'''<path d="{d}" fill="none" stroke="{color}" stroke-width="2.5" stroke-linejoin="round" marker-end="url(#{("control" if dash else "arrow")})"'''
            + (' stroke-dasharray="8 6"' if dash else "")
            + (' marker-start="url(#back)"' if start else "")
            + "/>"
        )

    def label(self, x, y, s, size=16):
        # Approximate white backing; reposition long labels after rendering.
        w = len(s) * size * 0.53 + 12
        self.add(
            f'<rect x="{x - 5}" y="{y - size}" width="{w}" height="{size + 7}" fill="white"/>'
        )
        self.text(x, y, s, size)

    def task(self, x, y, w, name, detail, meta, color="#dcecf8"):
        self.add(
            f'<g aria-label="{escape(name)}"><rect x="{x}" y="{y}" width="{w}" height="100" rx="9" fill="{color}" stroke="#36566d" stroke-width="2"/>'
        )
        self.text(
            x + w / 2,
            y + 31,
            name,
            23,
            anchor="middle",
            weight="700",
            family="monospace",
        )
        self.text(x + w / 2, y + 58, detail, 17, anchor="middle")
        self.text(x + w / 2, y + 82, meta, 15, "#506879", anchor="middle")
        self.add("</g>")

    def hardware(self, x, y, w, name, detail):
        self.add(
            f'<rect x="{x}" y="{y}" width="{w}" height="80" fill="#e3f3ec" stroke="#39745d" stroke-width="2"/>'
        )
        self.text(x + w / 2, y + 31, name, 22, anchor="middle", weight="700")
        self.text(x + w / 2, y + 58, detail, 17, anchor="middle")

    def queue(self, cx, y, name, kind, side="right", fifo=False):
        """Draw an open channel: 56 × 84 units, with labels on either side.

        Compartments distinguish one-slot mailboxes from FIFOs/rings; the
        printed capacity is authoritative, not the number of drawn cells.
        """
        x = cx - 28
        h = 84
        self.add(f'<g aria-label="{escape(name)}: {escape(kind)}">')
        self.add(
            f'<path d="M {x} {y} V {y + h} H {x + 56} V {y}" fill="none" stroke="#36566d" stroke-width="2"/>'
        )
        if fifo:
            for off in (24, 44, 64):
                self.add(
                    f'<rect x="{x}" y="{y + off}" width="56" height="20" fill="#c3d4df" stroke="#36566d" stroke-width="1.4"/>'
                )
        else:
            self.add(
                f'<rect x="{x}" y="{y + 54}" width="56" height="30" fill="#c3d4df" stroke="#36566d" stroke-width="1.5"/>'
            )
        tx = cx + 48 if side == "right" else cx - 48
        anchor = "start" if side == "right" else "end"
        self.text(tx, y + 39, name, 19, anchor=anchor, weight="700")
        self.text(tx, y + 65, kind, 16, "#506879", anchor=anchor)
        self.add("</g>")


def render():
    """Return deterministic standalone SVG; no timestamps or external assets."""
    svg = Svg()
    svg.add(SVG_HEADER)
    # Title and lane headers. All coordinates are SVG user units, origin at top left.
    svg.text(60, 62, "THS / RTOS task ownership & I/O data flow", 34, weight="700")
    svg.text(
        60,
        97,
        "STM32F411CE · FreeRTOS · FEATURE is the default navigation engine",
        20,
        "#506879",
    )
    for x, w, t in [
        (60, 460, "CONTROL & FEEDBACK"),
        (550, 440, "FEATURE → MOTION"),
        (1020, 350, "EKF OBSERVER"),
        (1410, 410, "TELEMETRY & BRIDGE"),
    ]:
        svg.add(
            f'<rect x="{x}" y="132" width="{w}" height="38" rx="4" fill="#eef3f7"/>'
        )
        svg.text(x + w / 2, 157, t, 16, anchor="middle", weight="700")
    # Connections. Draw first, under nodes; edit routes when moving their endpoints.
    svg.line("M 770 270 V 340")  # Sensor HAL → sensor task
    svg.line("M 770 440 V 485")  # Sensor task → bootstrap mailbox
    svg.line("M 770 460 H 1190 V 485")  # Sensor task → event FIFO
    svg.line("M 770 569 V 655")  # Bootstrap mailbox → feature task
    svg.line("M 770 600 H 560 V 1025 H 580")  # Bootstrap mailbox → path task (bypass)
    svg.line("M 1190 569 V 655")  # Event FIFO → fusion task
    svg.line("M 770 755 V 810")  # Feature task → feature snapshot
    svg.line("M 770 894 V 1000")  # Feature snapshot → path task
    svg.line("M 1190 755 V 810")  # Fusion task → pose snapshot
    svg.line(
        "M 1190 894 V 975 H 1010 V 1045 H 960", "#9a6031", True
    )  # Pose snapshot → path task (EKF mode)
    svg.line("M 1218 850 H 1430")  # Pose snapshot → logger
    svg.line("M 1610 900 V 1190")  # Logger → SPI task
    svg.line("M 1610 440 V 500")  # Bring-up task → log ring
    svg.line(
        "M 1610 584 V 620 H 1835 V 1240 H 1790"
    )  # Log ring → SPI task (outer right route)
    svg.line("M 960 1015 H 1390 V 545 H 1582")  # Path task → log ring
    svg.line(
        "M 1430 1260 H 1360 V 1080 H 960", "#9a6031", True
    )  # SPI task → navigation control
    svg.line("M 770 1100 V 1155")  # Path task → wheel command
    svg.line("M 770 1239 V 1315")  # Wheel command → motor task
    svg.line("M 770 1415 V 1495")  # Motor task → driver
    svg.line("M 580 1070 H 275 V 884")  # Path task → stop target
    svg.line("M 275 800 V 710 H 580")  # Stop target → feature task
    svg.line("M 580 1365 H 275 V 1344")  # Motor task → motor snapshot
    svg.line("M 275 1260 V 1095 H 580")  # Motor snapshot → path task
    svg.line(
        "M 150 440 V 950 H 630 V 1000", "#9a6031", True
    )  # Local key → navigation start
    svg.line("M 1610 1290 V 1495", start=True)  # SPI task ↔ external bridge
    # Junction dots: unmarked crossings do not connect.
    for x, y in [(770, 460), (770, 600)]:
        svg.add(f'<circle cx="{x}" cy="{y}" r="4.5" fill="#36566d"/>')
    # Nodes: task(x, y, width, name, role, cadence); queue(center_x, y, name, capacity).
    svg.hardware(580, 190, 380, "Sensor HAL", "MPU9250 + MAG · VL53L0X ×3 · TCS34725")
    svg.task(
        580,
        340,
        380,
        "loc_sensor",
        "Sole runtime sensor bus owner",
        "P2 · 10 ms loop / 50 ms ToF polling",
    )
    svg.task(
        90,
        340,
        340,
        "loc_key  (optional)",
        "Debounced local start request",
        "P2 · 20 ms key sampling",
        "#fff0df",
    )
    svg.task(
        1430,
        340,
        360,
        "bup_task",
        "One-shot bring-up status",
        "P1 · self-deletes after reporting",
    )
    svg.queue(770, 485, "BootstrapFrame", "Mailbox · 1 slot")
    svg.queue(1190, 485, "SensorEvent", "FIFO · 64 entries", fifo=True)
    svg.queue(1610, 500, "Deferred logs", "Ring · 32 records", side="left", fifo=True)
    svg.task(580, 655, 380, "feature_nav", "Feature hypotheses & heading", "P1 · 50 ms")
    svg.task(
        1040,
        655,
        300,
        "loc_fusion",
        "Sole EKF owner",
        "P3 · event-driven / ~20 ms pose",
    )
    svg.queue(770, 810, "FeatureSnapshot", "Mailbox · 1 slot")
    svg.queue(1190, 810, "PoseSnapshot", "Mailbox · 1 slot", side="left")
    svg.queue(275, 800, "Stop target", "Mailbox · 1 slot")
    svg.task(
        1430, 800, 360, "loc_logger", "Low-rate EKF pose submission", "P1 · 200 ms"
    )
    svg.task(580, 1000, 380, "path_exec", "Route, goal & end-tape state", "P2 · 20 ms")
    svg.queue(770, 1155, "WheelCommand", "Mailbox · 1 slot")
    svg.queue(275, 1260, "MotorSnapshot", "Mailbox · 1 slot")
    svg.task(
        580,
        1315,
        380,
        "motor_pwm",
        "Rate ramp, PWM & terminal guard",
        "P4 · 10 ms",
        "#dcefe6",
    )
    svg.task(
        1430,
        1190,
        360,
        "ekf_spi",
        "SPI owner & deferred log drain",
        "P1 · 50 ms SPI transactions",
    )
    svg.hardware(580, 1495, 380, "DRV8833", "Motor HAL → duty writes → wheels")
    svg.hardware(
        1430, 1495, 360, "External ESP32-C6", "Field-tool bridge · SPI transport"
    )
    # Connection labels and ownership note.
    svg.label(989, 447, "After fusion starts", 16)
    svg.label(345, 616, "Latest sensor frame", 16)
    svg.label(294, 976, "navigation_start()", 16)
    svg.label(1030, 961, "Explicit EKF mode", 16)
    svg.label(1220, 831, "Latest pose", 16)
    svg.label(1404, 1000, "Navigation logs", 15)
    svg.label(1050, 1115, "Start / halt / gated config", 15)
    svg.label(1640, 1124, "Deferred log drain", 16)
    svg.label(1630, 1403, "SPI", 18)
    svg.label(790, 1460, "HAL duty writes", 16)
    svg.text(1050, 1420, "Ownership exception", 17, weight="700")
    svg.text(1050, 1447, "Terminal stop helpers may write zero", 16, "#506879")
    svg.text(1050, 1471, "through HAL from caller tasks.", 16, "#506879")
    # Legend and footer.
    svg.add('<path d="M 60 1615 H 1820" stroke="#cbd6df" stroke-width="1.5"/>')
    svg.add(
        '<rect x="65" y="1642" width="36" height="26" rx="4" fill="#dcecf8" stroke="#36566d" stroke-width="1.5"/>'
    )
    svg.text(114, 1662, "RTOS task · P = priority", 17)
    svg.add(
        '<path d="M 435 1636 V 1674 H 463 V 1636 M 435 1659 H 463" fill="none" stroke="#36566d" stroke-width="1.8"/>'
    )
    svg.text(477, 1662, "Queue / mailbox (slot symbol)", 17)
    svg.line("M 855 1655 H 925")
    svg.text(940, 1662, "Data / HAL I/O", 17)
    svg.line("M 1180 1655 H 1250", "#9a6031", True)
    svg.text(1265, 1662, "API request / conditional path", 17)
    svg.text(
        65,
        1705,
        "One-slot state mailboxes retain the latest value for readers; stop targets are consumed. FIFO events preserve order; full-queue drops are counted.",
        17,
        "#506879",
    )
    svg.text(
        65,
        1735,
        "Queue slots are symbolic; labels give capacity. Dots join flows; line crossings without dots do not. This is data flow, not a scheduler timeline.",
        17,
        "#506879",
    )
    svg.text(
        65,
        1763,
        "Source: docs/fw_ths_architecture.md · “RTOS task ownership and data flow” and linked STM32 task implementations.",
        15,
        "#647b8c",
    )
    svg.add("</svg>")
    return "\n".join(svg.parts) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().with_suffix(".svg"),
        help="Output SVG (default: beside this script).",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Verify output matches the generator without writing it; exit 1 if stale or missing.",
    )
    args = parser.parse_args()
    generated = render().encode("utf-8")
    if args.check:
        if not args.output.is_file() or args.output.read_bytes() != generated:
            print(
                f"Missing or stale SVG: {args.output}. Regenerate with this script.",
                file=sys.stderr,
            )
            return 1
        print(f"Up to date: {args.output}")
        return 0
    args.output.write_bytes(generated)
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
