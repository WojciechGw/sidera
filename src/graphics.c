#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sidera.h"
#include "solar.h"

static unsigned moon_image=XR_MOON1;
static int loaded_phase=-1;
static unsigned char header_visible;
static unsigned char calculating,status_drawn=255;
static char text[120];
int fov=120,gx_circle=1800; /* Horizontal degrees; pixels per full azimuth turn. */
static int last_elevation=-100,origin_y;
static int last_bearing=-1;
static uint16_t bearing_angle;
unsigned char planets=0;
Position planet_positions[PLANET_COUNT];
static const char *const planet_names[PLANET_COUNT]={"Mercury","Venus","Mars","Jupiter","Saturn","Uranus"};
int gx_body_x[PLANET_COUNT+2],gx_body_y[PLANET_COUNT+2];
static int lx[PLANET_COUNT],ly[PLANET_COUNT];
static int damage[4];
static unsigned char drawing;
static unsigned draw_item;

static void body_cache(void)
{
    unsigned char i;
    for(i=0;i<PLANET_COUNT+2;++i)
        if(!gx_project(i<2?(i?&sky.moon:&sky.sun):&planet_positions[i-2],&gx_body_x[i],&gx_body_y[i]))
            gx_body_x[i]=-10000;
}
int gx_label_damaged(int x,int y,int w)
{
    if(x-1>damage[2]||x+w<damage[0]||y-1>damage[3]||y+8<damage[1])return 0;
    /* Repainting an opaque label can cover a later label. Propagate damage
     * in the same order as the original drawing, including planet labels. */
    if(x-1<damage[0])damage[0]=x-1;if(y-1<damage[1])damage[1]=y-1;
    if(x+w>damage[2])damage[2]=x+w;if(y+8>damage[3])damage[3]=y+8;
    return 1;
}

void gx_read(unsigned addr,void *data,unsigned char count)
{
    unsigned char *p=data;
    RIA.step1=1;RIA.addr1=addr;
    while(count--)*p++=RIA.rw1;
}
void gx_write(unsigned addr,const void *data,unsigned char count)
{
    const unsigned char *p=data;
    RIA.step0=1;RIA.addr0=addr;
    while(count--)RIA.rw0=*p++;
}

