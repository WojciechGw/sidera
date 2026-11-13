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
