/* Offline CITIES locator. Map rows are pre-shifted for screen x=334. */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sidera.h"
#ifdef SIDERA_OVERLAYS
#pragma code-name ("HIGH_CODE")
#pragma rodata-name ("HIGH_RODATA")
#pragma bss-name ("CITIES_BSS")
#pragma data-name ("CITIES_DATA")
#endif
static int map_fd=-1;
static unsigned char buffer[117];
static unsigned char want,map_row,dirty,point,my;
static unsigned mx;

static void marker(unsigned x,unsigned y)
{
    unsigned char pos[4];
    pos[0]=x;pos[1]=x>>8;pos[2]=y;pos[3]=y>>8;
    gx_write(XR_CITY_CFG,pos,4);
}

void world_close(void)
{
    if(map_fd>=0){marker(0xFFC0u,0);close(map_fd);}map_fd=-1;
}
static void unavailable(void)
{
    world_close();gx_text(334,77,"Map unavailable");
}
void world_begin(void)
{
    /* A 5x5 plus, one pixel thick, in a transparent 8x8 1bpp sprite. */
    static const unsigned char image[12]={32,32,248,32,32,0,0,0,0,0,63,0};
    world_close();dirty=point=0;want=255;map_row=150;
    gx_write(XR_CITY_IMAGE,image,sizeof(image));
    map_fd=open("ROM:sidera.map",O_RDONLY);
    if(map_fd<0||read(map_fd,buffer,16)!=16||memcmp(buffer,"SIDMAP1",8)||
       buffer[8]!=44||buffer[9]!=1||buffer[10]!=150||buffer[11]||
       lseek(map_fd,0,SEEK_END)!=91816L)unavailable();
}
void world_select(unsigned char continent,const Observer *place)
{
    if(map_fd<0)return;
    point=place!=0;dirty=1;
    if(point) {
        mx=(unsigned)(((place->longitude/100+64800L)*299+64800L)/129600L);
        my=(unsigned char)(((32400L-place->latitude/100)*149+32400L)/64800L);
        /* Older external SIDCITY2 catalogues have no region tag. */
        if(!continent||continent>7) {
            if(lseek(map_fd,46816L+(long)my*300+mx,SEEK_SET)<0||read(map_fd,&continent,1)!=1)
                {unavailable();return;}
        }
    } else continent=0;
    if(want!=continent){want=continent;map_row=0;marker(0xFFC0u,0);}
}
unsigned char cities_step(void)
{
    unsigned a;unsigned char r,b;
    if(map_fd<0)return 0;
    if(map_row<150) {
        if(lseek(map_fd,16L+(long)want*5850+(unsigned)map_row*39u,SEEK_SET)<0||
           read(map_fd,buffer,117)!=117){unavailable();return 0;}
        a=(77u+map_row)*80u+41;
        for(r=0;r<3;++r,a+=80) {
            gx_read(a,&b,1);buffer[r*39]=(buffer[r*39]&3)|(b&252);
            gx_read(a+38,&b,1);buffer[r*39+38]=(buffer[r*39+38]&192)|(b&63);
            gx_write(a,buffer+r*39,39);
        }
        map_row+=3;if(map_row<150)return 1;
    }
    if(dirty){marker(point?332u+mx:0xFFC0u,75u+my);dirty=0;}
    return 0;
}
