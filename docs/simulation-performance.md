# Simulation performance

Measured on 13 November 2026 with RP6502-EMU at 8 MHz, sampling at 60 Hz.
These are emulated-machine times, not host execution times or physical PICO
measurements. The baseline is commit `67ff367`. The initial optimization
measurements below describe the recorded ROM hashes; see the input-latency
correction at the end for the subsequent responsiveness fix.

Both builds use Warsaw, UTC `2026-11-12 20:00:00`, view `180,24`, FOV 120,
G=3, T=on, C=3 and P=on. Startup, the cover and initial daily calculations
finish before measurement. No astronomical precision or display layers were
reduced; the six reference images retain identical pixels.

## Results

| Measurement | Before | After |
| --- | ---: | ---: |
| Same manual change, 20:00 to 20:15, until all work finishes | 29.28 s | 14.43 s |
| Time spent visibly clearing/drawing during that change | 20.68 s | 5.18 s |
| Complete drawing passes for that change | 2 | 1 |
| Median interval between SIM time advances in a 60-second run | 29.83 s | 14.48 s |
| Automatic simulated-time step | 15 minutes | 5 minutes |

The identical manual change finishes about 2.03 times faster. Visible redraw
time falls by about 75%. Projection preparation takes another 1.95 seconds in
the new build, but preserves the previous bitmap during that preparation.
Star calculation takes about 6.33 seconds, and synchronous work/other sampled
states account for about 0.97 seconds of the manual-change measurement.

Full-detail playback still has long pauses: this is not continuous animation.
The new automatic step produces smaller jumps and more frequent updates, but
simulated time progresses more slowly in this heavy scene: roughly 21 simulated
seconds per real second versus 30 before. The minimum 42-tick interval is a
scheduling limit, not a promise of three rendered frames per second.

Single-frame samples can straddle operations; individual phase totals have
approximately one-frame boundary uncertainty. The manual comparison uses the
same two instants. The automatic runs deliberately use different step sizes,
so they visit different intermediate scenes.

## Implementation

- Keep an overdue simulation deadline pending until required jobs finish.
  Advance once by 300 seconds, with no catch-up or skipped instants.
- Poll input between bounded work batches and sleep only when no work remains.
- Finish the new star catalogue before drawing the scene. Previously, enabling
  planets could cause one redraw with old stars followed by a second redraw.
- Reuse the inactive star-angle buffer for exact screen coordinates while
  drawing. Invalidate it before the next catalogue calculation and on view
  changes. This needs no additional XRAM allocation.
- Cache grid rows while elevation and FOV are unchanged. Keep exact signed
  rounding when replacing projection division by unsigned shifts.

The bitmap still uses incremental drawing. No second full framebuffer was
added. The old plot remains visible during calculations and projection
preparation, then the plot is cleared and drawn once. Body sprites and the
header update when the prepared scene starts.

## Reproduce

Use the Python interpreter configured by CMake and a built ROM with its matching
`.sym` file. Build the baseline in a separate checkout of `67ff367` and use this
version of the benchmark script to measure both builds.

```text
python tests/benchmark_simulation.py --rom build/cc65/release/sidera/sidera.rp6502 --emulator build/cc65/release/test/sidera_emu/rp6502-emu.exe --output build/benchmark/manual.json --manual-step
python tests/benchmark_simulation.py --rom build/cc65/release/sidera/sidera.rp6502 --emulator build/cc65/release/test/sidera_emu/rp6502-emu.exe --output build/benchmark/play.json --frames 3600
```

