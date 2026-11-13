"""Run the actual ROM, switch overlays, save settings, then exit cleanly."""
from pathlib import Path
import os
import shlex
import sys
import rp6502

emulator = Path(os.environ['RP6502_EMU'])
if sys.platform == 'linux' and emulator.read_bytes()[:2] == b'MZ':
    wrapper = Path('emu-wsl.sh').resolve()
    bridge = Path(__file__).resolve().parents[1]/'scripts/emu_wsl.py'
    wrapper.write_text('#!/bin/sh\nexec '+shlex.join([sys.executable,str(bridge),str(emulator)])+' "$@"\n')
    wrapper.chmod(0o755)
    os.environ['RP6502_EMU'] = str(wrapper)

with rp6502.Emu() as emu:
    emu.cmd('run 1200')  # Includes the unskippable five-second cover.
    assert 'EXIT' not in emu.status(), 'ROM exited before the sky view'
    emu.cmd('peek _ovl_failed 0')
    emu.cmd('wait _state 2 7200')  # Local rise/set calculation has completed.
    emu.cmd('run 60')
    emu.cmd('shot "sky.png"')
    emu.cmd('key x')
    emu.cmd('run 180')
    emu.cmd('peek _solar_visible 1')
    emu.cmd('peek _ovl_failed 0')
    emu.cmd('shot "solar.png"')
    emu.cmd('key x')
    emu.cmd('run 180')
    emu.cmd('peek _solar_visible 0')
    emu.cmd('key l')
    emu.cmd('run 120')
    emu.cmd('peek _editing 1')
    emu.cmd('key escape')
    emu.cmd('run 120')
    emu.cmd('peek _editing 0')
    emu.cmd('key k')
    emu.cmd('run 180')
    emu.cmd('peek _cities_active 1')
    emu.cmd('peek _ovl_failed 0')
    emu.cmd('key escape')
    emu.cmd('run 120')
    emu.cmd('peek _cities_active 0')
    emu.cmd('key s')
    emu.cmd('run 120')
    emu.cmd('key q')
    emu.cmd('wait exit 120')
    emu.cmd('expect exit 0')
assert (Path('save')/'sidera.cfg').is_file(), 'S must save the configuration'
assert 'Time 2026-11-12 20:00:00' in (Path('save')/'sidera.cfg').read_text(), 'Monitor argv must preserve /date'
print('PASS: ROM startup, overlay switching, SAVE configuration and exit')
