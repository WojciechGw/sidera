#include <stdlib.h>
#include <string.h>
#include "sidera.h"

#define SIDERA_EXTERNAL_STARS
#define SIDERA_EXTERNAL_METADATA
typedef struct { uint16_t first,count; char label[28]; } Constellation;
#include "tables/constellations.h"
#if STAR_COUNT != SIDERA_STAR_COUNT
#error Catalogue counts disagree
#endif
static Constellation figure;
static void load_figure(unsigned char i) { gx_read(XR_META+760u+(unsigned)i*32u,&figure,32); }
#if XR_STARS0+STAR_COUNT*4u > XR_STARS1 || XR_STARS1+STAR_COUNT*4u > XR_PLANETS
#error Star buffers overlap XRAM assets
#endif
#if XR_STARS1+STAR_COUNT*4u > XR_VECTORS || XR_VECTORS+64u*6u > XR_TRAILS || XR_TRAILS+TRAIL_COUNT*8u > XR_PLANETS
#error Catalogue read buffer overlaps XRAM assets
#endif

unsigned char constellation_mode=0;
static unsigned positions=XR_STARS0;
static StarFrame frame;
static unsigned next_star=STAR_COUNT;
static unsigned char active=0,ready=0;
static unsigned char cache_valid;
static Date computed_date;
static Observer computed_observer;
typedef struct { int left,top,right; } Box;
typedef struct { int16_t x,y; unsigned char w; } Label;
typedef struct { int x,y; } Marker;
#define NAMED_COUNT 8
static const unsigned named_stars[NAMED_COUNT]={703,702,727,155,156,231,230,758};
static const char *const star_names[NAMED_COUNT]={"Merak","Dubhe","Polaris",
    "Alpha Centauri","Beta Centauri","Gacrux","Acrux","Sigma Octantis"};
/* Alpha Centauri uses the catalogue's HIP 71681 (component B). */
static Marker markers[NAMED_COUNT];
#define LABEL_COUNT (NAMED_COUNT+CONSTELLATION_COUNT)
#if XR_LABELS < XR_META+3576u || XR_LABELS+LABEL_COUNT*5u > XR_PLANET_PAL
#error Label cache overlaps XRAM assets
#endif
static Box box; /* Edges are processed one constellation at a time. */
static unsigned char labels_valid;
static Label candidate;
static Label label; /* Shared XRAM transfer record; drawing is non-reentrant. */
static int bottom;
static const char catalogue_error[]="Cannot read star catalogue.";

void cs_cycle(void) { if(++constellation_mode==5)constellation_mode=0; }
unsigned char cs_busy(void) { return next_star<STAR_COUNT; }
unsigned char cs_current(void)
{
    int32_t days,seconds;
    if(!cache_valid||observer.latitude!=computed_observer.latitude||
       observer.longitude!=computed_observer.longitude||observer.altitude!=computed_observer.altitude)return 0;
    days=date_day(&moment)-date_day(&computed_date);
    if(days< -1||days>1)return 0; /* Avoid overflow across the full 1900-2200 range. */
    seconds=days*86400L+((int32_t)moment.hour-computed_date.hour)*3600L+
        ((int)moment.minute-computed_date.minute)*60L+(int)moment.second-computed_date.second;
    return live?(seconds>=0&&seconds<300):seconds==0;
}
void cs_begin(void)
{
    assets_forget_vectors();
    computed_date=moment;computed_observer=observer;cache_valid=1;
    star_frame(&moment,&observer,&frame);
    next_star=0;
}
unsigned char cs_step(void)
{
    unsigned char n;
    unsigned char count;
    int16_t vectors[4][3];
    Position p;
    if(!cs_busy())return 0;
    count=STAR_COUNT-next_star<4?(unsigned char)(STAR_COUNT-next_star):4;
    if(!assets_vectors(next_star,count,vectors)) {
        strcpy(note,catalogue_error);cache_valid=0;next_star=STAR_COUNT;return 1;
    }
    /* Bound each batch so the application can poll keys and update its clock.
     * Keep the displayed catalogue intact until the replacement is complete. */
    for(n=0;n<count;++n,++next_star) {
        star_position(&frame,vectors[n],&p);
        gx_write((active?XR_STARS0:XR_STARS1)+next_star*4u,&p,4);
    }
    if(cs_busy())return 0;
    if(!strcmp(note,catalogue_error))note[0]=0;
    active^=1;positions=active?XR_STARS1:XR_STARS0;ready=1;return 1;
}
static int project_star(unsigned i,int *x,int *y)
{
    Position p;
    gx_read(positions+i*4u,&p,4);return gx_project(&p,x,y);
}
static unsigned char outside(int x,int y)
{
    return (x<VIEW_LEFT?1:x>=VIEW_RIGHT?2:0)|(y<VIEW_TOP?4:y>bottom?8:0);
}
/* Both ends may be outside: retain a segment crossing the visible window.
 * The horizon is a reference line, not a clipping boundary. */
