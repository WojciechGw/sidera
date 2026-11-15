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

- SIDERA is an offline sky observatory for **RP6502-PICO, RP6502-EMU and RP6502-WEB**, built
  with cc65 and RP6502-SDK into one `sidera.rp6502` containing code and resources.
- Only standalone operation is supported. RMT/razemOS compatibility and obsolete
  active-code names were removed. Preserve historical provenance and attribution.
- WEB uses SDK player 0.43 and the same ROM through `rp6502_web()`; builds produce
  `web/sidera.zip` and `web/sidera/`. F5 serves the latter on `127.0.0.1:8000`;
  keep the origin stable for IndexedDB saves (`SAVE:/sidera.cfg`). `web.yml` builds
  and tests Release on Windows, then deploys Pages after pushes to `main` or a
  manual run on `main`. Ubuntu only uploads/deploys the prepared static files.
  `scripts/package_pages.py` stages the player at `sidera/`, the preview from
  `src/assets/screenshot.png` as `sidera/screenshot.png`,
  and a source ZIP from the same checkout. Pages must use GitHub Actions as source.
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
| Object search and selected target | `src/find.c`, `src/target.c`, `src/figure.c`, `scripts/find_catalogue.py`, `src/data/find-names.json` |
| Memory, loader, ROM packing | [Architecture](docs/architecture.md), `src/overlays/`, `scripts/pack_overlays.py`, `cmake/SideraOverlays.cmake` |
| Build and validation | [README](README.md), [test guide](docs/tests.md), `CMakeLists.txt`, `cmake/SideraTests.cmake`, `tests/` |
| User-visible behaviour | [English manual](src/rom/sidera-manual-en.txt), `src/help.txt` |
| Data origins and historical changes | [Provenance](docs/provenance.md), [cleanup report](docs/rmt-cleanup.md), `docs/source-manifest.json` |

## Constraints that matter when editing code

- One resident polling loop performs incremental work. Preserve responsiveness
  and LIVE/SIM behaviour when changing rendering or astronomical calculations.
  `astro.c` keeps resident caches keyed by UTC day and the complete observer;
  sidereal terms are prepared per instant. Keep astronomical entry calls serial
  and preserve exact rounding when sharing these intermediates.
- After the splash, show the header and footer before astronomy runs, including
  saved settings. Sun/Moon fields start as placeholders with header icons hidden.
  Publish their first results before planet/star preparation; keep plot sprites
  hidden until the prepared scene. Later time updates retain their existing order.