The JSON contains ROM SHA-256, settings, sampled phase totals and time/redraw
events. Measurements use the emulator's documented
[script commands](https://picocomputer.github.io/emu.html#scripting), without
modifying the program's RAM. The benchmark's SAVE directory is local to its
output directory; it does not save settings.

Measured ROM SHA-256 values:

- Before: `577debf63b46440c67a234ae8873a8dc40bc0b5dae3ea19c4359d1541bc73eb6`
- After: `f7fcdfc30003169fbcee264f701093273a9846c1df3237cdcde1fb73e1066949`

## Validation and PICO comparison

Automated coverage includes exact cached coordinates at all 12 FOVs, signed
rounding on 65C02, all 20 G/C combinations with T/P enabled, local and UTC
midnight, the year-2200 limit, pause/resume, manual 15-minute changes, unsigned
deadline wrap and quitting during star preparation. The actual-ROM smoke test
also runs full-detail five-minute playback followed by a manual +15-minute pause.

For the physical PICO comparison, use the same date, view and G/T/C/P settings
listed above. Compare one manual `+` change after the initial Working indicator
clears, then SIM/PLAY. Check responsiveness during preparation and drawing,
and test both UTC midnight and local midnight. This ROM has not yet been
measured on physical hardware.

## Input-latency correction

The initial optimization delayed visible manual navigation: although arrow
keys were read within 17–50 ms, the plot waited for the full catalogue and
projection preparation. A repeated key could restart preparation again.

Manual navigation now bypasses that prepass and projects the last complete
catalogue on demand. It starts incremental drawing immediately, preserves the
pending calculation's buffer, and updates only the View/FOV header fields.
Automatic time updates still use the prepared scene. When the pending catalogue
finishes, a subsequent redraw supplies its new positions.

At 8 MHz with G=3/T=on/C=3/P=on, the actual-ROM `sidera_input` test measures:

| Action | Before fix: first plot change | After fix: first plot change |
| --- | ---: | ---: |
| Right arrow after all work finishes | 2.017 s | 0.117 s |
| Right arrow during catalogue calculation | 8.317 s | 0.150 s |
| Physical LCtrl+Left FOV change after work finishes | Not measured | 0.100 s |

The test also sends six repeat keys 100 ms apart and verifies all six steps
are retained. The limit for acceptance and first visible feedback is 250 ms.
This measures the start of drawing, not a completed frame. At the same settings,
a subsequent 60-second automatic-playback measurement retained a 14.47-second
median interval between time advances.

Run `ctest --preset cc65/Release -R "^sidera_input$"`; measurements are written to
`build/cc65/release/test/sidera_input/input-latency.json`. Physical PICO response
still needs checking with the updated ROM.

## Preserved perimeter scales

The renderer now keeps unchanged upper/lower compasses and side scales in the
bitmap, including during LIVE and SIM/PLAY updates. It still clears and redraws
the plot interior. Masked boundary writes protect pixels in shared bytes, and
scale validity is recorded only after the relevant drawing stage completes.
Host tests reject writes to preserved regions and compare interrupted rendering
with a fresh full redraw across all 12 FOVs. The six reference images are unchanged.

A 60-second playback run at the same 8 MHz, G=3/T=on/C=3/P=on settings measured
a **14.28-second** median interval between advances, compared with 14.47 seconds
in the preceding input-fix measurement (about 1.3% shorter). The primary visible
benefit is that unchanged scales no longer disappear during plot clearing.
The report is `build/audit/simulation/axes-after.json`, with ROM SHA-256
`cff394c47a3489e87b97f34377c978453c1011cf14bcdaeffe7465885c2b4416`.

The extended input test measured the first plot change after azimuth, elevation
and LCtrl FOV input at 0.100 seconds when idle, and azimuth input during catalogue
calculation at 0.150 seconds. These are emulator measurements; verification on
physical PICO remains with the owner.

## Exact arithmetic, variant A (15 November 2026)

This is a separate measurement of the working tree based on `1a1fab0`, using
cl65/sim65 V2.19 (Git `6fd2669`), `-O`, and RP6502-EMU at 8 MHz. The observer is
Warsaw (catalogue coordinates, 113 m AMSL), the path day is 17 November 2026 UTC,
and the ROM scenarios use G3, P on, FOV 120 and view 180/24. ROM sampling is 60 Hz;
these are emulated times, not physical PICO measurements.

| Scenario | Before A | After A |
| --- | ---: | ---: |
| CPU only: 97 Sun/Moon pairs plus 15-minute date stepping | 16.586 s | 10.954 s |
| T enabled after all initial work, C0 | 18.817 s | 13.083 s |
| T enabled after all initial work, C3 | 24.450 s | 18.750 s |
| Next UTC day, C3 and T already on, all jobs complete | 61.467 s | 47.983 s |

The CPU benchmark drops from 132,688,412 to 87,631,018 cycles (33.96%). The
application measurements include graphics and other resident-loop work. The
date-change improvement also includes faster shared astronomy used by other
jobs; it is not exclusively a path-generation time.

Variant A uses bounded 16-bit sine interpolation, exact signed Q14 division
through a biased unsigned shift, and early termination of Kepler iteration only
at zero integer correction. It keeps the model, path cadence and scheduling.
The compact biased-shift implementation is slightly slower than the original
analysis prototype (10.912 s), but saves 16 bytes of code. Moving the 12-byte
month-length table to HIGH leaves 7 free bytes below the overlays and 3 above
HIGH, with no new BSS. Both Debug and Release produce the same ROM hash.

The 5,820 Sun/Moon position pairs used in the analysis remain bit-identical,
covering 1900/2026/2200, four seasonal dates, Warsaw, the equator, latitude
23.44 N and both poles. Equal angles imply equal projections at every FOV for
these samples. The permanent `sidera_fast_6502` checks also cover all 65,536
sine angles, 327,905 multiplication pairs and 1,024 Kepler cases against the
former arithmetic rules. These checks establish regression equivalence, not
improved absolute astronomical accuracy.

ROM SHA-256 before A:
`3eba1caf32d6ee8b1fcfc568cd5a8f9de7689bc72817c4dc93e6503c14027ec6`.
ROM SHA-256 after A:
`f178f29f16793ac89e562dd4e7a2c4f406c7fd1b1e0c9fdeddded2428c9626ed`.
Local measurement scripts and raw reports are in `build/trajectory-analysis/`
and `build/variant-a-after/`. Validation passed: Debug and Release builds, the
full Release CTest suite (13/13), and Debug host/ROM/emulator checks (5/5 including
SDK fixtures). Both host suites include the six unchanged reference images and
the new exact-arithmetic checks. `git diff --check` also passed. The owner's
manual test had not yet been reported at this checkpoint. Variant B was pending
explicit approval; variant A adds no daily-constant cache or rise/set sharing.

## Daily and observer caches, variant B (15 November 2026)

The owner subsequently authorized B; physical PICO execution remains with the
owner. Measurements use the same settings and tooling as A above.

| Scenario | After A | After B |
| --- | ---: | ---: |
| CPU only: 97 Sun/Moon pairs plus date stepping | 10.954 s | 8.903 s |
| T enabled after all initial work, C0 | 13.083 s | 11.133 s |
| T enabled after all initial work, C3 | 18.750 s | 16.783 s |
| Next UTC day, C3 and T already on, all jobs complete | 47.983 s | 43.500 s |

The CPU benchmark uses 71,220,777 cycles: 18.73% fewer than A, and 46.33% fewer
than the original 132,688,412-cycle baseline. B caches unrounded orbital bases,
obliquity sine/cosine and solar eccentricity by UTC day. Observer latitude,
longitude and height key the shared WGS84/parallax terms. Sidereal rotation is
prepared for each instant, then reused for the bodies at that instant. Fixed
lunar inclination lookup results are constants checked by the arithmetic test.

The caches occupy 54 bytes of resident state on cc65. Removing repeated setup
and the now-redundant orbital-element wrapper leaves the whole resident image
six bytes smaller than A: 13 bytes remain below the overlays and 3 above HIGH.
No XRAM allocation, overlay lifetime, path cadence or job order changes. Full
rise/set sample sharing is excluded from this implementation: only 26 of the
97 UTC path timestamps matched the measured local-day adaptive scan, and the
additional sample-validity/transfer machinery would need more resident space.

All 5,820 Sun/Moon pairs remain identical to A and to the original analysis
baseline. The new host and 65C02 cache tests additionally compare 72 interleaved
contexts across all astronomical APIs against a reference captured from A.
Raw local reports and measurement scripts are in `build/variant-b-after/`;
the pre-B source and ROM are retained in `build/variant-b-before/`.

ROM SHA-256 after B:
`bbc6bb38a490dc6b09b05a9bb53522492a02f05c4fe5d2995a863713021079f2`.
Debug and Release builds produce this same ROM. Validation passed: the complete
Release suite (13/13), Debug host/ROM/emulator checks (5/5 including SDK fixtures),
both host suites' new cache tests and six unchanged reference views, and
`git diff --check`. Physical PICO testing remains with the owner.
