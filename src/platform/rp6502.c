/* Direct console and clock access for the standalone RP6502 application. */
#include <time.h>
#include "platform.h"

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