- Space enters SIM/PAUSE from LIVE, then toggles SIM/PAUSE ↔ SIM/PLAY.
  N returns to LIVE and immediately reads the clock; Shift+Right/Left select the
  next/previous compass direction. Only physical LCtrl+Left/Right changes FOV;
  RCtrl and the former H shortcuts do not. The USB HID bitmap occupies XRAM
  `$FFC0–$FFDF` and must be unmapped on exit. VGA is required.
  CITIES fills x=0..639, y=71..479, covering the plot, compasses, side scales and
  footer, with no outer margin, a 1-pixel frame and 22 rows per page. Keep the
  inverse " CITIES & LOCATIONS " title at x=2, y=83. Preserve
  the header and show only city controls. Closing clears the panel and
  invalidates axes and footer so the sky and its shortcuts are restored.
  SOLAR uses the same 640x409 bounds and frame, with an enlarged centred orbit
  diagram and unchanged sprite/text sizes. S opens SOLAR (X is unused).
  Esc alone closes SOLAR; S and plain Q are ignored there. Closing clears the
  panel and restores axes and footer.
  Ctrl+Q (either Ctrl) is the only application quit shortcut, including inside
  panels and editors. Plain Q and Esc in the sky view do not quit.
  Ctrl+S (either Ctrl) saves only in the main sky view; panels/editors ignore it.
  Ctrl+R (either Ctrl) reloads the same config_path only in the sky view, through
  INIT selected by resident main. Restore all saved fields, LIVE with a fresh
  clock or the saved date in SIM/PAUSE, then invalidate axes and refresh sky/jobs.
  A missing/invalid file or unavailable LIVE clock preserves current settings.
  Keep the selected FIND target. Save/reload report "[INFO] Configuration saved"/
  "[INFO] Configuration reloaded" or "[!] Failed to save configuration"/
  "[!] Failed to reload configuration". These results expire after five seconds;
  repeating the action restarts the timer. Expiration preserves G/C mode labels
  and never paints over modal panels or clears a newer, unrelated note.
  Footer layout and mode bars reside in HIGH; the message-cache helper is lower resident code.
  CITIES reserves x=334..633, y=77..226 for a 300x150 geographic locator;
  the 22-row list stays left of x=328 and its last row starts at y=425.
  Nine rows below the map at x=334, y=240+14*n show city:, country:, continent:,
  LAT:, LON:, AMSL:, timezone:, UTC:, time:, with values at x=400.
  LAT/LON retain DMS with 0.1 arcsec precision and N/S/E/W; AMSL is in metres.
  The former bottom coordinate line is removed; the result count stays at right.
  Zone offset and winter/summer/no-DST status use the displayed LIVE/SIM instant;
  browsing preserves the active zone/cache. DST1 metadata in sidera.tz classifies
  each interval for its local year; negative DST is normalized to winter/summer.
  Same-zone navigation preserves the timezone/UTC/time rows without I/O or drawing.
  `world_map.c` streams three rows per `cities_step()` and moves only the sprite
  when the continent is unchanged. Keep its state in CITIES and
  call it only with that overlay loaded; rendering code resides in HIGH.
  SIDCITY2 flags bits 4..6 carry geographic region 1..7; zero uses the map's
  coarse lookup for older external catalogues. Preserve low role bits and format.
  Bit 3 marks a research station. `src/data/stations.json` records reviewed
  COMNAP stations and exclusions, with separate educational/non-commercial
  data terms in sidera-stations-license.txt. Stations precede cities under AQ;
  operator codes/full names are searchable, and details use station:.
  AQ defaults to Arctowski; PL defaults to Warsaw. AQ/Arctowski and
  PL/Arctowski both work. Operator aliases occupy SIDCITY2 country padding
  bytes 14..42; keep the 96-byte format and ordinary city records unchanged.
  CITIES record/query/line use 248 scratch bytes at $DF00; configuration I/O
  may borrow this buffer only outside CITIES.
  SIDCITY2 bit 7 marks Special locations [XX], ordered before stations/cities.
  `src/data/locations.json` holds 21 points: four original reference points,
  nine summits (including Mont Blanc/Kosciuszko alternatives) and eight landmarks.
  The original four and Vinson use Etc/UTC; other points use local IANA zones.
  Details use location: and the actual country, or - for the original four.
  Special country bytes 3..31 hold the name; 32..42 hold |CC search aliases.
  Names show country codes; XX grouping and country searches are independent.
  South Pole has region 7; the other three have explicit region 0 (contours and
  marker). Bit 7 suppresses the legacy region lookup. XX has no CLI default;
  names and XX/Name work. Keep the original city/station records unchanged.
  The city marker is a red 5x5 plus, one pixel thick and centered on the city,
  without backing, in a transparent 8x8 Mode 5
  sprite on plane 0, below the header Sun. Its descriptor occupies `$B0F0–$B0F9`
  and image/palette `$E700–$E70B`; hide it on close and no matches. No results
  shows contours alone. Natural Earth provenance is in sidera-map-license.txt.
