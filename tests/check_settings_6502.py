"""Check the real settings parser and disk round trip with 16-bit int on 65C02."""
from pathlib import Path
import subprocess
import sys

root=(Path(__file__).resolve().parents[1] / 'src')
work=Path(sys.argv[3]).resolve();work.mkdir(parents=True,exist_ok=True)
(work/'platform.h').write_text('''#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
int read_xram(unsigned,unsigned,int);
''')
(work/'test.c').write_text(r'''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "sidera.h"
Observer observer={3240000L,-6480000L,10000};Date moment={2200,12,31,23,59,59};
int bearing=359,elevation=-54,fov=160;
unsigned char grid=3,trails=0,planets=0,constellation_mode=4,live=0,playing=1;
char zone_name[40]="Europe/Warsaw";
  void tz_close(void){}
int read_xram(unsigned a,unsigned n,int f){return -1;}
void gx_read(unsigned a,void *p,unsigned char n){}
int gx_zoom(int step){fov+=step;return 1;}
int tz_set(const char *p){if(strcmp(p,"Europe/Warsaw"))return 0;strcpy(zone_name,p);return 1;}
int main(void){int a;unsigned char id;FILE *file;Date utc={2026,11,14,12,34,56};
strcpy(config_path,"settings.cfg");memset(location_name,'X',44);location_name[44]=0;
assert(config_save());bearing=elevation=0;fov=120;grid=0;live=1;
assert(config_load()==1&&bearing==359&&elevation==-54&&fov==160&&grid==3);
assert(!live&&!playing&&constellation_mode==4&&moment.year==2200&&strlen(location_name)==44);
playing=1;assert(config_reload(NULL)==1&&!live&&!playing&&moment.year==2200);
assert(observer.latitude==3240000L&&observer.longitude==-6480000L&&observer.altitude==10000);
for(a=0;a<=9;++a){char v[2];v[0]='0'+a;v[1]=0;
assert(option_value(OPT_GRID,v,0)==(a<=3));assert(option_value(OPT_CONSTELLATIONS,v,0)==(a<=4));}
assert(option_code("VIEW")==OPT_VIEW&&!option_code("unknown"));
assert(option_value(OPT_VIEW,"65,+14",1)&&bearing==65&&elevation==14);
assert(!option_value(OPT_VIEW,"360,0",1)&&bearing==65);
assert(!option_value(OPT_VIEW,"0,-91",1)&&elevation==14);
assert(option_value(OPT_VIEW,"65,90",1)&&elevation==90);
assert(option_value(OPT_VIEW,"65,-90",1)&&elevation==-90);
assert(!option_value(OPT_VIEW,"999999999999,0",1));
assert(!option_value(OPT_FOV,"55",1)&&fov==160);
assert(option_value(OPT_FOV,"50",1)&&fov==50);
assert(option_value(OPT_DATE,"1945-09-01 02:45:00",1)&&!live&&moment.year==1945);
assert(!option_value(OPT_DATE,"1900-02-29 00:00:00",1));
assert(option_value(OPT_PATHS,"ON",1)&&trails);
assert(option_value(OPT_PLANETS,"off",1)&&!planets);
assert(option_value(OPT_LIVE,"",1)&&live);
assert(config_save());live=0;playing=1;assert(config_load()==1&&live&&!playing);
live=0;playing=1;bearing=12;
assert(config_reload(NULL)==-1&&!live&&playing&&bearing==12);
assert(config_reload(&utc)==1&&live&&!playing&&bearing==65&&!memcmp(&moment,&utc,sizeof(utc)));
file=fopen(config_path,"wb");assert(file);fputs("broken",file);fclose(file);
assert(config_load()==-1&&live&&bearing==65);
remove(config_path);puts("PASS: 65C02 settings ranges, signs, full disk round trip and LIVE/PAUSE");return 0;}
''')
subprocess.run([sys.argv[1],'-t','sim65c02','-O','-I',str(work),'-I',str(root),
                '-I',str(root/'platform'),'-o',str(work/'check'),
                str(work/'test.c'),str(root/'files.c'),str(root/'astro.c')],check=True)
result=subprocess.run([sys.argv[2],'-c',str(work/'check')],cwd=work,text=True,capture_output=True,check=True)
print(result.stdout);print(result.stderr)
