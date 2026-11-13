/* Temporary 320x240 canvas; all pixels and fade palettes live in XRAM. */
#include "sidera.h"

#ifdef SIDERA_OVERLAYS
#pragma code-name ("COVER_CODE")
#pragma rodata-name ("COVER_RODATA")
#endif
void show_cover(void)
{
    static const uint16_t config[]={0,0,30,320,180,0x1000,0x0400};
    unsigned start,elapsed,wait,palette=0x0400;
    unsigned char level,last=0,v;
    int r;
#ifndef RPMT_BARE
    unsigned char gen=rpmt_fg_gen();
#endif
    /* Last palette also checks that the optional tail is complete. */
    if(!asset_read(DATA_COVER,0x1000,28800)||
       !asset_read(DATA_COVER+28800L,0x8080,28800)||
       !asset_read(DATA_COVER_PAL+16384L,0x0400,512)||
       !asset_read(DATA_COVER_PAL,0x0400,512))return;
#ifndef RPMT_BARE
    if(gen!=rpmt_fg_gen()||rpmt_sigterm())return;
#endif
    gx_write(0x0200,config,14);
    rpmt_os_lock();r=xreg(1,0,0,1);
    if(r>=0)r=xreg(1,0,1,3,3,0x0200,0);
    rpmt_os_unlock();
    start=rpmt_ticks();
    while(r>=0) {
#ifndef RPMT_BARE
        if(gen!=rpmt_fg_gen()||rpmt_sigterm())break;
#endif
        while(rpmt_getc_nb()>=0){} /* discard input, never skip or defer it */
        elapsed=(unsigned)(rpmt_ticks()-start);
        /* 125 Hz OS ticks: fade in 0..125, hold to 500, fade out to 625. */
        level=elapsed>=625?0:elapsed>=500?32-(elapsed-500)*32/125:elapsed>=125?32:elapsed*32/125;
        if(level!=last) {
            palette^=0x0200;
            if(!asset_read(DATA_COVER_PAL+(int32_t)level*512,palette,512))break;
            v=RIA.vsync;wait=rpmt_ticks();
            while(v==RIA.vsync&&(unsigned)(rpmt_ticks()-wait)<4)rpmt_yield();
#ifndef RPMT_BARE
            if(gen!=rpmt_fg_gen()||rpmt_sigterm())break;
#endif
            gx_write(0x020C,&palette,2);last=level;
        }
        if(elapsed>=625)break;
        rpmt_sleep_ticks(1);
    }
    while(rpmt_getc_nb()>=0){}
    rpmt_os_lock();xreg(1,0,0,3);rpmt_os_unlock();
}
