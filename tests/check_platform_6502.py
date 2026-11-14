"""Exercise timing, USB modifiers, argv storage and RIA key decoding on 65C02."""
from pathlib import Path
import subprocess
import sys

compiler, simulator, destination = sys.argv[1:]
work = Path(destination).resolve()
work.mkdir(parents=True, exist_ok=True)
platform = Path(__file__).resolve().parents[1] / 'src/platform'
(work / 'platform.h').unlink(missing_ok=True)

# The implementation is unchanged; only hardware registers and the clock are fake.
(work / 'rp6502.h').write_text('''#include <stdint.h>
typedef struct { unsigned char ready,rx,tx,vsync; } TestRIA;
extern volatile TestRIA RIA;
int xreg(unsigned device,unsigned channel,unsigned reg,unsigned address);
void xram0_set(unsigned address,unsigned char value,unsigned count);
unsigned char xram0_peek8(unsigned address);
''')
(work / 'runtime.c').write_text(r'''#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include "platform.h"
#include "key.h"
volatile TestRIA RIA;
static unsigned char modifiers,map_fail;
static unsigned mapped=0xFFFFu,clears;
int xreg(unsigned device,unsigned channel,unsigned reg,unsigned address)
{
    assert(!device&&!channel&&!reg);
    assert(address==KEYBOARD_XRAM||address==0xFFFFu);
    if(map_fail&&address!=0xFFFFu)return -1;
    mapped=address;return 0;
}
void xram0_set(unsigned address,unsigned char value,unsigned count)
{
    assert(address==KEYBOARD_XRAM&&!value&&count==32);
    modifiers=0;++clears;
}
unsigned char xram0_peek8(unsigned address)
{
    assert(mapped==KEYBOARD_XRAM&&address==KEYBOARD_XRAM+28);
    return modifiers;
}
static clock_t value,step;
static unsigned calls;
static unsigned char vga;
clock_t test_clock(void)
{
    clock_t result=value;
    value+=step;++calls;
    if(vga&&calls%3==0)++RIA.vsync;
    return result;
}
void *__fastcall__ __argv_mem(size_t size);
int main(void)
{
    unsigned start;void *buffer;
    assert(platform_keyboard_modifiers()==0);
    platform_keyboard_stop();assert(mapped==0xFFFFu);
    map_fail=1;assert(!platform_keyboard_start());
    assert(platform_keyboard_modifiers()==0&&mapped==0xFFFFu);
    map_fail=0;assert(platform_keyboard_start()&&clears==2);
    assert(platform_keyboard_modifiers()==0);
    modifiers=0x01;assert(platform_keyboard_modifiers()==0x01);
    modifiers=0x10;assert(platform_keyboard_modifiers()==0x10);
    platform_keyboard_stop();assert(mapped==0xFFFFu);
    assert(platform_keyboard_modifiers()==0);
    value=CLOCKS_PER_SEC;step=0;assert(platform_ticks()==125);
    /* 524.288 seconds crosses the 16-bit 125 Hz time counter. */
    value=(clock_t)(524280UL*CLOCKS_PER_SEC/1000);
    step=CLOCKS_PER_SEC/100;start=platform_ticks();
    platform_sleep_ticks(5);assert((unsigned)(platform_ticks()-start)>=5);
    assert(platform_ticks()<20);
    value=0;calls=0;step=CLOCKS_PER_SEC/100;vga=0;
    assert(!platform_vga_ready()&&calls>=20&&calls<30);
    calls=0;vga=1;assert(platform_vga_ready()&&calls<10);
    RIA.ready=0;assert(platform_getc_nb()==-1);
    RIA.ready=0x40;RIA.rx=255;assert(platform_getc_nb()==255);
    RIA.ready=0x80;platform_puts("OK");assert(RIA.tx=='K');
    buffer=__argv_mem(512);assert(buffer&&__argv_mem(1)==buffer);
    assert(!__argv_mem(513));
    puts("PASS: USB bitmap mapping/failure/unmapping, clock/wrap, VGA, UART, argv");
    return 0;
}
''')
subprocess.run([compiler, '-t', 'sim65c02', '-O', '-Dclock=test_clock', '-DCLOCKS_PER_SEC=1000UL',
                '-I', str(work), '-I', str(platform), '-o', str(work/'runtime'),
                str(work/'runtime.c'), str(platform/'rp6502.c'), str(platform/'argv.c')], check=True)
