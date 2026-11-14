# RMT cleanup

Scope agreed on 13 November 2026: SIDERA runs standalone on RP6502-PICO and
RP6502-EMU. RMT/razemOS support and the former names were removed from active
code. Provenance information, the import manifest and licenses were preserved.

## Changes

- Removed task, signal, application-switching and shared-console APIs,
  the `RPMT_BARE` variant, no-op locks and `yield`.
  Also removed unused star-catalogue resets after task switching; refreshing
  after a date or observer change still uses `cs_begin()`.
- Retained the keyboard decoder from `rpc_common` in `src/platform/`.
  Removed unused text-screen, inter-application state transfer, directory
  and shared-XRAM modules.
- File operations use cc65 directly. A small platform layer handles the 125 Hz
  clock, waiting, UART and VGA detection.
- `src/platform/argv.c` provides a 512-byte monitor argument buffer.
  It is compiled directly into the executable: putting it only in an archive
  allows the linker to select cc65's default implementation without a buffer.
- `src/startup.c` shares application startup with tests. Tests no longer cut
  the old initialization variant out of the text of `main.c`.
- Replaced `src/bare/` with `src/overlays/`. The five overlays, loader, CRC checks,
  build identifier, XRAM layout and data formats remain in place.
- Preserved all startup options, including `/assets` and `/config`, settings
  persistence, keys, LIVE/SIM, astronomy, cities, time zones and the X panel.

## Baseline comparison

| Metric | Before | After |
| --- | ---: | ---: |
| `.c`, `.h`, `.inc` files in `src/` | 55 | 33 |
| Lines in those files, including comments and tables | 7357 | 4823 |
| `sidera.rp6502` size | 1 226 243 B | 1 225 825 B |
| End of resident data, exclusive | `$B4B1` | `$B3F1` |
| Free RAM below the stack after reserving the overlay window | 10 831 B | 11 023 B |

The change removed 2534 lines (34.4%), reduced the ROM by 418 B and freed 192 B
of RAM. Much of the removed source code had not previously been included in the
ROM, so the source reduction is much larger than the image reduction.
A 160-byte variable-path buffer was also removed from the CITIES overlay.

All four ROMs (Windows/WSL, Debug/Release) are byte-for-byte identical.
SHA-256 after refactoring:

```text
770902ca485a5e645273283514be52e1ee37e4bf18a9588e069c95baed3209df
```

## Automated validation

Final results from 13 November 2026 (historical; Linux/WSL build support was
removed on 14 November 2026):

| Environment | Debug | Release |
| --- | --- | --- |
| Windows, cc65 + MSVC | PASS | PASS |
| Ubuntu 24.04 / WSL 2, cc65 + GCC | PASS | PASS |

Each run includes 24 host/sim65/image tests, plus ROM validation, a test in the
actual emulator and asset reproduction: 27 tests and two SDK fixtures.
WSL ran the Windows emulator through the adapter available at that time; this was
neither a native Linux emulator test nor a PICO hardware test.

Six reference images match their baselines pixel for pixel. Screenshots of the
sky view and X panel from the actual ROM in the emulator were also inspected.
Byte comparisons confirmed that 36 files containing data, assets, licenses,
the manifest and core astronomy modules were unchanged. Generators reproduce
the supplied files without changing the source tree.

The added `sidera_platform_6502` test covers the actual timing and keyboard-decoder
code. The emulator confirms `/config` and `/date` argument handling, overlay
switching, S saving and Q exit. Retained tests also check corrupt overlays,
cleanup after errors and suspension of simulation in the X panel. Only scenarios
related to RMT tasks and signals were removed.

A search of active code, tests, generators and CMake finds no references to
`rpmt`, `rpc_common` or `RPMT_BARE`. `git diff --check` passes.
The baseline and working-tree backup are stored locally in
`build/audit/pre-rmt-cleanup-20261113-074717/`, with logs in
`build/audit/cleanup-*.log` and `Testing/Temporary/` directories.
No commits, publication or PICO transfers were performed.

## Physical PICO checks

On 13 November 2026, Wojciech Gwioździk (WojciechGw) confirmed that SIDERA
had been launched successfully and was working on a physical Picocomputer 6502.
This is an owner-reported hardware result. The tested ROM's hash and individual
checklist results were not recorded, so this confirmation is not tied to a
specific binary or a completed run of every check below.

The following checklist is retained for detailed hardware regression testing.
Automated validation does not upload the ROM to PICO. Release file to transfer:
`build/cc65/release/sidera/sidera.rp6502`.

1. Run the ROM with a separate configuration to make the saved result easy to identify:

   ```text
   LOAD MSC0:/sidera.rp6502 /config MSC0:/sidera-check.cfg /date "2026-11-12 20:00:00"
   ```

2. Check the roughly five-second splash screen, sky view and specified date in
   SIM/PAUSE mode. Wait for rise/set calculations to finish.
3. Check arrows, Shift+arrows, G/T/C/P and L/D/Z editing: Enter applies changes,
   Esc cancels; Home/End/Backspace/Delete work in the editor.
4. In K, find a city, confirm it and check the location and time zone. In X,
   check the planet panel, close it with X and return to the current sky view.
5. Check Space, `+`, `-`, `[` and `]`, then N and LIVE clock updates.
6. Press S, quit with Q and run the ROM again with only
   `/config MSC0:/sidera-check.cfg`. The settings should be restored.
7. Run `/help` and an invalid option, such as `/fov 55`: help or an error should
   return to the monitor. If you use external assets, also check
   `/assets MSC0:/sidera.dat` with an unchanged data file.

Hardware startup and operation are confirmed by the owner; completion of the
individual checklist items has not been reported.
