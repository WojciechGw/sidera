"""Run civil-day events, cache invalidation and completion on real 65C02 code."""
from pathlib import Path
import struct
import subprocess
import sys

root=(Path(__file__).resolve().parents[1] / 'src')
work=Path(sys.argv[3]).resolve();work.mkdir(parents=True,exist_ok=True)
blob=(root/'rom/sidera.tz').read_bytes()
count=struct.unpack_from('<H',blob,8)[0]
compact=bytearray(blob[:24+count*48])
for i in range(count):
    name=blob[24+i*48:64+i*48].split(b'\0')[0]
    if name not in (b'Europe/Warsaw',b'Etc/UTC'):continue
    pos,n=struct.unpack_from('<IH',blob,24+i*48+40)
    struct.pack_into('<I',compact,24+i*48+40,len(compact))
    compact.extend(blob[pos:pos+n*12])
assert len(compact)<65536
(work/'sidera.tz').write_bytes(compact)
(work/'rpmt.h').write_text('#include <stdint.h>\nint rpmt_open(const char *,int);\nint rpmt_close(int);\nvoid rpmt_os_lock(void);\nvoid rpmt_os_unlock(void);\n')
(work/'test.c').write_text('''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "sidera.h"
Date moment={2026,11,10,12,0,0};Observer observer={1880272L,756424L,0};
char asset_path[160]="sidera.dat";
void rpmt_os_lock(void){} void rpmt_os_unlock(void){}
int rpmt_open(const char *p,int f){return open(p,f);} int rpmt_close(int f){return close(f);}
int main(void){unsigned steps=0;char text[40];
assert(tz_set("Europe/Warsaw"));assert(rs_sync());
rs_text(0,text);assert(!strcmp(text,"Rise ..:.. Set ..:.."));
while(rs_busy()){rs_step();assert(++steps<2000);}
rs_text(0,text);assert(!strcmp(text,"Rise 06:47 Set 15:51"));puts(text);
rs_text(1,text);assert(!strcmp(text,"Rise 08:29 Set 15:43"));puts(text);
assert(!rs_sync());assert(date_step(&moment,60));assert(!rs_sync());
assert(date_step(&moment,86400L));assert(rs_sync()&&rs_busy());
rs_step();observer.longitude+=36000L;assert(rs_sync());
assert(tz_set("Etc/UTC"));assert(rs_sync());
rs_text(1,text);assert(!strcmp(text,"Rise ..:.. Set ..:.."));
tz_close();printf("PASS: 65C02 rises/sets, %u batches; same-day reuse, date/location/zone cancellation\\n",steps);
return 0;}
''')
subprocess.run([sys.argv[1],'-t','sim65c02','-O','-I',str(work),'-I',str(root),'-I',str(root/'common'/'rpc_common'),
               '-o',str(work/'check'),str(work/'test.c'),str(root/'riseset.c'),str(root/'timezone.c'),str(root/'astro.c')],check=True)
result=subprocess.run([sys.argv[2],'-c',str(work/'check')],cwd=work,text=True,capture_output=True,check=True)
print(result.stdout);print(result.stderr)
