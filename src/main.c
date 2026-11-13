/* sidera -- local Sun/Moon sky, a foreground razemOS(mt) application. */
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "sidera.h"
#include "bare/overlay.h"
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
    rpmt_os_lock();t=time(NULL);p=gmtime(&t);
    if(p) {
        d.year=p->tm_year+1900;d.month=(unsigned char)(p->tm_mon+1);
        d.day=(unsigned char)p->tm_mday;d.hour=(unsigned char)p->tm_hour;
        d.minute=(unsigned char)p->tm_min;d.second=(unsigned char)p->tm_sec;
    }
    rpmt_os_unlock();
    if(t==(time_t)-1||!p||!date_valid(&d))return 0;
    *result=d;return 1;
}
static int now(void) { return read_clock(&moment); }
static void location_view(void)
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
#ifndef SIDERA_OVERLAYS
static void help(void)
{
    rpmt_con_puts("sidera /config PATH /date \"YYYY-MM-DD HH:MM:SS\" /live\r\n"
        " /location \"CC/City\" /view AZ,ALT /fov 50..160\r\n"
        " /grid 0..3 /paths on|off /planets on|off /constellations 0..4\r\n"
        "All options optional. S saves settings.\r\n"
#ifdef RPMT_BARE
        "Standalone: resources embedded in ROM; Q returns to the monitor.\r\n");
#else
        "\r\n");
