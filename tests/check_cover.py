"""Validate the shipped splash against the original indexed BMP."""
from pathlib import Path
import struct
import json

root=(Path(__file__).resolve().parents[1] / 'src')
bmp=(root/'assets/sidera-cover.bmp').read_bytes()
catalogue=json.loads((root/'data/constellations.json').read_text(encoding='utf8'))
records=sum(len(g['edges'])+len(g.get('points',[])) for g in catalogue['constellations'])
cover_offset=270216+len(catalogue['stars'])*6+records*4
data=(root/'rom/sidera.dat').read_bytes()[cover_offset:cover_offset+57600+33*512]
assert len(data)==57600+33*512
offset=struct.unpack_from('<I',bmp,10)[0]
height=struct.unpack_from('<i',bmp,22)[0]
for y in range(180):
    src=179-y if height>0 else y
    assert data[y*320:(y+1)*320]==bmp[offset+src*320:offset+(src+1)*320]
for i in range(256):
    b,g,r,_=bmp[54+i*4:58+i*4]
    previous=(0,0,0)
    for level in range(33):
        color=struct.unpack_from('<H',data,57600+level*512+i*2)[0]
        assert color&32
        channels=tuple((color>>shift)&31 for shift in (0,6,11))
        assert all(a>=p for a,p in zip(channels,previous));previous=channels
        if level==0:assert channels==(0,0,0)
        if level==32:assert channels==(r>>3,g>>3,b>>3)
print('PASS: original 320x180 indexed pixels, row orientation, 33 opaque monotonic RGB555 palettes')
