# Architecture diagram sources

## THS RTOS task ownership and I/O data flow

[`ths_rtos_data_flow.py`](ths_rtos_data_flow.py) is the editable source for
[`ths_rtos_data_flow.svg`](ths_rtos_data_flow.svg). Generation requires only
Python 3 and its standard library. The SVG contains vector shapes and editable
text, with no embedded bitmap or external asset dependencies.

Regeneration, freshness checks and optional PNG preview commands are in the
[build guide](../building.md#architecture-diagrams).

Edit the following sections of `render()`:

- **Title and lane headers:** title, subtitle and column headings.
- **Connections:** named arrow routes; each comment identifies its endpoints.
  `M x y` begins a path, `H x` draws horizontally, and `V y` draws vertically.
  `dash=True` selects a control/conditional path; `start=True` adds the reverse
  arrowhead used by the bidirectional SPI connection.
- **Junction dots:** explicit fan-out connections. Crossings without dots do
  not connect.
- **Nodes:** tasks, hardware and queues. Task and hardware positions are their
  top-left corners; queue positions use the channel's horizontal center and top.
  All coordinates use SVG units with the origin at the canvas's top-left.
- **Connection labels and ownership note**, then **Legend and footer:** annotations
  and explanations.

Edit the `Svg` helper methods for shared shape styles, font sizes and colors;
`SVG_HEADER` defines the canvas, accessible description and arrow markers.
Task boxes are 100 units tall, hardware boxes 80, and queue channels 56 × 84.
Queue cell counts are symbolic; update the printed capacity when it changes.

Positions and routes are deliberately explicit. After moving a node, update
its connecting paths and nearby labels, regenerate, and open the SVG in a
browser or vector editor to check spacing. The generator does not route edges
or extract facts from firmware automatically. Keep names, capacities, cadence,
ownership and conditional paths aligned with the
[THS architecture section](../fw_ths_architecture.md#rtos-task-ownership-and-data-flow)
and its linked source implementations.

Keep the Python source and generated SVG together. Direct SVG edits will be
replaced on regeneration. The generator resolves its default output beside
itself, so it works from any working directory. Generation is deterministic;
`--check` compares bytes without modifying the output and exits with status 1
if it is missing or stale. Output text uses common system fonts; font substitution
can affect appearance across machines even when the SVG bytes are identical.
