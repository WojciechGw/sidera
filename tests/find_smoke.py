"""Run FIND in the shipped ROM at 8 MHz, across overlay swaps and simulation."""
import json
from pathlib import Path
import rp6502

root = Path(__file__).resolve().parents[1]
catalogue = json.loads((root/'src/data/constellations.json').read_text(encoding='utf8'))
stars = catalogue['stars']
sirius = 8+next(i for i,s in enumerate(stars) if s[0]==32349)

with rp6502.Emu() as emu:
    def read(address,count):
        return bytes.fromhex(emu.cmd(f'dump {address} {count}'))
    def word(symbol,signed=False):
        return int.from_bytes(read(symbol,2),'little',signed=signed)
    def locate(query,ident,figure=False):
        emu.cmd('key f')
        emu.cmd('wait _find_active 1 7200')
        emu.cmd(f'type "{query}"')
        emu.cmd('key enter')
        emu.cmd('wait _find_active 0 14400')
        assert word('_target_id')==ident
        emu.cmd('run 5')
        emu.cmd('wait _calculating 0 14400')
        emu.cmd('run 2')  # Finish status text and latch the complete VGA frame.
        assert 50<=word('_fov')<=160 if figure else word('_fov')==50

    emu.cmd('run 1200')
    emu.cmd('wait _calculating 0 14400')
    emu.cmd('run 5')
    emu.cmd('key f')
    emu.cmd('wait _find_active 1 7200')
    emu.cmd('run 1200')
    emu.cmd('shot "find.png"')
    panel = read('xram:5680',32720)
    assert all(panel[y*80]&128 and panel[y*80+79]&1 for y in range(409))
    # Send the next arrow as soon as its highlight is visible, before details finish.
    latency=[]
    for index in range(1,7):
        emu.cmd('key down')
        for frames in range(1,61):
            emu.cmd('run 1')
            if read(f'xram:{(130+index*14)*80+2}',8)==b'\xff'*8:
                latency.append(frames);break
        else: raise AssertionError('FIND arrow was lost or blocked for one second')
    print(f'FIND successive Down highlights: {latency} frames at 60 Hz',flush=True)
    assert max(latency)<=6, 'FIND row navigation exceeded 100 ms at 8 MHz'
    emu.cmd('key home')
    emu.cmd('run 1200')
    frozen = read('_moment',7)
    # Escape must interrupt a fresh search, without waiting for all 855 records.
    emu.cmd('type "zz"')
    emu.cmd('run 2')
    emu.cmd('key escape')
    frames=0
    for frames in range(1,16):
        emu.cmd('run 1')
        if read('_find_active',1)==b'\0':break
    else: raise AssertionError('FIND Escape did not respond within 250 ms')
    print(f'FIND Escape: {frames} frames',flush=True)
    assert read('_moment',7)==frozen
    emu.cmd('run 5')
    emu.cmd('wait _calculating 0 14400')
    locate('HIP 32349',sirius)
    emu.cmd('peek _constellation_mode 1')
    emu.cmd('shot "find-sirius.png"')
    # C4 keeps the selected star; reopening/cancelling keeps the target.
    emu.cmd('type "ccc"')
    emu.cmd('run 60')
    emu.cmd('wait _calculating 0 14400')
    emu.cmd('peek _constellation_mode 4')
    locate('Sirius',sirius)
    emu.cmd('peek _constellation_mode 4')
    bearing=word('_bearing')
    emu.cmd('key space')
    emu.cmd('run 5')
    emu.cmd('key f')
    emu.cmd('wait _find_active 1 7200')
    frozen=read('_moment',7)
    emu.cmd('run 600')
    assert read('_moment',7)==frozen
    emu.cmd('key escape')
    emu.cmd('wait _find_active 0 120')
    emu.cmd('run 1800')
    assert read('_moment',7)!=frozen and word('_bearing')==bearing and word('_target_id')==sirius
    emu.cmd('key space')  # SIM/PLAY -> LIVE
    emu.cmd('run 60')
    emu.cmd('key space')  # LIVE -> SIM/PAUSE
    emu.cmd('run 60')
    emu.cmd('wait _calculating 0 14400')
    locate('Mercury',2)
    emu.cmd('peek _planets 1')
    # Another overlay must not destroy the resident marker or catalogue handles.
    emu.cmd('key k')
    emu.cmd('wait _cities_active 1 7200')
    emu.cmd('run 120')
    emu.cmd('key escape')
    emu.cmd('wait _cities_active 0 120')
    emu.cmd('run 60')
    emu.cmd('wait _calculating 0 14400')
    assert word('_target_id')==2
    locate('Moon',1)
    emu.cmd('key n')
    emu.cmd('run 120')
    emu.cmd('wait _calculating 0 14400')
    locate('Sun',0)
    emu.cmd('peek _live 1')
    assert read('_target_position',4)==read('_sky',4)
    az=word('_sky')
    alt=word('(_sky+2)',signed=True)
    assert word('_bearing')==((az*3600//65536+5)//10)%360
    assert word('_elevation',signed=True)==int(alt*90/16384)
    emu.cmd('key space')  # LIVE -> SIM/PAUSE
    emu.cmd('run 60')
    emu.cmd('wait _calculating 0 14400')
    emu.cmd('key c')  # C4 -> C0
    emu.cmd('run 60')
    emu.cmd('peek _constellation_mode 0')
    ori=next(i for i,g in enumerate(catalogue['constellations']) if g['abbr']=='Ori')
    uma=next(i for i,g in enumerate(catalogue['constellations']) if g['abbr']=='UMa')
    locate('oRi',8+len(stars)+ori,True)
    emu.cmd(f'peek _target_figure {ori}')
    emu.cmd('peek _constellation_mode 0')
    emu.cmd('shot "find-orion.png"')
    # A selected figure updates at exact simulation times without camera tracking.
    view=read('_bearing',2)+read('_elevation',2)+read('_fov',2)
    frozen=read('_moment',7)
    emu.cmd('key space')
    emu.cmd('run 1800')
    assert read('_moment',7)!=frozen
    assert view==read('_bearing',2)+read('_elevation',2)+read('_fov',2)
    emu.cmd('key space')
    emu.cmd('run 60')
    emu.cmd('key space')
    emu.cmd('run 60')
    emu.cmd('wait _calculating 0 14400')
    emu.cmd('type "cccc"')
    emu.cmd('run 60')
    emu.cmd('wait _calculating 0 14400')
    locate('Ursa Major',8+len(stars)+uma,True)
    emu.cmd('peek _constellation_mode 4')
    emu.cmd('shot "find-ursa-major.png"')
    locate('Ori',8+len(stars)+ori,True)
    emu.cmd('peek _constellation_mode 4')
    emu.cmd('key f')
    emu.cmd('wait _find_active 1 7200')
    emu.cmd('run 120')  # Allow the modal frame to open before measuring search cancellation.
    emu.cmd('type "UMa"')
    emu.cmd('run 30')
    emu.cmd('key escape')
    emu.cmd('wait _find_active 0 15')
    assert word('_target_id')==8+len(stars)+ori
    emu.cmd('run 60')
    emu.cmd('wait _calculating 0 14400')
    emu.cmd('key q')
    emu.cmd('wait exit 120')
    emu.cmd('expect exit 0')
print('PASS: actual ROM FIND, responsive cancel, HIP/name/code selection, C0/C4 figures, fixed camera, frozen SIM, LIVE centring and overlay lifetime')
