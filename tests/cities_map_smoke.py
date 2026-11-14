"""Exercise the CITIES locator in the actual ROM at 8 MHz."""
import json,struct,zlib
from pathlib import Path
import rp6502
root=Path(__file__).resolve().parents[1]
catalogue=json.loads((root/'src/data/cities.json').read_text())['cities']
raw=(root/'src/rom/sidera.map').read_bytes()
city_data=(root/'src/rom/sidera.cty').read_bytes()

def expected(city=None):
    region=0 if city is None else city_data[16+catalogue.index(city)*96+93]>>4
    return raw[16+region*5850:16+(region+1)*5850]

with rp6502.Emu() as emu:
    def read(a,n):return bytes.fromhex(emu.cmd(f'dump {a} {n}'))
    def map_bytes():
        screen=read('xram:6160',150*80);out=bytearray()
        for y in range(150):
            row=bytearray(screen[y*80+41:y*80+80]);row[0]&=3;row[38]&=192;out.extend(row)
        return out
    def marker_matches(city):
        x,y,src,pal,size,options=struct.unpack('<hhHHBB',read('xram:0xb0f0',10))
        assert (src,pal,size,options)==(0xe700,0xe708,0x11,0)
        assert read('xram:0xe700',12)==bytes((32,32,248,32,32,0,0,0,0,0,63,0))
        if city is None:return x==-64
        return (x+2,y+2)==(334+int((city['longitude']+180)*299/360+.5),
                       77+int((90-city['latitude'])*149/180+.5))
    def wait_map(city,budget=1200):
        wanted=expected(city)
        for frames in range(1,budget+1):
            emu.cmd('run 1')
            if map_bytes()==wanted and marker_matches(city):return frames
        raise AssertionError('City map did not reach the selected location')
    def choose(query,name):
        emu.cmd('key k');emu.cmd('wait _cities_active 1 7200')
        emu.cmd(f'type "{query}"')
        city=next(c for c in catalogue if c['city']==name)
        wait_map(city);emu.cmd('run 2');return city
    def capture(name,city):
        emu.cmd(f'shot "{name}"')
        data=Path(name).read_bytes();offset=8;payload=bytearray()
        assert data[:8]==b'\x89PNG\r\n\x1a\n'
        while offset<len(data):
            size=struct.unpack_from('>I',data,offset)[0]
            kind=data[offset+4:offset+8];chunk=data[offset+8:offset+8+size]
            if kind==b'IHDR':assert struct.unpack('>IIBBBBB',chunk)==(640,480,8,6,0,0,0)
            if kind==b'IDAT':payload.extend(chunk)
            offset+=size+12
        # Emulator captures use unfiltered RGBA rows. Check the rendered sprite,
        # including VGA-plane composition, rather than only its XRAM descriptor.
        pixels=zlib.decompress(payload)
        assert len(pixels)==2561*480 and set(pixels[::2561])=={0}
        red={(x,y) for y in range(75,229) for x in range(332,636)
             if pixels[y*2561+1+x*4:y*2561+4+x*4]==b'\xff\0\0'}
        cx=334+int((city['longitude']+180)*299/360+.5)
        cy=77+int((90-city['latitude'])*149/180+.5)
        assert red=={(cx+d,cy) for d in range(-2,3)}|{(cx,cy+d) for d in range(-2,3)},red
    emu.cmd('run 1200');emu.cmd('wait _calculating 0 14400')
    city=choose('PL','Warsaw')
    capture('cities-map-warsaw.png',city)
    frozen=read('_moment',7)
    emu.cmd('key down')
    frames=wait_map(next(c for c in catalogue if c['city']=='Bialystok'),60)
    print(f'CITIES same-continent move: {frames} frames at 60 Hz',flush=True)
    assert frames<=30 and read('_moment',7)==frozen
    emu.cmd('key escape');emu.cmd('wait _cities_active 0 120')
    assert marker_matches(None)
    city=choose('Honolulu','Honolulu');capture('cities-map-honolulu.png',city)
    emu.cmd('type "zz"');wait_map(None)
    emu.cmd('key escape');emu.cmd('wait _cities_active 0 120')
    city=choose('Jayapura','Jayapura');capture('cities-map-jayapura.png',city)
    emu.cmd('key enter');emu.cmd('wait _cities_active 0 120')
    assert marker_matches(None)
    city=next(c for c in catalogue if c['city']=='Jayapura')
    assert int.from_bytes(read('_observer',4),'little',signed=True)==round(city['latitude']*36000)
    emu.cmd('run 60');emu.cmd('wait _calculating 0 14400')
    emu.cmd('key q');emu.cmd('wait exit 120');emu.cmd('expect exit 0')
print('PASS: actual-ROM 300x150 map, geographic region, red 5x5 cross, frozen time, empty results and selected-city apply')
