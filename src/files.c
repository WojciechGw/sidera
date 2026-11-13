#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sidera.h"
#include "bare/overlay.h"

char config_path[160] = "MSC0:/sidera.cfg";
char asset_path[160];
char location_name[45] = "Warsaw [PL]"; /* Catalogue name (39), space, [CC], terminator. */
int asset_fd = -1; /* shared with the INIT translation unit */
static unsigned vector_first=SIDERA_STAR_COUNT,vector_count;
#ifdef SIDERA_OVERLAYS
#define buffer ((char *)OVL_SCRATCH)
#else
static char buffer[224];
#endif
#define CONFIG_BYTES 224u /* includes UTF-8 DMS; shared with the resident save path */

#ifndef SIDERA_OVERLAYS
#include "bare/init_files.inc"
#endif
static char *coord_out(char *p,int32_t v,char pos,char neg,unsigned char utf8)
{
    int d,m,s,t;
    char c=pos;
    if(v<0){v=-v;c=neg;}
    d=(int)(v/36000L);v%=36000L;m=(int)(v/600);v%=600;
    s=(int)(v/10);t=(int)(v%10);
    sprintf(p,"%02d%s%02d%s%02d.%d%s %c",d,utf8?"\xC2\xB0":"\x01",m,
        utf8?"\xE2\x80\xB2":"\x02",s,t,utf8?"\xE2\x80\xB3":"\x03",c);
    return p+strlen(p);
}
void location_format(char *text,const Observer *o,unsigned char utf8)
{
    char *p=coord_out(text,o->latitude,'N','S',utf8);
    strcpy(p,", ");p+=2;p=coord_out(p,o->longitude,'E','W',utf8);
    sprintf(p,", %d m AMSL",o->altitude);
}
void date_format(char *text,const Date *d)
{
    sprintf(text,"%04d-%02u-%02u %02u:%02u:%02u",d->year,(unsigned)d->month,
        (unsigned)d->day,(unsigned)d->hour,(unsigned)d->minute,(unsigned)d->second);
}
int config_save(void)
{
    int fd,n,r;
    location_format(buffer,&observer,1);
    strcat(buffer,"\r\nZone ");strcat(buffer,zone_name);
    strcat(buffer,"\r\nCity ");strcat(buffer,location_name);
    sprintf(buffer+strlen(buffer),"\r\nView %d,%d %d %u %s %s %u\r\nTime ",
        bearing,elevation,fov,(unsigned)grid,trails?"on":"off",planets?"on":"off",(unsigned)constellation_mode);
    if(live)strcat(buffer,"LIVE");else date_format(buffer+strlen(buffer),&moment);
    strcat(buffer,"\r\n");n=(int)strlen(buffer);
    fd=rpmt_open(config_path,O_WRONLY|O_CREAT|O_TRUNC);if(fd<0)return 0;
    r=write(fd,buffer,n);if(rpmt_close(fd)<0)return 0;return r==n;
}
void assets_close(void) {if(asset_fd>=0)rpmt_close(asset_fd);asset_fd=-1;}
int asset_read(int32_t offset,unsigned addr,unsigned count)
{
    int n;
    long off;
    rpmt_os_lock();off=lseek(asset_fd,offset,SEEK_SET);
    n=off==offset?read_xram(addr,count,asset_fd):-1;
    rpmt_os_unlock();return n==(int)count;
}
#ifdef SIDERA_OVERLAYS
#pragma code-name ("COVER_CODE")
#endif
int assets_base(void)
{
    assets_forget_vectors();
    return asset_read(8,XR_FONT,1024) && asset_read(1032,XR_PAL,512) &&
        asset_read(1544,XR_SUN,1024) &&
        asset_read(DATA_PLANETS,XR_PLANETS,PLANET_COUNT*PLANET_BYTES) &&
        asset_read(DATA_PLANETS+PLANET_COUNT*64L,XR_PLANET_PAL,PLANET_COUNT*512u) &&
        asset_read(DATA_EDGES,XR_EDGES,CATALOGUE_RECORD_BYTES) && asset_read(DATA_META,XR_META,3576);
}
#ifdef SIDERA_OVERLAYS
#pragma code-name ("CODE")
#endif
int assets_phase(unsigned char phase,unsigned target)
{
    return phase<=128 && asset_read(DATA_PHASES+(int32_t)phase*2048,target,2048);
}
int assets_vectors(unsigned first,unsigned char count,void *vectors)
{
    if(!count||count>4||first>=SIDERA_STAR_COUNT||count>SIDERA_STAR_COUNT-first)return 0;
    if(first<vector_first||first+count>vector_first+vector_count) {
        vector_first=SIDERA_STAR_COUNT;vector_count=SIDERA_STAR_COUNT-first<64?SIDERA_STAR_COUNT-first:64;
        if(!asset_read(DATA_VECTORS+(long)first*6,XR_VECTORS,vector_count*6))return 0;
        vector_first=first;
    }
    gx_read(XR_VECTORS+(first-vector_first)*6,vectors,count*6);return 1;
}
void assets_forget_vectors(void) { vector_first=SIDERA_STAR_COUNT;vector_count=0; }
