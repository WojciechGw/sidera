# SIDERA — Technical Reference

This document introduces the standalone SIDERA implementation to developers.
It explains the execution model, memory layout, rendering, data and build process.
For controls and command-line options, see the
[English user manual](../src/rom/sidera-manual-en.txt).

## Application and target

SIDERA is an offline sky observatory for RP6502-PICO and RP6502-EMU, written in C
and compiled with cc65. It displays the Sun, Moon, six planets and stars from all
88 constellations on a 640 × 480 VGA display. Users can change the observing
location, explore dates from 1900 through 2200, animate time, display daily paths
and inspect local rise/set times. A separate panel shows a schematic Solar System.

The deliverable is one `sidera.rp6502` file containing resident code, six code
overlays and the data resources. All astronomical calculations run on the 6502;
the application needs no network service. The RP6502 interface supplies console,
clock, filesystem and VGA access. The supported execution model is standalone.

## Source map

Start with `main.c`, then follow the relevant subsystem through this table.

| Source | Responsibility |
| --- | --- |
| [main.c](../src/main.c), [startup.c](../src/startup.c) | Resident application state, event loop, startup and overlay orchestration |
| [platform/](../src/platform/) | Console I/O, keyboard decoding, timing, VGA detection and monitor arguments |
| [overlays/](../src/overlays/) | Loader, linker layout, startup option and configuration parsing |
| [astro.c](../src/astro.c), [astro.h](../src/astro.h) | Fixed-point astronomy, date arithmetic and coordinate transforms |
| [graphics.c](../src/graphics.c), [constellations.c](../src/constellations.c) | Sky rendering, star projection, constellation caches and labels |
| [riseset.c](../src/riseset.c), [timezone.c](../src/timezone.c) | Local-day rise/set searches and time-zone lookup |
| [cities.c](../src/cities.c), [editor.c](../src/editor.c) | Location selection and location/date/zone editors |
| [world_map.c](../src/world_map.c) | Incremental geographic locator inside CITIES |
| [find.c](../src/find.c), [target.c](../src/target.c), [figure.c](../src/figure.c) | Object/constellation search, persistent selection and figure fitting |
| [files.c](../src/files.c), [cover.c](../src/cover.c) | Resource access, settings persistence and splash screen |
| [solar_math.c](../src/solar_math.c), [solar_draw.c](../src/solar_draw.c), [solar_labels.c](../src/solar_labels.c), [solar_ui.c](../src/solar_ui.c) | Solar System snapshot calculations, drawing, label placement and UI |
| [sidera.h](../src/sidera.h), [solar.h](../src/solar.h) | Shared declarations, graphics constants and XRAM layout |

File boundaries do not always match memory placement: segment pragmas and the
linker configuration determine which functions and data belong to each overlay.

## Startup and event loop

Startup first loads INIT to validate arguments, establish resource paths and read
settings. An explicit `/location` temporarily selects CITIES to resolve the
requested place. Resident startup code then selects INIT again to apply options.
The application opens its graphics resources, selects COVER for the splash and
initial graphics setup, and enters the sky view.

```mermaid
flowchart TD
    A[Resident startup] --> B[INIT: prepare and read configuration]
    B --> C{Explicit location?}
    C -->|Yes| D[CITIES: resolve location]
    C -->|No| E[INIT: apply startup options]
    D --> E
    E --> F[COVER: splash and graphics assets]
    F --> G[Resident sky event loop]
    G --> H[CITIES, EDIT or SOLAR when requested]
    H --> G
```

The program uses a single polling loop with incremental work. Keyboard input,
time updates, scene rendering, star transforms, daily paths and rise/set searches
share that loop. Persistent state lives mainly in globals and fixed-size buffers.

Immediately after the splash, startup draws the header and footer using the
loaded settings, before the first astronomical calculation. Sun/Moon AZ/ALT/LIT
fields show `...`; rise/set fields use the normal pending `..:..` format. Header
icons remain hidden. The first Sun/Moon result updates just those header fields
and reveals their correctly initialized icons before planet/star preparation.
Plot sprites stay hidden until the scene is ready, and rise/set results appear
when their incremental search finishes. This also applies without a saved preset.