- Ctrl+F (either Ctrl) opens FIND; plain F is inactive. The panel uses the same
  bounds as CITIES and 20 rows. The offline catalogue
  includes Sun, Moon, six planets, all 759 HIP stars and 88 constellations.
  The 312 px table on the left has Type/HIP/HD/Designation/Name columns and a
  full-row highlight; complete names, aliases and positions appear on the right.
  Types are Sun/Moon/Pl/St/Con. The generator prepares 52-character padded rows
  in SIDFIND3 records; all aliases remain searchable. On a completed page, arrows
  invert only the old/new row (800 bitmap bytes, no catalogue read). Accept
  navigation during page painting and figure calculations. Defer details until
  250 ms without navigation; Enter bypasses this delay for the latest selection.
  Match constellation Latin names and abbreviations; prioritize exact matches.
  Freeze the displayed time and sky jobs while the panel is open;
  scan, clear and paint in bounded batches. Enter locates the object without
  changing FOV for a single object, enables P or C1 if required, and keeps C4 plus
  the chosen star. A constellation selection instead fits its figure with margin
  at FOV 50..160; report when 160 is insufficient. Show its stars, solid lines and
  inverse name independently of C, including C0/C4, without changing the C mode.
  Refresh its cached angles at every displayed time, including LIVE.
  A resident target marker follows the object's position, not the camera, until
  another selection or exit; cancelling FIND preserves it. Selection is not saved.
  View elevation, CLI and saved settings accept -90..+90 degrees.
- SIM/PLAY uses exact 5-minute steps, a 42-tick minimum interval and waits for
  pending jobs; manual +/- stays 15 minutes. Prepare stars before redrawing.
  Manual navigation must bypass catalogue/projection preparation and update
  only the View/FOV header fields; test input latency with `sidera_input`.
  C selects its mode and updates the footer immediately, but waits for pending
  star calculations before drawing once. Repeated C presses replace the pending
  selection; preserve the immediate response of arrows and FOV.
  G/T/P/C update their footer highlights before calculation or chart drawing.
  Three shortcut lines at y=441/451/461 have 8 px side margins and gaps
  differing by at most one pixel within each row. Row 1:
  "Arrows/+Shift/+Ctrl: view/NWSE/FOV", "Ctrl+S/+R: safe/reload config", "Ctrl+F: find object",
  "Ctrl+Q: quit". Row 2: N, Space, +/-, [/], D, Z; Space reads
  "SIM PAUSE/PLAY". Row 3: "L: location", "K: cities & locations", "S: Solar system",
  "P: planets", "T: paths", "G: grid", "C: constellations".
  Row 1 gaps are 20 px; row 2 gaps are 48 px; row 3 gaps are 15 px.
  FIND appears only in row 1.
  Notes start at x=8, y=472, aligned with Arrows. Active G/C replace their
  colons with 1 px white lines at x=481/538, y=462..479; mode 0 restores
  the colons. Preserve these lines through message clearing and panel restoration.
  At x=485, G1/G2/G3 show "scale"/"cross"/"net";
  at x=542, C1/C2/C3/C4 show "stars"/"charts"/"charts & names"/
  "navigational", white on black. G0/C0 leave their fields blank.
  Keep ordinary notes short enough to end before these persistent mode labels.
  Sky work uses the bottom message field at x=8, y=472 without a timer:
  "[INFO] Calculating Sun & Moon...", "Calculating planets...", "Calculating stars...",
  "Calculating rise/set...", "Calculating paths..." or "Drawing sky...", all with
  the INFO prefix. Select the stage before its work; gx_status(1) preserves it,
  gx_status(0) clears it. Stars, rise/set and paths are a pending-job bitmask:
  show one combined description and remove finished/disabled jobs, rather than
  alternating descriptions for each batch. Nonempty notes take priority; after save/reload results
  expire, restore the latest stage if work remains. Stage changes keep the Moon
  animation running continuously. Detailed text is sky-only, never in panels.
  Message updates draw only 78 padded/clipped cells at x=8..475, never clearing
  G/C fields or their separators. Full row clearing is only for invalidated
  footer restoration. CITIES, L and Z acceptance clear notes; no save reminder.
  All nonempty sky messages (work, save/reload and errors) invert the entire
  x=8..475, y=472..479 field. Empty messages restore black. Appearance/disappearance
  redraws all 78 cells; changing a nonempty message preserves the inverted tail.
  Reuse descriptions for unchanged work sets and redraw only through the last
  differing character. Startup-only error strings are in COVER's startup_messages.
  Keep the top-right LIVE/SIM status separate and never paint over modal panels.
  Busy work also animates the existing unscaled 32x32 Moon at x=3, y=33 after
  63 ticks (~0.5 s). Twenty phase/orientation frames take 250 ticks (2 s).
  Only this indicator has a fixed 15-degree counterclockwise tilt about its centre.
  It applies to sky and modal work, never idle text entry. The Mode 4 descriptor
  at $E6E0..$E6F3 uses plane 2, clipped to y=33..64, above the header sprites.
  Reuse only the inactive Moon image buffer; real phase changes hide/invalidate
  the animation before swapping buffers. Ordinary animation uploads keep the
  previous sprite visible throughout the read; never hide it per frame.
  Animation failure hides it without
  changing notes. Service it at bounded work points, not from interrupts.
  sky_start, gx_canvas and gx_header_pending are startup-only COVER code, called while
  COVER remains loaded; gx_wait_step and its state remain resident. Never call
  these startup functions with another overlay loaded. Do not retain a sequential asset_fd read position across
  gx_wait_step: animation uploads seek within sidera.dat.
  The compass ends at y=438; keep y=439..440 blank. Scene clearing stops before
  FOOTER_TOP (441); editors clear from there. Keep highlights aligned.
  Cache footer highlights, mode labels and note text; unchanged content must cause no bitmap
  writes. Clear operations invalidate the affected rows; Ctrl+L and canvas setup
  invalidate the whole footer. Editors overwrite it and require restoration.
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
- Six overlays (INIT, CITIES, EDIT, COVER, SOLAR, FIND) share CPU `$C800–$DEFF`
  (5888 bytes for code, constants, data and BSS). Scratch is `$DF00–$DFFF`;
  the C software stack is `$F700–$FEFF`. Target code uses resident HIGH starting
  at `$E000`, alongside shared figure geometry and rendering helpers. Package this
  second RAM block, including initialized HIGH_DATA, in the build identity.
  FIND packs 855 match indices into 1069 bytes of XRAM `$E200–$E62C` (10 bits each)
  only while sky jobs are suspended.
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
Builds and tests require Windows; native host tests use MSVC. CI runs Windows
Debug and Release. Linux/WSL build support was removed on 14 November 2026;
retain earlier validation results as historical evidence, not current instructions.
In PowerShell, read UTF-8 documentation with `Get-Content -Encoding utf8`.

