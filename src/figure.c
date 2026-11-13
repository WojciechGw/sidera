/* Geometry shared by FIND previews and the one-shot view fit after selection.
 * Angular bins find the shortest azimuth arc, including figures across north. */
#include <string.h>
#include "sidera.h"
#ifdef SIDERA_OVERLAYS
#pragma code-name ("HIGH_CODE")
#pragma rodata-name ("HIGH_RODATA")
#pragma data-name ("HIGH_DATA")
#endif
static struct {
    unsigned char az[45];
    int16_t low,high;
    Position center;
    int zoom;
    StarFrame frame;
    uint16_t first,count,next;
} work={ {0},0,0,{0,0},0,{{0}},0,0,0 };

void figure_reset(void)
{
    memset(work.az,0,sizeof(work.az));work.low=16384;work.high=-16384;
}
void figure_add(const Position *p)
{
    unsigned a=(unsigned)as_degrees(p->az)/10;
    work.az[a/8]|=1u<<(a&7);
    if(p->alt<work.low)work.low=p->alt;
    if(p->alt>work.high)work.high=p->alt;
}
void figure_finish(void)
{
    unsigned a,i,run=0,longest=0,end=0,width;
    int horizontal,vertical;
    /* Two revolutions include a gap straddling 359/0. Four degrees of padding
     * cover binning and whole-degree view rounding, in addition to 8 px margins. */
    for(i=0,a=0;i<720;++i) {
        if(work.az[a/8]&(1u<<(a&7)))run=0;
        else if(++run>longest){longest=run;end=a;}
        if(++a==360)a=0;
    }
    width=longest<360?359-longest:0;
    work.center.az=as_angle((int32_t)((end+1+width/2)%360)*1000L);
    work.center.alt=(int16_t)(((int32_t)work.low+work.high)/2);
    horizontal=(int)(((int32_t)width+4)*600/584)+1;
    vertical=(int)((((int32_t)work.high-work.low)*90/16384+4)*600/316)+1;
    work.zoom=((horizontal>vertical?horizontal:vertical)+9)/10*10;
    if(work.zoom<50)work.zoom=50;
}
void figure_result(Position *p,int *zoom){*p=work.center;*zoom=work.zoom;}
unsigned char figure_side(void){return work.low>=0?1:work.high<0?0:2;}
void figure_begin(unsigned char id,const Date *at)
{
    uint16_t meta[2];
    gx_read(XR_META+760u+(unsigned)id*32u,meta,4);
    work.first=meta[0];work.count=meta[1];work.next=0;
    star_frame(at,&observer,&work.frame);figure_reset();
}
unsigned char figure_step(void)
{
    uint16_t edge[2];int16_t vector[3];Position p;unsigned char i;
    if(work.next>=work.count){figure_finish();return 1;}
    gx_read(XR_EDGES+(work.first+work.next)*4u,edge,4);
    for(i=0;i<2;++i) {
        if(!assets_vectors(edge[i],1,vector))return 2;
        star_position(&work.frame,vector,&p);figure_add(&p);
    }
    ++work.next;return 0;
}
