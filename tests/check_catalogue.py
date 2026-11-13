"""Verify shipped city records and streamed stellar vectors against sources."""
import json
import hashlib
import math
from pathlib import Path
import struct
import re

root=(Path(__file__).resolve().parents[1] / 'src')
source=json.loads((root/'data/cities.json').read_text(encoding='utf8'))['cities']
blob=(root/'rom/sidera.cty').read_bytes()
magic,count,size,reserved=struct.unpack_from('<8sHHI',blob)
assert (magic,count,size,reserved)==(b'SIDCITY2',345,96,0)
tz=(root/'rom/sidera.tz').read_bytes()
assert len(blob)==16+count*size
countries=set();defaults={};names=set()
for i,city in enumerate(source):
    code,country,name,lat,lon,height,flags,zone=struct.unpack_from('<3s40s40siihBH',blob,16+i*size)
    assert tz[24+zone*48:24+zone*48+40].split(b'\0')[0].decode()==city['timezone']
    code,country,name=(v.split(b'\0',1)[0].decode('ascii') for v in (code,country,name))
    assert (code,country,name)==(city['code'],city['country'],city['city'])
    assert abs(lat/36000-city['latitude'])<=.5/36000+1e-12
    assert abs(lon/36000-city['longitude'])<=.5/36000+1e-12
    assert height==city['altitude'] and -500<=height<=10000
    assert bool(flags&2)==city['capital'] and bool(flags&4)==city['seat']
    assert (code,name) not in names
    names.add((code,name));countries.add(code)
    if flags&1:
        assert code not in defaults and flags&6
        defaults[code]=name
assert len(countries)==195 and set(defaults)==countries
assert defaults['PW']=='Ngerulmud' and defaults['LK']=='Sri Jayewardenepura Kotte'
assert defaults['GQ']=='Ciudad de la Paz'
assert ('US','New York') in names and ('US','Los Angeles') in names
assert ('ZA','Cape Town') in names and ('ZA','Bloemfontein') in names
assert ('CN','Urumqi') in names and ('RU','Vladivostok') in names
polish={c['city']:c for c in source if c['code']=='PL'}
assert set(polish)==set('Warsaw|Bialystok|Bydgoszcz|Gdansk|Gorzow Wielkopolski|Katowice|Kielce|Krakow|Lodz|Lublin|Olsztyn|Opole|Poznan|Rzeszow|Szczecin|Torun|Wroclaw|Zielona Gora|Sosnowiec|Radziechowy'.split('|'))
for name,ident in [('Sosnowiec',3085128),('Radziechowy',3087431)]:
    assert polish[name]['geonames_id']==ident and polish[name]['admin1']=='83'
assert all(c['timezone']=='Europe/Warsaw' for c in polish.values())
catalogue=json.loads((root/'data/constellations.json').read_text(encoding='utf8'))
stars=catalogue['stars']
assert len(stars)==759 and len({s[0] for s in stars})==759
for abbr,hip,expected in [('Cru',60260,[-174.66,-60.4011,3.59]),
                          ('Oct',104382,[-42.8048,-88.9565,5.45])]:
    group=next(g for g in catalogue['constellations'] if g['abbr']==abbr)
    assert len(group['points'])==1
    i=group['points'][0]
    assert stars[i]==[hip,*expected]
    assert not any(i in edge for edge in group['edges']), 'No invented connecting segments'
assets=(root/'rom/sidera.dat').read_bytes()
assert assets[:8]==b'SIDERA5\0'
# The 8x8 variant must preserve the approved Sun palette/image and Moon phases.
assert hashlib.sha256(assets[1032:266760]).hexdigest()=='2712141ffb17e99580f89ad00fec952a93b08cc7b318f2978b1663cef238ed01'
masks=[]
for i in range(6):
    sprite=assets[266760+i*64:266760+(i+1)*64]
    palette=struct.unpack_from('<256H',assets,266760+6*64+i*512)
    assert palette[0]==0
    masks.append([bool(palette[p]&32) for p in sprite])
    assert sum(masks[-1])==52 and len(set(sprite))>8
    def brightness(index):
        c=palette[sprite[index]]
        return sum((c>>shift)&31 for shift in (0,6,11))
    assert brightness(2*8+2)>brightness(5*8+5), 'Light must come from upper left'
assert all(mask==masks[0] for mask in masks), 'Every planet, including Saturn, has the same circular outline'
records=lambda group: group['edges']+[[i,i] for i in group.get('points',[])]
edges=[edge for group in catalogue['constellations'] for edge in records(group)]
assert sum(len(g['edges']) for g in catalogue['constellations'])==743 and len(edges)==745
assert len(assets)==270216+len(stars)*6+len(edges)*4+57600+33*512+3576
edges_offset=270216+len(stars)*6
metadata=assets[edges_offset+len(edges)*4+57600+33*512:]
assert bytes(v&3 for v in metadata[:len(stars)])==bytes(3 if s[3]<1.5 else 2 if s[3]<3.5 else 1 for s in stars)
selected={i for g in catalogue['constellations'] if g['abbr'] in ('UMa','UMi','Cru','Cen','Oct')
          for edge in records(g) for i in edge}
assert {i for i in range(len(stars)) if metadata[i]&128}==selected
# Keep the eight names tied to their Hipparcos identities, including component
# B of Alpha Centauri, if a future catalogue import changes record ordering.
named=[int(i) for i in re.search(r'named_stars\[NAMED_COUNT\]=\{([^}]+)',
    (root/'constellations.c').read_text()).group(1).split(',')]
assert [stars[i][0] for i in named]==[53910,54061,11767,71681,68702,61084,60718,104382]
assert set(named)<=selected
first=0
for i,group in enumerate(json.loads((root/'data/constellations.json').read_text(encoding='utf8'))['constellations']):
    start,count,label=struct.unpack_from('<HH28s',metadata,760+i*32)
    assert (start,count)==(first,len(records(group)))
    assert label.rstrip(b'\0').decode('ascii')==(group['name']+' ['+group['abbr']+']').replace('ö','\x04')
    first+=count
assert assets[edges_offset:edges_offset+len(edges)*4]==b''.join(struct.pack('<2H',*edge) for edge in edges)
for i,(_,ra,dec,_) in enumerate(stars):
    r,d=math.radians(ra),math.radians(dec)
    wanted=(math.cos(d)*math.cos(r),math.cos(d)*math.sin(r),math.sin(d))
    actual=struct.unpack_from('<3h',assets,270216+i*6)
    assert all(abs(v/16384-w)<=.5/16384+1e-12 for v,w in zip(actual,wanted))
print('PASS: cities, 759 J2000 vectors, Epsilon Cru / Sigma Oct isolated stars, 743 segments')