#endif
}
#endif
int main(int argc,char **argv)
{
    int r,exit_code=0;
#ifndef SIDERA_OVERLAYS
    int i;
    unsigned char option;
    const char *location_arg=0;
#endif
    unsigned k,tick,calc_tick,star_tick;
    unsigned char refresh=1,scene=1,head=1,clock_dirty=0,star_check=1,result;
#ifndef RPMT_BARE
    unsigned char gen;
#endif
    int32_t jump;
#ifdef SIDERA_OVERLAYS
    if(!ovl_select(OVL_INIT))goto done;
    r=boot_prepare(argc,argv);
    if(r<=0)return r<0;
    if(boot_location) {
        if(!ovl_select(OVL_CITIES))goto done;
        cities_init(argv[0]);
        if(!cities_lookup(boot_location)){rpmt_con_puts(note);rpmt_con_puts("\r\n");return 1;}
    }
    if(boot_config!=1||boot_location)location_view();
    if(!ovl_select(OVL_INIT))goto done;
    boot_apply(argc,argv);
#else
    paths_init(argv[0]);cities_init(argv[0]);
    for(i=1;i<argc;++i) {
        option=argv[i][0]=='/'?option_code(argv[i]+1):0;
        if(option==OPT_HELP){help();return 0;}
        if(option==OPT_LIVE)continue;
        if(!option||++i==argc||!option_value(option,argv[i],0)){help();return 1;}
        if(option==OPT_CONFIG)strcpy(config_path,argv[i]);
        if(option==OPT_ASSETS)strcpy(asset_path,argv[i]);
        if(option==OPT_LOCATION)location_arg=argv[i];
    }
#ifdef RPMT_BARE
    /* No shell flag: a connected VGA advances VSYNC. */
    {
        clock_t start=clock();
        unsigned char v=RIA.vsync;
        while(RIA.vsync==v&&(clock_t)(clock()-start)<CLOCKS_PER_SEC/5)rpmt_yield();
        if(RIA.vsync==v){rpmt_con_puts("sidera: requires VGA.\r\n");return 1;}
    }
#else
    if(!RPMT_GFX||!rpmt_out_tty()||!rpmt_in_tty()) {
        rpmt_con_puts("sidera: requires VGA and an interactive foreground console.\r\n");return 1;
    }
#endif
    r=config_load();
    if(r<0)strcpy(note,"Cannot read configuration. S saves current settings.");
    if(location_arg&&!cities_lookup(location_arg)){rpmt_con_puts(note);rpmt_con_puts("\r\n");return 1;}
    if(r!=1||location_arg)location_view();
    for(i=1;i<argc;++i) {
        option=option_code(argv[i]+1);
        if(option==OPT_LIVE)option_value(option,"",1);
        else {++i;if(option<OPT_LOCATION)option_value(option,argv[i],1);}
    }
#endif
    if(!assets_open()){rpmt_con_puts("sidera: missing or invalid sidera.dat.\r\n");return 1;}
#ifndef RPMT_BARE
    /* Do not touch shared graphics before the shell hands us the keyboard. */
    while(RPMT_FG_TASK!=rpmt_getpid())rpmt_sleep_ticks(1);
    rpmt_set_xram(0,XR_END);rpmt_set_swf(RPMT_SWF_CTRL_L|RPMT_SWF_GFX);
    rpmt_set_title("sidera");
#endif
    if(!ovl_select(OVL_COVER))goto done;
    show_cover();
#ifndef RPMT_BARE
    if(rpmt_sigterm())goto done;
#endif
    if(!assets_base()){assets_close();rpmt_con_puts("sidera: graphics file is truncated.\r\n");return 1;}
    if(live&&!now()){live=0;strcpy(note,"System clock unavailable. D sets UTC date.");}
    solar_hide();gx_clear(0,480);
    if(!gx_objects()||!gx_canvas()){gx_console();assets_close();rpmt_con_puts("sidera: cannot load sprite or configure VGA.\r\n");return 1;}
    gx_status(1); /* First astronomical calculation runs in the loop below. */
    trail_reset();tick=calc_tick=star_tick=rpmt_ticks();
#ifndef RPMT_BARE
    gen=rpmt_fg_gen();
#endif
    for(;;) {
        rpmt_yield();
#ifndef RPMT_BARE
        /* Shell restores only $0000-$BFFF. Reload upper assets and rebuild stars. */
        if(gen!=rpmt_fg_gen()) {
            gen=rpmt_fg_gen();
            if(!assets_base())break;
            cs_forget();trail_reset();
            if(!gx_canvas())break;
            if(cities_active)cities_draw();
            if(live)now();refresh=scene=head=1;
        }
        if(rpmt_sigterm())break;
#endif
        k=key_poll();
        if(solar_visible) {
            if(k=='q'||k=='Q')goto done;
            if(k=='x'||k=='X') {
                solar_hide();if(live)now();
                refresh=scene=head=star_check=1;tick=rpmt_ticks();k=0;
            } else {
                /* Modal snapshot: freeze all sky jobs and simulation, only LIVE clocks run. */
                if(live&&(unsigned)(rpmt_ticks()-tick)>=RPMT_TICK_HZ) {
                    tick=rpmt_ticks();
                    if(now())gx_clock();
                    else {live=0;strcpy(note,"Cannot read system clock.");}
                }
                rpmt_sleep_ticks(1);continue;
            }
        }
        if(cities_active) {
            if(k) {
                result=cities_key(k);
                if(result) {
                    if(!gx_canvas())break;
                    if(live)now();refresh=scene=head=1;
                    if(result==1)location_view();
                }
            }
            if(cities_active){rpmt_sleep_ticks(1);continue;}
        } else if(editing) {
            if(k) {
                r=editing;result=edit_key(k);
                if(result){if(result==1){if(r==1)location_view();refresh=1;}scene=head=1;}
            }
            if(editing){rpmt_sleep_ticks(1);continue;}
        } else {
            jump=0;
            switch(k) {
            case 'q':case 'Q':case K_ESC:goto done;
            case K_LEFT:bearing=(bearing+355)%360;scene=head=1;break;
            case K_RIGHT:bearing=(bearing+5)%360;scene=head=1;break;
            case K_LEFT|K_SHIFT:case K_RIGHT|K_SHIFT:
                if(gx_zoom(k==(K_LEFT|K_SHIFT)?-10:10))scene=head=1;break;
            case K_UP:if(elevation<54)elevation+=5;if(elevation>54)elevation=54;scene=head=1;break;
            case K_DOWN:if(elevation>-54)elevation-=5;if(elevation< -54)elevation=-54;scene=head=1;break;
            case 'h':case 'H':bearing=((bearing/45+1)*45)%360;scene=head=1;break;
            case 'n':case 'N':live=now();playing=0;refresh=head=1;break;
            case ' ':live=0;playing=!playing;head=star_check=1;break;
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
                gx_status(0);tick=rpmt_ticks();continue;
            case 'l':case 'L':if(!ovl_select(OVL_EDIT))goto done;edit_start(1);continue;
            case 'd':case 'D':if(!ovl_select(OVL_EDIT))goto done;edit_start(2);continue;
            case 'z':case 'Z':if(!ovl_select(OVL_EDIT))goto done;edit_start(3);continue;
            case 'k':case 'K':
                if(!ovl_select(OVL_CITIES))goto done;
                cities_init(argv[0]);
                if(cities_start()){if(!gx_canvas())goto done;cities_draw();continue;}
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
        if((unsigned)(rpmt_ticks()-tick)>=RPMT_TICK_HZ) {
            tick=rpmt_ticks();
            if(live){if(!now()){live=0;strcpy(note,"Cannot read system clock.");head=1;}clock_dirty=1;}
            else if(playing&&!gx_busy()&&!rs_busy()&&(!constellation_mode||!cs_busy())){if(!date_step(&moment,900))playing=0;refresh=head=1;}
        }
        if(live&&(unsigned)(rpmt_ticks()-calc_tick)>=60u*RPMT_TICK_HZ)refresh=1;
        if(live&&constellation_mode&&(unsigned)(rpmt_ticks()-star_tick)>=300u*RPMT_TICK_HZ)star_check=1;
        if(refresh&&(!gx_busy()||scene)) {
            gx_status(1);
            sky_compute(&moment,&observer,&sky);calc_tick=rpmt_ticks();
            if(planets){planets_compute(&moment,&observer,planet_positions);scene=1;}
            if(date_day(&moment)!=trail_day||memcmp(&observer,&trail_observer,sizeof(observer)))
                {trail_reset();scene=1;}
            if(gx_busy())scene=1;
            refresh=0;head=star_check=1;
        }
        if(star_check) {
            if(constellation_mode&&!cs_current()) {
                gx_status(1);cs_begin();star_tick=rpmt_ticks();
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
        if(!gx_busy())rpmt_sleep_ticks(1);
    }
done:
    if(ovl_loaded(OVL_CITIES))cities_close();
    tz_close();gx_console();assets_close();
    if(ovl_failed||exit_code){rpmt_con_puts(note);rpmt_con_puts("\r\n");return 1;}
    return 0;
}
