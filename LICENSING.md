# Licensing by material

This repository uses separate licenses for software and original hardware
designs. Directory placement does not relicense third-party material or
manufacturer firmware dumps.

## Software: GNU GPL version 3

Project-owned firmware, tests and development software are
licensed under **GNU General Public License version 3 only**
(`SPDX-License-Identifier: GPL-3.0-only`). The full text is in [LICENSE.software](LICENSE.software).

This covers original software in `fw/`, including tests, scripts, firmware
settings schemas and generators, together with root build helpers.
Third-party code, manufacturer dumps and hardware designs are exceptions as
specified below. Generated files retain any applicable source and generator
notices; this policy does not remove those notices.

## Original hardware designs: CERN-OHL-S-2.0

Original hardware design sources and their rendered exports are licensed under
**CERN Open Hardware Licence version 2 — Strongly Reciprocal**
(`SPDX-License-Identifier: CERN-OHL-S-2.0`). See the
[full license text](LICENSE.hardware) and
[CERN license overview](https://cern-ohl.web.cern.ch/).

The current hardware-design scope is:

- [STM32 electrical drawing](docs/stm32_electrical.drawio).
- [Hardware wiring documentation](docs/hardware_wiring.md).
- [Original mounting drawing](docs/diagrams/mount_axes.svg).
- V2 mounting drawing: [SVG](docs/diagrams/mount_axes_v2_proposal.svg) and
  [PNG](docs/diagrams/mount_axes_v2_proposal.png).
- [THS HAL breadboard drawing](docs/diagrams/ths_hal_breadboard.svg).

Original hardware CAD, schematics, PCB layouts, mechanical drawings and their
exports added to this repository use this hardware license unless explicitly
marked otherwise. Software used to generate drawings remains covered by the
software license. Software architecture diagrams, validation charts, screenshots
and logos are not hardware designs merely because they are stored under `docs/`.
This hardware grant does not cover third-party component designs or vendor data.

## Manufacturer dumps: no public license grant

Manufacturer firmware dumps, binary images and extracted contents are **not
licensed for public use or redistribution** by this project. Under the
manufacturer terms identified by the repository owner, they may be used only on
the manufacturer's devices. Neither GPLv3 nor CERN-OHL-S-2.0 applies to them.
This notice does not grant additional rights under those manufacturer terms.

This includes the local
`fw/static_analysis_adr/fw_dump_cyberpi_head/` tree: `flash.bin`, extracted
certificates and any other material recovered from that image. The tree is
ignored by Git and must be excluded from public source bundles, release assets
and distributed binaries. Ignore rules do not remove copies already tracked or
included by a filesystem-based archive tool.

See [the manufacturer-material notice](LICENSE.dumps).

## Third-party materials and other documentation

Third-party code, libraries, vendor drivers, adapted UI resources and other
third-party assets retain their original licenses and notices, including when
located within a GPL-covered software directory. Consult
[third-party notes](third_party/README.md) and retained component notices.

The root GPL text is the software license, not a blanket license for every file.
Documentation and assets outside the hardware and software scopes above retain
any existing notices; this policy does not assign an additional license to them.
