"""Reproducible 6x8 font, 32x32 Sun/Moon and shaded 8x8 planet sprites.
No target-side RAM copies: sidera.dat is read straight into XRAM.
Run with --output FILE; --tables also regenerates the Q14 sine table.
"""
import argparse
import json
import math
import struct
from collections import Counter
from pathlib import Path

# Five drawn columns, one spacing column; seven rows, one spacing row.
GLYPHS = {
'0':['01110','10001','10011','10101','11001','10001','01110'],
'1':['00100','01100','00100','00100','00100','00100','01110'],
'2':['01110','10001','00001','00010','00100','01000','11111'],
'3':['11110','00001','00001','01110','00001','00001','11110'],
'4':['00010','00110','01010','10010','11111','00010','00010'],
'5':['11111','10000','10000','11110','00001','00001','11110'],
'6':['01110','10000','10000','11110','10001','10001','01110'],
'7':['11111','00001','00010','00100','01000','01000','01000'],
'8':['01110','10001','10001','01110','10001','10001','01110'],
'9':['01110','10001','10001','01111','00001','00001','01110'],
'A':['01110','10001','10001','11111','10001','10001','10001'],
'B':['11110','10001','10001','11110','10001','10001','11110'],
'C':['01111','10000','10000','10000','10000','10000','01111'],
'D':['11110','10001','10001','10001','10001','10001','11110'],
'E':['11111','10000','10000','11110','10000','10000','11111'],
'F':['11111','10000','10000','11110','10000','10000','10000'],
'G':['01111','10000','10000','10111','10001','10001','01111'],
'H':['10001','10001','10001','11111','10001','10001','10001'],
'I':['01110','00100','00100','00100','00100','00100','01110'],
'J':['00001','00001','00001','00001','10001','10001','01110'],
'K':['10001','10010','10100','11000','10100','10010','10001'],
'L':['10000','10000','10000','10000','10000','10000','11111'],
'M':['10001','11011','10101','10101','10001','10001','10001'],
'N':['10001','11001','10101','10011','10001','10001','10001'],
'O':['01110','10001','10001','10001','10001','10001','01110'],
'P':['11110','10001','10001','11110','10000','10000','10000'],
'Q':['01110','10001','10001','10001','10101','10010','01101'],
'R':['11110','10001','10001','11110','10100','10010','10001'],
'S':['01111','10000','10000','01110','00001','00001','11110'],
'T':['11111','00100','00100','00100','00100','00100','00100'],
'U':['10001','10001','10001','10001','10001','10001','01110'],
'V':['10001','10001','10001','10001','10001','01010','00100'],
'W':['10001','10001','10001','10101','10101','11011','10001'],
'X':['10001','10001','01010','00100','01010','10001','10001'],
'Y':['10001','10001','01010','00100','00100','00100','00100'],
'Z':['11111','00001','00010','00100','01000','10000','11111'],
' ':[], '.':['00000']*6+['00100'], ',':['00000']*5+['00100','01000'],
':':['00000','00100','00000','00000','00100'],
'-':['00000']*3+['11111'], '+':['00000','00100','00100','11111','00100','00100'],
'/':['00001','00010','00010','00100','01000','01000','10000'],
'%':['11001','11010','00010','00100','01000','01011','10011'],
'=':['00000','00000','11111','00000','11111'],
'[':['01110','01000','01000','01000','01000','01000','01110'],
']':['01110','00010','00010','00010','00010','00010','01110'],
'(':['00010','00100','01000','01000','01000','00100','00010'],
')':['01000','00100','00010','00010','00010','00100','01000'],
'?':['01110','10001','00001','00010','00100','00000','00100'],
'!':['00100']*5+['00000','00100'],
'<':['00010','00100','01000','10000','01000','00100','00010'],
'>':['01000','00100','00010','00001','00010','00100','01000'],
'\x01':['01100','10010','10010','01100'],
'\x02':['00100','00100','01000'],
'\x03':['01010','01010','10100'],
'\x04':['01010','00000','01110','10001','10001','10001','01110'],
"'":['00100','00100','01000'], '"':['01010','01010','10100'],
}
# Lowercase for the displayed units, names, and help.
LOWER = {
'a':['00000','00000','01110','00001','01111','10001','01111'],
'b':['10000','10000','11110','10001','10001','10001','11110'],
'c':['00000','00000','01111','10000','10000','10000','01111'],
'd':['00001','00001','01111','10001','10001','10001','01111'],
'e':['00000','00000','01110','10001','11111','10000','01111'],
'f':['00110','01001','01000','11100','01000','01000','01000'],
'g':['00000','01111','10001','10001','01111','00001','01110'],
'h':['10000','10000','11110','10001','10001','10001','10001'],
'i':['00100','00000','01100','00100','00100','00100','01110'],
'j':['00010','00000','00110','00010','00010','10010','01100'],
'k':['10000','10000','10010','10100','11000','10100','10010'],
'l':['01100','00100','00100','00100','00100','00100','01110'],
'm':['00000','00000','11010','10101','10101','10101','10101'],
'n':['00000','00000','11110','10001','10001','10001','10001'],
'o':['00000','00000','01110','10001','10001','10001','01110'],
'p':['00000','11110','10001','10001','11110','10000','10000'],
'q':['00000','01111','10001','10001','01111','00001','00001'],
'r':['00000','00000','10110','11001','10000','10000','10000'],
's':['00000','00000','01111','10000','01110','00001','11110'],
't':['01000','01000','11100','01000','01000','01001','00110'],
'u':['00000','00000','10001','10001','10001','10001','01111'],
'v':['00000','00000','10001','10001','10001','01010','00100'],
'w':['00000','00000','10001','10001','10101','10101','01010'],
'x':['00000','00000','10001','01010','00100','01010','10001'],
'y':['00000','10001','10001','10001','01111','00001','01110'],
'z':['00000','00000','11111','00010','00100','01000','11111'],
}

