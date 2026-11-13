#include "solar.h"
#ifdef SIDERA_OVERLAYS
#pragma code-name ("SOLAR_CODE")
#pragma rodata-name ("SOLAR_RODATA")
#pragma bss-name ("SOLAR_BSS")
#endif
const char *const solar_names[8]={"Mercury","Venus","Earth","Mars","Jupiter","Saturn","Uranus","Neptune"};
static const unsigned char widths[]={42,30,30,24,42,36,36,42};
SolarLabel solar_labels[8];
int solar_moon_x,solar_moon_y;
static unsigned char choices[8];
static int left,top,width;

/* Candidate includes the opaque one-pixel padding drawn by gx_label(). */
static unsigned char overlaps(int x,int y,int w,int h)
{
    return left<x+w&&left+width>x&&top<y+h&&top+10>y;
}
static unsigned char clear(unsigned char placed)
{
    unsigned char j;
    if(left<SOLAR_LEFT+2||left+width>SOLAR_RIGHT-2||
       top<SOLAR_TOP+24||top+10>SOLAR_BOTTOM-14)return 0;
    if(overlaps(SOLAR_CX-16,SOLAR_CY-16,32,32))return 0;
    if(overlaps(solar_moon_x-1,solar_moon_y-1,4,4))return 0;
    for(j=0;j<8;++j)
        if(overlaps(solar_labels[j].x-4,solar_labels[j].y-4,8,8))return 0;
    for(j=0;j<placed;++j)
        if(overlaps(solar_labels[j].left-1,solar_labels[j].top-1,widths[j]+2,10))return 0;
    return 1;
}
unsigned char solar_place_labels(void)
{
    unsigned char i=0,k,align,gap=0;
    int x,y,w;
    for(k=0;k<8;++k)choices[k]=0;
    /* Retry earlier choices instead of accepting a collision or dropping a name.
     * Start with centered/edge alignments, then slide along the sprite's sides.
     * All candidates remain directly adjacent; none needs a leader line. */
    while(i<8) {
        x=solar_labels[i].x;y=solar_labels[i].y;w=widths[i];
        k=choices[i]++;
        if(k==12+2*(w+11)+38) {
            choices[i]=0;
            if(!i) {
                if(gap==10)return 0;
                gap+=2;continue;
            }
            --i;continue;
        }
        if(k>=12) {
            k-=12;
            if(k<2*(w+11)) {left=x-w-6+k/2;top=k&1?y+4+gap:y-14-gap;}
            else {k-=2*(w+11);left=k&1?x-w-6-gap:x+4+gap;top=y-14+k/2;}
        } else {
          align=k/4;k&=3;
          if(k<2) {
            left=k?x-w-6-gap:x+4+gap;
            top=y-5+(align==1?-5:align==2?5:0);
          } else {
            left=align==1?x-5:align==2?x+3-w:x-w/2-1;
            top=k==2?y-14-gap:y+4+gap;
          }
        }
        width=w+2;
        if(clear(i)) {
            solar_labels[i].left=left+1;solar_labels[i].top=top+1;++i;
        }
    }
    return 1;
}
