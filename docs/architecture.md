# SIDERA architecture

A standalone ROM for RP6502-PICO, RP6502-EMU and RP6502-WEB. [Building](../README.md).

## Memory window and layout

A single contiguous window at `$C800–$DFFF` occupies 6144 B, starts on a 2048-byte
boundary and stays at the same address for all overlays. `$C800–$DEFF` (5888 B)
holds the active overlay's code, constants, initialized data and BSS.
`$DF00–$DFFF` (256 B) is an independent scratch buffer, including 224 B for settings.

| `ROM:` resource | Contents | Bytes loaded into the window | BSS | Free within 5888 B |
| --- | --- | ---: | ---: | ---: |
| `sidera-init.ovl` | Options, settings and opening data files | 4637 B | 0 B | 1251 B |
| `sidera-cities.ovl` | City search, selection and map state | 4178 B | 431 B | 1279 B |
| `sidera-edit.ovl` | Location, date and time-zone editors | 3111 B | 112 B | 2665 B |
| `sidera-cover.ovl` | Splash screen and initial VGA assets | 829 B | 0 B | 5059 B |
| `sidera-solar.ovl` | Orbits and the Solar System panel opened with S | 5523 B | 82 B | 283 B |
| `sidera-find.ovl` | Object and constellation search/details | 5071 B | 455 B | 362 B |

Sizes are from the current cc65 build. Each resource also has a 24-byte header,
read outside the window. Build-time checks reject memory boundary violations.

The lower resident code, constants and BSS end at `$C623` (exclusive).
Target selection, figure geometry, world map and rendering helpers occupy 5728 B
in resident HIGH at `$E000–$F65F`, including 79 B of initialized workspace,
packaged as a second RAM block. After reserving the entire overlay window,
**637 B of unallocated RAM** remain below the C stack: 477 B at `$C623–$C7FF`
and 160 B at `$F660–$F6FF`. The C stack retains `$F700–$FEFF`, 2048 B.
Unused space is not an automatically available heap.

In the original standalone build used for comparison, data ends at `$D5B1`,
leaving 8527 B free. After adding X, removing the RMT layer and updating the
controls, city panel/map, FIND, simulation scheduling, early panels and partial rendering, **7890 B**
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

A single ld65 invocation links the program and six overlay files sharing the
same execution address. Calls use ordinary addresses fixed by that build;
`.ovl` files from different builds cannot be mixed. The packer assigns a shared
build identifier derived from both resident images and all overlays, and records
each overlay payload's CRC32.

Resident `main` selects an overlay through `ovl_select()` before calling its
functions. An overlay cannot replace its own window or retain pointers into it
after returning. Startup proceeds through INIT prepare, optional CITIES for
`/location`, INIT apply, COVER and the sky view. During observation, overlays
load when entering K, L/D/Z, S or Ctrl+F, and INIT loads for Ctrl+R configuration
reloads. Sky calculations and Ctrl+S saving are resident; footer rendering is
resident HIGH code. Reload validates the whole file and the clock for LIVE before
applying settings, then refreshes the sky without resetting the selected target.

FIND holds a frozen date and a four-record scan budget. Its 855 match indices use
1069 bytes of XRAM at `$E200–$E62C`, packed as 10-bit values, after label storage
and before planet palettes.
Sky jobs are suspended while this scratch list is in use. Closing FIND schedules
a full plot clear through the existing incremental renderer and invalidates the
footer, so Esc does not block on clearing the whole screen. Target identity,
name, position and stellar vector stay resident across overlay replacements.
The selected constellation index is resident too. Shared figure geometry uses
initialized HIGH_DATA so its workspace is part of the second loaded RAM block;
it does not rely on startup clearing the lower resident BSS. FIND computes
preview endpoints incrementally, then selection fits the completed sky cache.
The offline generator prepares 52-character table rows in five fixed columns;
FIND reads them from SIDFIND3 records rather than parsing identifiers while
drawing. The 312 px table occupies the left half; the right half holds full
names, wrapped aliases and positions. Search uses the complete alias field.
Changing rows on a finished page only inverts two 314-by-10 px rectangles,
touching 800 bitmap bytes without file reads. Partial-byte masks preserve the
outer frame. The detail delay uses the 125 Hz clock, starts after the highlight
update and can be bypassed by Enter. Details clear and paint in bounded steps;
new navigation cancels pending geometry and replaces the selected page/row.

## City panel

K opens CITIES at x=0..639, y=71..479: a 640×409 px rectangle covering the plot,
both compasses, side scales and footer, with no outer margin and a 1 px white frame.
The black-on-white ` CITIES ` label uses the Solar System title style. Search, 20 rows
spaced 14 px apart, coordinates, result count, messages and help stay inside.
Long text is clipped with an ellipsis before it reaches the frame.