subprocess.run([simulator, str(work/'runtime')], cwd=work, check=True, timeout=20)

# Compile the real decoder against a timed byte stream, including incomplete input.
(work/'platform.h').write_text('unsigned platform_ticks(void);\nint platform_getc_nb(void);\n'
                              'unsigned char platform_keyboard_modifiers(void);\n')
(work/'keys.c').write_text(r'''#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "key.h"
static const unsigned char *input;
static unsigned ticks,delay,polls;
static unsigned char modifiers;
unsigned char platform_keyboard_modifiers(void){return modifiers;}
unsigned platform_ticks(void){assert(++polls<100);return ticks++;}
int platform_getc_nb(void)
{
    if(delay){--delay;return -1;}
    return *input ? *input++ : -1;
}
static void check(const char *text,unsigned expected)
{
    input=(const unsigned char *)text;ticks=UINT_MAX-3;polls=delay=0;
    assert(key_poll()==expected);
}
int main(void)
{
    check("",K_NONE);check("q",'q');check("\n",'\r');check("\177",K_BS);
    check("\033",K_ESC);check("\033\033",K_ESC);
    check("\033[A",K_UP);check("\033[B",K_DOWN);
    check("\033[C",K_RIGHT);check("\033[D",K_LEFT);
    check("\033[1;2D",K_LEFT|K_SHIFT);check("\033[1;2C",K_RIGHT|K_SHIFT);
    check("\033[1;5A",K_UP|K_CTRL);check("\033[1;3A",K_UP|K_ALT);
    /* Only physical left Ctrl qualifies horizontal arrows for FOV. */
    check("\033[1;5D",K_LEFT|K_CTRL);
    modifiers=0x01;check("\021",CTRL('Q'));check("\023",CTRL('S'));check("\022",CTRL('R'));check("\006",CTRL('F'));
    check("\033[1;5D",K_LEFT|K_LCTRL);check("\033[1;5C",K_RIGHT|K_LCTRL);
    check("\033[1;6D",K_LEFT|K_LCTRL|K_SHIFT);
    check("\033[1;7C",K_RIGHT|K_LCTRL|K_ALT);
    check("\033[1;5A",K_UP|K_CTRL);check("h",'h');check("H",'H');
    modifiers=0x10;check("\021",CTRL('Q'));check("\023",CTRL('S'));check("\022",CTRL('R'));check("\006",CTRL('F'));
    check("\033[1;5D",K_LEFT|K_CTRL);check("\033[1;5C",K_RIGHT|K_CTRL);
    modifiers=0x11;check("\021",CTRL('Q'));check("\023",CTRL('S'));check("\022",CTRL('R'));check("\006",CTRL('F'));
    check("\033[1;5D",K_LEFT|K_CTRL);check("\033[1;5C",K_RIGHT|K_CTRL);
    modifiers=0;
    check("\033[H",K_HOME);check("\033[F",K_END);
    check("\033[1~",K_HOME);check("\033[4~",K_END);
    check("\033[5~",K_PGUP);check("\033[6~",K_PGDN);
    check("\033[3~",K_DEL);check("\010",K_BS);
    check("\033OA",K_UP);check("\033OP",K_F1);
    check("\033[15~",K_F1+4);check("\0331",K_F1);
    check("\033z",K_ALT|'z');check("\033[",K_NONE);
    check("\033[999~",K_NONE);check("\033[Z",K_NONE);
    /* Two consecutive keys remain separate even across the tick wrap. */
    input=(const unsigned char *)"\033[Dq";ticks=UINT_MAX-2;polls=0;
    assert(key_poll()==K_LEFT&&key_poll()=='q'&&key_poll()==K_NONE);
    /* Empty polling leaves later bytes available. */
    input=(const unsigned char *)"x";delay=1;polls=0;
    assert(key_poll()==K_NONE&&key_poll()=='x');
    puts("PASS: arrows, exclusive LCtrl vs RCtrl/both, editing, ESC timeout/wrap");
    return 0;
}
''')
subprocess.run([compiler, '-t', 'sim65c02', '-O', '-I', str(work), '-I', str(platform),
                '-o', str(work/'keys'), str(work/'keys.c'), str(platform/'key.c')], check=True)
subprocess.run([simulator, str(work/'keys')], cwd=work, check=True, timeout=20)
