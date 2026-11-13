#include <fcntl.h>
#include <string.h>
#include "sidera.h"
#include "solar.h"
#ifdef SIDERA_OVERLAYS
#pragma code-name ("SOLAR_CODE")
#pragma rodata-name ("SOLAR_RODATA")
#endif
#if XR_SOLAR_CFG+90u > XR_STARS0 || XR_LABELS+480u > XR_SOLAR_IMAGES || XR_SOLAR_IMAGES+128u > XR_SOLAR_PAL || XR_SOLAR_PAL+1024u > XR_PLANET_PAL
#error Solar panel overlaps XRAM resources
#endif
static int coordinate(int16_t value,unsigned char radius)
{
    int32_t n=(int32_t)value*radius;
    return (int)((n+(n<0?-8192:8192))/16384L);
}
static void circle(int radius)
{
    int x=radius,y=0,error=1-radius;
    while(x>=y) {
        gx_pixel(SOLAR_CX+x,SOLAR_CY+y);gx_pixel(SOLAR_CX-x,SOLAR_CY+y);
        gx_pixel(SOLAR_CX+x,SOLAR_CY-y);gx_pixel(SOLAR_CX-x,SOLAR_CY-y);
        gx_pixel(SOLAR_CX+y,SOLAR_CY+x);gx_pixel(SOLAR_CX-y,SOLAR_CY+x);
        gx_pixel(SOLAR_CX+y,SOLAR_CY-x);gx_pixel(SOLAR_CX-y,SOLAR_CY-x);
        ++y;
        if(error<0)error+=2*y+1;
        else {--x;error+=2*(y-x)+1;}
    }
}
static void word(unsigned char *p,unsigned v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void sprite(unsigned char i,int x,int y,unsigned image,unsigned palette,unsigned char size)
{
    unsigned char cfg[10];
    word(cfg,x);word(cfg+2,y);word(cfg+4,image);word(cfg+6,palette);
    cfg[8]=size;cfg[9]=3;gx_write(XR_SOLAR_CFG+i*10,cfg,10);
}
int solar_draw(void)
{
    /* Index in the six existing sprites; Earth/Neptune use the new asset. */
    static const unsigned char images[]={0,1,6,2,3,4,5,7};
    /* >=12 px between orbit radii separates even diagonally adjacent 8x8 sprites.
     * The first radius also clears the unscaled 32x32 Sun. */
    static const unsigned char radii[]={30,42,54,66,83,100,117,134};
    unsigned char row[SOLAR_ROW_BYTES],i,k;
    unsigned a,src,pal;
    int fd,r,x,y;
    int16_t hx,hy;
    uint16_t longitude;
    SolarOrbit orbit;
    char stamp[48];
    fd=rpmt_open("ROM:sidera-solar.dat",O_RDONLY);
    if(fd<0)goto bad;
    rpmt_os_lock();
    r=read_xram(XR_SOLAR_IMAGES,128,fd)==128&&read_xram(XR_SOLAR_PAL,1024,fd)==1024;
    rpmt_os_unlock();
    if(rpmt_close(fd)<0)r=0;
    if(!r)goto bad;
    solar_hide();
    memset(row,0,sizeof(row));
    for(y=SOLAR_TOP;y<SOLAR_BOTTOM;++y) {
        a=(unsigned)y*80+SOLAR_LEFT/8;
        gx_read(a,row,1);row[0]&=0xFFu<<(8-(SOLAR_LEFT&7));
        gx_read(a+sizeof(row)-1,row+sizeof(row)-1,1);
        row[sizeof(row)-1]&=0xFFu>>(SOLAR_RIGHT&7);
        gx_write(a,row,sizeof(row));
    }
    gx_line(SOLAR_LEFT,SOLAR_TOP,SOLAR_RIGHT-1,SOLAR_TOP,0);
    gx_line(SOLAR_RIGHT-1,SOLAR_TOP,SOLAR_RIGHT-1,SOLAR_BOTTOM-1,0);
    gx_line(SOLAR_RIGHT-1,SOLAR_BOTTOM-1,SOLAR_LEFT,SOLAR_BOTTOM-1,0);
    gx_line(SOLAR_LEFT,SOLAR_BOTTOM-1,SOLAR_LEFT,SOLAR_TOP,0);
    gx_label(SOLAR_LEFT+2,SOLAR_TOP+12," SOLAR SYSTEM ",1);
    strcpy(stamp,"snapshot ");
    date_format(stamp+strlen(stamp),&solar_date);strcat(stamp," UTC");
    gx_text(SOLAR_RIGHT-2-(int)strlen(stamp)*6,SOLAR_TOP+4,stamp);
    gx_text(SOLAR_LEFT+2,SOLAR_BOTTOM-11,"Schematic view (not in AU scale)");
    gx_text(SOLAR_RIGHT-2-8*6,SOLAR_BOTTOM-11,"X: close");
    sprite(0,SOLAR_CX-16,SOLAR_CY-16,XR_SUN,XR_PAL,0x77);
    for(i=8;i>0;) {
        --i;solar_orbit(i,&solar_date,&orbit);circle(radii[i]);
        solar_point(&orbit,orbit.phase,&hx,&hy);
        longitude=as_atan(hy,hx);
        x=SOLAR_CX+coordinate(as_cos(longitude),radii[i]);
        y=SOLAR_CY-coordinate(as_sin(longitude),radii[i]);k=images[i];
        solar_labels[i].x=x;solar_labels[i].y=y;
        src=k<6?XR_PLANETS+(unsigned)k*64:XR_SOLAR_IMAGES+(unsigned)(k-6)*64;
        pal=k<6?XR_PLANET_PAL+(unsigned)k*512:XR_SOLAR_PAL+(unsigned)(k-6)*512;
        sprite(8-i,x-4,y-4,src,pal,0x11);
        rpmt_yield();
    }
    longitude=moon_longitude(&solar_date);
    solar_moon_x=solar_labels[2].x+coordinate(as_cos(longitude),8)-1;
    solar_moon_y=solar_labels[2].y-coordinate(as_sin(longitude),8)-1;
    if(!solar_place_labels()) {
        strcpy(note,"Cannot place planet names.");return 0;
    }
    for(i=0;i<8;++i)gx_label(solar_labels[i].left,solar_labels[i].top,solar_names[i],0);
    gx_dot(solar_moon_x,solar_moon_y);
    solar_visible=1;return 1;
bad:
    strcpy(note,"Cannot read ROM:sidera-solar.dat.");return 0;
}