/* Read/modify/write through the RIA portals. */
static void put16(unsigned v) { RIA.rw0=(unsigned char)v;RIA.rw0=(unsigned char)(v>>8); }
void gx_clear(int y,int rows)
{
    unsigned a;
    unsigned char i;
    for(;rows>0;--rows,++y) {
        if(y<0||y>=480)continue;
        a=(unsigned)y*80u;
        RIA.step0=1;RIA.addr0=a;
        for(i=0;i<80;++i)RIA.rw0=0;
    }
}
void gx_pixel(int x,int y)
{
    unsigned a;unsigned char b;
    if(x<0||x>=640||y<0||y>=480)return;
    a=(unsigned)y*80u+(unsigned)x/8;
    RIA.step0=0;RIA.addr0=a;b=RIA.rw0;
    RIA.rw0=b|(0x80u>>(x&7));
}
void gx_dot(int x,int y)
{
    unsigned a;
    unsigned char row,shift,v;
    uint16_t mask,bits;
    if(x<1||x>637||y<1||y>477)return;
    --x;--y;shift=(unsigned char)(x&7);
    mask=0xF000u>>shift;
    a=(unsigned)y*80u+(unsigned)x/8;
    for(row=0;row<4;++row,a+=80) {
        bits=(row==1||row==2)?0x6000u>>shift:0;
        RIA.step0=0;
        RIA.addr0=a;v=RIA.rw0;RIA.rw0=(v&~(mask>>8))|(bits>>8);
        RIA.addr0=a+1;v=RIA.rw0;RIA.rw0=(v&~mask)|bits;
    }
}
void gx_line(int x0,int y0,int x1,int y1,unsigned char dashed)
{
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1;
    int err=dx+dy,e;unsigned char n=0;
    for(;;) {
        /* 0: solid, 1: Moon trail, 2: constellation dots every five pixels. */
        if(!dashed||(dashed==2?!n:!(n&4)))gx_pixel(x0,y0);
        ++n;if(dashed==2&&n==5)n=0;
        if(x0==x1&&y0==y1)break;
        e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}
    }
}
static void draw_text(int x,int y,const char *p,unsigned char inverse)
{
    unsigned a;unsigned char row,c,b,v,shift;
    while(*p && x<=634) {
        c=(unsigned char)*p++;
        if(c==0xC2 && (unsigned char)*p==0xB0){c=1;++p;}
        else if(c==0xC3 && (unsigned char)*p==0xB6){c=4;++p;}
        else if(c==0xE2 && (unsigned char)p[0]==0x80 &&
                ((unsigned char)p[1]==0xB2||(unsigned char)p[1]==0xB3)) {
            c=(unsigned char)p[1]-0xB0;p+=2;
        }
        if(c>=128)c='?';
        if(x>=0 && y>=0 && y<=472) {
            shift=(unsigned char)(x&7);
            RIA.step1=1;RIA.addr1=XR_FONT+(unsigned)c*8;
            RIA.step0=0;
            for(row=0;row<8;++row) {
                b=RIA.rw1;a=(unsigned)(y+row)*80u+(unsigned)x/8;
                if(inverse)b^=0xFC;
                RIA.addr0=a;v=RIA.rw0;RIA.rw0=(v&~(0xFCu>>shift))|(b>>shift);
                if(shift>2) {RIA.addr0=a+1;v=RIA.rw0;RIA.rw0=(v&~(0xFCu<<(8-shift)))|(b<<(8-shift));}
            }
        }
        x+=6;
        if(inverse)--inverse;
    }
}
void gx_text(int x,int y,const char *p)
{
    draw_text(x,y,p,0);
}
void gx_label(int x,int y,const char *p,unsigned char white)
{
    int w=(int)strlen(p)*6;
    unsigned first,last,b,a;
    unsigned char row,mask,v;
    if(white) {
        gx_line(x-1,y-1,x+w,y-1,0);gx_line(x-1,y+8,x+w,y+8,0);
        gx_line(x-1,y,x-1,y+7,0);gx_line(x+w,y,x+w,y+7,0);
    } else {
        /* Opaque black padding as well as the glyph cells. */
        first=(unsigned)(x-1)/8;last=(unsigned)(x+w)/8;
        for(row=0;row<10;++row) {
            a=(unsigned)(y-1+row)*80u;
            RIA.step0=0;
            for(b=first;b<=last;++b) {
                mask=255;
                if(b==first)mask>>=(x-1)&7;
                if(b==last)mask&=255u<<(7-((x+w)&7));
                RIA.addr0=a+b;v=RIA.rw0;RIA.rw0=v&~mask;
            }
        }
    }
    draw_text(x,y,p,white?255:0);
}
int gx_zoom(int step)
{
    int next=fov+step;
    if(next<50||next>160)return 0;
    fov=next;gx_circle=(int)(216000L/fov);last_elevation=-100;return 1;
}
static int scale(int degrees)
{
    return (int)((int32_t)degrees*gx_circle/360);
}
static int azimuth_x(int az)
{
    int delta=az-bearing;
    if(delta< -180)delta+=360;
    if(delta>=180)delta-=360;
    return VIEW_CENTER+scale(delta);
}
static int altitude_y(int alt)
{
    /* Thousands of star projections share the same viewing elevation. */
    if(last_elevation!=elevation) {
        last_elevation=elevation;origin_y=VIEW_MIDDLE+scale(elevation);
    }
    return origin_y-(alt?scale(alt):0);
}
int gx_project(const Position *p,int *x,int *y)
{
    int16_t da;
    if(last_bearing!=bearing){last_bearing=bearing;bearing_angle=as_angle((int32_t)bearing*1000);}
    da=(int16_t)(p->az-bearing_angle);
    *x=VIEW_CENTER+(int)((int32_t)da*gx_circle/65536L);
    *y=altitude_y(0)-(int)((int32_t)p->alt*gx_circle/65536L);
    return *x>=VIEW_LEFT && *x<VIEW_RIGHT && *y>=VIEW_TOP && *y<VIEW_BOTTOM;
}
static void header_icons(unsigned char show)
{
    header_visible=show;
    RIA.step0=1;RIA.addr0=XR_SUN_HEADER_CFG;
    put16(show?195:-64);RIA.addr0=XR_MOON_HEADER_CFG+12;put16(show?379:-64);
}
static void wait_vsync(void)
{
    unsigned char v=RIA.vsync;
    unsigned start=platform_ticks();
    /* A disconnected VGA must never hang the application. */
    while(v==RIA.vsync && (unsigned)(platform_ticks()-start)<4) {}
}
int gx_canvas(void)
{
    int r;
    unsigned char i;
    header_icons(0);
    status_drawn=255;
    RIA.step0=1;RIA.addr0=XR_CFG;
    put16(0);put16(0);put16(0);put16(640);put16(480);put16(0);put16(0xFFFF);
    r=xreg(1,0,0,3);
    if(r>=0)r=xreg(1,0,1,3,0,XR_CFG,0,0,0);
    if(cities_active)return r>=0;
    /* Positions are supplied after the next calculation/scene refresh. */
    RIA.addr0=XR_MOON_CFG+12;put16(-64);
    for(i=0;i<=PLANET_COUNT;++i){RIA.addr0=XR_BODY_CFG+i*10;put16(-64);}
    if(r>=0)r=xreg(1,0,1,5,56,XR_BODY_CFG,PLANET_COUNT+10,1,VIEW_TOP,VIEW_BOTTOM);
    if(r>=0)r=xreg(1,0,1,4,1,XR_MOON_CFG,1,2,VIEW_TOP,VIEW_BOTTOM);
    if(r>=0)r=xreg(1,0,1,5,19,XR_SUN_HEADER_CFG,1,0,0,VIEW_TOP);
    if(r>=0)r=xreg(1,0,1,4,1,XR_MOON_HEADER_CFG,1,1,0,VIEW_TOP);
    return r>=0;
}
void gx_console(void)
{
    xreg(1,0,0,0);
}
int gx_objects(void)
{
    int sx,sy,mx,my,c,s;
    unsigned next,distance;
    unsigned char i,j,rank;
    if(loaded_phase!=(int)sky.phase) {
        next=moon_image==XR_MOON0?XR_MOON1:XR_MOON0;
        if(!assets_phase(sky.phase,next))return 0;
        moon_image=next;loaded_phase=sky.phase;
    }
    if(!gx_project(&sky.moon,&mx,&my)||solar_hit(mx-16,my-16,32,32))mx=-64;
    c=as_cos(sky.tilt)/64;s=as_sin(sky.tilt)/64;
    wait_vsync();
    RIA.step0=1;RIA.addr0=XR_SUN_HEADER_CFG;
    put16(header_visible?195:-64);put16(35);put16(XR_SUN);put16(XR_PAL);
    for(i=0;i<2;++i) {
        RIA.addr0=i?XR_MOON_HEADER_CFG:XR_MOON_CFG;
        /* |c|, |s| <= 256: even 768*31 fits signed 16-bit arithmetic. */
        put16(c);put16(s);put16((256-c-s)*31/2);
        put16(-s);put16(c);put16((256+s-c)*31/2);
        put16(i?(header_visible?379:-64):mx-16);put16(i?35:my-16);
        put16(moon_image);RIA.rw0=5;RIA.rw0=0;
    }
    for(i=0;i<=PLANET_COUNT;++i) {
        /* Equal distances keep catalogue order. The nearest sprite goes last. */
        rank=0;distance=body_distance[i];
        for(j=0;j<=PLANET_COUNT;++j)
            if(body_distance[j]>distance||(body_distance[j]==distance&&j<i))++rank;
        if(!gx_project(i==PLANET_COUNT?&sky.sun:&planet_positions[i],&sx,&sy)||
           (i<PLANET_COUNT&&!planets))sx=-64;
        c=i==PLANET_COUNT?16:4;
        if(solar_hit(sx-c,sy-c,c*2,c*2))sx=-64;
        RIA.step0=1;RIA.addr0=XR_BODY_CFG+rank*10;
        put16(sx-c);put16(sy-c);
        put16(i==PLANET_COUNT?XR_SUN:XR_PLANETS+i*PLANET_BYTES);
        put16(i==PLANET_COUNT?XR_PAL:XR_PLANET_PAL+i*512u);
        RIA.rw0=i==PLANET_COUNT?0x77:0x11;RIA.rw0=3; /* 32x32 / 8x8, 8 bpp */
    }
    return 1;
}
int gx_body_hit(int x,int y,int w,int h)
{
    unsigned char j;
    int ox,oy,r;
    for(j=0;j<(planets?PLANET_COUNT+2:2);++j) {
        ox=gx_body_x[j];oy=gx_body_y[j];if(ox<0)continue;
        r=j<2?18:6;
        if(x<ox+r&&x+w>ox-r&&y<oy+r&&y+h>oy-r)return 1;
    }
    return 0;
}
void gx_planet_labels(void)
{
    unsigned char i,j,attempt,blocked;
    int x,y,w,sx,sy;
    if(!planets)return;
    body_cache();
    for(i=0;i<PLANET_COUNT;++i) {
        lx[i]=-100;
        sx=gx_body_x[i+2];sy=gx_body_y[i+2];if(sx<0)continue;
        w=(int)strlen(planet_names[i])*6;
        x=sx+6;if(x+w>=VIEW_RIGHT)x=sx-6-w;
        if(x<VIEW_LEFT+1)x=VIEW_LEFT+1;
        y=sy-4;
        for(attempt=0;attempt<40;++attempt) {
            y=sy-4+(attempt&1?1:-1)*((attempt+1)/2)*10;
            if(y<VIEW_TOP+13||y+9>=VIEW_BOTTOM)continue;
            blocked=0;
            for(j=0;j<i;++j)if(lx[j]>=0&&x<lx[j]+(int)strlen(planet_names[j])*6+2&&x+w+2>lx[j]&&y<ly[j]+10&&y+10>ly[j])blocked=1;
            if(gx_body_hit(x,y,w,8))blocked=1;
            if(!blocked)break;
        }
        if(attempt==40){y=sy+6;if(y+9>=VIEW_BOTTOM)y=sy-14;if(y<VIEW_TOP+13)y=VIEW_TOP+13;}
        lx[i]=x;ly[i]=y;gx_label(x,y,planet_names[i],0);
    }
}
void gx_clock(void)
{
    Date local;int32_t offset;
    int h,m,s;
    strcpy(text,"Time ");date_format(text+5,&moment);strcat(text," UTC");gx_text(VIEW_LEFT-20,21,text);
    if(tz_local(&moment,&local,&offset)) {
        date_format(text,&local);strcat(text," LOCAL");gx_text(38,31,text);
        s=offset<0?-1:1;offset*=s;h=(int)(offset/3600);m=(int)(offset/60%60);
        sprintf(text,"UTC%c%02d:%02d",s<0?'-':'+',h,m);
        if(offset%60)sprintf(text+strlen(text),":%02d",(int)(offset%60));
    } else {
        gx_text(38,31,"---- -- -- --:--:-- LOCAL");strcpy(text,"Zone unavailable");
    }
    while(strlen(text)<25)strcat(text," ");gx_text(38,41,text);
}
static void text_right(int y,const char *p)
{
    gx_text(VIEW_RIGHT-(int)strlen(p)*6,y,p);
}
void gx_status(unsigned char busy)
{
    unsigned char state,row,i;
    int x;
    const char *label;
    calculating=!!busy;state=(live?0:playing?1:2)+calculating*4;
    if(state==status_drawn)return;
    status_drawn=state;
    /* Clear only the top-right status, including the old LIVE bar. */
    for(row=0;row<9;++row) {
        RIA.step0=1;RIA.addr0=(unsigned)row*80+60;
        for(i=0;i<20;++i)RIA.rw0=0;
    }
    label=live?" LIVE ":playing?"SIM / PLAY":"SIM / PAUSE";
    x=640-(int)strlen(label)*6;
    if(calculating)gx_text(x-66,1,"Working...");
    draw_text(x,1,label,live?6:0);
    if(live)gx_line(604,0,639,0,0);
}
void gx_header(void)
{
    int a,az;
    unsigned char i;
    const Position *p;
    header_icons(0);
    /* Let VGA latch the hidden descriptors before changing the bitmap. */
    wait_vsync();
    gx_clear(0,VIEW_TOP-13);
    draw_text(0,1," SIDERA  The Celestial Observatory",8);
    gx_line(0,0,47,0,0);
    status_drawn=255;gx_status(calculating);
    gx_clock();
    strcpy(text,"Location ");location_format(text+9,&observer,0);gx_text(202,21,text);
    gx_text(256,31,location_name);
    sprintf(text,"View %03d\x01 / %+d\x01",bearing,elevation);text_right(21,text);
    sprintf(text,"FOV %3d\x01",fov);text_right(31,text);
    /* Fixed fields: Sun text ends 9 px before the Moon sprite. */
    for(i=0;i<2;++i) {
        p=i?&sky.moon:&sky.sun;az=as_degrees(p->az);
        a=(int)((int32_t)p->alt*3600L/65536L);
        sprintf(text,"%s AZ %03d.%d\x01 ALT %s%02d.%d\x01",i?"Moon":"Sun",
            az/10,az%10,a<0?"-":"+",abs(a)/10,abs(a)%10);
        if(i)sprintf(text+strlen(text)," LIT %3u.%u%%",sky.illumination/10,sky.illumination%10);
        gx_text(i?412:226,43,text);
    }
    gx_riseset();header_icons(1);
}
void gx_riseset(void)
{
    unsigned char i;
    gx_clear(53,8);
    for(i=0;i<2;++i){rs_text(i,text);gx_text(i?442:250,53,text);}
}
void gx_footer(void)
{
    gx_clear(448,32);
    gx_text(12,450,"Arrows: view  N: now  Space: play/pause  +/-: 15 min  [/]: day  D: date  P: Planets  K: Cities");
    gx_text(12,462,"L: location  S: save  G: grid  T: paths  H: N/NE/E/SE/S/SW/W/NW  C: constellations  Z: Zone  Q: quit");
    if(grid)draw_text(156,462," grid ",255);
    if(trails)draw_text(210,462," paths ",255);
    if(constellation_mode)draw_text(414,462," constellations ",255);
    if(planets)draw_text(462,450," Planets ",255);
    gx_text(582,450,"X: Solar");
    gx_text(20,472,note);
}
void gx_trail(unsigned char body,unsigned char n)
{
    int x,y,px,py;
    Position p;
    unsigned char i;
    if(!n)return;
    gx_read(XR_TRAILS+(unsigned)n*8+body*4,&p,4);
    if(!gx_project(&p,&x,&y))return;
    gx_read(XR_TRAILS+(unsigned)(n-1)*8+body*4,&p,4);
    if(!gx_project(&p,&px,&py)||abs(x-px)>scale(30))return;
    gx_line(px,py,x,y,body);
    if(drawing)return;
    damage[0]=x<px?x:px;damage[2]=x>px?x:px;
    damage[1]=y<py?y:py;damage[3]=y>py?y:py;
    cs_labels(1);
    if(planets)for(i=0;i<PLANET_COUNT;++i)
        if(lx[i]>=0&&gx_label_damaged(lx[i],ly[i],(int)strlen(planet_names[i])*6))
            gx_label(lx[i],ly[i],planet_names[i],0);
}
void gx_grid_cycle(void) { grid=(grid+1)&3; }
static void grid_pixel(int x,int y)
{
    if(x>=VIEW_LEFT&&x<VIEW_RIGHT&&y>=VIEW_TOP&&y<VIEW_BOTTOM)gx_pixel(x,y);
}
static void grid_column(int az,unsigned char crosses,unsigned char dots)
{
    int alt,x,y,d;
    x=azimuth_x(az);
    if(x<VIEW_LEFT-2||x>=VIEW_RIGHT+2)return;
    if(dots)for(alt=-90;alt<=90;alt+=10)
        if(alt)grid_pixel(x,altitude_y(alt));
    if(az%10)return;
    if(x<VIEW_LEFT||x>=VIEW_RIGHT)return;
    if(crosses)for(alt=-90;alt<=90;alt+=10) {
        if(!alt)continue;
        y=altitude_y(alt);
        if(y<VIEW_TOP||y>=VIEW_BOTTOM)continue;
        for(d=-2;d<=2;++d){grid_pixel(x+d,y);grid_pixel(x,y+d);}
    }
    if(dots)for(alt=-90;alt<=90;alt+=2)
        if(alt)grid_pixel(x,altitude_y(alt));
}
static void grid_marks(unsigned char crosses,unsigned char dots)
{
    int az;for(az=0;az<360;az+=2)grid_column(az,crosses,dots);
}
/* Signed altitude labels start in the margin; their degree glyph may
 * extend into the plot. Draw the tick after the opaque text cells. */
