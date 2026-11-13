# SIDERA

A standalone sky observatory for **RP6502-PICO and RP6502-EMU**. The project
uses cc65 and the [RP6502-SDK](https://picocomputer.github.io/sdk.html)
to produce a single `sidera.rp6502` containing all data and five overlays.
It does not require the mtRAM repository or razemOS.

On 13 November 2026, the owner confirmed that SIDERA starts and runs on a physical
Picocomputer 6502. See the [hardware validation record](docs/rmt-cleanup.md#physical-pico-checks).

## Requirements

- CMake 3.28+, Ninja, Python 3.12+, Git.
- A current cc65 version with RP6502 support, including `cl65`, `ca65`, `ld65`, `ar65`, `sim65`.
  Follow the [SDK installation instructions](https://picocomputer.github.io/sdk.html#installing-the-prerequisites).
- For host tests: on Windows, Visual Studio 2022 Build Tools with C++ and the Windows SDK;
  on Linux, GCC and Make (`build-essential` on Ubuntu).

Tests and generators use the Python standard library. Reference data, source
artwork and the pinned tzdata package are stored locally. The first SDK configure
step may download emulator 0.43; subsequent builds use the local copy.
The WEB emulator is not downloaded.

## Building and testing

From the project directory, in PowerShell or a Linux shell:

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
`.sym`, the core image and five `.ovl` files. Do not mix files from different builds.
The build automatically checks the ROM structure, CRC, build identifier,
BSS and memory boundaries. Each configuration also builds native test executables.

Both presets retain the original `-O` optimization, since overlay sizes are part
of the architecture. Debug uses `-g`, Release uses `-DNDEBUG`; the SDK also provides
debugger information for Release outputs. Native test assertions remain enabled
in both configurations.

Windows and WSL must use separate output directories. In WSL, for the same checkout:

```text
cmake --preset cc65/Debug -B build/linux/cc65/debug
cmake --build build/linux/cc65/debug
ctest --test-dir build/linux/cc65/debug --output-on-failure
```

For Release, use the `cc65/Release` preset and `build/linux/cc65/release` directory.
In WSL, the SDK selects the Windows emulator; the test uses the project's path
adapter, `scripts/emu_wsl.py`. On native Linux, the SDK selects the Linux emulator.

## Running

In VS Code, select a preset, the `sidera` target and either the `RP6502-EMU` or
`RP6502-PICO` launch configuration. F5 for PICO **uploads the program to the device**;
use it deliberately. PICO connection settings (`device`, `key`, `workdir`, `args`)
are stored in the local, ignored `.rp6502` file, as specified by the SDK.
Do not store keys in the repository.

On hardware, the default configuration file is `MSC0:/sidera.cfg`. Use `/config`
to select another path. The VS Code EMU configuration passes
`/config SAVE:/sidera.cfg` to enable persistent saves in the emulator.
When launching from the command line, pass the same arguments after `--`.
The emulator test uses its own empty save directory and does not access the device.

The [user manual](src/rom/sidera-manual-en.txt) describes keys and options.
Help and the manual are also available as `ROM:help` and `ROM:manual` resources.

## Directory structure

| Directory | Contents |
| --- | --- |
| `src/` | Application, RP6502 support in `platform/`, overlays and linker configuration in `overlays/`, data and resources |
| `scripts/` | Generators, overlay packing and the WSL test adapter |
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
In particular, it may restore the WEB launch configuration.

WEB is disabled: there is no `rp6502_web()` call, package or active configuration.
The original files are retained as `.github/workflows/web.yml.disabled` and
`.vscode/launch-web.json.disabled`. Do not restore their active filenames without
a separate decision to enable WEB. The `ci.yml` workflow builds and tests
Windows/Linux × Debug/Release and retains artifacts. It does not publish a site
or releases.

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
