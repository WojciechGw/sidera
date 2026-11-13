"""Actual 65C02 planet positions against independent, offline JPL Horizons."""
import datetime as dt
import json
import math
from pathlib import Path
import subprocess
import sys

here=Path(__file__).resolve().parent
work=Path(sys.argv[3]).resolve();work.mkdir(parents=True,exist_ok=True)
data=json.loads((here/'planets_horizons.json').read_text())
source='#include <assert.h>\n#include <stdio.h>\n#include <string.h>\n#include "astro.h"\nstatic const Date dates[]={\n'
for case in data['cases']:
    d=dt.datetime.strptime(case['date'],'%Y-%m-%d %H:%M:%S')
    source+='{'+','.join(map(str,(d.year,d.month,d.day,d.hour,d.minute,d.second)))+'},\n'
source+='''};
int main(void) {
    static Observer o={1787346L,689597L,395};
    static Position p[PLANET_COUNT];unsigned i,j;
    uint16_t saved[PLANET_COUNT+1];Sky trail;
    for(i=0;i<sizeof(dates)/sizeof(dates[0]);++i) {
        planets_compute(&dates[i],&o,p);
        for(j=0;j<PLANET_COUNT;++j)printf("%u %u %u %d %u %u\\n",i,j,p[j].az,p[j].alt,
            body_distance[j],body_distance[PLANET_COUNT]);
        memcpy(saved,body_distance,sizeof(saved));
        sky_positions(&dates[0],&o,&trail);
        assert(!memcmp(saved,body_distance,sizeof(saved))); /* trails cannot change depth */
    }
    for(i=0;i<4;++i) {
        o.latitude=i&1?-3240000L:3240000L;o.longitude=i&2?-6480000L:6480000L;
        o.altitude=i&1?-500:10000;planets_compute(&dates[0],&o,p);
        for(j=0;j<PLANET_COUNT;++j)assert(p[j].alt>=-16384&&p[j].alt<=16384);
    }
    return 0;
}
'''
(work/'check.c').write_text(source)
subprocess.run([sys.argv[1],'-t','sim65c02','-O','-I',str((here.parent / 'src')),
    '-o',str(work/'check'),str(work/'check.c'),str((here.parent / 'src')/'astro.c')],check=True)
p=subprocess.run([sys.argv[2],'-c',str(work/'check')],capture_output=True,text=True,check=True)
rows=[list(map(int,line.split())) for line in p.stdout.splitlines() if len(line.split())==6]
assert len(rows)==len(data['cases'])*6,(p.stdout,p.stderr)
errors=[0.]*6
distances={}
# Broad geocentric orbital bounds catch overflow and lost normalization scale.
bounds=((.45,1.55),(.20,1.8),(.30,2.8),(3.8,6.5),(7,11.5),(17,22.5))
for i,j,az,alt,dist,sun in rows:
    assert .97<sun/1024<1.03,(i,sun)
    assert bounds[j][0]<dist/1024<bounds[j][1],(i,j,dist)
    distances[i,j]=dist
    ref=data['cases'][i]['planets'][j]
    a,h=math.radians(az*360/65536),math.radians(alt*360/65536)
    ar,hr=map(math.radians,ref)
    error=math.degrees(math.acos(max(-1,min(1,math.sin(h)*math.sin(hr)+math.cos(h)*math.cos(hr)*math.cos(a-ar)))))
    errors[j]=max(errors[j],error)
    assert error<0.3,(data['names'][j],data['cases'][i]['date'],error)
for i in range(len(data['cases'])):
    assert distances[i,2]<distances[i,3]<distances[i,4]<distances[i,5]
for j in (0,1):
    assert any(dist<sun for i,k,az,alt,dist,sun in rows if k==j)
    assert any(dist>sun for i,k,az,alt,dist,sun in rows if k==j)
print('PASS: geocentric distance bounds, outer planet ordering, Mercury/Venus on both sides of Sun, trail isolation')
print('PASS:',len(rows),'planet positions, 1900-2200; poles/dateline bounded')
for name,error in zip(data['names'],errors):print(f'{name}: max sky separation {error:.4f} deg')
print(p.stderr.strip())
