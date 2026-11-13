# SIDERA: project context for agents

## Start with the task, not a full repository audit

- Check `git status --short` and relevant diffs; preserve existing user changes.
- Use this file as the starting map. Read only the documentation sections and
  source files relevant to the task. Expand the search when dependencies,
  failures or conflicting evidence require it.
- Reuse verified context from the current conversation. Do not repeatedly read
  the entire project or rerun unrelated checks just to regain orientation.
- Treat current code and build output as authoritative when documentation is
  stale. Historical reports describe their dated snapshots, not every later ROM.
- Communicate with the owner in Polish unless asked otherwise. Keep repository
  documentation in English. Resolve routine implementation details independently;
  ask about missing requirements that materially affect the result.

## Established decisions

- SIDERA is an offline sky observatory for **RP6502-PICO and RP6502-EMU**, built
  with cc65 and RP6502-SDK into one `sidera.rp6502` containing code and resources.
- Only standalone operation is supported. RMT/razemOS compatibility and obsolete
  active-code names were removed. Preserve historical provenance and attribution.
- WEB support is disabled; files ending in `.disabled` are retained SDK templates.
- Original project code and documentation use **GPL-3.0-only**.
  Copyright (c) 2026 Wojciech Gwioździk (WojciechGw).
  Third-party materials retain their own terms: see [LICENSE](LICENSE) and
  [LICENSE-NOTICE.txt](LICENSE-NOTICE.txt). License notices are also ROM resources.
