"""Validate geographic grouping and bounded, reproducible world-map resources."""
import json,struct,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'scripts'))
from world_regions import region
data=(root/'src/rom/sidera.map').read_bytes()
assert struct.unpack_from('<8sHHI',data)==(b'SIDMAP1\0',300,150,46816)
assert len(data)==91816 and set(data[46816:])==set(range(1,8))
for kind in range(8):
    rows=data[16+kind*5850:16+(kind+1)*5850]
    assert all(not(rows[y*39]&252 or rows[y*39+38]&63) for y in range(150))
    if kind:assert all((a&b)==a for a,b in zip(data[16:5866],rows))
    x=round((50+180)*299/360);y=round((90-43)*149/180)
    assert not(rows[y*39+(x+6)//8]&(128>>((x+6)%8))), 'Caspian Sea must remain water'
samples={(21,52):1,(139,35):2,(31,30):3,(-74,40):4,(-43,-23):5,(151,-34):6,(0,-80):7,
         (-157,21):6,(140,-3):6,(119,-5):2,(37,56):1,(61,57):2,(-25,38):1,(-16,28):3,
         (-22,75):4,(-20,64):1,(173,53):4,(29,41.1):1,(33,35):2,(-61.5,10.6):4}
for point,expected in samples.items():assert region(*point)==expected,(point,expected)
rows=json.loads((root/'src/data/cities.json').read_text())['cities']
catalogue=(root/'src/rom/sidera.cty').read_bytes()
for i,city in enumerate(rows):
    flags=catalogue[16+i*96+93]
    assert flags>>4==region(city['longitude'],city['latitude'])
    assert flags&7==int(city['default'])+2*int(city['capital'])+4*int(city['seat'])
print('PASS: seven geographic regions, dateline/islands, catalogue tags, map dimensions and protected edge bits')
