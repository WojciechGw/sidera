/* Resident panel state and sprite bounds; orbital code lives in SOLAR. */
#include "sidera.h"
#include "solar.h"
unsigned char solar_visible;
Date solar_date;
unsigned char solar_hit(int x,int y,int w,int h)
{
    return solar_visible&&!cities_active&&x<SOLAR_RIGHT&&x+w>SOLAR_LEFT&&
           y<SOLAR_BOTTOM&&y+h>SOLAR_TOP;
}
void solar_hide(void)
{
    unsigned char i;
    static const unsigned char hidden[]={0xC0,0xFF};
    solar_visible=0;
    for(i=0;i<9;++i)gx_write(XR_SOLAR_CFG+i*10,hidden,2);
}
