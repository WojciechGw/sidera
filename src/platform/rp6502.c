/* Direct console and clock access for the standalone RP6502 application. */
#include <time.h>
#include "platform.h"
#include "key.h"

static unsigned char keyboard_mapped;
unsigned char platform_keyboard_start(void)
{
    xram0_set(KEYBOARD_XRAM,0,32);
    keyboard_mapped=xreg(0,0,0,KEYBOARD_XRAM)>=0;
    return keyboard_mapped;
}
void platform_keyboard_stop(void)
{
    if(keyboard_mapped)xreg(0,0,0,0xFFFFu);
    keyboard_mapped=0;
}
unsigned char platform_keyboard_modifiers(void)
{
    /* USB HID usages E0..E7: LCtrl is bit 0, RCtrl is bit 4. */
    return keyboard_mapped?xram0_peek8(KEYBOARD_XRAM+28):0;
}

unsigned platform_ticks(void)
{
    return (unsigned)(clock() * PLATFORM_TICK_HZ / CLOCKS_PER_SEC);
}
void __fastcall__ platform_sleep_ticks(unsigned ticks)
{
    unsigned start=platform_ticks();
    while((unsigned)(platform_ticks()-start)<ticks) {}
}
void __fastcall__ platform_puts(const char *text)
{
    while(*text) {
        while(!(RIA.ready & 0x80)) {}
        RIA.tx=*text++;
    }
}
int platform_getc_nb(void)
{
    return (RIA.ready & 0x40) ? RIA.rx : -1;
}
unsigned char platform_vga_ready(void)
{
    clock_t start=clock();
    unsigned char v=RIA.vsync;
    while(RIA.vsync==v && (clock_t)(clock()-start)<CLOCKS_PER_SEC/5) {}
    return RIA.vsync!=v;
}
