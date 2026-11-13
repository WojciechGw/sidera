"""Actual 65C02 fixed-point stars vs full double-precision IAU 1976 / GMST.

Reference equations: Lieske precession (full cubic terms), standard GMST
polynomial at J2000. No catalogue quantization in the reference positions.
"""
import datetime as dt
import json
import math
from pathlib import Path
import subprocess
import sys

here=Path(__file__).resolve().parent
work=Path(sys.argv[3]).resolve();work.mkdir(parents=True,exist_ok=True)
catalog=json.loads(((here.parent / 'src')/'data/constellations.json').read_text(encoding='utf8'))
hips=[11767,32349,30438,24608,91262,69673,27989,113368,60718,54061,102098,24436]
indices=[next(i for i,s in enumerate(catalog['stars']) if s[0]==hip) for hip in hips]
cases=[(dt.datetime(y,m,d,h,mi,s),lat,lon) for y,m,d,h,mi,s,lat,lon in [
 (1900,1,1,0,0,0,49.6485,19.155472), (1950,6,21,12,30,45,-33.9,151.2),
 (2000,1,1,12,0,0,0,0), (2026,11,17,14,0,0,49.6485,19.155472),
 (2050,3,20,18,59,59,-90,-180), (2100,12,31,23,59,59,90,180),
 (2150,9,22,6,15,0,0,-179.99), (2200,12,31,23,59,59,-45,170)]]
source='''#include <assert.h>
#include <stdio.h>
#include "astro.h"
typedef struct { int16_t v[3]; unsigned char size; } Star;
typedef struct { unsigned a,b; } Edge;
typedef struct { unsigned first,count; const char *label; } Constellation;
#include "tables/constellations.h"
static const unsigned ids[]={'''+','.join(map(str,indices))+'''};
int main(void) {
 static StarFrame f; static Position p; static Date d; static Observer o;
 unsigned i; unsigned long sum=0;
'''
for d,lat,lon in cases:
    source+=f'd.year={d.year};d.month={d.month};d.day={d.day};d.hour={d.hour};d.minute={d.minute};d.second={d.second};\n'
    source+=f'o.latitude={round(lat*36000)}L;o.longitude={round(lon*36000)}L;o.altitude=395;star_frame(&d,&o,&f);\n'
    source+='for(i=0;i<sizeof(ids)/sizeof(ids[0]);++i){star_position(&f,stars[ids[i]].v,&p);printf("%u %d\\n",p.az,p.alt);}\n'
source+='''for(i=0;i<STAR_COUNT;++i){star_position(&f,stars[i].v,&p);sum+=p.az;
 assert(p.alt>=-16384&&p.alt<=16384);}
 printf("all %lu\\n",sum);return 0;}
'''
(work/'stars.c').write_text(source)
subprocess.run([sys.argv[1],'-t','sim65c02','-O','-I',str((here.parent / 'src')),'-o',str(work/'stars'),
               str(work/'stars.c'),str((here.parent / 'src')/'astro.c')],check=True)
p=subprocess.run([sys.argv[2],'-c',str(work/'stars')],capture_output=True,text=True,check=True)
rows=[tuple(map(int,line.split())) for line in p.stdout.splitlines()
      if len(line.split())==2 and all(v.lstrip('-').isdigit() for v in line.split())]
assert len(rows)==len(cases)*len(indices)

def reference(d,lat,lon,star):
    days=(d-dt.datetime(2000,1,1,12)).total_seconds()/86400
    t=days/36525
    zeta=math.radians((2306.2181*t+0.30188*t*t+0.017998*t**3)/3600)
    z=math.radians((2306.2181*t+1.09468*t*t+0.018203*t**3)/3600)
    theta=math.radians((2004.3109*t-0.42665*t*t-0.041833*t**3)/3600)
    ra,dec=map(math.radians,star[1:3])
    a=math.cos(dec)*math.sin(ra+zeta)
    b=math.cos(theta)*math.cos(dec)*math.cos(ra+zeta)-math.sin(theta)*math.sin(dec)
    c=math.sin(theta)*math.cos(dec)*math.cos(ra+zeta)+math.cos(theta)*math.sin(dec)
    ra=math.atan2(a,b)+z;dec=math.asin(c)
    sid=math.radians((280.46061837+360.98564736629*days+0.000387933*t*t-t**3/38710000+lon)%360)
    lat=math.radians(lat);ha=sid-ra
    east=-math.cos(dec)*math.sin(ha)
    north=math.sin(dec)*math.cos(lat)-math.cos(dec)*math.cos(ha)*math.sin(lat)
    up=math.sin(dec)*math.sin(lat)+math.cos(dec)*math.cos(ha)*math.cos(lat)
    return east,north,up

worst=0
for case in cases:
    for index in indices:
        az,alt=rows.pop(0);az=az*math.tau/65536;alt=alt*math.tau/65536
        wanted=reference(*case,catalog['stars'][index])
        actual=(math.cos(alt)*math.sin(az),math.cos(alt)*math.cos(az),math.sin(alt))
        error=math.degrees(math.acos(max(-1,min(1,sum(a*b for a,b in zip(wanted,actual))))))
        worst=max(worst,error)
        assert error<0.06,(case,index,error)
print(f'PASS: 96 stellar positions, 1900-2200, both poles, dateline; max separation {worst:.4f} deg')
print('PASS: all 759 catalogue vectors on 65C02')
print(p.stderr.strip())
print('\n'.join(line for line in p.stdout.splitlines() if 'cycles' in line))
