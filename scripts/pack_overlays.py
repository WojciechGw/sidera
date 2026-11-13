"""Stamp the linked resident regions and six overlays, then embed them in ROM.

No relinking or address relocation: every file comes from the same ld65 run.
"""
import hashlib
import re
import struct
import sys
import zlib
from pathlib import Path

NAMES = ('init', 'cities', 'edit', 'cover', 'solar', 'find')
BASE, LIMIT = 0xC800, 0x1700


def labels(path):
    return {name: int(addr, 16) for addr, name in re.findall(
        r'^al ([0-9A-Fa-f]+) \.(\S+)$', Path(path).read_text(), re.M)}


def package(work):
    work = Path(work)
    symbols = labels(work / 'sidera.lbl')
    core = bytearray((work / 'sidera').read_bytes())
    payloads = [(work / f'sidera.linked.bin.{name}').read_bytes() for name in NAMES]
    high = (work / 'sidera.linked.bin.high').read_bytes()
    assert 0 < len(high) <= 0x1700
    build = hashlib.sha256(bytes(core) + high + b''.join(payloads)).digest()[:8]
    stamp = symbols['_ovl_build_id'] - 0x200
    assert 0 <= stamp <= len(core) - 8
    core[stamp:stamp + 8] = build
    (work / 'sidera.bin').write_bytes(core)
    for number, (name, payload) in enumerate(zip(NAMES, payloads), 1):
        prefix = '__' + name.upper() + '_BSS_'
        bss_size = symbols.get(prefix + 'SIZE__', 0)
        bss = symbols.get(prefix + 'RUN__', BASE + len(payload)) - BASE
        assert 0 < len(payload) <= bss <= LIMIT and bss_size <= LIMIT - bss, name
        header = struct.pack('<4sBBHHH8sI', b'SOV1', number, 1, len(payload),
                             bss, bss_size, build, zlib.crc32(payload))
        asset = header + payload
        filename = f'sidera-{name}.ovl'
        (work / filename).write_bytes(asset)
        print(f'{name}: {len(payload)} bytes loaded + {bss_size} BSS; '
              f'{LIMIT - bss - bss_size} bytes spare in overlay')
    # SDK rp6502_byproduct() packages these outputs into the ROM.


if __name__ == '__main__':
    package(*sys.argv[1:])
