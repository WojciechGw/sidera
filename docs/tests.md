# SIDERA validation

`ctest --preset cc65/Debug` and `ctest --preset cc65/Release` run:

- `sidera_host_suite`: host and sim65 tests, plus pixel comparisons of six images
  against their references (24 tests in total). They cover astronomy, catalogues,
  time zones, rise/set times, settings, graphics, labels, the loader and the full
  application loop with overlays, including error scenarios.
  Application-loop checks include the Space LIVE/PAUSE/PLAY cycle, immediate
  clock resynchronization, clock failure and Shift+Left/Right direction wrapping.
  They also check FOV limits and that the retired H shortcuts and generic Ctrl
  arrows do not change the view.
  Simulation scenarios exercise all 20 G/C combinations with T/P enabled,
  exact five-minute steps, one redraw per instant, local/UTC date rollover,
  unsigned deadline wrap, manual +15-minute pause/resume and the year-2200
  boundary. Q is also tested during catalogue preparation.
  Projection checks cover all 12 FOVs, negative rounding and inactive-buffer
  ownership. Six reference PNGs cover sky and Solar System views; the daytime
  C views include the inverted compass band.
  Perimeter checks reject writes to unchanged compasses and side scales during
  elevation, azimuth and LIVE/SIM time updates. They cover all 12 FOVs, every
  grid mode, shared boundary bytes, interrupted redraws and explicit refresh;
  the resulting bitmap must match a fresh full redraw.
  Daylight checks cover local Rise/Set minute boundaries, reverse time, polar
  UP/DOWN, midnight-spanning and single-crossing days, and stale event caches.
  Bitmap checks require an exact inversion of rows 426–440 with every other
  pixel (including V) unchanged, also after interrupted view/FOV redraws.
  City rendering checks verify the frame, preservation of surrounding pixels,
  incremental redraws, search and 14-row pagination.
  `sidera_platform_6502` checks the 125 Hz clock code, 16-bit counter wraparound,
  waiting, VGA detection, console I/O, the monitor argument buffer and the
  keyboard decoder (RIA sequences, physical LCtrl versus RCtrl/both modifiers,
  Esc and timeout), USB bitmap mapping, failure handling and unmapping.
  The test controls RIA registers and time; sim65 executes the code.
- `sidera_rom`: checks the actual built ROM: only the core and reset vector in
  initial memory, five overlays as resources, headers, CRC32, the shared build
  identifier, BSS, resident addresses and the stack. This check is also a required
  packing step.
- `sidera_emu`: runs the actual ROM in RP6502-EMU with a fixed date and a fixed seed
  for random RAM initialization. It exercises startup with the splash screen,
  the sky view, physical LCtrl-only FOV, Shift+arrow directions, retired H keys,
  the X panel, the L editor, K cities, S saving to `SAVE:`, full-detail simulation
  with five-minute steps and manual +15-minute pause, and Q exit.
  It checks application states, the absence of overlay errors and the existence
  of the saved file. The saved time must match the `/date` option, which also
  verifies monitor argument handling by the actual ROM.
- `sidera_generated_assets`: reproduces the catalogue, tables, graphics, cities
  and time zones offline and compares them with the supplied files. For text,
  only line endings are normalized.
- `sidera_daylight`: crosses sunrise in SIM/PLAY, then moves backward and forward
  with manual time keys in the actual ROM at 8 MHz. It checks exact band inversion
  and preservation of every other plot/compass pixel, including V.
- `sidera_input`: measures actual-ROM response at 8 MHz with full G/T/C/P.
  Horizontal arrows (idle and during catalogue preparation), elevation arrows
  (idle) and physical LCtrl FOV (idle)
  must be accepted and begin visibly changing the plot within 15 video frames
  (250 ms). A burst of repeat keys must retain all six steps. The output
  `input-latency.json` distinguishes key handling from visible feedback;
  it does not measure the time to finish rendering the entire plot.

The SDK adds two helper fixtures: `rp6502.build` and `rp6502.save`.
All test assertions remain enabled in Release as well.
Tests do not upload anything to PICO or download astronomical data.

`tests/benchmark_simulation.py` is an optional real-ROM measurement tool, separate
from the pass/fail suite. It samples emulated frames without modifying application
memory. Reproduction commands and results are in
[simulation performance](simulation-performance.md).

Application startup and option tests share `src/startup.c` and
`src/overlays/boot.c`. Host adapters substitute files, time and RIA without
emulating a task or lock system. `SIDERA_OVERLAYS` distinguishes ROM code
placement from native tests; it does not select an operating-system variant.

## Images

The host test saves the XRAM state produced by the actual rendering algorithms.
`reference_views.py` creates seven PNG files and three configurations in
`build/cc65/<debug|release>/host-native/views/`:

- `sidera-testA.png`, `sidera-testB.png`, `sidera-testC.png`;
- `sidera-solarA.png`, `sidera-solarB.png`, `sidera-solarC.png`.
- `sidera-cities.png`: an additional preview of the framed city selector.

The original references are in `src/assets/`. These are host-test renderings;
they must not be presented as screenshots from a physical device.
The six sky/Solar System images have committed pixel references; the city panel
also has direct bounds and redraw assertions in the host tests.
Separate screenshots from the actual emulator (`sky.png`, `solar.png`, `cities.png`) are
created in `build/cc65/<debug|release>/test/sidera_emu/`.

## Diagnostics

To see detailed output from an individual host test:

```text
ctest --test-dir build/cc65/debug/host-native -C Debug -R sidera_timezone --output-on-failure
```

CTest logs are in `Testing/Temporary/` within the main and native build
directories. Do not mix Windows and WSL directories. The time-zone test copies
its data file to a native temporary directory so that hundreds of thousands of
small reads do not cross the Windows drive mount in WSL.

When debugging, remember that all five overlays share execution addresses.
Some symbols may be ambiguous, and a breakpoint at an overlay address refers
to the currently loaded code. Loader and map checks do not replace verification
of switching time and device behaviour on physical PICO hardware.

## Generators

`scripts/regenerate.py --output build/generated --check` runs the local generators
`constellations.py`, `assets.py`, `solar_assets.py`, `cities.py` and `timezones.py`.
Without `--check`, it only generates outputs. Catalogue importers accept
`--import-dir`; imported results remain in the output directory for manual review.
`tests/fetch_reference.py` and `fetch_planets.py` are retained for deliberate
reference-data updates; they require network access and are not part of CI.
They write to `build/references/` by default; `--output` changes the destination.
Replacing references in `tests/` requires review and an explicit copy.
