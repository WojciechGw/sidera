"""Compile the actual integer astronomy code for 65C02; run against JPL.
Usage: python check_6502.py <cl65> <sim65> <work-directory>
"""
import datetime as dt
import json
import math
from pathlib import Path
import subprocess
import sys

here=Path(__file__).resolve().parent
work=Path(sys.argv[3]).resolve();work.mkdir(parents=True,exist_ok=True)
data=json.loads((here/'horizons.json').read_text())
dates=[dt.datetime.strptime(c['date'],'%Y-%m-%d %H:%M:%S') for c in data['cases']]
source='''#include <assert.h>
#include <stdio.h>
#include "astro.h"
static const Date dates[] = {\n'''
source+=',\n'.join('{'+','.join(map(str,(d.year,d.month,d.day,d.hour,d.minute,d.second)))+'}' for d in dates)
source+='''};
int main(void) {
  static Observer o={1787346L,689597L,395};
  static Sky s; static Date d; unsigned i;
  d.year=1900;d.month=2;d.day=28;d.hour=23;d.minute=59;d.second=59;
  assert(date_step(&d,1)); assert(d.month==3 && d.day==1);
  d.year=2000;d.month=2;d.day=28;assert(date_step(&d,86400L));assert(d.day==29);
  d.year=2100;d.month=2;d.day=28;assert(date_step(&d,86400L));assert(d.month==3);
  d.year=2200;d.month=12;d.day=31;d.hour=23;d.minute=59;d.second=59;
  assert(!date_step(&d,1));assert(d.year==2200 && d.day==31);
  d.year=1900;d.month=1;d.day=1;d.hour=d.minute=d.second=0;
  assert(!date_step(&d,-1));assert(d.year==1900 && d.day==1);
  for(i=0;i<sizeof(dates)/sizeof(dates[0]);++i) {
    sky_compute(&dates[i],&o,&s);
    printf("%ld %u %d %u %d %u %u\\n",date_day(&dates[i]),s.sun.az,s.sun.alt,
      s.moon.az,s.moon.alt,s.illumination,s.tilt);
  }
  /* Exact poles and dateline must be finite, with bounded altitude. */
  for(i=0;i<4;++i) {
    o.latitude=i&1?-3240000L:3240000L;o.longitude=i&2?-6480000L:6480000L;
    o.altitude=10000;sky_compute(&d,&o,&s);
    assert(s.sun.alt>=-16384&&s.sun.alt<=16384);
    assert(s.moon.alt>=-16384&&s.moon.alt<=16384);
  }
  return 0;
}
'''
(work/'check.c').write_text(source)
subprocess.run([sys.argv[1],'-t','sim65c02','-O','-I',str((here.parent / 'src')),
    '-o',str(work/'check'),str(work/'check.c'),str((here.parent / 'src')/'astro.c')],check=True)
p=subprocess.run([sys.argv[2],'-c',str(work/'check')],capture_output=True,text=True,check=True)
lines=[s for s in p.stdout.splitlines() if len(s.split())==7]
assert len(lines)==len(dates),(p.stdout,p.stderr)
maxerr=[0.,0.];maxphase=0.;maxtilt=0.
def distance(a,b):
    az1,al1=map(math.radians,a);az2,al2=map(math.radians,b)
    c=math.sin(al1)*math.sin(al2)+math.cos(al1)*math.cos(al2)*math.cos(az1-az2)
    return math.degrees(math.acos(max(-1,min(1,c))))
for case,d,line in zip(data['cases'],dates,lines):
    day,sa,sh,ma,mh,illum,tilt=map(int,line.split())
    assert day==(d.date()-dt.date(1999,12,31)).days
    for b,az,alt in [(0,sa,sh),(1,ma,mh)]:
        err=distance([az*360/65536,alt*360/65536],case[['sun','moon'][b]][:2])
        maxerr[b]=max(maxerr[b],err)
        assert err<0.3,(case['date'],b,err,line)
    phaseerr=abs(illum/10-case['moon'][2]);maxphase=max(maxphase,phaseerr)
    assert phaseerr<1.0,(case['date'],phaseerr)
    sa,sh=map(math.radians,case['sun'][:2]);ma,mh=map(math.radians,case['moon'][:2])
    tx=math.cos(sh)*math.sin(sa-ma)
    ty=math.sin(sh)*math.cos(mh)-math.cos(sh)*math.cos(sa-ma)*math.sin(mh)
    if math.hypot(tx,ty)>0.08: # direction is undefined at exact new/full Moon
        wanted=math.degrees(math.atan2(tx,ty))
        error=abs((tilt*360/65536-wanted+180)%360-180);maxtilt=max(maxtilt,error)
        assert error<2,(case['date'],error)
print(f'PASS: {len(dates)} JPL cases, dates 1900-2200, Gregorian boundaries, poles/dateline')
print(f'Max sky separation: Sun {maxerr[0]:.4f} deg, Moon {maxerr[1]:.4f} deg; '
      f'illumination {maxphase:.3f} percentage points; tilt {maxtilt:.3f} deg')
print(p.stderr.strip())
