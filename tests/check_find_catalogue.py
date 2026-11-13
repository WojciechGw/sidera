"""Validate searchable identities independently of the binary generator."""
import json
from pathlib import Path
import struct

root=Path(__file__).resolve().parents[1]
catalogue=json.loads((root/'src/data/constellations.json').read_text(encoding='utf8'))
stars=catalogue['stars']
names=json.loads((root/'src/data/find-names.json').read_text(encoding='utf8'))
raw=(root/'src/rom/sidera.find').read_bytes()
magic,count,size=struct.unpack_from('<8sHH',raw)
assert (magic,count,size)==(b'SIDFIND3',855,256)
assert len(raw)==16+count*size
seen=set()
for n in range(count):
    record=raw[16+n*size:16+(n+1)*size]
    ident=struct.unpack_from('<H',record)[0]
    assert ident not in seen and ident<count
    seen.add(ident)
    label=record[2:42].split(b'\0')[0].decode('ascii')
    search=record[42:].split(b'\0')[0].decode('ascii')
    assert label and label in search and not record[41] and not record[191]
    row=record[192:244].decode('ascii')
    assert not any(record[244:])
    columns=[row[start:start+width].rstrip() for start,width in zip((0,5,12,19,35),(4,6,6,15,17))]
    assert columns[4]==(label if len(label)<=17 else label[:14]+'...')
    for start,end in ((4,5),(11,12),(18,19),(34,35)):
        assert row[start:end].isspace()
    if ident>=8+len(stars):
        group=catalogue['constellations'][ident-8-len(stars)]
        assert group['abbr'] in search and 'Constellation' in search
        assert columns[:4]==['Con','','',group['abbr']]
    elif ident>=8:
        hip=stars[ident-8][0]
        assert f'HIP {hip}' in search
        assert str(hip) in names['stars']
        info=names['stars'][str(hip)]
        assert columns[0]=='St' and columns[1]==str(hip) and columns[2]==info['hd'].removeprefix('HD ')
        designation=info['bayer'] or info['flam']
        assert columns[3]==(designation+' '+info['c'] if designation else '')
        for key in ('name','bayer','flam','var'):
            if info[key]:assert info[key] in search
        if names['stars'][str(hip)]['name']:
            assert names['stars'][str(hip)]['name'] in search
    else:
        assert label==('Sun','Moon','Mercury','Venus','Mars','Jupiter','Saturn','Uranus')[ident]
        assert columns[0]==('Sun' if ident==0 else 'Moon' if ident==1 else 'Pl')
        assert columns[1:4]==['','','']
    # The right pane has five 49-character lines for every full, searchable alias.
    import textwrap
    assert len(textwrap.wrap(search,49))<=5
print('PASS: 855 unique FIND objects, 88 constellations, exact HIP mapping, ASCII aliases and record bounds')
