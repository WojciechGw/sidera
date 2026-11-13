"""Run actual 65C02 orbit code against an independent floating-point solution.

Elements: Paul Schlyter, https://stjarnhimlen.se/comp/ppcomp.html#4.
Tests include fractional UTC days, Gregorian century boundaries and all eight
orbits, including their perihelion/aphelion, over the application's range.
"""
import datetime as dt
import math
from pathlib import Path
import subprocess
import sys

compiler, simulator, directory = sys.argv[1:]
work = Path(directory).resolve()
work.mkdir(parents=True, exist_ok=True)
app = (Path(__file__).resolve().parents[1] / 'src')
dates = [dt.datetime(y,m,d,h,n,s) for y,m,d,h,n,s in (
    (1900,1,1,0,0,0), (1900,3,1,23,59,59), (1945,9,1,2,45,0),
    (2000,1,1,12,0,0), (2000,2,29,6,30,0), (2026,11,10,14,47,0),
    (2026,11,11,14,47,0), (2099,12,31,23,59,59), (2100,3,1,0,0,0),
    (2200,12,31,23,59,59))]
# N, i, w, a, e, M: independent decimal source values and daily rates.
elements = [
    ((48.3313,3.24587e-5),(7.0047,5e-8),(29.1241,1.01444e-5),(.387098,0),(.205635,5.59e-10),(168.6562,4.0923344368)),
    ((76.6799,2.46590e-5),(3.3946,2.75e-8),(54.891,1.38374e-5),(.723330,0),(.006773,-1.302e-9),(48.0052,1.6021302244)),
    ((0,0),(0,0),(102.9404,4.70935e-5),(1,0),(.016709,-1.151e-9),(356.047,.9856002585)),
    ((49.5574,2.11081e-5),(1.8497,-1.78e-8),(286.5016,2.92961e-5),(1.523688,0),(.093405,2.516e-9),(18.6021,.5240207766)),
    ((100.4542,2.76854e-5),(1.303,-1.557e-7),(273.8777,1.64505e-5),(5.20256,0),(.048498,4.469e-9),(19.895,.0830853001)),
    ((113.6634,2.38980e-5),(2.4886,-1.081e-7),(339.3939,2.97661e-5),(9.55475,0),(.055546,-9.499e-9),(316.967,.0334442282)),
    ((74.0005,1.3978e-5),(.7733,1.9e-8),(96.6612,3.0565e-5),(19.18171,-1.55e-8),(.047318,7.45e-9),(142.5905,.011725806)),
    ((131.7806,3.0173e-5),(1.77,-2.55e-7),(272.8461,-6.027e-6),(30.05826,3.313e-8),(.008606,2.15e-9),(260.2471,.005995147)),
]
source = '#include <stdio.h>\n#include "solar.h"\nstatic const Date dates[]={\n'
source += ',\n'.join('{'+','.join(map(str,(d.year,d.month,d.day,d.hour,d.minute,d.second)))+'}' for d in dates)
source += '''};
int main(void) {
    unsigned i,j,k;SolarOrbit o;int16_t x,y;
    for(i=0;i<sizeof(dates)/sizeof(dates[0]);++i)for(j=0;j<8;++j) {
        solar_orbit((unsigned char)j,&dates[i],&o);
        for(k=0;k<5;++k) {
            solar_point(&o,k?(uint16_t)((k-1)*16384u):o.phase,&x,&y);
            printf("%u %u %u %d %d\\n",i,j,k,x,y);
        }
    }
    return 0;
}
'''
(work/'check.c').write_text(source)
subprocess.run([compiler,'-t','sim65c02','-O','-I',str(app),'-o',str(work/'check'),
                str(work/'check.c'),str(app/'solar_math.c'),str(app/'astro.c')],check=True)
result = subprocess.run([simulator,'-c',str(work/'check')],capture_output=True,text=True,check=True)
rows = [list(map(int,line.split())) for line in result.stdout.splitlines() if len(line.split())==5]
assert len(rows)==len(dates)*8*5, result.stdout
worst=0
for i,j,k,x,y in rows:
    day=(dates[i]-dt.datetime(1999,12,31)).total_seconds()/86400
    n,inc,w,a,e,m=[b+r*day for b,r in elements[j]]
    n,inc,w,m=map(math.radians,(n,inc,w,m%360))
    angle=m
    for _ in range(12):
        angle-=(angle-e*math.sin(angle)-m)/(1-e*math.cos(angle))
    if k:angle=(k-1)*math.pi/2
    xx=a*(math.cos(angle)-e)
    yy=a*math.sqrt(1-e*e)*math.sin(angle)
    xp=xx*math.cos(w)-yy*math.sin(w)
    yp=xx*math.sin(w)+yy*math.cos(w)
    expected_x=xp*math.cos(n)-yp*math.sin(n)*math.cos(inc)
    expected_y=xp*math.sin(n)+yp*math.cos(n)*math.cos(inc)
    error=math.hypot(x/256-expected_x,y/256-expected_y)
    assert error<.04,(dates[i],j,k,error,(x,y),(expected_x,expected_y))
    worst=max(worst,error)
print(f'PASS: {len(rows)} heliocentric/orbit points on 65C02, worst error {worst:.5f} AU')