The list is clipped at 52 characters, with clears restricted to x=1..327.
A 300x150 map occupies x=334..633, y=77..226, with five empty pixels to the top
and right frame. Full city/country names are below it. `world_map.c` loads three
pre-shifted 39-byte rows per event-loop pass. A new continent restarts this
incremental paint; moving within it updates only four sprite-coordinate bytes.
The marker is a red 5x5 plus, one pixel thick, centered on the city in a transparent
8x8, 1bpp Mode 5 sprite with no backing. Plane 0 overlays the bitmap at y=75..228,
allowing the arms to extend into the map margin at its edges, separately from the
header Sun at y=0..70. The descriptor uses XRAM `$B0F0–$B0F9`, after the 16 sky/SOLAR
descriptors; its eight bitmap rows and two palette entries use `$E700–$E70B`.
Map code is resident HIGH, but its file descriptor, 117-byte buffer and flags
belong to CITIES. Close the map and hide the sprite before unloading the overlay.
No sky XRAM caches are borrowed. Missing map data leaves city selection available.

Partial-byte masks preserve the frame during incremental row updates.
Opening the selector keeps VGA planes and header configured; sky sprites
are hidden. The resident event loop suspends sky jobs while CITIES is active.
Only city controls are shown while the selector is active. On selection or
cancellation, the entire panel is cleared and axes and footer invalidated before
redrawing the sky, both compasses, side scales and sky shortcuts.

## Solar System panel

S opens a static Solar System view for the current `moment` (UTC). Esc
closes it; reopening captures a new time. Ctrl+S/Ctrl+R are ignored inside panels and
does not save panel state. The **640×409 px** panel occupies x=0..639, y=71..479, matching CITIES:
it preserves the header and covers the plot, compasses, side scales and footer,
with no outer margin and a one-pixel frame. The diagram is centred at (320, 280).

The eight orbits are concentric circles with radii of 39, 55, 71, 87, 109, 131,
154 and 176 px. The layout is schematic, without an AU scale. Spacing prevents
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
by 8 px (text at y=83). `snapshot YYYY-MM-DD HH:MM:SS UTC` stays at y=75,
right-aligned with a 2 px margin.
At the bottom, `Schematic view (not in AU scale)` is left-aligned and `Esc: close`
is right-aligned, each with a 2 px margin. The sky keyboard shortcuts return after
closing. All three rows span x=8..631, leaving 8 px side margins. The first row
(y=441) contains
`Arrows/+Shift/+Ctrl: view/NWSE/FOV`, `Ctrl+S: save`, `Ctrl+R: reload`,
`Ctrl+F: find object` and `Ctrl+Q: quit`, with 19-20 px gaps. The second (y=451) contains N,
`Space: SIM PAUSE/PLAY`, +/-, [/], D and Z with 48 px gaps.
The third (y=461) contains `L: location`, `K: cities`, `S: Solar system`,
`P: planets`, `T: paths`, `G: grid` and `C: constellations`, with 27 px gaps.
The final C highlight ends at the label boundary to preserve the right margin.
The compass ends at y=438; y=439..440
stay empty. Scene clearing stops before `FOOTER_TOP` (441), and editors clear
from there through the screen bottom. G/T/C/P retain
inverse-video state labels. Messages start at x=8 on a separate row at y=472, aligned with Arrows.
When no note is present, pending work shows `[INFO] Working, please wait...`
until it finishes. Save/reload results override it for five seconds. The
LIVE/SIM header is drawn independently and contains no busy text.
Active G/C replace their colons with 1 px white lines at x=469/538, y=462..479;
mode 0 restores the colons. Message clearing and panel restoration preserve them.
The same row shows persistent white-on-black mode labels under G (x=473,
30 px) and C (x=542, 84 px). G1/G2/G3 read `scale`/`cross`/`full`;
C1/C2/C3/C4 read `stars`/`charts`/`charts & names`/`navigational`.
G0/C0 are blank. Ordinary messages must stay short enough to end before the G separator at x=469.
Independent caches redraw only a changed mode field; clearing the message row
restores both labels. Padded strings clear the full field on every transition.

Six planets use the existing images and palettes. The additional resource
`ROM:sidera-solar.dat` contains Earth and Neptune.
Panel descriptors occupy XRAM `$B096–$B0EF`, images occupy `$E160–$E1DF`,
and the two palettes occupy `$E200–$E5FF`, all in previously unused areas.

The SOLAR sprite list extends through y=479 on plane 1; sky sprites keep their
existing y=84..415 clip.

Only Esc (close) and Ctrl+Q (quit the application) work while the panel is open.
Calculations, sky drawing and simulation are suspended; in LIVE mode, only the
UTC/LOCAL clocks in the top bar refresh. Sky sprites that overlap the panel are
hidden until it closes. Esc clears the panel and invalidates axes and footer,
then triggers recalculation and redraw for the current time. Simulation returns
to its previous PLAY/PAUSE state. Ctrl+Q works with either Ctrl key, from the
sky, every panel and every editor. Plain Q and sky-view Esc do not quit.

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