def colour(r,g,b):
    return (max(0,min(31,round(r))) | (max(0,min(31,round(g)))<<6)
            | (max(0,min(31,round(b)))<<11) | 32)

def planet_sprite(body):
    """A common shaded sphere, with a distinct base colour for each planet."""
    pixels=[]
    for y in range(8):
        for x in range(8):
            dx,dy=x-3.5,y-3.5
            radius=4
            nx,ny=dx/radius,dy/radius
            rr=nx*nx+ny*ny
            c=0
            if rr<=1:
                z=math.sqrt(1-rr)
                light=0.35+0.65*max(0,-0.3*nx-0.4*ny+0.866*z)
                if body==0:rgb=(24,24,24) # Mercury: grey.
                elif body==1:rgb=(31,19,5) # Venus: orange.
                elif body==2:rgb=(30,7,4) # Mars: red.
                elif body==3:rgb=(31,25,17) # Jupiter: cream-orange, no bands.
                elif body==4:rgb=(31,24,10) # Saturn: golden, no rings.
                elif body==5:rgb=(13,26,28) # Uranus: pale cyan.
                elif body==6:rgb=(7,23,11) if (x+2*y)%7<2 else (5,14,31) # Earth.
                else:rgb=(5,8,28) # Neptune: blue.
                c=colour(*(v*light for v in rgb))
            pixels.append(c)
    return indexed_sprite(pixels)

def indexed_sprite(pixels):
    colors=[c for c,_ in Counter(pixels).most_common() if c][:255]
    def distance(a,b):
        return sum((((a>>shift)&31)-((b>>shift)&31))**2 for shift in (0,6,11))
    indices={0:0}
    indices.update({c:i+1 for i,c in enumerate(colors)})
    for c in set(pixels)-indices.keys():
        indices[c]=1+min(range(len(colors)),key=lambda i:distance(c,colors[i]))
    palette=[0]+colors+[0]*(255-len(colors))
    return bytes(indices[c] for c in pixels),struct.pack('<256H',*palette)

