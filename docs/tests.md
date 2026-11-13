# SIDERA validation

`ctest --preset cc65/Debug` and `ctest --preset cc65/Release` run:

- `sidera_host_suite`: host and sim65 tests, plus pixel comparisons of six images
  against their references (24 tests in total). They cover astronomy, catalogues,
  time zones, rise/set times, settings, graphics, labels, the loader and the full
  application loop with overlays, including error scenarios.
  `sidera_platform_6502` checks the 125 Hz clock code, 16-bit counter wraparound,
  waiting, VGA detection, console I/O, the monitor argument buffer and the
  keyboard decoder (VT sequences, modifiers, Esc and timeout).
  The test controls RIA registers and time; sim65 executes the code.
- `sidera_rom`: checks the actual built ROM: only the core and reset vector in
  initial memory, five overlays as resources, headers, CRC32, the shared build
  identifier, BSS, resident addresses and the stack. This check is also a required
  packing step.
- `sidera_emu`: runs the actual ROM in RP6502-EMU with a fixed date and a fixed seed
  for random RAM initialization. It exercises startup with the splash screen,
  the sky view, the X panel, the L editor, K cities, S saving to `SAVE:` and Q exit.
  It checks application states, the absence of overlay errors and the existence
  of the saved file. The saved time must match the `/date` option, which also
  verifies monitor argument handling by the actual ROM.
- `sidera_generated_assets`: reproduces the catalogue, tables, graphics, cities
  and time zones offline and compares them with the supplied files. For text,
  only line endings are normalized.

The SDK adds two helper fixtures: `rp6502.build` and `rp6502.save`.
All test assertions remain enabled in Release as well.
Tests do not upload anything to PICO or download astronomical data.

Application startup and option tests share `src/startup.c` and
`src/overlays/boot.c`. Host adapters substitute files, time and RIA without
emulating a task or lock system. `SIDERA_OVERLAYS` distinguishes ROM code
placement from native tests; it does not select an operating-system variant.

## Images

The host test saves the XRAM state produced by the actual rendering algorithms.
`reference_views.py` creates six PNG files and three configurations in
`build/cc65/<debug|release>/host-native/views/`:

- `sidera-testA.png`, `sidera-testB.png`, `sidera-testC.png`;
- `sidera-solarA.png`, `sidera-solarB.png`, `sidera-solarC.png`.

The original references are in `src/assets/`. These are host-test renderings;
they must not be presented as screenshots from a physical device.
Separate screenshots from the actual emulator (`sky.png`, `solar.png`) are
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