- Documentation only: check meaning, relative links and `git diff --check`.
  No application build is needed unless an edited text file is packed into ROM.
- Code changes: build and run the affected host/sim65 tests. Use the full suite
  for changes spanning subsystems, shared startup or application flow.
- ROM resources, packaging or overlays: build and run
  `ctest --preset cc65/Release -R "^sidera_(rom|emu)$"` (or the matching Debug preset).
  Inspect memory headroom when code or layout changes.
- WEB packaging/launch changes: also run `sidera_web_package` and the separate
  Chromium smoke test in `tests/web_smoke.py`; setup and coverage are in
  [the test guide](docs/tests.md#web-browser-smoke-test). CI runs it for both presets.
- Graphics changes: include the six reference-image checks in `sidera_host_suite`.
  Review intended visual differences before replacing reference images.
- Generator or source-data changes: run
  `python scripts/regenerate.py --output build/generated --check`.
- For compiler/platform-sensitive changes, validate Debug/Release and the affected
  host environments. Report exactly what ran; do not infer untested results.

Automated tests do not upload to PICO. Physical PICO runs are performed by the
owner; agents use host, sim65 and emulator checks. Hardware launch through F5
uploads to PICO.
The ignored `.rp6502` contains local device settings; keep credentials out of Git.

## Keep this context useful

Update this file when durable decisions, module locations, commands or constraints
change. Update the relevant technical document for implementation details and
dated reports for new validation evidence. Keep this file concise; do not turn it
into a transcript, a task log, or a duplicate of the technical reference.
