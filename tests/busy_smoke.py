"""Observe the busy Moon during real ROM work, including modal panels."""
import hashlib
import math
import struct
from pathlib import Path
import rp6502

root=Path(__file__).resolve().parents[1]
assets=(root/'src/rom/sidera.dat').read_bytes()
phases={hashlib.sha256(assets[2568+i*2048:2568+(i+1)*2048]).digest():i for i in range(129)}

with rp6502.Emu() as emu:
    def read(address,n=1):return bytes.fromhex(emu.cmd(f'dump {address} {n}'))
    def sample():
        cfg=struct.unpack('<8hHBB',read('xram:0xe6e0',20))
        if cfg[6]!=3:return None
        assert cfg[7:]==(33,cfg[8],5,0) and cfg[8] in (0xa000,0xa800),cfg
        # The inverse map retains scale/centre and adds a visual 15-degree CCW tilt.
        a,b,tx,c,d,ty=cfg[:6]
        direction=1 if b>0 else -1
        assert abs(math.degrees(math.atan2(direction*a,direction*b))-15)<0.1,cfg
        assert abs(math.hypot(a,b)/256-1)<0.002 and c==-b and d==a,cfg
        assert abs((a+b)*15.5+tx-15.5*256)<=1,cfg
        assert abs((c+d)*15.5+ty-15.5*256)<=1,cfg
        source=int.from_bytes(read('xram:0xb048',2),'little')
        assert source!=cfg[8],(source,cfg)
        pixels=read(f'xram:{cfg[8]}',2048)
        phase=phases[hashlib.sha256(pixels).digest()]
        return phase,cfg[1]
    def observe(label,frames,minimum=2):
        seen=set()
        quarter=False
        stride=1 if label=='find' else 3
        for _ in range(frames//stride):
            emu.cmd(f'run {stride}');state=sample()
            if label in ('cities','find','solar') and seen and read('_wait_state')[0]==3:
                assert state is not None,(label,'busy Moon disappeared between frames')
            if state is not None:
                if not seen:emu.cmd(f'shot "busy-{label}.png"')
                if state[0]==64 and not quarter:
                    emu.cmd(f'shot "busy-quarter-{label}.png"');quarter=True
                seen.add(state)
        assert len(seen)>=minimum,(label,seen,emu.status())
        print(f'{label}: {len(seen)} distinct phase/orientation frames',flush=True)
    def idle():
        emu.cmd('wait _wait_state 1 14400');emu.cmd('run 1')
        assert sample() is None

    emu.cmd('wait _wait_state 2 7200')
    emu.cmd('run 20');assert sample() is None  # Less than the 0.5-second delay.
    observe('sky',240,6)
    emu.cmd('wait _calculating 0 14400');idle()
    emu.cmd('key k');emu.cmd('wait _cities_active 1 7200')
    idle();emu.cmd('type "Long"');observe('cities',300)
    idle();emu.cmd('key enter');emu.cmd('wait _cities_active 0 120')
    emu.cmd('run 1');assert read('_note')[0]==0
    observe('location',240)
    emu.cmd('wait _calculating 0 14400');idle()
    emu.cmd('key f+ctrl');emu.cmd('wait _find_active 1 7200')
    observe('find',300);idle()
    emu.cmd('key escape');emu.cmd('wait _find_active 0 120')
    emu.cmd('wait _calculating 0 14400');idle()
    emu.cmd('key s');observe('solar',240)
    emu.cmd('wait _solar_visible 1 7200');idle()
    emu.cmd('key escape');emu.cmd('wait _solar_visible 0 120')
    emu.cmd('wait _calculating 0 14400');idle()
    emu.cmd('key z');emu.cmd('wait _editing 3 7200');emu.cmd('run 120')
    assert sample() is None  # Waiting for text is not computational work.
    emu.cmd('key enter');emu.cmd('wait _editing 0 7200')
    emu.cmd('run 1');assert read('_note')[0]==0
    emu.cmd('wait _calculating 0 14400');idle()
    emu.cmd('key q+ctrl');emu.cmd('wait exit 120');emu.cmd('expect exit 0')
print('PASS: real-ROM busy Moon, original RGB phases, independent header Moon, modal work and idle editors')
