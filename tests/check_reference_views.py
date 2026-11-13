"""Compare decoded PNG scanlines, independent of zlib compression version."""
from pathlib import Path
import struct
import sys
import zlib

def pixels(path):
    data=path.read_bytes()
    assert data[:8]==b'\x89PNG\r\n\x1a\n', path
    offset=8; payload=bytearray(); header=None
    while offset<len(data):
        size=struct.unpack_from('>I',data,offset)[0]
        kind=data[offset+4:offset+8]
        chunk=data[offset+8:offset+8+size]
        crc=struct.unpack_from('>I',data,offset+8+size)[0]
        assert zlib.crc32(kind+chunk)==crc, path
        if kind==b'IHDR': header=chunk
        if kind==b'IDAT': payload.extend(chunk)
        offset+=12+size
    # Both reference and renderer use unfiltered RGB scanlines.
    return header,zlib.decompress(payload)

reference=Path(__file__).resolve().parents[1]/'src/assets'
generated=Path(sys.argv[1])
for kind in ('test','solar'):
    for view in 'ABC':
        name=f'sidera-{kind}{view}.png'
        assert pixels(generated/name)==pixels(reference/name), name
print('PASS: six rendered views match the reference pixels')
