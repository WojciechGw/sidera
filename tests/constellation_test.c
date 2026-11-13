#include <assert.h>
#include <stdio.h>
#include "../src/constellations.c"

Observer observer={0,0,0};Date moment={2000,1,1,12,0,0};Sky sky;
int bearing=0,elevation=24,gx_circle=1800;
unsigned target_id=0xFFFFu;
unsigned char target_figure=255;
unsigned char grid=0,live=0;
unsigned char planets=0;
Position planet_positions[PLANET_COUNT];
int gx_body_x[PLANET_COUNT+2],gx_body_y[PLANET_COUNT+2];
int gx_body_hit(int x,int y,int w,int h){return 0;} /* No bodies in these figure tests. */
int gx_label_damaged(int x,int y,int w){return 1;}
static void cs_draw(void) { cs_draw_begin();while(!cs_draw_step()){} }
char note[100];
int assets_vectors(unsigned first,unsigned char count,void *p){assert(0);return 0;}
void assets_forget_vectors(void){}
static unsigned char star_xram[65536];
void gx_read(unsigned a,void *p,unsigned char n){memcpy(p,star_xram+a,n);}
void gx_write(unsigned a,const void *p,unsigned char n){memcpy(star_xram+a,p,n);}
static unsigned pixels,label_calls;
static int minx,maxx,miny,maxy,lastx;
static unsigned char drawn[480][640];
static unsigned named_calls;
int gx_project(const Position *p,int *x,int *y){*x=(int16_t)p->az;*y=p->alt;return *x>=28&&*x<628&&*y>=VIEW_TOP&&*y<VIEW_BOTTOM;}
void gx_label(int x,int y,const char *p,unsigned char white)
{
    unsigned i;
    assert(x>=29&&x+(int)strlen(p)*6<=627);
    assert(y>=VIEW_TOP+1&&y+8<=bottom);++label_calls;
    assert(!white);
    for(i=0;i<NAMED_COUNT;++i)if(!strcmp(p,star_names[i]))break;
    if(i<NAMED_COUNT){assert(constellation_mode==4);++named_calls;}
    else if(constellation_mode==4)assert(!strcmp(p,"Ursa Major [UMa]")||
        !strcmp(p,"Ursa Minor [UMi]")||!strcmp(p,"Crux [Cru]")||
        !strcmp(p,"Centaurus [Cen]")||!strcmp(p,"Octans [Oct]"));
}
void gx_pixel(int x,int y)
{
    assert(!outside(x,y));
    drawn[y][x]=1;
    if(!pixels){minx=maxx=x;miny=maxy=y;}else {
        if(x<minx)minx=x;if(x>maxx)maxx=x;if(y<miny)miny=y;if(y>maxy)maxy=y;
    }
    lastx=x;++pixels;
}
void gx_line(int x0,int y0,int x1,int y1,unsigned char pattern)
{
    int dx=abs(x1-x0),dy=-abs(y1-y0),sx=x0<x1?1:-1,sy=y0<y1?1:-1;
    int err=dx+dy,e,n=0;
    assert(pattern==2);
    for(;;) {
        if(n++%5==0)gx_pixel(x0,y0);
        if(x0==x1&&y0==y1)break;
        e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}
    }
}
static void labels_clear(void)
{
    memset(star_xram+XR_LABELS,0,LABEL_COUNT*5);labels_valid=1;
}
static void check_labels(void)
{
    unsigned i,j;Label a,b;
    for(i=0;i<LABEL_COUNT;++i) {
        gx_read(XR_LABELS+i*5,&a,5);if(!a.w)continue;
        for(j=0;j<i;++j) {
            gx_read(XR_LABELS+j*5,&b,5);if(!b.w)continue;
            assert(a.x+a.w<=b.x||b.x+b.w<=a.x||a.y+10<=b.y||b.y+10<=a.y);
        }
        for(j=0;j<NAMED_COUNT;++j)
            assert(a.x+a.w<=markers[j].x-4||a.x>markers[j].x+4||
                   a.y+10<=markers[j].y-4||a.y>markers[j].y+4);
    }
}
int main(int argc,char **argv)
{
    int x,y,x1,y1;unsigned i,j,c;Position p;uint16_t edge[2];FILE *f;
    assert(argc==2);f=fopen(argv[1],"rb");assert(f);
    assert(!fseek(f,DATA_EDGES,SEEK_SET));assert(fread(star_xram+XR_EDGES,1,CATALOGUE_RECORD_BYTES,f)==CATALOGUE_RECORD_BYTES);assert(!fseek(f,DATA_META,SEEK_SET));assert(fread(star_xram+XR_META,1,3576,f)==3576);fclose(f);
    bottom=VIEW_BOTTOM-1;
    x=-100;y=100;x1=700;y1=100;
    assert(clip(&x,&y,&x1,&y1)&&x==28&&x1==627&&y==100&&y1==100);
    x=300;y=300;x1=300;y1=400;
    assert(clip(&x,&y,&x1,&y1)&&y1==400); /* crosses horizon without truncation */
    x=300;y=400;x1=400;y1=500;assert(clip(&x,&y,&x1,&y1)&&y1==415);
    x=300;y=416;x1=400;y1=500;assert(!clip(&x,&y,&x1,&y1));
    x=-100;y=100;x1=0;y1=300;assert(!clip(&x,&y,&x1,&y1));
    box.left=628;box.right=27;box.top=416;
    segment(-100,100,700,100,0);
    assert(pixels==120&&minx==28&&maxx==623&&miny==100&&maxy==100&&lastx==623);
    assert(box.left==28&&box.right==627);
    assert(!constellation_mode);
    for(i=1;i<=5;++i){cs_cycle();assert(constellation_mode==(i%5));}
    labels_clear();
    box.left=28;box.right=31;box.top=VIEW_TOP;place_label(NAMED_COUNT);
    box.left=623;box.right=627;box.top=350;place_label(NAMED_COUNT+1);
    constellation_mode=3;cs_labels(0);assert(label_calls==2);
    /* C4 shows these five constellations, excluding all other figures. */
    p.az=100;p.alt=200;
    for(i=0;i<STAR_COUNT;++i)gx_write(positions+i*4u,&p,4);
    for(c=0;c<CONSTELLATION_COUNT;++c)if(c==CS_UMA||c==CS_UMI||c==CS_CRU||c==CS_CEN||c==CS_OCT)
        for(load_figure(c),j=figure.first;j<figure.first+figure.count;++j) {
            p.az=300;
            gx_read(XR_EDGES+j*4u,edge,4);
            gx_write(positions+edge[0]*4u,&p,4);gx_write(positions+edge[1]*4u,&p,4);
        }
    ready=1;
    constellation_mode=4;{
        pixels=label_calls=0;cs_draw();
        assert(pixels&&minx==296&&maxx==304&&label_calls==13&&named_calls==8);
        check_labels();
        label_calls=named_calls=0;cs_labels(1);assert(label_calls==13&&named_calls==8);
    }
    constellation_mode=4;pixels=0;segment(28,100,627,100,0);assert(!pixels);
    /* Each newly added isolated star is visible in every enabled C mode,
       without extra line pixels; its figure gets a label even when alone. */
    for(i=757;i<759;++i)for(c=1;c<=4;++c) {
        p.az=10000;p.alt=-1000;
        for(j=0;j<STAR_COUNT;++j)gx_write(positions+j*4u,&p,4);
        p.az=340;p.alt=240;gx_write(positions+i*4u,&p,4);
        constellation_mode=c;pixels=label_calls=0;cs_draw();
        if(i==758&&c==4) {
            assert(pixels==21&&minx==336&&maxx==344&&miny==236&&maxy==244);
            assert(label_calls==2);check_labels();
        } else {
            assert(pixels==(c==2||c==3?2u:1u)&&minx==340&&maxx==340&&miny==240&&maxy==240);
            assert(label_calls==(c>=3?1u:0u));
        }
    }
    /* Every named star, including positions below the horizon: exact open
       9x9 frame, single-line name, no collisions and no markers in C1-C3. */
    for(i=0;i<NAMED_COUNT;++i)for(c=1;c<=4;++c) {
        p.az=10000;p.alt=-1000;
        for(j=0;j<STAR_COUNT;++j)gx_write(positions+j*4u,&p,4);
        p.az=340;p.alt=380;gx_write(positions+named_stars[i]*4u,&p,4);
        memset(drawn,0,sizeof(drawn));pixels=label_calls=named_calls=0;
        constellation_mode=c;cs_draw();
        assert(named_calls==(c==4));
        if(c==1||c==4)for(y=-4;y<=4;++y)for(x=-4;x<=4;++x)if(abs(x)>1||abs(y)>1)
            assert(drawn[380+y][340+x]==(c==4&&
                ((abs(x)==4&&abs(y)>=2)||(abs(y)==4&&abs(x)>=2))));
        if(c==4)check_labels();
    }
    /* Marker corners clip at all four edges; names stay inside the chart. */
    constellation_mode=4;
    for(c=0;c<4;++c) {
        p.az=c&1?VIEW_RIGHT-1:VIEW_LEFT;p.alt=c&2?VIEW_BOTTOM-1:VIEW_TOP;
        gx_write(positions+named_stars[7]*4u,&p,4);
        named_calls=0;cs_draw();assert(named_calls==1);check_labels();
    }
    /* Edges behind the observer never cut across the visible chart. Conversely,
       a long edge across the front stays visible at every zoom. */
    constellation_mode=2;gx_read(XR_EDGES,edge,4);
    for(i=50;i<=160;i+=10) {
        gx_circle=216000/i;
        p.az=(uint16_t)(VIEW_CENTER-gx_circle/2+5);p.alt=244;
        for(j=0;j<STAR_COUNT;++j)gx_write(positions+j*4u,&p,4);
        p.az=(uint16_t)(VIEW_CENTER+gx_circle/2-5);
        gx_write(positions+edge[1]*4u,&p,4);
        pixels=0;cs_draw();assert(!pixels);
        p.az=(uint16_t)(VIEW_CENTER-gx_circle/4+5);
        for(j=0;j<STAR_COUNT;++j)gx_write(positions+j*4u,&p,4);
        p.az=(uint16_t)(VIEW_CENTER+gx_circle/4-5);
        gx_write(positions+edge[1]*4u,&p,4);
        pixels=0;cs_draw();assert(pixels>=120&&minx==VIEW_LEFT&&maxx>=VIEW_RIGHT-5);
    }
    puts("PASS: clipping, five modes, C4 navigation markers/names, priority and collision-free labels");
    return 0;
}
