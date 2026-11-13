/* Execute the real splash with a deterministic RIA clock and asset file. */
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sidera.h"
#include "../src/cover.c"

HostRIA RIA;
uint8_t xram[65536];
static FILE *asset;
static const char *preview;
static unsigned ticks,base,enabled,blanked,queued,changed,started,ended,full,fade;
static unsigned char generation;
static int locked,scenario,last_level,pending_level;
static unsigned word(unsigned at){return xram[at]|((unsigned)xram[at+1]<<8);}
void rpmt_os_lock(void){assert(!locked);locked=1;}
void rpmt_os_unlock(void){assert(locked);locked=0;}
unsigned rpmt_ticks(void){return ticks;}
void rpmt_yield(void){++ticks;if(scenario!=4)++RIA.vsync;}
void rpmt_sleep_ticks(unsigned n){while(n--)rpmt_yield();}
int rpmt_getc_nb(void){if(queued){--queued;return 'Q';}return -1;}
unsigned char rpmt_fg_gen(void){return generation||(scenario==2&&ticks-base>=200);}
unsigned char rpmt_sigterm(void){return scenario==3&&ticks-base>=200;}
int asset_read(int32_t off,unsigned addr,unsigned n)
{
    assert(!locked&&addr+n<=0xF100);
    if(scenario==1)return 0;
    if(scenario==5&&off==DATA_COVER_PAL+16384L)return 0;
    if(scenario==6&&off==DATA_COVER_PAL+17*512L)return 0;
    assert(!fseek(asset,off,SEEK_SET));assert(fread(xram+addr,1,n,asset)==n);
    if(off>=DATA_COVER_PAL)pending_level=(int)((off-DATA_COVER_PAL)/512);
    if(scenario==7)generation=1; /* switched task while loading, before VGA setup */
    return 1;
}
void gx_write(unsigned addr,const void *p,unsigned char n)
{
    assert(!locked);assert((addr==0x200&&n==14)||(addr==0x20C&&n==2));
    memcpy(xram+addr,p,n);
    if(addr==0x20C) {
        assert(enabled);++changed;
        assert(word(addr)==0x400||word(addr)==0x600);
        if(!full)assert(pending_level>=last_level);
        else assert(pending_level<=last_level);
        last_level=pending_level;
        if(last_level==32) {
            full=ticks-started;
            if(!scenario&&preview) {
                FILE *f=fopen(preview,"wb");assert(f);
                assert(fwrite(xram,1,sizeof(xram),f)==sizeof(xram));fclose(f);
            }
        }
        if(full&&last_level<32&&!fade)fade=ticks-started;
    }
}
int xreg(int a,int b,int c,...)
{
    va_list ap;int value;unsigned i;
    assert(locked&&a==1&&b==0);va_start(ap,c);value=va_arg(ap,int);
    if(c==0) {
        if(value==1){enabled=1;started=ticks;}
        else {assert(value==3);blanked=1;ended=ticks;}
    } else {
        assert(value==3&&va_arg(ap,int)==3&&va_arg(ap,int)==0x200&&va_arg(ap,int)==0);
        assert(word(0x200)==0&&word(0x202)==0&&word(0x204)==30);
        assert(word(0x206)==320&&word(0x208)==180&&word(0x20A)==0x1000);
        for(i=0;i<256;++i)assert(word(0x400+i*2)==32); /* opaque black before display */
    }
    va_end(ap);return 0;
}
int main(int argc,char **argv)
{
    
    assert(argc==2||argc==3);if(argc==3)preview=argv[2];
    asset=fopen(argv[1],"rb");assert(asset);
    for(scenario=0;scenario<9;++scenario) {
        memset(xram,0xA5,sizeof(xram));memset(&RIA,0,sizeof(RIA));
        ticks=base=scenario==8?UINT_MAX-80:0;
        enabled=blanked=changed=started=ended=full=fade=generation=0;
        queued=4;last_level=0;show_cover();assert(!locked);
        if(scenario==1||scenario==5||scenario==7){assert(!enabled&&!blanked);continue;}
        assert(enabled&&blanked&&!queued);
        if(scenario==2||scenario==3)assert(ended-base>=200&&ended-base<=202);
        else if(scenario==6)assert(!full&&ended-started<125);
        else {
            fprintf(stderr,"splash case %d: full=%u fade=%u end=%u frames=%u\n",scenario,full,fade,ended-started,changed);
            assert(last_level==0&&(scenario==4?changed>32&&changed<=64:changed==64));
            assert(full>=125&&full<=(scenario==4?135:129)&&fade>=500&&fade<=508);
            assert(ended-started>=625&&ended-started<=(scenario==4?635:629));
        }
    }
    fclose(asset);
    puts("PASS: splash 1/3/1 timing, black endpoints, palette-only fade, discarded keys, task switch, SIGTERM, missing/truncated data, stopped VSYNC and tick wrap");
    return 0;
}
