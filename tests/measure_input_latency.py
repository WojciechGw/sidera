"""Measure key handling and visible pan latency in the actual ROM at 8 MHz."""
import json
import os
from pathlib import Path
import rp6502


with rp6502.Emu() as emu:
    def read(address, count=1):
        return bytes.fromhex(emu.cmd(f'dump {address} {count}'))

    def pan(label, key='right', variable='_bearing', change=5, sample_bytes=1280):
        before = read('xram:5680', sample_bytes)  # Upper scale and beginning of plot.
        value = int.from_bytes(read(variable, 2), 'little')
        emu.cmd('key '+key)
        handled = changed = None
        for frame in range(1, 721):
            emu.cmd('run 1')
            current = int.from_bytes(read(variable, 2), 'little')
            if handled is None and current != value:
                assert current == value+change, (variable, current, value+change)
                handled = frame
            if changed is None and read('xram:5680', sample_bytes) != before:
                changed = frame
            if handled is not None and changed is not None:
                break
        assert handled is not None and changed is not None
        result = {'scenario': label, 'key_frames': handled, 'visible_frames': changed,
                  'key_seconds': handled/60, 'visible_seconds': changed/60}
        print(json.dumps(result), flush=True)
        return result

    emu.cmd('run 1200')
    emu.cmd('wait _calculating 0 14400')
    emu.cmd('run 5')
    results = [pan('paused_ready')]
    # Auto-repeat must not continually restart a full projection prepass.
    start = int.from_bytes(read('_bearing', 2), 'little')
    for repeat in range(6):
        emu.cmd('key right')
        emu.cmd('run 6')
    emu.cmd('run 15')
    assert int.from_bytes(read('_bearing', 2), 'little') == start+30
    emu.cmd('wait _calculating 0 14400')
    emu.cmd('run 5')
    emu.cmd('press lctrl')
    results.append(pan('left_ctrl_fov', 'left+ctrl', '_fov', -10))
    emu.cmd('release lctrl')
    emu.cmd('wait _calculating 0 14400')
    emu.cmd('run 5')
    # Tilt preserves the top compass. Include side labels and interior grid
    # rows when looking for its first visible change.
    results.append(pan('elevation', 'up', '_elevation', 5, 8320))
    emu.cmd('wait _calculating 0 14400')
    emu.cmd('run 5')
    emu.cmd('key equal+shift')
    # Wait until the new catalogue is actually being calculated.
    for frame in range(180):
        emu.cmd('run 1')
        if int.from_bytes(read('_next_star', 2), 'little') < 759:
            break
    else:
        raise AssertionError('star calculation did not start')
    results.append(pan('during_stars'))
    # Finish the immediate pan; its pending catalogue must still be incomplete.
    # Test FOV from a visible plot, so an already-cleared compass cannot hide
    # the start of the next redraw from the pixel probe.
    emu.cmd('wait _drawing 0 14400')
    assert int.from_bytes(read('_next_star', 2), 'little') < 759
    emu.cmd('press lctrl')
    results.append(pan('left_ctrl_during_stars', 'left+ctrl', '_fov', -10))
    emu.cmd('release lctrl')
    emu.cmd('key q+ctrl')
    emu.cmd('wait exit 120')
    emu.cmd('expect exit 0')
Path(os.environ.get('SIDERA_LATENCY_OUTPUT','input-latency.json')).write_text(
    json.dumps(results, indent=2)+'\n', encoding='utf-8')
if os.environ.get('SIDERA_CHECK_LATENCY'):
    assert all(r['key_frames'] <= 15 and r['visible_frames'] <= 15 for r in results), results
    print('PASS: pan/tilt/LCtrl FOV begin changing the plot within 250 ms; repeat keys are retained')
