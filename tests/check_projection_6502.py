"""Exercise actual projection/scaling with 16-bit int, then time cached projection.

Only extract the graphics functions that do not access VGA, as prepare_host.py
does for the editor. Reference coordinates use independent degree arithmetic.
"""
from pathlib import Path
import re
import subprocess
import sys

here = Path(__file__).resolve().parent
work = Path(sys.argv[3]).resolve()
work.mkdir(parents=True, exist_ok=True)
graphics = ((here.parent / 'src') / 'graphics.c').read_text()
header = ((here.parent / 'src') / 'sidera.h').read_text()
source = '#include <assert.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include "astro.h"\n'
source += '\n'.join(re.findall(r'^#define VIEW_.*', header, re.M)) + '\n'
source += 'int bearing,elevation;\n'
source += re.search(r'^int fov=.*', graphics, re.M)[0] + '\n'
source += re.search(r'^static int last_elevation=.*', graphics, re.M)[0] + '\n'
source += re.search(r'^static int last_bearing=.*', graphics, re.M)[0] + '\n'
source += re.search(r'^static uint16_t bearing_angle;', graphics, re.M)[0] + '\n'
source += graphics[graphics.index('int gx_zoom('):graphics.index('static void header_icons(')]
checks = r'''
int main(void) {
    static const int views[]={0,5,180,355};
    int zoom,v,tilt,az,alt,x,y,dx,wantx,wanty;
    long angle;
    Position p;
    assert(fov==120);
    for(zoom=50;zoom<=160;zoom+=10) {
        assert(gx_zoom(zoom-fov));
        for(angle=-32768L;angle<=32767L;angle+=97L)
            assert(project_scale((int16_t)angle)==(int)(angle*gx_circle/65536L));
        assert(project_scale(-1)==0&&project_scale(0)==0&&project_scale(1)==0);
        assert(project_scale(32767)==(int)(32767L*gx_circle/65536L));
        for(v=0;v<4;++v)for(tilt=-54;tilt<=54;tilt+=54) {
            bearing=views[v];elevation=tilt;
            for(az=0;az<360;az+=30)for(alt=-90;alt<=90;alt+=30) {
                p.az=as_angle(az*1000L);p.alt=(int16_t)as_angle(alt*1000L);
                dx=(az-bearing+540)%360-180;
                wantx=VIEW_CENTER+(int)((int32_t)dx*600/zoom);
                wanty=VIEW_MIDDLE+(int)((int32_t)(tilt-alt)*600/zoom);
                gx_project(&p,&x,&y);
                assert(abs(x-wantx)<=2&&abs(y-wanty)<=2);
                assert(abs(azimuth_x(az)-wantx)<=1);
                assert(abs(altitude_y(alt)-wanty)<=1);
                if(zoom==120) {
                    assert(x==VIEW_CENTER+(int)((int32_t)(int16_t)(p.az-as_angle(bearing*1000L))*1800L/65536L));
                    assert(y==VIEW_MIDDLE+elevation*5-(int)((int32_t)p.alt*1800L/65536L));
                }
            }
        }
    }
    assert(!gx_zoom(10)&&fov==160);
    assert(gx_zoom(-110)&&fov==50&&!gx_zoom(-10));
    puts("PASS: 12096 projections on 65C02; 12 FOVs, wrapping, extrema, exact 120-degree regression");
    return 0;
}
'''


def run(name, body):
    path = work / f'{name}.c'
    path.write_text(body)
    subprocess.run([sys.argv[1], '-t', 'sim65c02', '-O', '-I', str((here.parent / 'src')),
                    '-o', str(work / name), str(path), str((here.parent / 'src') / 'astro.c')], check=True)
    result = subprocess.run([sys.argv[2], '-c', str(work / name)], capture_output=True, text=True, check=True)
    return result.stdout + result.stderr


print(run('projection', source + checks).strip())
benchmark = r'''
int main(void) {
    Position p;int x,y;unsigned i;long sum=0;
    bearing=180;elevation=24;p.az=as_angle(175000L);p.alt=(int16_t)as_angle(31000L);
    for(i=0;i<1000;++i){gx_project(&p,&x,&y);sum+=x+y;}
    printf("checksum %ld\n",sum);return 0;
}
'''
old = source[:source.index('int gx_project(')] + r'''
int gx_project(const Position *p,int *x,int *y)
{
    int16_t da=(int16_t)(p->az-as_angle((int32_t)bearing*1000));
    *x=VIEW_CENTER+(int)((int32_t)da*1800L/65536L);
    *y=VIEW_MIDDLE+elevation*5-(int)((int32_t)p->alt*1800L/65536L);
    return *x>=VIEW_LEFT && *x<VIEW_RIGHT && *y>=VIEW_TOP && *y<VIEW_BOTTOM;
}
'''
baseline = run('projection_before', old + benchmark)
current = run('projection_after', source + benchmark)
assert re.search(r'checksum \d+', baseline)[0] == re.search(r'checksum \d+', current)[0]
print('1000 projections, previous fixed FOV:', baseline.strip())
print('1000 projections, variable FOV:', current.strip())