- On 13 November 2026 the owner confirmed startup and operation on a physical
  Picocomputer 6502. This does not certify each checklist item or a particular
  ROM hash. Details: [hardware record](docs/rmt-cleanup.md#physical-pico-checks).

## Where to look

| Task | Starting points |
| --- | --- |
| Architecture overview | [Technical reference](docs/technical-reference.md); `src/sidera.h`, `src/astro.h` |
| Startup, input, application state | `src/main.c`, `src/startup.c`, `src/platform/`, `src/overlays/boot.c` |
| Astronomy, dates, rise/set, time zones | `src/astro.c`, `src/riseset.c`, `src/timezone.c` |
| Sky rendering and constellations | `src/graphics.c`, `src/constellations.c` |
| Solar System panel | `src/solar.h`, `src/solar_*.c` |
| Settings, resources, editors, cities | `src/files.c`, `src/overlays/init_files.inc`, `src/editor.c`, `src/cities.c` |
| Memory, loader, ROM packing | [Architecture](docs/architecture.md), `src/overlays/`, `scripts/pack_overlays.py`, `cmake/SideraOverlays.cmake` |
| Build and validation | [README](README.md), [test guide](docs/tests.md), `CMakeLists.txt`, `cmake/SideraTests.cmake`, `tests/` |
| User-visible behaviour | [English manual](src/rom/sidera-manual-en.txt), `src/help.txt` |
| Data origins and historical changes | [Provenance](docs/provenance.md), [cleanup report](docs/rmt-cleanup.md), `docs/source-manifest.json` |

## Constraints that matter when editing code

- One resident polling loop performs incremental work. Preserve responsiveness
  and LIVE/SIM behaviour when changing rendering or astronomical calculations.
- Space cycles LIVE → SIM/PAUSE → SIM/PLAY → LIVE; Shift+Right/Left select the
  next/previous compass direction. Only physical LCtrl+Left/Right changes FOV;
  RCtrl and the former H shortcuts do not. The USB HID bitmap occupies XRAM
  `$FFC0–$FFDF` and must be unmapped on exit. VGA is required.
  CITIES stays inside the sky plot with a 5-pixel margin,
  1-pixel frame and 14 rows per page; preserve the surrounding display.
- SIM/PLAY uses exact 5-minute steps, a 42-tick minimum interval and waits for
  pending jobs; manual +/- stays 15 minutes. Prepare stars before redrawing.
  Manual navigation must bypass catalogue/projection preparation and update
  only the View/FOV header fields; test input latency with `sidera_input`.
  The inactive star buffer doubles as a screen-coordinate cache: invalidate it
  in `cs_begin()` before any new angle writes. See `tests/benchmark_simulation.py`
  and [simulation measurements](docs/simulation-performance.md).
- Preserve unchanged plot margins: top/bottom compasses depend on bearing/FOV,
  side scales on elevation/FOV; the top and sides also depend on grid visibility.
  Time updates preserve all three regions except the lower compass day/night
  inversion: its full-width band below V is white from Sun Rise (inclusive) to
  Set (exclusive), using displayed local minutes; Sun UP/DOWN handles polar days.
  Pending/unavailable rise/set results use the normal dark band.
  Restore left ticks inside the cleared
  plot, and invalidate margins on full clears and Ctrl+L. Interrupted redraws
  must not mark partially cleared axes as complete.
- Five overlays (INIT, CITIES, EDIT, COVER, SOLAR) share CPU `$C800–$DEFF`
  (5888 bytes for code, constants, data and BSS). Scratch is `$DF00–$DFFF`;
  the C software stack is `$F700–$FEFF`.
- Resident code controls overlay replacement. Do not keep pointers into an
  unloaded overlay or replace an overlay while executing its code. Build IDs,
  CRC validation and BSS initialization are required parts of loading.
- SOLAR has very little spare space. Inspect the current map and packing output
  after size-sensitive changes; do not treat historical free-byte counts as fixed.
- XRAM is separate from CPU RAM. Check shared allocations in `src/sidera.h`
  and `src/solar.h` before changing graphics buffers or caches.
- `src/platform/argv.c` must remain a direct executable source: putting it only
  in a library can cause cc65's default argument implementation to be selected.
- `SIDERA_OVERLAYS` controls placement for ROM versus test builds; it is not an
  operating-system variant. Keep shared startup usable by application and tests.
- Respect cc65's integer widths and fixed-point units. Use explicit-width types
  for serialized fields; host `long` is not a portable four-byte data format.
- `tools/` is SDK-maintained. Keep project extensions in `cmake/` and `scripts/`.
- Normal builds use committed `src/rom/` and `src/tables/` assets. Generators write
  to `build/`; review regenerated outputs before copying them into the source tree.
  Preserve external licenses and the historical import manifest.

## Build and choose validation proportionately

Standard commands from the repository root:

```text
cmake --preset cc65/Debug
cmake --build --preset cc65/Debug
ctest --preset cc65/Debug
```

Use `cc65/Release` for Release. ROM output is
`build/cc65/<debug|release>/sidera/sidera.rp6502`. Both presets use `-O`.
Windows host tests use MSVC; Linux uses GCC. Keep Windows and WSL build directories
separate; the [README](README.md) gives WSL commands and emulator path handling.
In PowerShell, read UTF-8 documentation with `Get-Content -Encoding utf8`.

- Documentation only: check meaning, relative links and `git diff --check`.
  No application build is needed unless an edited text file is packed into ROM.
- Code changes: build and run the affected host/sim65 tests. Use the full suite
  for changes spanning subsystems, shared startup or application flow.
- ROM resources, packaging or overlays: build and run
  `ctest --preset cc65/Release -R "^sidera_(rom|emu)$"` (or the matching Debug preset).
  Inspect memory headroom when code or layout changes.
- Graphics changes: include the six reference-image checks in `sidera_host_suite`.
  Review intended visual differences before replacing reference images.
- Generator or source-data changes: run
  `python scripts/regenerate.py --output build/generated --check`.
- For compiler/platform-sensitive changes, validate Debug/Release and the affected
  host environments. Report exactly what ran; do not infer untested results.

Automated tests do not upload to PICO. Hardware launch through F5 does.
The ignored `.rp6502` contains local device settings; keep credentials out of Git.

## Keep this context useful

Update this file when durable decisions, module locations, commands or constraints
change. Update the relevant technical document for implementation details and
dated reports for new validation evidence. Keep this file concise; do not turn it
into a transcript, a task log, or a duplicate of the technical reference.
