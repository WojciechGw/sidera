"""Run the production loader and execute replaced machine code at $C800.

Also inject short reads, invalid assets and I/O failures, with guards around
the window and the configuration scratch area. No host pointer simulation.
"""
from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib

compiler, simulator, folder = sys.argv[1:]
work = Path(folder).resolve()
work.mkdir(parents=True, exist_ok=True)
app = (Path(__file__).resolve().parents[1] / 'src')
cfg = '''SYMBOLS {
 __EXEHDR__: type=import;
 __STACKSIZE__: type=weak, value=$0800;
 _peripherals: type=export, value=$FFC0;
 __MAIN_START__: type=export, value=$0200;
 __MAIN_SIZE__: type=export, value=$F5C0;
}
MEMORY {
 ZP: file="", start=$0000, size=$0100;
 HEADER: file=%O, start=$0000, size=$000C;
 CORE: file=%O, start=$0200, size=$C600;
 ONE: file="%O.one", start=$C800, size=$1700;
 TWO: file="%O.two", start=$C800, size=$1700;
}
SEGMENTS {
 ZEROPAGE: load=ZP, type=zp;
 EXEHDR: load=HEADER, type=ro;
 STARTUP: load=CORE, type=ro;
 LOWCODE: load=CORE, type=ro, optional=yes;
 ONCE: load=CORE, type=ro, optional=yes;
 CODE: load=CORE, type=ro;
 RODATA: load=CORE, type=ro;
 DATA: load=CORE, type=rw;
 BSS: load=CORE, type=bss, define=yes;
 ONE_CODE: load=ONE, type=ro;
 ONE_BSS: load=ONE, type=bss, define=yes;
 TWO_CODE: load=TWO, type=ro;
 TWO_BSS: load=TWO, type=bss, define=yes;
}
FEATURES {
 CONDES: type=constructor, label=__CONSTRUCTOR_TABLE__, count=__CONSTRUCTOR_COUNT__, segment=ONCE;
 CONDES: type=destructor, label=__DESTRUCTOR_TABLE__, count=__DESTRUCTOR_COUNT__, segment=RODATA;
 CONDES: type=interruptor, label=__INTERRUPTOR_TABLE__, count=__INTERRUPTOR_COUNT__, segment=RODATA, import=__CALLIRQ__;
}
'''
(work / 'test.cfg').write_text(cfg)
(work / 'loader.c').write_text('''#include <fcntl.h>
#include <unistd.h>
int test_open(const char *,int);
int test_read(int,void *,unsigned);
int test_close(int);
#define open test_open
#define read test_read
#define close test_close
''' + '#include "overlay.c"\n')
for name, step in [('one', 3), ('two', 7)]:
    (work / f'{name}.c').write_text(f'''#pragma code-name ("{name.upper()}_CODE")
#pragma bss-name ("{name.upper()}_BSS")
static unsigned counter;
unsigned probe_{name}(void) {{ counter+={step}; return counter; }}
''')
(work / 'test.c').write_text(r'''#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "overlay.h"
char note[100];
static unsigned opens,handles,reads;
static unsigned char mode;
unsigned probe_one(void); unsigned probe_two(void);
int test_open(const char *p,int flags) {
 int fd;assert(!strncmp(p,"ROM:",4));++opens;reads=0;
 fd=open(p+4,flags);if(fd>=0)++handles;return fd;
}
int test_read(int fd,void *p,unsigned n) {
 ++reads;if(mode==2&&reads==3)return -1;
 return read(fd,p,n>17?17:n); /* deliberate short successful reads */
}
int test_close(int fd) {
 int r=close(fd);assert(handles);--handles;return mode==3?-1:r;
}
static void guards(void) {
 unsigned i;assert(*(unsigned char *)0xC7FF==0x39);
 assert(*(unsigned char *)0xE000==0x73);
 for(i=0;i<256;++i)assert(((unsigned char *)OVL_SCRATCH)[i]==0xA5);
 assert(!handles);
}
int main(int argc,char **argv) {
 unsigned before;assert(argc==2);
 *(unsigned char *)0xC7FF=0x39;*(unsigned char *)0xE000=0x73;
 memset((void *)OVL_SCRATCH,0xA5,256);
 assert(ovl_select(OVL_INIT)&&probe_one()==3&&probe_one()==6);guards();
 before=opens;assert(ovl_select(OVL_INIT)&&opens==before&&probe_one()==9);
 if(argv[1][0]=='0') {
  assert(ovl_select(OVL_CITIES)&&probe_two()==7);guards();
  before=opens;assert(ovl_select(OVL_CITIES)&&opens==before&&probe_two()==14);
  assert(ovl_select(OVL_INIT)&&probe_one()==3);guards();
  puts("PASS: real 65C02 overlay execution, cache, BSS reset, short reads and scratch guards");
 } else {
  mode=(unsigned char)atoi(argv[1]);
  assert(!ovl_select(OVL_CITIES)&&ovl_failed&&!ovl_loaded(OVL_INIT)&&!ovl_loaded(OVL_CITIES));
  before=opens;assert(!ovl_select(OVL_INIT)&&opens==before);guards();
  assert(strstr(note,"sidera:")&&strstr(note,"cities.ovl"));puts(note);
 }
 return 0;
}
''')
subprocess.run([compiler, '-t', 'sim65c02', '-O', '-DSIDERA_OVERLAYS',
                '-I', str(app / 'overlays'), '-C', str(work / 'test.cfg'),
                '-Ln', str(work / 'test.lbl'), '-o', str(work / 'check'),
                *[str(work / f'{n}.c') for n in ('test', 'loader', 'one', 'two')]], check=True)
labels = {n: int(a, 16) for a, n in re.findall(r'^al ([0-9A-F]+) \.(\S+)$',
                                             (work / 'test.lbl').read_text(), re.M)}

def asset(number, name):
    data = (work / f'check.{name}').read_bytes()
    bss = labels[f'__{name.upper()}_BSS_RUN__'] - 0xC800
    zeros = labels[f'__{name.upper()}_BSS_SIZE__']
    return struct.pack('<4sBBHHH8sI', b'SOV1', number, 1, len(data), bss, zeros,
                       bytes(8), zlib.crc32(data)) + data

(work / 'sidera-init.ovl').write_bytes(asset(1, 'one'))
good = asset(2, 'two')
cases = [('valid', good, '0'), ('missing', None, '1'),
         ('short header', good[:12], '1'), ('short payload', good[:-1], '1'),
         ('extra bytes', good + b'x', '1'), ('read error', good, '2'),
         ('close error', good, '3')]
for name, offset, value in [('magic', 0, 0), ('wrong ID', 4, 4), ('version', 5, 9),
                            ('build', 12, 1), ('checksum', 20, good[20] ^ 1)]:
    bad = bytearray(good)
    bad[offset] = value
    cases.append((name, bad, '1'))
for name, offset, value in [('zero size', 6, 0), ('oversize', 6, 5889),
                            ('BSS overlaps code', 8, 0), ('BSS outside window', 8, 5889),
                            ('BSS overflow', 10, 65535)]:
    bad = bytearray(good)
    struct.pack_into('<H', bad, offset, value)
    cases.append((name, bad, '1'))
for name, content, mode in cases:
    path = work / 'sidera-cities.ovl'
    if content is None:
        path.unlink(missing_ok=True)
    else:
        path.write_bytes(content)
    result = subprocess.run([simulator, '-c', str(work / 'check'), mode], cwd=work,
                            text=True, capture_output=True, timeout=30)
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    print(name + ': ' + result.stdout.strip())
print(f'PASS: {len(cases)} loader scenarios on simulated 65C02')
