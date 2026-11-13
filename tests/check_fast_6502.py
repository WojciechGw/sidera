"""65C02 atan accuracy, position-only equivalence and reproducible CPU timings.

Optional fourth argument: directory with the pre-change astro.c/astro.h/tables.
"""
from pathlib import Path
import math
import re
import subprocess
import sys

source = (Path(__file__).resolve().parents[1] / 'src')
work = Path(sys.argv[3]).resolve()
work.mkdir(parents=True, exist_ok=True)


def run(name, code, directory=source):
    (work / f'{name}.c').write_text(code)
    subprocess.run([sys.argv[1], '-t', 'sim65c02', '-O', '-I', str(directory),
                    '-o', str(work / name), str(work / f'{name}.c'), str(directory / 'astro.c')], check=True)
    result = subprocess.run([sys.argv[2], '-c', str(work / name)], capture_output=True, text=True, check=True)
    return result.stdout + result.stderr


prefix = '#include <stdio.h>\n#include <assert.h>\n#include <string.h>\n#include "astro.h"\n'
angles = run('angles', prefix + r'''
int main(void) {
    static const int16_t values[]={-32768,-16384,-8192,-1000,-1,0,1,1000,8192,16384,32767};
    unsigned i,j;
    for(i=0;i<11;++i)for(j=0;j<11;++j)
        printf("%d %d %u\n",values[i],values[j],as_atan(values[i],values[j]));
    return 0;
}
''')
worst = 0
for line in angles.splitlines():
    if not re.fullmatch(r'-?\d+ -?\d+ \d+', line):
        continue
    y, x, angle = map(int, line.split())
    error = abs((angle * 360 / 65536 - math.degrees(math.atan2(y, x)) + 180) % 360 - 180)
    worst = max(worst, error)
    assert error < 0.025, (x, y, error)
print(f'PASS: atan quadrants, axes, zero and INT16_MIN; worst error {worst:.5f} deg')

equivalence = run('positions', prefix + r'''
int main(void) {
    Date d;Observer o;Sky a,b;unsigned year,location,i;
    static const int years[]={1900,2000,2026,2100,2200};
    for(year=0;year<5;++year)for(location=0;location<3;++location) {
        d.year=years[year];d.month=6;d.day=21;d.hour=d.minute=d.second=0;
        o.latitude=location==0?1787346L:location==1?-3240000L:3240000L;
        o.longitude=location==0?689597L:location==1?-6480000L:6480000L;o.altitude=395;
        for(i=0;i<97;++i) {
            sky_compute(&d,&o,&a);b.phase=255;b.tilt=123;b.illumination=456;
            sky_positions(&d,&o,&b);
            assert(a.sun.az==b.sun.az&&a.sun.alt==b.sun.alt&&a.moon.az==b.moon.az&&a.moon.alt==b.moon.alt);
            assert(b.phase==255&&b.tilt==123&&b.illumination==456);
            assert(date_step(&d,900));
        }
    }
    puts("PASS: 1455 full/position-only pairs agree exactly, 1900-2200, both poles");return 0;
}
''')
print(equivalence.strip())
benchmark = prefix + r'''
int main(void) {
    Date d={2026,11,17,0,0,0};Observer o={1787346L,689597L,395};Sky s;
    unsigned i;unsigned long sum=0;
    for(i=0;i<97;++i){CALL(&d,&o,&s);sum+=s.sun.az+s.moon.az;assert(date_step(&d,900));}
    printf("97 samples %lu\n",sum);return 0;
}
'''
current = run('trail_fast', benchmark.replace('CALL', 'sky_positions'))
print('Current daily trajectory:', current.strip())
if len(sys.argv) > 4:
    old = run('trail_before', benchmark.replace('CALL', 'sky_compute'), Path(sys.argv[4]).resolve())
    old_cycles = int(re.search(r'(\d+) cycles', old)[1])
    new_cycles = int(re.search(r'(\d+) cycles', current)[1])
    print('Previous daily trajectory:', old.strip())
    print(f'CPU speedup: {old_cycles/new_cycles:.2f}x; cycles reduced by {100*(1-new_cycles/old_cycles):.1f}%')
