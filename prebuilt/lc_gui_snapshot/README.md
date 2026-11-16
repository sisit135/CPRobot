# Lifecycle GUI snapshot

Copy this entire directory to another machine. No source checkout, compiler,
CMake, Python, SDL development package, or ImGui download is needed to run it.
Use the launcher, which selects the bundled loader and runtime libraries:

```sh
./run.sh
./run.sh --help
```

The executable includes both FEATURE and forced RHR, the supplied maze and its
rotations, the open-map fixture, the simulated plant, and the GUI's font/shaders.
The application uses RDSEED for omitted seeds. On CPUs without RDSEED, specify
`--seed N`; explicit seeds also replay a run.

## Destination requirements

- **x86-64-v3 / AVX2 CPU** and **Linux kernel 6.1 or newer**. These are the
  requirements of the included host runtime; the launcher checks them.
- A working X11 or Wayland desktop session and a Vulkan driver supporting
  desktop window presentation. Hardware-specific Vulkan ICDs, their manifests,
  and the display server come from the destination; they are not bundled.
- A directory/filesystem that permits executing files. If needed, run
  `chmod +x run.sh bin/robot_feature_lifecycle lib/ld-linux-x86-64.so.2`.

The package includes glibc and its matching loader, libstdc++, SDL2 compatibility
runtime plus SDL3, the Vulkan loader, and common X11/Wayland dependencies. It does
not require the destination's system glibc to match the build host. Use
`./run.sh --check-runtime` to inspect initial library resolution. Do not invoke
the executable in `bin/` directly on the destination.

## Examples and controls

```sh
# Known passing short FEATURE GUI fixture, accelerated and closed at result.
./run.sh --seed 1234 --entrance 0 --heading 2 --variant 0 --goal 4 2 --speed 20 --exit-on-result

# Full forced-RHR GUI mission with the ordinary fixed end.
./run.sh --seed 1234 --entrance 0 --heading 2 --variant 0 --force-rhr --speed 20 --exit-on-result

# Full FEATURE regression, no desktop required; uncapped virtual time.
./run.sh --headless --quiet --seed 1485012174

# Choose a display backend if automatic selection is unsuitable.
SDL_VIDEODRIVER=x11 ./run.sh --seed 1234
SDL_VIDEODRIVER=wayland ./run.sh --seed 1234
```

**R** toggles rays, **P** path/trail, **D** diagnostics, **Space** pauses,
**N** steps 10 ms while paused, and **Esc** exits. `--speed 1..20` changes GUI
playback speed. A fresh process starts a new boot. A failed random mission is
reported as a failure; this snapshot does not change the mission checks or
86-second run deadline. The short fixture above uses a diagnostic goal.

## Contents

- `bin/`: GUI-enabled Release executable; `lib/`: runtime libraries and loader.
- `build-info.json`: binary hash, build settings, bundled-library provenance
  and destination requirements; `source-sha256.json`: exact workspace input hashes.
- `licenses/`: project, linked-component and runtime-library license notices.
- `SHA256SUMS`: integrity manifest; run `sha256sum -c SHA256SUMS` from this folder.

Detailed validation reports and runtime traces are maintained privately and
are excluded from this package.

The source tree is deliberately absent from this runtime package. Input hashes
identify the current workspace build, which includes uncommitted files; Git HEAD
alone does not identify it. Runtime tests on the build host and a relocated copy
do not establish compatibility with every Linux desktop or GPU driver.
