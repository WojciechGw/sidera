"""Build the offline FIND catalogue; optionally import a pinned d3 name snapshot."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import unicodedata

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--import-names', type=Path)
p.add_argument('--revision')
p.add_argument('--output', type=Path, default=ROOT/'build/generated/rom/sidera.find')
a = p.parse_args()
catalogue = json.loads((ROOT/'src/data/constellations.json').read_text(encoding='utf8'))
stars = catalogue['stars']
source = ROOT/'src/data/find-names.json'
greek = dict(zip('αβγδεζηθικλμνξοπρστυφχψω',
    'Alpha Beta Gamma Delta Epsilon Zeta Eta Theta Iota Kappa Lambda Mu Nu Xi Omicron Pi Rho Sigma Tau Upsilon Phi Chi Psi Omega'.split()))

def ascii_text(text):
    for char, name in greek.items():
        text = text.replace(char, name)
    return ' '.join(unicodedata.normalize('NFKD', text).encode('ascii', 'ignore').decode().split())

if a.import_names:
    assert a.revision
    raw = a.import_names.read_bytes()
    names = json.loads(raw)
    rows = {}
    for hip, *_ in stars:
        item = names.get(str(hip), {})
        rows[str(hip)] = {k: ascii_text(item.get(k, '')) for k in ('name','bayer','flam','var','hd','c')}
    source.write_text(json.dumps({'source': f'https://github.com/ofrohn/d3-celestial/blob/{a.revision}/data/starnames.json',
        'sha256': hashlib.sha256(raw).hexdigest(), 'stars': rows}, indent=2)+'\n', encoding='utf8')

names = json.loads(source.read_text(encoding='utf8'))['stars']
assert set(names) == {str(s[0]) for s in stars}
records = []
for i, name in enumerate(('Sun','Moon','Mercury','Venus','Mars','Jupiter','Saturn','Uranus')):
    records.append((i, name, name+' | '+('Planet' if i>=2 else 'Solar System')))
for i,(hip, *_rest) in enumerate(stars):
    n = names[str(hip)]
    aliases = [n['name']]
    for key in ('bayer','flam','var'):
        if n[key]: aliases.append(n[key]+' '+n['c'])
    aliases += [f'HIP {hip}', n['hd']]
    # The navigation name refers to the system; this catalogue contains component B.
    if hip == 71681: aliases.insert(0, 'Alpha Centauri B')
    if hip == 68702: aliases.append('Beta Centauri')
    if hip == 104382: aliases.append('Sigma Octantis')
    aliases = list(dict.fromkeys(x for x in aliases if x))
    label = aliases[0]
    records.append((i+8, label, ' | '.join(aliases)))
records[8:] = sorted(records[8:], key=lambda r:r[1].lower())
for i, group in enumerate(catalogue['constellations']):
    name = ascii_text(group['name'])
    records.append((8+len(stars)+i, name, f'{name} | {group["abbr"]} | Constellation'))

def field(text, size):
    raw = text.encode('ascii')
    assert len(raw)<size, (text,size)
    return raw.ljust(size,b'\0')

def table_fields(ident, label):
    if ident >= 8+len(stars):
        return [label, 'Constellation', catalogue['constellations'][ident-8-len(stars)]['abbr'], '', '']
    if ident < 8:
        return [label, 'Star' if ident == 0 else 'Moon' if ident == 1 else 'Planet', '', '', '']
    hip = stars[ident-8][0]
    n = names[str(hip)]
    designation = n['bayer'] or n['flam']
    return [label, 'Star', designation+' '+n['c'] if designation else '', str(hip), n['hd'].removeprefix('HD ')]

data = bytearray(struct.pack('<8sHH4x', b'SIDFIND3', len(records), 256))
for ident, name, search in records:
    columns = table_fields(ident, name)
    kind = 'Sun' if ident==0 else {'Star':'St','Moon':'Moon','Planet':'Pl','Constellation':'Con'}[columns[1]]
    columns = [kind, columns[3], columns[4], columns[2], columns[0]]
    row = [' ']*52
    for value, start, width in zip(columns, (0,5,12,19,35), (4,6,6,15,17)):
        if len(value)>width:
            value = value[:width-3]+'...'
        row[start:start+len(value)] = value
    data += struct.pack('<H', ident)+field(name,40)+field(search,150)
    data += field(''.join(row),53)+bytes(11)
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_bytes(data)
print(f'FIND: {len(records)} objects, {sum(bool(n["name"]) for n in names.values())} proper star names, {len(data)} bytes')
