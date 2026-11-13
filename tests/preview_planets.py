"""Preview the shipped planet pixels at 1:1 and 8x, using SIDERA's own font.

Usage: python preview_planets.py output.png
"""
from pathlib import Path
import struct
import sys
import zlib

data=((Path(__file__).resolve().parents[1] / 'src')/'rom/sidera.dat').read_bytes()
assert data[:8]==b'SIDERA5\0'
width,height=600,184
pixels=bytearray(width*height*3)

def pixel(x,y,rgb):
    at=(y*width+x)*3
    pixels[at:at+3]=rgb

def text(x,y,label):
    for ch in label:
        for row in range(8):
            bits=data[8+ord(ch)*8+row]
            for col in range(6):
                if bits&(128>>col):pixel(x+col,y+row,b'\xff\xff\xff')
        x+=6

text(12,8,'SIDERA planets: 1:1')
text(12,70,'Same pixels enlarged 8x')
for i,name in enumerate(('Mercury','Venus','Mars','Jupiter','Saturn','Uranus')):
    src=266760+i*64
    pal=266760+6*64+i*512
    for scale,top in ((1,30),(8,92)):
        left=i*100+(100-8*scale)//2
        for y in range(8):
            for x in range(8):
                c=struct.unpack_from('<H',data,pal+data[src+y*8+x]*2)[0]
                if not c&32:continue
                rgb=bytes(((c&31)*255//31,((c>>6)&31)*255//31,((c>>11)&31)*255//31))
                for dy in range(scale):
                    for dx in range(scale):pixel(left+x*scale+dx,top+y*scale+dy,rgb)
        text(i*100+(100-len(name)*6)//2,top+8*scale+8,name)

def chunk(kind,payload):
    return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload)&0xffffffff)

rows=b''.join(b'\0'+pixels[y*width*3:(y+1)*width*3] for y in range(height))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))
Path(sys.argv[1]).write_bytes(png+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b''))