static void grid_axes(void)
{
    int a,x,y,tx;
    if(!grid)return;
    for(a=-90;a<=90;a+=10) {
        y=altitude_y(a);
        if(!a||y<VIEW_TOP||y>=VIEW_BOTTOM)continue;
        sprintf(text,"%+d\x01",a);
        gx_text(24-(int)strlen(text)*6,y-6,text);
        gx_line(VIEW_LEFT,y,VIEW_LEFT+2,y,0);gx_line(VIEW_RIGHT,y,VIEW_RIGHT+2,y,0);
    }
    for(a=0;a<360;a+=10) {
        x=azimuth_x(a);if(x<VIEW_LEFT||x>=VIEW_RIGHT)continue;
        gx_line(x,VIEW_TOP-4,x,VIEW_TOP-2,0);
        sprintf(text,"%d\x01",a);tx=x-(int)strlen(text)*3;
        if(tx<VIEW_LEFT+3)tx=VIEW_LEFT+3;
        if(tx+(int)strlen(text)*6>VIEW_RIGHT)tx=VIEW_RIGHT-(int)strlen(text)*6;
        gx_text(tx,VIEW_TOP-13,text);
    }
}
static void horizon_label(void)
{
    int y=altitude_y(0);
    if(y>=VIEW_TOP&&y<VIEW_BOTTOM) {
        gx_text(12,y-6,"0\x01");
        if(y>=VIEW_TOP+9)gx_text(VIEW_LEFT,y-9,"horizon");
    }
}
void gx_grid_add(void)
{
    /* Only add the new layer. Opaque labels must remain above the marks. */
    if(grid>=2)grid_marks(grid==2,grid==3);
    cs_labels(0);grid_axes();horizon_label();gx_planet_labels();
}
static void compass(void)
{
    static const char *const dirs[]={"N","NE","E","SE","S","SW","W","NW"};
    int a,x,y;
    unsigned char i;
    y=altitude_y(0);
    if(y>=VIEW_TOP && y<VIEW_BOTTOM) {
        for(x=VIEW_LEFT;x<VIEW_RIGHT;x+=5)gx_pixel(x,y);
    }
    for(a=0;a<360;a+=10) {
        x=azimuth_x(a);
        if(x<VIEW_LEFT||x>VIEW_RIGHT)continue;
        gx_line(x,428,x,430,0);
    }
    for(i=0;i<8;++i) {
        x=azimuth_x(i*45);
        if(x<VIEW_LEFT||x>VIEW_RIGHT)continue;
        gx_line(x,426,x,431,0);gx_text(x-(int)strlen(dirs[i])*3,434,dirs[i]);
    }
    gx_line(VIEW_CENTER-4,420,VIEW_CENTER,424,0);gx_line(VIEW_CENTER,424,VIEW_CENTER+4,420,0);
}
void gx_scene_begin(void)
{
    drawing=1;draw_item=VIEW_TOP-13;body_cache();cs_draw_begin();
}
unsigned char gx_busy(void) { return drawing!=0; }
void gx_scene_step(void)
{
    unsigned start=platform_ticks();unsigned char n;
    /* At most eight small units or two ticks before returning to the keyboard.
     * A single unit (e.g. label placement) always finishes atomically. */
    for(n=0;n<8&&drawing;++n) {
        switch(drawing) {
        case 1:
            gx_clear(draw_item,draw_item+8>448?448-draw_item:8);draw_item+=8;
            if(draw_item>=448){drawing=2;draw_item=0;}break;
        case 2:
            if(grid>=2){grid_column(draw_item,1,grid==3);draw_item+=2;}
            if(grid<2||draw_item>=360){drawing=3;draw_item=1;}break;
        case 3:compass();drawing=4;break;
        case 4:
            if(trails&&draw_item<trail_done){gx_trail(0,draw_item);gx_trail(1,draw_item);++draw_item;}
            else drawing=5;break;
        case 5:if(cs_draw_step())drawing=6;break;
        case 6:grid_axes();horizon_label();drawing=7;break;
        case 7:gx_planet_labels();drawing=0;break;
        }
        if((unsigned)(platform_ticks()-start)>=2)break;
    }
}
