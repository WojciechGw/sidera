"""Sample a real ROM at 60 Hz; no instrumentation or changes to emulated RAM.

Run with --rom, --emulator and --output. Times are emulated seconds at 8 MHz,
not host execution time or measurements of a physical PICO.
"""
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import statistics
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import rp6502

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rom', type=Path, required=True)
parser.add_argument('--emulator', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--frames', type=int, default=3600)
parser.add_argument('--constellations', type=int, default=3)
parser.add_argument('--manual-step', action='store_true', help='Measure one identical +15-minute change instead of PLAY')
args = parser.parse_args()
sys.argv = sys.argv[:1]  # All emulator arguments are supplied explicitly below.
os.environ['RP6502_EMU'] = str(args.emulator.resolve())

with rp6502.Emu('--phi2', '8000', '--memseed', '123',
                '--save-dir', str((args.output.parent/'benchmark-save').resolve()),
                str(args.rom.resolve()), '--',
                '/config', 'SAVE:/benchmark-unused.cfg', '/date', '2026-11-12 20:00:00',
                '/grid', '3', '/paths', 'on', '/constellations', str(args.constellations),
                '/planets', 'on', '/view', '180,24') as emu:
    def read(symbol, count=1):
        return bytes.fromhex(emu.cmd(f'dump {symbol} {count}'))

    emu.cmd('run 1200')
    emu.cmd('wait _calculating 0 14400')
    emu.cmd('run 60')
    emu.cmd('peek _live 0')
    emu.cmd('key equal+shift' if args.manual_step else 'key space')
    previous = read('_moment', 7)
    initial = previous
    previous_drawing = 0
    stages = Counter()
    drawing_stages = Counter()
    events = []
    for frame in range(args.frames):
        emu.cmd('run 1')
        moment = read('_moment', 7)
        drawing = read('_drawing')[0]
        stars = int.from_bytes(read('_next_star', 2), 'little') < 759
        riseset = read('_state')[0] == 1
        trails = read('_trail_done')[0] < 97
        if moment != previous:
            events.append({'frame': frame, 'event': 'time', 'utc': list(moment)})
            previous = moment
        visible_drawing = drawing not in (0, 8)
        if visible_drawing != (previous_drawing not in (0, 8)):
            events.append({'frame': frame, 'event': 'draw_start' if visible_drawing else 'draw_end'})
        previous_drawing = drawing
        if drawing:
            drawing_stages[drawing] += 1
        stages['drawing' if drawing else 'stars' if stars else 'rise_set' if riseset
               else 'trails' if trails else 'idle_or_synchronous_work'] += 1
        if args.manual_step and moment != initial and not read('_calculating')[0]:
            events.append({'frame': frame, 'event': 'ready'})
            break
    emu.cmd('key q+ctrl')
    emu.cmd('wait exit 120')
    emu.cmd('expect exit 0')

times = [e['frame'] for e in events if e['event'] == 'time']
periods = [(b-a)/60 for a,b in zip(times,times[1:])]
result = {'rom_sha256': hashlib.sha256(args.rom.read_bytes()).hexdigest(),
          'phi2_khz': 8000, 'sample_hz': 60, 'frames': frame+1, 'manual_step': args.manual_step,
          'options': {'G': 3, 'T': 1, 'C': args.constellations, 'P': 1},
          'updates': len(times), 'median_update_seconds': statistics.median(periods) if periods else None,
          'stage_seconds': {k:v/60 for k,v in stages.items()},
          'drawing_stage_seconds': {k:v/60 for k,v in drawing_stages.items()}, 'events': events}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k != 'events'}, indent=2))
