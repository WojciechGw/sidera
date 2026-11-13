# Extraction validation report

This report documents the state on 12 November 2026. Results after removing the
RMT layer are described in the [refactoring report](rmt-cleanup.md).

Local validation: 12 November 2026, Windows and Ubuntu 24.04 in WSL 2.
Compiler on both systems: cc65 V2.19, Git `6fd2669`. In WSL, it was installed using
the official installer linked from the SDK documentation. Emulator: RP6502 0.43.
Host tests: MSVC 2022 on Windows, GCC on Ubuntu. Python: 3.14.8 / 3.12.3.

| Environment | Configuration | Clean build | Full test suite |
| --- | --- | --- | --- |
| Windows | cc65/Debug | PASS | PASS |
| Windows | cc65/Release | PASS | PASS |
| Ubuntu 24.04 / WSL 2 | cc65/Debug | PASS | PASS |
| Ubuntu 24.04 / WSL 2 | cc65/Release | PASS | PASS |

Each run included 23 host/sim65/image tests, plus ROM validation, the actual
emulator and asset reproduction: **26 tests**, with two additional SDK fixtures.
After extending emulator checks, all four configurations were tested again:
completion of rise/set calculations, the X panel, L editing, opening/closing
K cities, S saving and Q exit.

WSL builds and runs host and sim65 tests as Linux programs. Following the SDK's
selection, it uses the Windows emulator through interop; the project's adapter
translates filesystem paths. The native Linux emulator is intended for the
Ubuntu CI job. The GitHub Actions workflow was prepared but had not been run on
GitHub; nothing was pushed or published.

## Results

All four ROMs are byte-for-byte identical, at **1 226 243 B**, with SHA-256:

```text
8ac123604cd3e2e630fd80288596b732e35d176e10a8bf6f830282ab1cc8baf1
```

Windows Release: `build/cc65/release/sidera/sidera.rp6502`.
WSL outputs are in `build/linux/cc65/<debug|release>/sidera/`.
Standard SDK packing preserves the core at `$0200`, reset at `$0200`, five named
overlay resources, their build identifier and CRC32.

| Overlay | Payload | BSS | Headroom within 5888 B |
| --- | ---: | ---: | ---: |
| INIT | 4591 B | 0 B | 1297 B |
| CITIES | 3653 B | 463 B | 1772 B |
| EDIT | 3106 B | 112 B | 2670 B |
| COVER | 846 B | 0 B | 5042 B |
| SOLAR | 5678 B | 82 B | 128 B |

Each overlay has an additional 24-byte header. Code and BSS window:
`$C800–$DEFF`; scratch buffer: `$DF00–$DFFF`; C stack: `$F700–$FEFF`.
End of resident data: `$B4B1` (exclusive).
Free RAM below the stack, after reserving the window: **10 831 B**.

## Images and sources

A/B/C and solar A/B/C were generated and visually inspected. All six Windows
and WSL images have identical pixels to the retained references in `src/assets/`.
Screenshots of the running ROM in the emulator were also inspected. Test images
and configurations are in `build/cc65/<debug|release>/host-native/views/`.

Generators reproduce data and tables offline; their outputs match the files
supplied in `src/`. Checks also verify that generation leaves the source tree
unchanged. SHA-256 hashes were checked for all **126 input files** in the manifest;
the source mtRAM files were not modified.

Full local logs are in `build/audit/`, with detailed test logs in
`Testing/Temporary/` under the respective build directories. `git diff --check`
reports no errors. No commits, publication or PICO transfers were performed.
PICO support is configured; this does not constitute a physical-device test.

## Subsequent hardware confirmation

On 13 November 2026, the owner confirmed successful startup and operation on a
physical Picocomputer 6502. This later result is recorded in the
[hardware validation record](rmt-cleanup.md#physical-pico-checks); it does not
change the scope or binary hashes of the 12 November validation above.
