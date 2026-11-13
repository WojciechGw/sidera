# SIDERA architecture

A standalone ROM for RP6502-PICO and RP6502-EMU. [Building](../README.md).

## Memory window and layout

A single contiguous window at `$C800–$DFFF` occupies 6144 B, starts on a 2048-byte
boundary and stays at the same address for all overlays. `$C800–$DEFF` (5888 B)
holds the active overlay's code, constants, initialized data and BSS.
`$DF00–$DFFF` (256 B) is an independent scratch buffer, including 224 B for settings.

| `ROM:` resource | Contents | Bytes loaded into the window | BSS | Free within 5888 B |
| --- | --- | ---: | ---: | ---: |
| `sidera-init.ovl` | Options, settings and opening data files | 4517 B | 0 B | 1371 B |
| `sidera-cities.ovl` | City search and selection | 3909 B | 295 B | 1684 B |
| `sidera-edit.ovl` | Location, date and time-zone editors | 3106 B | 112 B | 2670 B |
| `sidera-cover.ovl` | Splash screen and initial VGA assets | 829 B | 0 B | 5059 B |
| `sidera-solar.ovl` | Orbits and the Solar System panel opened with X | 5672 B | 82 B | 134 B |

Sizes are from the current cc65 build. Each resource also has a 24-byte header,
read outside the window. Build-time checks reject memory boundary violations.

Resident code, constants and BSS end at `$BE4D` (exclusive).
After reserving the entire window, **8371 B of unallocated RAM** remain below
the C stack: 2483 B at `$BE4D–$C7FF` and 5888 B at `$E000–$F6FF`.
The C stack retains `$F700–$FEFF`, 2048 B. The upper free area is currently neither
a linker segment nor a heap; using it in future code requires a separate segment
and corresponding changes to image packing.

In the original standalone build used for comparison, data ends at `$D5B1`,
leaving 8527 B free. After adding X, removing the RMT layer and updating the
controls, city panel, simulation scheduling and perimeter rendering, **156 B**
less RAM is available than in the original, after accounting for the window,
loader and panel code. Unused space inside
overlays is not included in this result.

Standalone hardware support is in `src/platform/`. The monitor argument buffer
is 512 B and resident. `argv.c` is compiled directly into the `sidera` target
so that the linker does not substitute cc65's default implementation.
Overlay files and the linker configuration are in `src/overlays/`; their memory
layout does not depend on the former RMT system.

Simulation rendering reuses the inactive star-angle buffer in XRAM for exact
screen coordinates. Calculation and projection preparation must never write
that buffer concurrently. A grid-row cache uses 273 B of resident BSS, plus
its view keys. See [simulation measurements](simulation-performance.md).

The physical keyboard bitmap uses 32 bytes of separate XRAM at `$FFC0–$FFDF`,
after the catalogue edge data ending at `$FFA4` (exclusive). A compile-time
check guards against overlap. Its modifier byte distinguishes LCtrl from RCtrl
for FOV shortcuts; the mapping is released before returning to the monitor.

## Calls and data lifetime

A single ld65 invocation links the program and five overlay files sharing the
same execution address. Calls use ordinary addresses fixed by that build;
`.ovl` files from different builds cannot be mixed. The packer assigns a shared
build identifier to the core and headers and records each payload's CRC32.

Resident `main` selects an overlay through `ovl_select()` before calling its
functions. An overlay cannot replace its own window or retain pointers into it
after returning. Startup proceeds through INIT prepare, optional CITIES for
`/location`, INIT apply, COVER and the sky view. During observation, overlays
load when entering K, L/D/Z or X. Sky calculations and S saving are resident.

## City panel

K opens CITIES inside the sky plot at x=33..622, y=89..410: a 590×322 px
rectangle with a 5 px outer margin and a 1 px white frame. The black-on-white
` CITIES ` label uses the same style as the Solar System title. Search, 14 rows
spaced 14 px apart, coordinates, result count, messages and help stay inside.
Long text is clipped with an ellipsis before it reaches the frame.

Partial-byte masks preserve bitmap pixels outside the panel, including its
margins. Opening the selector keeps VGA planes and header sprites configured;
only sky sprites intersecting the panel are hidden. The resident event loop
suspends sky jobs while CITIES is active and redraws the sky on selection or
cancellation. Incremental row updates retain the border.

## X panel

X opens a static Solar System view for the current `moment` (UTC). Pressing X
again closes it; reopening captures a new time. The S key does not save panel
state. The **317×317 px** square occupies x=301..617, y=93..409: 10 empty pixels
separate it from the plot's right boundary, the last upper compass tick and
the highest lower compass tick (y=420).

The eight orbits are concentric circles with radii of 30, 42, 54, 66, 83, 100,
117 and 134 px. The layout is schematic, without an AU scale. Spacing prevents
8×8 px sprites from overlapping even when planets have similar directions.
The Sun uses the original 32×32 px sprite without scaling.
English names of all eight planets sit directly beside their sprites, without
connecting lines. Placement accounts for the text area and its background:
it avoids the Sun, every planet, other labels and panel boundaries. In crowded
layouts, it tries successive positions along sprite edges, with gaps up to 10 px.
The Moon is an unlabelled white 2×2 px dot at a schematic distance of 8 px from
Earth's centre (after rounding to pixels). Its direction follows the geocentric
ecliptic longitude at the snapshot time, calculated with the existing lunar
model. A 1 px black border separates the dot from the orbits.
Labels, including `Earth`, avoid the entire 4×4 px area including the border.

Heliocentric directions are calculated for the captured date from
[Schlyter's orbital elements](https://stjarnhimlen.se/comp/ppcomp.html#4), without
perturbation corrections. The view is from ecliptic north: the equinox direction
points right, and 90° ecliptic longitude points up. Distances are not represented
to scale. The header has ` SOLAR SYSTEM ` in black on white on the left, lowered
by 8 px (text at y=105). `snapshot YYYY-MM-DD HH:MM:SS UTC` stays at y=97,
right-aligned with a 2 px margin.
At the bottom, `Schematic view (not in AU scale)` and `X: close` are right-aligned
with a 2 px margin. Both keyboard shortcut rows below the main plot start at x=12
and span 102 character cells. The first row (y=450) groups time and settings;
the second (y=462) groups view controls, panels and quitting. G/T/C/P retain
inverse-video state labels. Messages remain on a separate row at y=472.

Six planets use the existing images and palettes. The additional resource
`ROM:sidera-solar.dat` contains Earth and Neptune.
Panel descriptors occupy XRAM `$B096–$B0EF`, images occupy `$E160–$E1DF`,
and the two palettes occupy `$E200–$E5FF`, all in previously unused areas.

Only X (close) and Q (quit the application) work while the panel is open.
Calculations, sky drawing and simulation are suspended; in LIVE mode, only the
UTC/LOCAL clocks in the top bar refresh. Sky sprites that overlap the panel are
hidden until it closes. X triggers recalculation and redraw of the plot for the
current time. Simulation returns to its previous PLAY/PAUSE state.

The loader opens a resource through `open/read/close`, checks its header,
boundaries, length and CRC32, then clears BSS. The last successfully loaded
overlay stays in the window; selecting the same ID again skips the read and
preserves its BSS. Entry functions reset screen data as before. S uses the final
256 B and does not invalidate the overlay.

A read or validation error invalidates the window and ends the application:
files are closed, the console is restored, and the message identifies the
resource and cause. There is no retry or execution of partially loaded code.

## Validation

The test suite and reference images are described in [tests.md](tests.md).
