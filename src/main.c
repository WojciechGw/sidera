/* SIDERA -- standalone sky observatory for RP6502. */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "sidera.h"
#include "overlays/overlay.h"
#include "solar.h"

Observer observer={1880272L,756424L,113}; /* Warsaw [PL], catalogue coordinates. */
Date moment={2000,1,1,12,0,0};
Sky sky;
int bearing=0,elevation=24;
unsigned char grid=0,trails=0,live=1,playing=0;
unsigned char trail_done=0;
char note[100];
static Date trail_time;
static Sky trail_sky;
static int32_t trail_day;
static Observer trail_observer;
static unsigned char trail_end;
unsigned char editing=0;

static int read_clock(Date *result)
{
    time_t t;
    struct tm *p;
    Date d;
    t=time(NULL);p=gmtime(&t);
    if(p) {
        d.year=p->tm_year+1900;d.month=(unsigned char)(p->tm_mon+1);
        d.day=(unsigned char)p->tm_mday;d.hour=(unsigned char)p->tm_hour;
        d.minute=(unsigned char)p->tm_min;d.second=(unsigned char)p->tm_sec;
    }
    if(t==(time_t)-1||!p||!date_valid(&d))return 0;
    *result=d;return 1;
}
static int now(void) { return read_clock(&moment); }
void location_view(void)
{
    Date utc,local;
    int32_t offset;
    if(!read_clock(&utc)||!tz_local(&utc,&local,&offset))return;
    bearing=local.hour<4||local.hour>=22?0:local.hour<10?90:local.hour<16?180:270;
}
static void trail_reset(void)
{
    trail_done=trail_end=0;trail_day=date_day(&moment);trail_time=moment;
    trail_observer=observer;
    trail_time.hour=trail_time.minute=trail_time.second=0;
}
int main(int argc,char **argv)
{
    int r,exit_code=0;
    unsigned k,tick,calc_tick,star_tick;
    unsigned char refresh=1,scene=1,head=1,clock_dirty=0,star_check=1,result;
    int32_t jump;
    r=startup(argc,argv);
    if(ovl_failed)goto done;
    if(r<=0)return r<0;
    if(!assets_open()){platform_puts("sidera: missing or invalid sidera.dat.\r\n");return 1;}
    if(!ovl_select(OVL_COVER))goto done;
    show_cover();
    if(!assets_base()){assets_close();platform_puts("sidera: graphics file is truncated.\r\n");return 1;}
    if(live&&!now()){live=0;strcpy(note,"System clock unavailable. D sets UTC date.");}
    solar_hide();gx_clear(0,480);
    if(!gx_objects()||!gx_canvas()){gx_console();assets_close();platform_puts("sidera: cannot load sprite or configure VGA.\r\n");return 1;}
    if(!platform_keyboard_start()){strcpy(note,"Cannot map USB keyboard state.");exit_code=1;goto done;}
    gx_status(1); /* First astronomical calculation runs in the loop below. */
    trail_reset();tick=calc_tick=star_tick=platform_ticks();
    for(;;) {
        k=key_poll();
        if(solar_visible) {
            if(k=='q'||k=='Q')goto done;
            if(k=='x'||k=='X') {
                solar_hide();if(live)now();
                refresh=scene=head=star_check=1;tick=platform_ticks();k=0;
            } else {
                /* Modal snapshot: freeze all sky jobs and simulation, only LIVE clocks run. */
                if(live&&(unsigned)(platform_ticks()-tick)>=PLATFORM_TICK_HZ) {
                    tick=platform_ticks();
                    if(now())gx_clock();
                    else {live=0;strcpy(note,"Cannot read system clock.");}
                }
                platform_sleep_ticks(1);continue;
            }
        }
        if(cities_active) {
            if(k) {
                result=cities_key(k);
                if(result) {
                    if(live)now();refresh=scene=head=1;
                    if(result==1)location_view();
                }
            }
            if(cities_active){platform_sleep_ticks(1);continue;}
        } else if(editing) {
            if(k) {
                r=editing;result=edit_key(k);
                if(result){if(result==1){if(r==1)location_view();refresh=1;}scene=head=1;}
            }
            if(editing){platform_sleep_ticks(1);continue;}
        } else {
            jump=0;
            switch(k) {
            case 'q':case 'Q':case K_ESC:goto done;
            case K_LEFT:bearing=(bearing+355)%360;scene=head=1;break;
            case K_RIGHT:bearing=(bearing+5)%360;scene=head=1;break;
            case K_LEFT|K_LCTRL:case K_RIGHT|K_LCTRL:
                if(gx_zoom(k==(K_LEFT|K_LCTRL)?-10:10))scene=head=1;break;
            case K_UP:if(elevation<54)elevation+=5;if(elevation>54)elevation=54;scene=head=1;break;
            case K_DOWN:if(elevation>-54)elevation-=5;if(elevation< -54)elevation=-54;scene=head=1;break;
            case K_RIGHT|K_SHIFT:bearing=((bearing/45+1)*45)%360;scene=head=1;break;
            case K_LEFT|K_SHIFT:
                bearing=bearing?((bearing-1)/45)*45:315;scene=head=1;break;
            case 'n':case 'N':live=now();playing=0;refresh=head=1;break;
            case ' ':
                if(live)live=playing=0;
                else if(!playing)playing=1;
                else {
                    playing=0;live=now();refresh=1;
                    if(!live)strcpy(note,"Cannot read system clock.");
                }
                tick=platform_ticks();head=star_check=1;break;
            case '+':case '=':jump=900;break;
            case '-':jump=-900;break;
            case '[':jump=-86400L;break;
            case ']':jump=86400L;break;
            case 'g':case 'G':
                gx_grid_cycle();if(!grid||gx_busy())scene=1;else if(!scene){gx_status(1);gx_grid_add();gx_footer();}break;
            case 'c':case 'C':cs_cycle();if(constellation_mode==1)star_check=1;scene=1;break;
            case 'p':case 'P':planets=!planets;refresh=scene=1;break;
            case 't':case 'T':trails=!trails;scene=1;break;
            case 'x':case 'X':
                solar_date=moment;
                if(!ovl_select(OVL_SOLAR))goto done;
                if(!solar_draw()){exit_code=1;goto done;}
                /* Hide sky sprites covered by the panel, without repainting the chart. */
                if(!gx_objects()){strcpy(note,"Cannot load Moon phase from sidera.dat.");exit_code=1;goto done;}
                gx_status(0);tick=platform_ticks();continue;
            case 'l':case 'L':if(!ovl_select(OVL_EDIT))goto done;edit_start(1);continue;
            case 'd':case 'D':if(!ovl_select(OVL_EDIT))goto done;edit_start(2);continue;
            case 'z':case 'Z':if(!ovl_select(OVL_EDIT))goto done;edit_start(3);continue;
            case 'k':case 'K':
                if(!ovl_select(OVL_CITIES))goto done;
                if(cities_start()) {
                    if(!gx_objects()){strcpy(note,"Cannot load Moon phase from sidera.dat.");exit_code=1;goto done;}
                    cities_draw();continue;
                }
                gx_footer();break;
            case 's':case 'S':strcpy(note,config_save()?"":"Cannot save configuration.");gx_footer();break;
            case 12:scene=head=1;break;
            }
            if(jump) {
                live=playing=0;
                if(!date_step(&moment,jump))strcpy(note,"Date limit: 1900-01-01 through 2200-12-31.");
                refresh=head=1;
            }
        }
        if((unsigned)(platform_ticks()-tick)>=PLATFORM_TICK_HZ) {
            tick=platform_ticks();
            if(live){if(!now()){live=0;strcpy(note,"Cannot read system clock.");head=1;}clock_dirty=1;}
            else if(playing&&!gx_busy()&&!rs_busy()&&(!constellation_mode||!cs_busy())){if(!date_step(&moment,900))playing=0;refresh=head=1;}
        }
        if(live&&(unsigned)(platform_ticks()-calc_tick)>=60u*PLATFORM_TICK_HZ)refresh=1;
        if(live&&constellation_mode&&(unsigned)(platform_ticks()-star_tick)>=300u*PLATFORM_TICK_HZ)star_check=1;
        if(refresh&&(!gx_busy()||scene)) {
            gx_status(1);
            sky_compute(&moment,&observer,&sky);calc_tick=platform_ticks();
            if(planets){planets_compute(&moment,&observer,planet_positions);scene=1;}
            if(date_day(&moment)!=trail_day||memcmp(&observer,&trail_observer,sizeof(observer)))
                {trail_reset();scene=1;}
            if(gx_busy())scene=1;
            refresh=0;head=star_check=1;
        }
        if(star_check) {
            if(constellation_mode&&!cs_current()) {
                gx_status(1);cs_begin();star_tick=platform_ticks();
            }
            star_check=0;
        }
        if(constellation_mode&&cs_busy()&&!gx_busy()){gx_status(1);if(cs_step())scene=1;}
        if((head||clock_dirty)&&rs_sync())gx_riseset();
        if(head||scene) {
            gx_status(1);
            if(!gx_objects()){strcpy(note,"Cannot load Moon phase from sidera.dat.");playing=0;}
            if(scene)gx_scene_begin();
            if(head){gx_header();clock_dirty=0;}gx_footer();scene=head=0;
        }
        if(clock_dirty){gx_clock();clock_dirty=0;}
        if(gx_busy())gx_scene_step();
        if(rs_busy()&&!gx_busy()&&!refresh) {gx_status(1);if(rs_step())gx_riseset();}
        else if(trails&&!trail_end&&trail_done<TRAIL_COUNT&&!gx_busy()&&!refresh) {
            gx_status(1);
            sky_positions(&trail_time,&observer,&trail_sky);
            gx_write(XR_TRAILS+(unsigned)trail_done*8,&trail_sky,8);
            if(trail_done){gx_trail(0,trail_done);gx_trail(1,trail_done);}
            ++trail_done;
            if(!date_step(&trail_time,900))trail_end=1;
        }
        gx_status(gx_busy()||rs_busy()||refresh||(constellation_mode&&cs_busy())||(trails&&!trail_end&&trail_done<TRAIL_COUNT));
        if(!gx_busy())platform_sleep_ticks(1);
    }
done:
    platform_keyboard_stop();
    if(ovl_loaded(OVL_CITIES))cities_close();
    tz_close();gx_console();assets_close();
    if(ovl_failed||exit_code){platform_puts(note);platform_puts("\r\n");return 1;}
    return 0;
}
