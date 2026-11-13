"""Exercise actual-ROM daylight transitions without a full chart redraw."""
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
    def chart():
        return bytes.fromhex(emu.cmd('dump xram:5680 30160'))  # Rows 71..447.

    emu.cmd('run 1200')
    emu.cmd('wait _calculating 0 14400')
    emu.cmd('peek _compass_inverted 0')
    night = chart()
    emu.cmd('key space')
    emu.cmd('wait _compass_inverted 1 7200')  # SIM advances across local 06:47.
    day = chart()
    assert all(b == (a ^ (255 if (426-71)*80 <= i < (441-71)*80 else 0))
               for i,(a,b) in enumerate(zip(night,day)))
    emu.cmd('key minus')  # Pause and go back before sunrise.
    emu.cmd('wait _playing 0 120')
    # The mode flag changes before the remainder of the input iteration paints.
    emu.cmd('wait _compass_inverted 0 7200')
    assert chart() == night
    emu.cmd('key equal+shift')
    emu.cmd('wait _compass_inverted 1 7200')
    assert chart() == day
    emu.cmd('key q')
    emu.cmd('wait exit 120')
    emu.cmd('expect exit 0')
print('PASS: SIM sunrise, backward/forward manual time, exact band inversion; plot and V unchanged')
