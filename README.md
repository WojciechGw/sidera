# SIDERA

<!-- rp6502
preset: cc65/Release
publish: sidera.zip
-->

A standalone sky observatory for **RP6502-PICO, RP6502-EMU and RP6502-WEB**. The project
uses cc65 and the [RP6502-SDK](https://picocomputer.github.io/sdk.html)
to produce a single `sidera.rp6502` containing all data and six overlays.


[![Play Sidera](https://wojciechgw.github.io/sidera/sidera/screenshot.png)](https://wojciechgw.github.io/sidera/sidera/)

**[Play in your browser](https://wojciechgw.github.io/sidera/sidera/)** · **[Download the ROM](https://wojciechgw.github.io/sidera/sidera/sidera.rp6502)**

On 13 November 2026, the owner confirmed that SIDERA starts and runs on a physical
Picocomputer 6502. See the [hardware validation record](docs/rmt-cleanup.md#physical-pico-checks).

## Requirements

- Windows with PowerShell, CMake 3.28+, Ninja, Python 3.12+, Git.
- A current cc65 version with RP6502 support, including `cl65`, `ca65`, `ld65`, `ar65`, `sim65`.
  Follow the [SDK installation instructions](https://picocomputer.github.io/sdk.html#installing-the-prerequisites).
- For host tests: Visual Studio 2022 Build Tools with C++ and the Windows SDK.

Core tests and generators use the Python standard library. The separate WEB
browser smoke test uses Playwright and Chromium (also run by CI). Reference data, source
artwork and the pinned tzdata package are stored locally. The first SDK configure
step may download the native emulator and WEB player 0.43; subsequent builds use
the local copies. WEB runs in a desktop browser with WebAssembly, WebGL2 and a keyboard.

## Building and testing

From the project directory, in PowerShell:

```text
cmake --preset cc65/Debug
cmake --build --preset cc65/Debug
ctest --preset cc65/Debug

cmake --preset cc65/Release
cmake --build --preset cc65/Release
ctest --preset cc65/Release
```

Outputs: `build/cc65/debug/sidera/sidera.rp6502` and
`build/cc65/release/sidera/sidera.rp6502`. The same directories contain `.map`, `.lbl`, `.dbg`,
`.sym`, the resident images and six `.ovl` files. Do not mix files from different builds.
The build automatically checks the ROM structure, CRC, build identifier,
BSS and memory boundaries. Each configuration also builds native test executables.
Each build also writes `web/sidera.zip` and its unpacked `web/sidera/` directory
under the selected build directory. The package contains the same ROM, an HTML
page and the SDK JavaScript/WebAssembly player.

Both presets retain the original `-O` optimization, since overlay sizes are part
of the architecture. Debug uses `-g`, Release uses `-DNDEBUG`; the SDK also provides
debugger information for Release outputs. Native test assertions remain enabled
in both configurations.

## Running

In VS Code, select a preset, the `sidera` target and the `RP6502-EMU`,
`RP6502-PICO` or `RP6502-WEB` launch configuration. F5 for PICO **uploads the program to the device**;
use it deliberately. PICO connection settings (`device`, `key`, `workdir`, `args`)
are stored in the local, ignored `.rp6502` file, as specified by the SDK.
Do not store keys in the repository.

On hardware, the default configuration file is `MSC0:/sidera.cfg`. Use `/config`
to select another path. The VS Code EMU configuration passes
`/config SAVE:/sidera.cfg` to enable persistent saves in the emulator.
When launching from the command line, pass the same arguments after `--`.
The emulator test uses its own empty save directory and does not access the device.

For WEB, F5 builds the application, serves `web/sidera/` and opens
`http://127.0.0.1:8000/`. It requires the VS Code Python Debugger and CMake Tools
extensions. Alternatively, after building Debug, run:

```text
python -m http.server --bind 127.0.0.1 --directory build/cc65/debug/web/sidera 8000
```

Open `http://127.0.0.1:8000/` and click the emulator to focus the keyboard.
The page starts silently; SIDERA uses its normal controls. Opening `index.html`
directly with `file://` does not work. Stop the server with Ctrl+C (or stop F5).
WEB passes `/config SAVE:/sidera.cfg`; S saves settings to the browser's IndexedDB.
Return using the same browser profile, hostname and port to restore them.
Private browsing and clearing site data may discard saves. The fixed port keeps
the local origin stable; close another server using port 8000 before launching.
After the page and ROM have loaded, astronomy runs locally without network data.

The [user manual](src/rom/sidera-manual-en.txt) describes keys and options.
Help and the manual are also available as `ROM:help` and `ROM:manual` resources.

## Directory structure

| Directory | Contents |
| --- | --- |
| `src/` | Application, RP6502 support in `platform/`, overlays and linker configuration in `overlays/`, data and resources |
| `scripts/` | Generators and overlay packing |
| `cmake/` | Overlay and test integration with the standard SDK |
| `tests/` | Host, sim65 and complete-ROM tests, plus reference renderers |
| `tools/` | Tools supplied and updated by the SDK |
| `docs/` | Architecture, tests, provenance and validation reports |
| `build/` | All build, generation and test outputs |

## Reproducing assets

```text
python scripts/regenerate.py --output build/generated --check
cmake --build --preset cc65/Debug --target sidera_assets
```

The first command reproduces assets offline and compares them with the supplied
files. The second only generates them. A normal build uses the versions in
`src/rom/` and `src/tables/`. Generators write to `build/`; updating the data files
in the source tree requires a separate, deliberate copy of reviewed outputs
into `src/`. Optional importers and reference-data download scripts are retained,
but do not run during builds or tests.

## SDK, CI and WEB

`cmake -P tools/rp6502.cmake` updates the SDK tools. Project-specific extensions
live outside `tools/`. The updater may also change `.vscode/`, `.gitignore` and
the top-level CMake file; review the diff and run tests after updating.
Keep the WEB launch directory and stable port when reviewing SDK updates.
`rp6502_web()` packages SIDERA with player 0.43. The `ci.yml` workflow builds and
tests Debug and Release on Windows and retains the ROM and WEB ZIP artifacts.
The historical publishing template remains `.github/workflows/web.yml.disabled`;
it is not part of the supported Windows build workflow. No workflow publishes
a site or releases. To host WEB, serve the contents of the generated ZIP over
HTTP(S), together with the matching source distribution required by GPLv3.

## Documentation and licenses

- [GNU General Public License v3](LICENSE) — `GPL-3.0-only` for original project code and documentation
- [Technical reference — developer introduction](docs/technical-reference.md)
- [Architecture and memory](docs/architecture.md)
- [Tests, generators and six reference views](docs/tests.md)
- [Source provenance](docs/provenance.md)
- [Local validation report](docs/validation.md)
- [RMT cleanup and PICO checks](docs/rmt-cleanup.md)
- [License notices](LICENSE-NOTICE.txt)

Copyright (c) 2026 Wojciech Gwioździk (WojciechGw).
SIDERA's original code and documentation are licensed under **GPL-3.0-only**,
without warranty. Commercial use and redistribution are allowed under its terms.
Third-party data, artwork and tools retain their own licenses and attribution;
see [LICENSE-NOTICE.txt](LICENSE-NOTICE.txt) for the scope and exceptions.

The ROM embeds the GPL text as `ROM:sidera-gpl.txt`, the project notice as
`ROM:sidera-notice.txt` and the SDK license as `ROM:sidera-sdk-license.txt`,
alongside the existing data notices. When distributing a ROM for download,
also provide its complete corresponding source, including build scripts,
or a clear link to the matching source version, as required by GPLv3 section 6.