Expensive work is split into bounded steps. Scene rendering processes up to eight
work units or a two-tick budget per call; an individual unit completes atomically.
Star projection processes up to four stars per step, and rise/set search performs
at most one ephemeris evaluation per step. These budgets keep input responsive,
but are not hard real-time deadlines.

In LIVE mode, displayed clocks update each second, solar/lunar/planetary positions
refresh approximately every minute, and the star cache can be reused for up to
five minutes. SIM/PLAY advances by five minutes when the previous drawing and
calculation jobs, including enabled daily paths, are complete. The minimum
start-to-start interval is 42 ticks (0.336 seconds); a busy step keeps its deadline
pending rather than restarting a one-second wait. It never catches up by skipping
instants. Actual speed depends on the workload. SIM/PAUSE holds the selected UTC
instant; manual +/- remains 15 minutes and pauses playback.

The loop sleeps only when background work is finished. Keyboard polling remains
between bounded job batches. For time updates, a new catalogue is prepared before the scene is
redrawn, avoiding an extra redraw of the previous star positions. The header and
body sprites are presented with that scene. Projection preparation also precedes
clearing the bitmap, so the old plot remains visible longer.

C follows the same preparation rule when enabling stars or changing their mode:
the selected footer option and Working indicator update immediately, while the
existing plot stays visible until pending star positions are ready. Further C
presses replace the pending mode; one final scene uses the latest selection.
Switching back to mode 0 removes the general star layer without waiting for its
calculation; a selected constellation remains visible and still needs its angles.

G/T/P/C update their footer highlights before starting chart work. The footer
caches the four visible on/off states and the last message. Only a changed label
is repainted; changing between enabled G or C modes requires no footer writes.
A changed message clears and redraws only its own line, including removal of
old trailing text. Unchanged LIVE/SIM refreshes perform no footer bitmap writes.
Canvas setup and Ctrl+L invalidate the whole footer; clearing its rows (including
editor entry) invalidates the affected content so the next call restores it.

Manual navigation has priority over this preparation: arrows, compass steps
and FOV changes start drawing immediately from the last complete star angles,
with exact projections calculated on demand. View/FOV fields update without
redrawing the rest of the header or footer. Pending star calculation resumes
after that drawing; its completion requests a fresh scene for the new instant.
This preserves the inactive buffer and prevents auto-repeat from continually
restarting a full-catalogue projection prepass. Complete rendering is still
incremental; prompt feedback does not mean the whole plot finishes immediately.

After star calculation, its inactive XRAM angle buffer temporarily holds exact
screen coordinates. Stars, edges and labels reuse them. `cs_begin()` invalidates
this cache before reusing the buffer for the next catalogue; pan, elevation and
FOV changes invalidate screen coordinates too. There is no interpolation.
Grid pixel rows are cached until elevation or FOV changes. Projection scaling
uses unsigned shifts with an explicit sign to retain signed division's truncation
toward zero without cc65's general 32-bit division routine.

Space cycles LIVE → SIM/PAUSE → SIM/PLAY → LIVE. Leaving LIVE freezes the
displayed time; returning reads the current system clock immediately. Shift+Right
selects the next 45-degree compass direction; Shift+Left selects the previous
one, including from bearings between compass directions. LCtrl+Left/Right narrows
or widens FOV by 10 degrees within 50–160. H and Shift+H have no action.

CITIES is a modal 640 × 409 pixel panel from below the header to the bottom of
the screen, covering the sky plot, both compasses, side scales and footer.
It has no outer margin, a 1-pixel white border and 20 rows per page. Only city
controls and messages appear inside the frame; the save reminder appears after
selection, back in the sky view. Drawing preserves the header and its sprites; sky sprites are
hidden until it closes. The main sky jobs remain suspended while selecting a
city. Closing clears the panel and invalidates the axes and footer so the plot,
scales, compasses and sky shortcuts are restored, including daylight inversion.

The left-hand list retains 20 rows, clipped to 52 characters. A 300x150 north-up, equirectangular world map
occupies the upper-right corner with a five-pixel inset from the frame. The
selected city's geographic continent is filled white; other land remains in
outline. A red 5x5 cross, one pixel thick, is centered on the city without a backing. Full city/country
names appear below the map. No matches removes the highlight and marker.
Each pass streams three raster rows; a move within the same region updates only
the sprite coordinates, leaving the map bitmap intact. The map is optional at runtime:
an I/O failure does not prevent selecting a city.