def cover():
    """Keep the supplied indexed BMP pixels; precompute opaque RGB555 fades."""
    bmp=(Path(__file__).resolve().parents[1]/'src/assets/sidera-cover.bmp').read_bytes()
    assert bmp[:2]==b'BM' and len(bmp)>=1074
    dib,w,h,planes,bpp,compression=struct.unpack_from('<IiiHHI',bmp,14)
    assert dib==40 and w==320 and abs(h)==180 and planes==1 and bpp==8 and compression==0
    at=struct.unpack_from('<I',bmp,10)[0]
    assert at>=1074 and len(bmp)>=at+57600
    rows=[bmp[at+y*320:at+(y+1)*320] for y in range(180)]
    if h>0:rows.reverse()
    palette=[]
    for level in range(33):
        for i in range(256):
            b,g,r,_=bmp[54+i*4:58+i*4]
            palette.append(32+((r>>3)*level//32)+(((g>>3)*level//32)<<6)+(((b>>3)*level//32)<<11))
    return b''.join(rows)+struct.pack('<8448H',*palette)

def build():
    font = bytearray(1024)
    for ch,rows in (GLYPHS | LOWER).items():
        for y,row in enumerate(rows): font[ord(ch)*8+y] = int(row,2)<<3
    palette = [0] + [colour(31, 8 + 23*i/254, 10*(i/254)**3) for i in range(255)]
    sun = bytearray(1024)
    for y in range(32):
        for x in range(32):
            r = math.hypot(x-15.5,y-15.5)
            if r < 12.5:
                sun[y*32+x] = max(1,round(255-95*(r/12.5)**2))
    phases = bytearray()
    # Each phase is lit from the top of the source square; VGA rotates it.
    # A radius of 14 leaves room for the nearest-neighbour affine sampler.
    for frame in range(129):
        zlight = 2*frame/128-1
        ylight = math.sqrt(max(0,1-zlight*zlight))
        for y in range(32):
            for x in range(32):
                nx,ny=(x-15.5)/14,(15.5-y)/14
                rr=nx*nx+ny*ny
                c=0
                if rr <= 1:
                    nz=math.sqrt(1-rr)
                    lit=ny*ylight+nz*zlight
                    # Restrained, original surface detail, no copied texture.
                    texture=0.90+0.05*math.sin(13*nx+9*ny)*math.cos(8*ny-3*nx)
                    b=(7+23*math.sqrt(max(0,lit)))*texture if lit>0 else 2.3
                    c=colour(b*0.96,b,b*1.04)
                phases.extend(struct.pack('<H',c))
    planets=[planet_sprite(i) for i in range(6)]
    vectors=bytearray()
    for _,ra,dec,_ in json.loads(((Path(__file__).resolve().parents[1] / 'src')/'data/constellations.json').read_text(encoding='utf8'))['stars']:
        r,d=math.radians(ra),math.radians(dec)
        vectors+=struct.pack('<3h',*(round(16384*v) for v in
                           (math.cos(d)*math.cos(r),math.cos(d)*math.sin(r),math.sin(d))))
    catalogue=json.loads(((Path(__file__).resolve().parents[1] / 'src')/'data/constellations.json').read_text(encoding='utf8'))
    records=lambda group: group['edges']+[[i,i] for i in group.get('points',[])]
    edges=b''.join(struct.pack('<2H',*edge) for group in catalogue['constellations'] for edge in records(group))
    metadata=bytearray(3 if star[3]<1.5 else 2 if star[3]<3.5 else 1 for star in catalogue['stars'])
    metadata.extend(bytes(760-len(metadata)))
    first=0
    for group in catalogue['constellations']:
        if group['abbr'] in ('UMa','UMi','Cru','Cen','Oct'):
            for edge in records(group):
                for index in edge:
                    metadata[index] |= 128  # C4 membership; no runtime selection bitmap.
        label=(group['name']+' ['+group['abbr']+']').replace('ö','\x04').encode('ascii')
        assert len(label)<28
        metadata.extend(struct.pack('<HH28s',first,len(records(group)),label))
        first+=len(records(group))
    return (b'SIDERA5\0'+font+struct.pack('<256H',*palette)+sun+phases+
            b''.join(p[0] for p in planets)+b''.join(p[1] for p in planets)+vectors+edges+cover()+metadata)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
    p.add_argument('--tables',action='store_true');a=p.parse_args()
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(build())
    if a.tables:
        f=a.output.parent.parent/'tables'/'sine.h';f.parent.mkdir(parents=True,exist_ok=True)
        values=[round(16384*math.sin(i*math.pi/512)) for i in range(257)]
        f.write_text('/* Q14 quarter sine, generated by tools/assets.py. */\nstatic const int16_t sine[257] = {\n'+
            ',\n'.join(','.join(map(str,values[i:i+12])) for i in range(0,257,12))+'\n};\n')
        values=[round(math.atan(i/256)*65536/math.tau) for i in range(257)]
        f=f.with_name('atan.h')
        f.write_text('/* Octant atan, generated by tools/assets.py. */\nstatic const uint16_t atan_octant[257] = {\n'+
            ',\n'.join(','.join(map(str,values[i:i+12])) for i in range(0,257,12))+'\n};\n')
