"""Render the tested XRAM snapshot, including plot and header sprite descriptors.
No dependencies; PNG writer uses zlib. This is a host preview, not a board capture.
"""
from pathlib import Path
import struct
import sys
import zlib

data=Path(sys.argv[1]).read_bytes()
def u(a):return struct.unpack_from('<H',data,a)[0]
def s(a):return struct.unpack_from('<h',data,a)[0]
def rgb(c):return bytes(((c&31)*255//31,((c>>6)&31)*255//31,((c>>11)&31)*255//31))
pixels=bytearray(640*480*3)
for y in range(480):
    for x in range(640):
        if data[y*80+x//8]&(128>>(x&7)):
            a=(y*640+x)*3;pixels[a:a+3]=b'\xff\xff\xff'
for mode,cfg,top,bottom in ([] if '--bitmap' in sys.argv[3:] else
                           [(6,0xb050+i*10,84,416) for i in range(7)]+
                           ([(6,0xb096+i*10,84,480) for i in range(9)] if '--solar' in sys.argv else [])+
                           [(4,0xb018,84,416),(5,0xb030,0,71),(4,0xb038,0,84),
                            (7,0xb0f0,75,229)]):
    size=32
    if mode in (5,6,7):
        x,y,src,pal=s(cfg),s(cfg+2),u(cfg+4),u(cfg+6)
        if mode==6:
            assert data[cfg+9]==3
            size=((data[cfg+8]&15)+1)*4
            assert size==((data[cfg+8]>>4)+1)*4
        if mode==7:
            assert data[cfg+8]==0x11 and not data[cfg+9]
            size=8
    else:
        x,y,src=s(cfg+12),s(cfg+14),u(cfg+16)
        aa,ab,b0,ba,bb,b1=struct.unpack_from('<6h',data,cfg)
    for dy in range(size):
        for dx in range(size):
            if not (0<=x+dx<640 and top<=y+dy<bottom):continue
            if mode==7:c=u(pal+((data[src+dy]>>(7-dx))&1)*2)
            elif mode in (5,6):c=u(pal+data[src+dy*size+dx]*2)
            else:
                tx=(aa*dx+ab*dy+b0)//256;ty=(ba*dx+bb*dy+b1)//256
                if not (0<=tx<32 and 0<=ty<32):continue
                c=u(src+(ty*32+tx)*2)
            if c&32:
                a=((y+dy)*640+x+dx)*3;pixels[a:a+3]=rgb(c)
def chunk(t,b):return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b)&0xffffffff)
raw=b''.join(b'\0'+pixels[y*640*3:(y+1)*640*3] for y in range(480))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',640,480,8,2,0,0,0))
png+=chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
Path(sys.argv[2]).write_bytes(png)
print(sys.argv[2])
