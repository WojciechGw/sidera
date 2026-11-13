/* Persistent FIND selection. Never retain pointers into the FIND overlay. */
#include <string.h>
#include "sidera.h"

unsigned target_id=0xFFFFu;
unsigned char target_figure=255;
static char target_name[40];
static int16_t target_vector[3];
static Position target_position;
#ifdef SIDERA_OVERLAYS
#pragma code-name ("HIGH_CODE")
#pragma rodata-name ("HIGH_RODATA")
#endif

void target_center(void)
{
    int zoom;
    if(target_figure!=255) {
        if(!cs_focus(&target_position,&zoom))return;
        gx_zoom((zoom>160?160:zoom)-fov);
        if(zoom>160)strcpy(note,"Figure exceeds FOV 160: part of it remains outside the view.");
    }
    bearing=(as_degrees(target_position.az)+5)/10%360;
    elevation=(int)((int32_t)target_position.alt*90/16384);
}
int target_select(unsigned id,const char *name,const Position *position)
{
    int16_t vector[3];
    if(id>=FIND_COUNT)return 0;
    if(id>=FIND_FIGURE_FIRST) {
        target_figure=(unsigned char)(id-FIND_FIGURE_FIRST);
        target_id=id;strcpy(target_name,name);target_position=*position;return 1;
    }
    if(id>=8&&!assets_vectors(id-8,1,vector))return 0;
    if(id>=8)memcpy(target_vector,vector,sizeof(vector));
    target_figure=255;target_id=id;strcpy(target_name,name);target_position=*position;
    target_center();
    if(id>=8){if(!constellation_mode)constellation_mode=1;}
    else if(id>=2)planets=1;
    return 1;
}
void target_update(void)
{
    StarFrame frame;
    if(target_id==0xFFFFu||target_figure!=255)return;
    if(target_id>=8) {
        star_frame(&moment,&observer,&frame);
        star_position(&frame,target_vector,&target_position);
    } else if(target_id<2)target_position=target_id?sky.moon:sky.sun;
    else if(planets)target_position=planet_positions[target_id-2];
}
static void mark_pixel(int x,int y)
{
    if(x>=VIEW_LEFT&&x<VIEW_RIGHT&&y>=VIEW_TOP&&y<VIEW_BOTTOM)gx_pixel(x,y);
}
void target_draw(void)
{
    int x,y,r,i,lx,ly,w,dx,dy;
    unsigned char size;
    if(target_id==0xFFFFu||target_figure!=255||(target_id>=8&&!constellation_mode)||
       (target_id>=2&&target_id<8&&!planets)||!gx_project(&target_position,&x,&y))return;
    r=target_id<2?19:target_id<8?7:5;
    /* A selected star remains visible in C4 even outside its navigation groups. */
    if(target_id>=8) {
        gx_read(XR_META+target_id-8,&size,1);size&=3;
        for(dy=-1;dy<=1;++dy)for(dx=-1;dx<=1;++dx) {
            if(size==1&&(dx||dy))continue;
            if(size==2&&dx&&dy)continue;
            mark_pixel(x+dx,y+dy);
        }
    }
    for(i=0;i<4;++i) {
        mark_pixel(x-r+i,y-r);mark_pixel(x+r-i,y-r);
        mark_pixel(x-r+i,y+r);mark_pixel(x+r-i,y+r);
        mark_pixel(x-r,y-r+i);mark_pixel(x+r,y-r+i);
        mark_pixel(x-r,y+r-i);mark_pixel(x+r,y+r-i);
    }
    w=(int)strlen(target_name)*6;
    lx=x+r+3;if(lx+w>=VIEW_RIGHT)lx=x-r-w-3;
    if(lx<VIEW_LEFT+1)lx=VIEW_LEFT+1;
    ly=y-4;if(ly<VIEW_TOP+1)ly=VIEW_TOP+1;
    if(ly>VIEW_BOTTOM-10)ly=VIEW_BOTTOM-10;
    gx_label(lx,ly,target_name,1);
}
