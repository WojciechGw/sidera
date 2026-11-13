"""Render the actual host-test splash snapshot, with VGA canvas scaling."""
from pathlib import Path
import struct
import sys
import zlib

data=Path(sys.argv[1]).read_bytes()
_,x,y,w,h,src,pal=struct.unpack_from('<H4h2H',data,0x200)
rows=[]
for py in range(480):
    row=bytearray()
    for px in range(640):
        bx,by=px//2-x,py//2-y
        color=0
        if 0<=bx<w and 0<=by<h:
            index=data[src+by*w+bx]
            color=struct.unpack_from('<H',data,pal+index*2)[0]
        row.extend(((color>>shift)&31)*255//31 for shift in (0,6,11))
    rows.append(b'\0'+row)
def chunk(kind,payload):
    return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload)&0xffffffff)
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',640,480,8,2,0,0,0))
Path(sys.argv[2]).write_bytes(png+chunk(b'IDAT',zlib.compress(b''.join(rows)))+chunk(b'IEND',b''))
