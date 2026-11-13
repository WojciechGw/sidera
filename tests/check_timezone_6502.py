"""Run timezone conversion on the actual 65C02 using sim65 host-file I/O."""
import datetime as dt
import io
import struct
import subprocess
import sys
import zipfile
from pathlib import Path
from zoneinfo import ZoneInfo

root=(Path(__file__).resolve().parents[1] / 'src')
work=Path(sys.argv[3]).resolve();work.mkdir(parents=True,exist_ok=True)
blob=(root/'rom/sidera.tz').read_bytes()
count=struct.unpack_from('<H',blob,8)[0]
names=[blob[24+i*48:64+i*48].split(b'\0')[0].decode() for i in range(count)]
cases=[]
selected=('Europe/Warsaw','America/New_York','Pacific/Kiritimati','Asia/Kathmandu','Australia/Lord_Howe','Etc/UTC')
# This sim65 build truncates host lseek offsets to 16 bits. Keep the same
# index IDs and selected interval tables, repacked below 64 KiB for this test.
# The host test covers every table at its full shipped file offset.
compact=bytearray(blob[:24+count*48])
for name in selected:
    i=names.index(name);pos,n=struct.unpack_from('<IH',blob,24+i*48+40)
    struct.pack_into('<I',compact,24+i*48+40,len(compact))
    compact.extend(blob[pos:pos+n*12])
assert len(compact)<65536
(work/'sidera.tz').write_bytes(compact)
with zipfile.ZipFile(root/'data/tzdata-2026.6-py2.py3-none-any.whl') as archive:
    for name in selected:
        zone=ZoneInfo.from_file(io.BytesIO(archive.read('tzdata/zoneinfo/'+name)))
        for stamp in ('1900-01-01 00:00:00','1915-08-04 23:59:59','1915-08-05 00:00:00',
                      '2026-03-29 00:59:59','2026-03-29 01:00:00','2026-10-25 00:59:59',
                      '2026-10-25 01:00:00','2100-02-28 23:59:59','2200-12-31 23:59:59'):
            utc=dt.datetime.fromisoformat(stamp).replace(tzinfo=dt.timezone.utc)
            local=utc.astimezone(zone)
            cases.append((names.index(name),utc,local,int(local.utcoffset().total_seconds())))
(work/'platform.h').write_text('#include <stdint.h>\n#include <fcntl.h>\n#include <unistd.h>\n')
source='''#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include "sidera.h"
char asset_path[160]="sidera.dat";

typedef struct {unsigned id;Date utc,local;long offset;} Case;
static const Case cases[]={
'''
def fields(d):return '{'+','.join(map(str,(d.year,d.month,d.day,d.hour,d.minute,d.second)))+'}'
source+=',\n'.join('{'+f'{i},{fields(u)},{fields(l)},{o}L'+'}' for i,u,l,o in cases)
source+='''};
int main(void){unsigned i;Date d;long off;
for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i){
if(!tz_select(cases[i].id)){printf("select failed: case %u, zone %u\\n",i,cases[i].id);return 1;}
if(!tz_local(&cases[i].utc,&d,&off)){printf("local failed: case %u\\n",i);return 1;}
assert(off==cases[i].offset);
assert(d.year==cases[i].local.year&&d.month==cases[i].local.month&&d.day==cases[i].local.day);
assert(d.hour==cases[i].local.hour&&d.minute==cases[i].local.minute&&d.second==cases[i].local.second);
}tz_close();puts("PASS: 54 timezone conversions on actual 65C02");return 0;}
'''
(work/'test.c').write_text(source)
subprocess.run([sys.argv[1],'-t','sim65c02','-O','-I',str(work),'-I',str(root),'-I',str(root/'platform'),
               '-o',str(work/'check'),str(work/'test.c'),str(root/'timezone.c'),str(root/'astro.c')],check=True)
subprocess.run([sys.argv[2],str(work/'check')],cwd=work,check=True)