static int clip(int *x0,int *y0,int *x1,int *y1)
{
    unsigned char a,b,c;
    int x,y;
    for(;;) {
        a=outside(*x0,*y0);b=outside(*x1,*y1);
        if(!(a|b))return 1;
        if(a&b)return 0;
        c=a?a:b;
        if(c&12) {
            y=c&4?VIEW_TOP:bottom;
            x=*x0+(int)((int32_t)(*x1-*x0)*(y-*y0)/(*y1-*y0));
        } else {
            x=c&1?VIEW_LEFT:VIEW_RIGHT-1;
            y=*y0+(int)((int32_t)(*y1-*y0)*(x-*x0)/(*x1-*x0));
        }
        if(a){*x0=x;*y0=y;}else{*x1=x;*y1=y;}
    }
}
static void bounds(int x,int y)
{
    if(x<box.left)box.left=x;if(x>box.right)box.right=x;
    if(y<box.top)box.top=y;
}
static void segment(int x0,int y0,int x1,int y1)
{
    if(!clip(&x0,&y0,&x1,&y1))return;
    bounds(x0,y0);bounds(x1,y1);
    if(constellation_mode==4)return;
    gx_line(x0,y0,x1,y1,2);
}
static int free_label(unsigned char i)
{
    unsigned char j;
    for(j=0;j<i;++j) {
        gx_read(XR_LABELS+j*5u,&label,5);
        if(label.w && candidate.x<label.x+label.w+2 && candidate.x+candidate.w+2>label.x &&
           candidate.y<label.y+12 && candidate.y+12>label.y)return 0;
    }
    if(constellation_mode==4)for(j=0;j<NAMED_COUNT;++j) {
        if(candidate.x<markers[j].x+7 && candidate.x+candidate.w>markers[j].x-6 &&
           candidate.y<markers[j].y+7 && candidate.y+10>markers[j].y-6)return 0;
    }
    return !gx_body_hit(candidate.x,candidate.y,candidate.w,10);
}
static const char *label_text(unsigned char i)
{
    if(i<NAMED_COUNT)return star_names[i];
    load_figure(i-NAMED_COUNT);return figure.label;
}
static void place_label(unsigned char i)
{
    int start,step;
    unsigned char attempt;
    if(box.left>box.right)return;
    candidate.w=strlen(label_text(i))*6+2;
    candidate.x=i<NAMED_COUNT?box.left+7:(box.left+box.right-candidate.w)/2;
    if(i<NAMED_COUNT&&candidate.x+candidate.w>VIEW_RIGHT)candidate.x=box.left-candidate.w-6;
    if(candidate.x<VIEW_LEFT)candidate.x=VIEW_LEFT;
    if(candidate.x+candidate.w>VIEW_RIGHT)candidate.x=VIEW_RIGHT-candidate.w;
    start=box.top-(i<NAMED_COUNT?5:12);
    if(start<VIEW_TOP)start=VIEW_TOP;if(start+9>bottom)start=bottom-9;
    for(attempt=0;attempt<80;++attempt) {
        step=((attempt+1)/2)*12;
        candidate.y=start+(attempt&1?step:-step);
        if(candidate.y<VIEW_TOP||candidate.y+9>bottom)continue;
        if(free_label(i))break;
    }
    /* C4 gives star names priority; omit a label if no clear slot remains. */
    if(attempt==80){if(constellation_mode==4)return;candidate.y=start;}
    gx_write(XR_LABELS+i*5u,&candidate,5);
}
static void mark_star(unsigned char i)
{
    static const signed char dx[]={-4,-3,-2,2,3,4,-4,4,-4,4};
    static const signed char dy[]={4,4,4,4,4,4,3,3,2,2};
    unsigned char j;int x,y;
    if(markers[i].x<0)return;
    for(j=0;j<20;++j) {
        x=markers[i].x+dx[j/2];y=markers[i].y+(j&1?dy[j/2]:-dy[j/2]);
        if(!outside(x,y))gx_pixel(x,y);
    }
}
static void draw_one_label(unsigned char i,unsigned char repair)
{
    gx_read(XR_LABELS+i*5u,&label,5);
    if(label.w&&(!repair||gx_label_damaged(label.x+1,label.y+1,label.w-2)))
        gx_label(label.x+1,label.y+1,label_text(i),0);
}
void cs_labels(unsigned char repair)
{
    unsigned char i;
    if(constellation_mode<3||!labels_valid)return;
    if(constellation_mode==4)for(i=0;i<NAMED_COUNT;++i)mark_star(i);
    for(i=0;i<LABEL_COUNT;++i)draw_one_label(i,repair);
}
static unsigned draw_edge,draw_star;
static unsigned char draw_figure,draw_label,draw_named;
void cs_draw_begin(void)
{
    unsigned char i;
    memset(&label,0,sizeof(label));
    for(i=0;i<LABEL_COUNT;++i)gx_write(XR_LABELS+i*5u,&label,5);
    labels_valid=1;
    bottom=VIEW_BOTTOM-1;
    draw_edge=draw_star=draw_figure=draw_label=draw_named=0;
    if(constellation_mode==4)for(i=0;i<NAMED_COUNT;++i)
        if(!ready||!project_star(named_stars[i],&markers[i].x,&markers[i].y))markers[i].x=-10000;
}
/* One edge, one star or one label at a time; the caller supplies a budget. */
unsigned char cs_draw_step(void)
{
    uint16_t edge[2];unsigned j;
    unsigned char c,size;
    int x,y,x1,y1,dx,dy;
    if(!constellation_mode||!ready)return 1;
    if(constellation_mode==4&&draw_named<NAMED_COUNT) {
        c=draw_named++;
        if(markers[c].x>=0) {
            box.left=box.right=markers[c].x;box.top=markers[c].y;
            place_label(c);
        }
        return 0;
    }
    while(draw_figure<CONSTELLATION_COUNT) {
        c=draw_figure;
        load_figure(c);
        if(!draw_edge) {
            box.left=VIEW_RIGHT;box.right=VIEW_LEFT-1;
            box.top=VIEW_BOTTOM;
        }
        if(constellation_mode<2 || (constellation_mode==4&&c!=CS_UMA&&c!=CS_UMI&&c!=CS_CRU&&c!=CS_CEN&&c!=CS_OCT) ||
           draw_edge==figure.count){if(constellation_mode>=3)place_label(c+NAMED_COUNT);++draw_figure;draw_edge=0;continue;}
        gx_read(XR_EDGES+(figure.first+draw_edge++)*4u,edge,4);
        project_star(edge[0],&x,&y);project_star(edge[1],&x1,&y1);
        if(abs(x1-x)>gx_circle/2) {
            if(x<x1)x+=gx_circle;else x1+=gx_circle;
            segment(x,y,x1,y1);segment(x-gx_circle,y,x1-gx_circle,y1);
        } else segment(x,y,x1,y1);
        return 0;
    }
    if(draw_star<STAR_COUNT) {
        j=draw_star++;
        gx_read(XR_META+j,&size,1);
        if(constellation_mode==4&&!(size&128))return 0;
        if(!project_star(j,&x,&y))return 0;
        size&=3;
        for(dy=-1;dy<=1;++dy)for(dx=-1;dx<=1;++dx) {
            if(size==1&&(dx||dy))continue;
            if(size==2&&dx&&dy)continue;
            if(!outside(x+dx,y+dy))gx_pixel(x+dx,y+dy);
        }
        return 0;
    }
    if(constellation_mode>=3&&draw_label<LABEL_COUNT) {
        c=draw_label++;
        if(constellation_mode==4&&c<NAMED_COUNT)mark_star(c);
        draw_one_label(c,0);
        return 0;
    }
    return 1;
}