Seven conventional geographic regions are assigned from coordinates by
`scripts/world_regions.py`, independently of country membership. Europe/Asia
uses a simplified Urals–Caspian–Caucasus–Turkish Straits division. Oceania includes
Hawaii and New Guinea; Greenland is North American, Madeira/Canaries African.
The three region bits occupy bits 4..6 of the existing SIDCITY2 role byte, leaving
the default/capital/seat bits intact. Untagged legacy catalogues use a coarse
lookup raster from the map resource. These assignments serve a schematic locator.

F opens FIND with the same bounds and 20 rows. The 855-entry offline catalogue
contains eight Solar System bodies, the existing 759 stars and 88 constellations,
indexed back to their unchanged vectors and figures. Search matches a case-insensitive substring of names,
Bayer/Flamsteed designations, HIP or HD identifiers from two characters onward.
Constellations match Latin names and three-letter abbreviations. An exact name
or constellation abbreviation is placed first, ahead of partial matches.
The selected row shows exact AZ/ALT and Above/Below horizon for the frozen date.
The 312 px list on the left has five fixed columns: Type, HIP, HD, Designation
and Name. Types use Sun/Moon/Pl/St/Con. Empty cells stay blank, long values use an
ellipsis, and selection inverts the whole row.
The primary designation is Bayer's when available, otherwise Flamsteed's;
constellations use their three-letter code. Full names, all aliases and positions
appear on the right of the 20-row list. All original aliases remain searchable.
On a finished page, arrow navigation XORs the old and new row rectangles, with
800 bitmap writes and no catalogue reads. Page clearing, painting and figure
calculations accept new navigation. Details wait for 250 ms without navigation,
then calculate the latest object and paint one detail line per polling step.
Enter bypasses the delay but never accepts a stale position.
Queries replace an unfinished scan; Esc cancels, and Enter during work waits for
the latest result. Scanning, clearing old results and painting yield to input.

For single objects, Enter sets bearing/elevation, preserves FOV, and enables P or C1 if necessary.
LIVE first resynchronizes the time, then centres the freshly computed position.
In C4 the selected star is drawn in addition to the restricted groups. A resident
marker follows its changing position without moving the camera. Cancellation
preserves the previous selection; replacing it or exiting ends that selection.
Turning its layer off hides the marker. Selection is not part of saved settings.
The view range, CLI validation and settings now allow elevation -90..+90 degrees.
The selected star is omitted from the normal cached star pass and rendered once
at its exact current position with its catalogue size. Its marker replaces the
usual navigation/planet label to avoid duplicate labels.

For constellations, `figure.c` finds the shortest azimuth arc and altitude bounds
of all figure endpoints, centres the view and fits both dimensions with an 8 px
margin plus angular rounding allowance. FOV is rounded up to 10-degree steps,
within 50..160 degrees; a message reports an insufficient maximum. Previews
calculate two endpoints per polling step for the frozen date and report whether
the figure is above, below or crossing the horizon. After selection, the fit uses
the completed angular cache for the current sky time. Selected stars, solid lines
and an inverse figure label are drawn independently of C, including C0 and C4.
The mode remains unchanged. LIVE refreshes the angular cache at each displayed
time while a figure is selected; the camera moves only on selection.

The X panel captures a snapshot of the displayed date. While open, it suspends
sky work and simulation advancement; LIVE clocks can still update. X closes the
panel and refreshes the sky, and Q exits. Panel state is not saved in settings.

## CPU memory and overlays

The CPU address layout is defined in
[overlay.cfg](../src/overlays/overlay.cfg). Addresses below are hexadecimal and
ranges include both endpoints.

| CPU address range | Use |
| --- | --- |
| `$0200` to below `$C800` | Resident code, constants and application data |
| `$C800–$DEFF` | One active overlay: 5,888 bytes shared by code, constants, initialized data and BSS |
| `$DF00–$DFFF` | Independent 256-byte scratch area, including a 224-byte configuration buffer |
| `$E000–$F6FF` | Resident HIGH target/figure code and initialized workspace; unused tail is not a heap |
| `$F700–$FEFF` | 2,048-byte C software stack |

The six overlays serve distinct UI phases:

| Overlay | Contents |
| --- | --- |
| INIT | Startup options, configuration parsing and initial file handling |
| CITIES | Catalogue search and city selection |
| EDIT | Observer, UTC date and time-zone editors |
| COVER | Splash screen and initial VGA asset loading |
| SOLAR | Solar System calculations, drawing and labels |
| FIND | Incremental catalogue search, results and exact selected-object details |

A single ld65 link assigns all overlay execution addresses. The
[packer](../scripts/pack_overlays.py) produces the ROM resources and stamps the
both resident images and overlay headers with a common eight-byte build identifier.
Overlays are specific to that linked build; they are not independently relocatable
plugins.

Each overlay resource starts with a 24-byte header containing the `SOV1` magic,
overlay ID, format version, payload length, BSS offset and length, build identifier
and payload CRC32. The [resident loader](../src/overlays/overlay.c) checks the
header and bounds, reads the payload, rejects trailing data, checks the checksum
and clears the overlay's BSS. A failed load invalidates the active overlay and
ends the application with a diagnostic. Selecting an already active overlay
does not reload it, so its local state survives that selection.

Resident code must control overlay changes. An overlay cannot replace the memory
containing its own executing code. Any state needed after replacement must live
outside the overlay window, and pointers into a replaced overlay are invalid.
Reloading an overlay resets its BSS. Breakpoints at shared addresses also depend
on which overlay is active.

`SIDERA_OVERLAYS` enables this placement and loader integration; tests can build
the same code without the overlay layout. It does not select a different operating
system. For measured sizes and available headroom, see the
[memory report](architecture.md) and the current build's linker map. In particular,
SOLAR has little spare space, so changes there require a size check.

## RP6502 interface and graphics memory

