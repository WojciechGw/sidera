"""Check early panels in the actual ROM with a saved, full-detail preset."""
from pathlib import Path
import rp6502

root = Path(__file__).resolve().parents[1]
Path('save').mkdir(exist_ok=True)
Path('save/startup.cfg').write_bytes((root/'src/assets/sidera-testC.cfg').read_bytes())

with rp6502.Emu() as emu:
    def read(address, count):
        return bytes.fromhex(emu.cmd(f'dump {address} {count}'))

    def text_at(data, x, y, text, inverse=False):
        for char in text:
            for row in range(8):
                bits = data[0x9600+ord(char)*8+row] ^ (0xFC if inverse else 0)
                for col in range(6):
                    assert bool(data[(y+row)*80+(x+col)//8] & (128 >> ((x+col)%8))) == bool(bits & (128 >> col)), (text,x,y)
            x += 6

    def empty_plot(data):
        assert not any(data[71*80:441*80])
        for offset in range(0,70,10):
            assert int.from_bytes(read(f'xram:{0xB050+offset}',2),'little',signed=True) < 0
        assert int.from_bytes(read('xram:45092',2),'little',signed=True) < 0  # Moon x at $B024.

    emu.cmd('run 60')  # Allow ROM initialization; still inside the splash screen.
    emu.cmd('wait _footer_valid 3 7200')
    data = read('xram:0', 39424)
    text_at(data,6,1,'SIDERA',True)
    text_at(data,256,31,'Rio de Janeiro [BR]')
    text_at(data,226,43,'Sun AZ ... ALT ...')
    text_at(data,412,43,'Moon AZ ... ALT ... LIT ...')
    for x,y,text in ((473,461,' grid '),(398,461,' paths '),(311,461,' planets '),(542,461,' constellations')):
        text_at(data,x,y,text,True)
    text_at(data,473,472,'scale')
    text_at(data,542,472,'navigational')
    empty_plot(data)
    emu.cmd('peek _header_visible 0')
    emu.cmd('shot "startup-panels.png"')
    emu.cmd('wait _header_visible 1 7200')
    emu.cmd('run 1')  # Finish the descriptor writes after the visibility flag.
    data = read('xram:0',39424)
    empty_plot(data)
    emu.cmd('peek _drawing 0')
    emu.cmd('peek xram:0xb030 195 0')
    emu.cmd('peek xram:0xb044 123 1')  # Header Moon x = 379.
    emu.cmd('shot "startup-sun-moon.png"')
    emu.cmd('wait _calculating 0 14400')
    assert any(read('xram:5680',30160))
    emu.cmd('key q+ctrl')
    emu.cmd('wait exit 120')
    emu.cmd('expect exit 0')
print('PASS: saved preset panels before astronomy, early Sun/Moon, hidden plot until prepared scene')