The [platform implementation](../src/platform/rp6502.c) reads and writes the RIA
console registers directly. The [keyboard decoder](../src/platform/key.c) handles
RIA character sequences for navigation, editing and modifier keys. Since these
sequences combine left and right Ctrl, startup also maps the 32-byte USB HID
keyboard bitmap at XRAM `$FFC0–$FFDF` using `xreg(0,0,0,address)`. Ctrl+horizontal
arrows read its modifier byte at `$FFDC`: left Ctrl must be down and right Ctrl
must be up to produce a FOV event. An unmapped keyboard or a generic Ctrl sequence
alone cannot change FOV. Cleanup unmaps the bitmap with address `$FFFF`.
See the [RIA keyboard interface](https://picocomputer.github.io/ria.html#keyboard).
The application requires VGA. Timing uses
125 ticks per second; elapsed-time comparisons use unsigned subtraction to allow
the 16-bit tick counter to wrap. Sleeps are busy waits. VGA detection and frame
synchronization use bounded waits.

The [monitor argument buffer](../src/platform/argv.c) is a resident 512-byte
`__argv_mem` implementation. It is compiled directly into the executable target,
because putting it only in a static library can allow cc65's default implementation
to satisfy the symbol instead. File access uses cc65 descriptors and operations
such as `open`, `read`, `lseek` and `close`.

VGA data lives in **XRAM, a separate address space from CPU RAM**. RIA address,
step and data registers provide access to it; assets can also be read directly
from a file into XRAM. Equal numeric addresses in the CPU and XRAM maps do not
refer to the same storage.

The 640 × 480 one-bit canvas occupies XRAM `$0000–$95FF` (38,400 bytes).
The rest holds the font, palettes, sprite images, VGA descriptors, star positions,
constellation metadata and edges, label storage and daily-path caches. The Moon
and star angles use double buffers; the inactive star buffer also serves as the
screen-coordinate cache between catalogue calculations. Consult
[sidera.h](../src/sidera.h) and [solar.h](../src/solar.h) before changing allocations;
the graphics and calculation subsystems share this address space.

The sky combines a monochrome canvas with coloured bitmap sprites and an affine
Moon sprite. Its projection maps azimuth and altitude linearly to screen
coordinates, wrapping azimuth around the view direction. Horizontal field of view
ranges from 50 to 160 degrees. The Moon selects one of 129 stored phase images
and rotates it to the calculated orientation. Sun and planet sprite ordering uses
geocentric distance; labels and daily paths require additional overlap handling.

Perimeter scales are preserved independently of the plot interior. The upper
and lower compasses change with bearing or FOV; the side scales change with
elevation or FOV. Grid visibility also controls the upper and side scales.
LIVE and SIM/PLAY updates leave all unchanged margins in place, except for the
lower compass's daylight style. Its full-width band (rows 426–440, below the V)
uses black ticks and directions on white from the displayed Sun Rise minute
(inclusive) to Set (exclusive). At night it uses white on black. Polar UP/DOWN
and civil days with just one crossing are supported; pending, stale or unavailable
event results use the normal dark band. A style transition XORs only that band,
including when time runs backward, without clearing the plot or the V.
Direction glyphs occupy rows 433–439, leaving one empty row after the ticks
and one row of background below the letters.
Clearing uses
masked writes at the plot edges because margin and plot pixels share bytes.
Left scale ticks inside the plot and the interior horizon are restored on each
scene. Regions become valid only after drawing finishes, so interrupted view
changes cannot leave an incomplete scale cached. Full bitmap clears and Ctrl+L
invalidate the affected regions. The interior grid still redraws normally.

The Solar System panel occupies 317 × 317 pixels. It shows all eight planets,
including Earth and Neptune, on eight fixed concentric circles. Directions follow
the calculated heliocentric positions, but circle radii are schematic rather than
an astronomical-unit scale. Its calculations and label placement are separate
from the main sky renderer.

## Astronomy, numeric units and time

Astronomy uses fixed-point arithmetic, lookup tables and integer intermediates.
The public types and conversion functions are in [astro.h](../src/astro.h).

| Quantity | Representation |
| --- | --- |
| Azimuth and orbital angles | One full turn is 65,536 units, with unsigned wraparound |
| Altitude angles | Signed units on the same angular scale |
| Trigonometric values and unit vectors | Signed Q14: 16,384 represents 1.0 |
| Observer latitude and longitude | Signed 32-bit integers in tenths of an arcsecond |
| Observer height | Integer metres |
| Planet/Sun distances used for ordering | Units of 1/1,024 AU |
| Application date | Gregorian UTC fields, years 1900–2200; no leap-second representation |

Custom date arithmetic uses 1999-12-31 as day zero and avoids depending on the
platform's `time_t` range for simulated dates. LIVE mode obtains the current UTC
date from the platform clock. Time-zone offsets affect local display and civil-day
boundaries, while the simulation instant remains UTC.

The Sun, Moon and planetary calculations adapt Paul Schlyter's low-precision
orbital model. The model and its simplifications are described in
[How to compute planetary positions](https://stjarnhimlen.se/comp/ppcomp.html).
SIDERA's implementation is in [astro.c](../src/astro.c); the snapshot panel's
heliocentric calculations are in [solar_math.c](../src/solar_math.c).

The sky calculation includes lunar perturbations, observer-dependent lunar
parallax and selected perturbations for Jupiter, Saturn and Uranus. Stars start
as J2000 unit vectors and use a cached matrix combining precession and the local
coordinate frame. The bundled catalogue contains 759 stars and 745 constellation
edges. Daily Sun/Moon paths contain 97 samples at 15-minute intervals across a UTC
day.

[Rise/set search](../src/riseset.c) instead follows the selected **local civil
day**, converting its boundaries through the time-zone data. It scans for horizon
crossings and refines brackets, accounting for the upper limb and standard
refraction. This distinction matters around daylight-saving changes. The UI also
handles days without a crossing and dates where a complete search is unavailable.

The implementation targets an interactive visual observatory. Regression tests
compare selected cases with stored reference data; their tolerances are not a
global accuracy guarantee over all dates and observers. See
[astronomy and rendering tests](tests.md) for coverage and reference provenance.

## Resources and settings

Resources are embedded in the ROM and opened through `ROM:` paths.

| Resource | Format and purpose |
| --- | --- |
| `sidera.dat` | `SIDERA5` header; font, palettes, Sun and Moon images, six sky-planet images, star/constellation data and splash assets |
| `sidera.find` | `SIDFIND3` header and 855 fixed 256-byte records: object index, label, complete searchable aliases and a padded 52-character table row |
| `sidera-solar.dat` | Additional Earth and Neptune images and palettes for the X panel |
| `sidera.cty` | `SIDCITY2` header and fixed-size city records with coordinates, height and time-zone ID |
| `sidera.map` | `SIDMAP1` header, eight 300x150 map variants in shifted 39-byte rows, then a geographic lookup raster; 91816 bytes |
| `sidera.tz` | `SIDTZ001` header, sorted zone index and UTC intervals carrying local offsets |
| `help`, `manual` | Startup help and the English user manual |
| `sidera-gpl.txt`, `sidera-notice.txt`, `sidera-sdk-license.txt` | GPLv3 text, project licensing scope and the retained SDK license |

The current city data contains 345 cities in 195 countries. The time-zone database
contains 598 zones, generated from the pinned tzdata package in
[src/data/](../src/data/). Zone names and offset intervals are searched in the file;
the whole database is not loaded into CPU RAM. Binary readers and generators must
agree on field sizes and byte order independently of the host compiler's C types.

`/assets` can override the main graphics resource. Asset, city and time-zone
lookup rules are implemented in [files.c](../src/files.c),
[startup file handling](../src/overlays/init_files.inc),
[cities.c](../src/cities.c) and [timezone.c](../src/timezone.c).
Other resources, including overlays, remain bundled with the application.

Settings are a small text file containing the observer coordinates, IANA zone
name, city label, view options and either LIVE mode or a UTC date. The default
path is `MSC0:/sidera.cfg`; `/config` overrides it. The emulator launch configuration
uses `SAVE:/sidera.cfg` for persistence.

Precedence is **defaults → saved configuration → startup options**. The last
repeated option wins. An explicit `/location` chooses a direction based on the
current local hour unless `/view` overrides it. Settings are written explicitly
with S, and a restored simulation date starts paused. Parsing uses a bounded
buffer and validates fields before applying the configuration.

## Building and validating changes

The build requires CMake 3.28+, Ninja, Python 3.12+, Git and a cc65 installation
with RP6502 support, including sim65. Builds require Windows; native tests
additionally need MSVC.
See the [README](../README.md) for environment setup and device launch instructions.

From the repository root:

```text
cmake --preset cc65/Debug
cmake --build --preset cc65/Debug
ctest --preset cc65/Debug
```

Use `cc65/Release` for the Release build. Both presets enable optimization;
Debug also emits debug information. The Debug ROM is written to
`build/cc65/debug/sidera/sidera.rp6502`, alongside linker and debug artifacts.
The [top-level CMake file](../CMakeLists.txt) defines the application;
[SideraOverlays.cmake](../cmake/SideraOverlays.cmake) integrates packing, and
[SideraTests.cmake](../cmake/SideraTests.cmake) integrates the tests.

Validation covers several distinct boundaries:

- Native C tests exercise calculations, settings, catalogues, rendering and
  application flow with host implementations of platform services.
- sim65 tests execute compiled 6502 code to catch target integer-width,
  arithmetic, parser and loader issues that a native build can miss.
- Image checks compare three sky views and three Solar System views with the
  committed reference images.
- ROM checks validate the packed overlays and resources. Emulator smoke tests
  exercise the complete ROM, keyboard interaction and saved settings.
- Generator checks reproduce the committed assets from local source data.

Hardware timing and device behaviour still require testing on PICO. For a code
change, run the applicable tests and inspect the linker map if memory placement
or size changed. The [test guide](tests.md) describes individual checks.

Ordinary builds use committed files from `src/rom/` and `src/tables/`. To verify
their reproducibility without replacing them:

```text
python scripts/regenerate.py --output build/generated --check
```

Generators use local inputs and write to `build/`. Updating committed assets is
a separate review step. Optional importers and reference-data download scripts
are not part of normal builds or tests.

## Further reading

- [English user manual](../src/rom/sidera-manual-en.txt): controls, startup options
  and observable behaviour.
- [Architecture and memory](architecture.md): detailed overlay sizes and graphics
  layout notes.
- [Tests](tests.md): validation procedures and reference data.
- [Provenance](provenance.md) and [source manifest](source-manifest.json): source
  history, data origins and extraction records.
- [License notices](../LICENSE-NOTICE.txt): retained author and third-party notices.
- [GPLv3](../LICENSE): the project's original code and documentation use
  `GPL-3.0-only`; third-party components retain their identified terms.
