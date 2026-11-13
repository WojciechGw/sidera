/* Run the actual standalone event loop with a host RIA and ROM filesystem. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "host_platform.h"
#include "sidera.h"

static clock_t test_clock(void);
static time_t test_time(time_t *p);
static void checked_sky_compute(const Date *d,const Observer *o,Sky *s);
static void checked_sky_positions(const Date *d,const Observer *o,Sky *s);
static void checked_planets_compute(const Date *d,const Observer *o,Position *p);
#define sky_compute checked_sky_compute
#define sky_positions checked_sky_positions
#define planets_compute checked_planets_compute
#define clock test_clock
#define time test_time
#define main sidera_main
#include "../src/main.c"
#undef main
#undef clock
#undef time
#undef sky_compute
#undef sky_positions
#undef planets_compute
#include "../src/editor.c"

#ifdef SIDERA_OVERLAY_HOST
unsigned char ovl_memory[6144];
static unsigned overlay_reads;
static const char *failed_overlay;
static unsigned solar_test,solar_step;
static Date frozen_date;
static unsigned char frozen_frame[65536];
static unsigned frozen_sky,frozen_planets,frozen_stars,frozen_trails,frozen_done,frozen_overlays;
static int frozen_bearing,frozen_fov;
static unsigned char saved_overlay[OVL_BYTES];
#else
#define overlay_reads 0
#endif
uint8_t xram[65536];
HostRIA RIA;
static unsigned ticks,key_index,console_restored,opened,rom_reads,menu_seen,portal_writes;
static int locked,vga;
static unsigned cover_active,cover_start,cover_duration,cover_keys=4,cover_shown;
int rpmt_getc_nb(void){if(cover_keys){--cover_keys;return 'Q';}return -1;}
long host_lseek(int fd,long offset,int whence){assert(locked);return _lseek(fd,offset,whence);}
int host_read(int fd,void *p,unsigned n){assert(!locked);return _read(fd,p,n);}
static unsigned sky_checks,planet_checks,star_checks,idle_steps;
static unsigned trail_checks,render_polls;
static unsigned cache_test,cache_done,cache_checks,render_test;
static unsigned replay,previous_stars,previous_sky,previous_planets;
static unsigned zoom_step,zoom_noop;
static int expected_fov=120;
static unsigned char saved_trail[TRAIL_COUNT*8];
static time_t fake_time;
static time_t test_time(time_t *p){if(p)*p=fake_time;return fake_time;}
static const char *rom_dir,*save_path;
static char output[4096];
static const unsigned keys[]={'P','K',K_ESC,'G','G','G','G','C','K','W','a','r','s','a','w','\r','S','Z',K_ESC,'T','Q'};
static void expect_busy(unsigned char busy)
{
    const char *p="Working...";unsigned x=(live?604:playing?580:574)-66,row,col,c;
    while(*p) {
        c=(unsigned char)*p++;
        for(row=0;row<8;++row)for(col=0;col<6;++col)
            assert(!!(xram[(row+1)*80+(x+col)/8]&(128>>((x+col)&7)))==
                   !!(busy&&(xram[XR_FONT+c*8+row]&(128>>col))));
        x+=6;
    }
}
static void checked_sky_compute(const Date *d,const Observer *o,Sky *s)
{
    expect_busy(1);++sky_checks;sky_compute(d,o,s);
}
static void checked_sky_positions(const Date *d,const Observer *o,Sky *s)
{
    expect_busy(1);++trail_checks;sky_positions(d,o,s);
}
static void checked_planets_compute(const Date *d,const Observer *o,Position *p)
{
    expect_busy(1);++planet_checks;planets_compute(d,o,p);
}
static clock_t test_clock(void){static clock_t value;return ++value;}
void rpmt_os_lock(void){assert(!locked);locked=1;}
void rpmt_os_unlock(void){assert(locked);locked=0;}
void rpmt_yield(void){++ticks;if(vga)++RIA.vsync;}
unsigned rpmt_ticks(void){return ticks;}
void rpmt_sleep_ticks(unsigned n){ticks+=n;if(vga)++RIA.vsync;}
void rpmt_puts(const char *s){assert(strlen(output)+strlen(s)<sizeof(output));strcat(output,s);}
#ifdef SIDERA_OVERLAY_HOST
static unsigned solar_keys(void)
{
    static const unsigned ignored[]={'+','K','D','S',K_LEFT,'G',' ','N',K_ESC,12,'L','Z','P','T','C','[',']','-'};
    static unsigned ignored_index,clock_changes;
    unsigned a,x,y;
    if(!solar_visible&&(solar_step==0||solar_step==4)&&gx_busy())return 0;
    if(solar_visible&&solar_step==3) {
        assert(!memcmp(&solar_date,&frozen_date,sizeof(Date)));
        assert(sky_checks==frozen_sky&&planet_checks==frozen_planets&&star_checks==frozen_stars);
        assert(trail_checks==frozen_trails&&trail_done==frozen_done&&overlay_reads==frozen_overlays);
        assert(!cities_active&&!editing&&bearing==frozen_bearing&&fov==frozen_fov&&!grid&&!planets&&!trails);
        assert(solar_test==2?live:playing&&!live);
        if(solar_test==1)assert(!memcmp(&moment,&frozen_date,sizeof(Date)));
        else assert(memcmp(&moment,&frozen_date,sizeof(Date)));
        for(a=0;a<sizeof(xram);++a)if(xram[a]!=frozen_frame[a]) {
            y=a/80;x=(a%80)*8;
            assert(solar_test==2&&y>=21&&y<49&&x>=8&&x<192);
            ++clock_changes;
        }
    }
    switch(solar_step) {
    case 0:++solar_step;return solar_test==2?'N':' ';
    case 1:++solar_step;return 'X';
    case 2:
        assert(solar_visible&&ovl_loaded(OVL_SOLAR));frozen_date=solar_date;
        memcpy(frozen_frame,xram,sizeof(xram));
        frozen_sky=sky_checks;frozen_planets=planet_checks;frozen_stars=star_checks;
        frozen_trails=trail_checks;frozen_done=trail_done;frozen_overlays=overlay_reads;
        frozen_bearing=bearing;frozen_fov=fov;
        ++solar_step;fake_time+=61;ticks+=61*RPMT_TICK_HZ;return 0;
    case 3:
        if(ignored_index<sizeof(ignored)/sizeof(ignored[0])) {
            ++fake_time;ticks+=RPMT_TICK_HZ;return ignored[ignored_index++];
        }
        assert(solar_test==1||clock_changes);++solar_step;return 'X';
    case 4:
        assert(!solar_visible&&sky_checks>frozen_sky); /* full recalculation on close */
        assert(solar_test==2?live:playing);++solar_step;return 'X';
    case 5:
        assert(solar_visible&&overlay_reads==frozen_overlays);
        assert(!memcmp(&solar_date,&moment,sizeof(Date)));++solar_step;return 'Q';
    default:assert(0);return 'Q';
    }
}
#endif
unsigned key_poll(void)
{
    unsigned i;
#ifdef SIDERA_OVERLAY_HOST
    if(solar_test)return solar_keys();
    if(failed_overlay)return strstr(failed_overlay,"solar")?'X':strstr(failed_overlay,"edit")?'Z':'K';
#endif
    if(gx_busy()&&!editing&&!cities_active) {
        ++render_polls;expect_busy(1);
        switch(render_test) {
        case 1:assert(bearing==50);++render_test;return K_LEFT|K_SHIFT;
        case 2:assert(fov==110);++render_test;return 'G';
        case 3:case 4:case 5:assert(grid==render_test-2);++render_test;return 'G';
        case 6:assert(!grid);++render_test;return K_LEFT;
        case 7:assert(bearing==45);++render_test;return K_RIGHT|K_SHIFT;
        case 8:assert(fov==120);++render_test;fake_time+=1;ticks+=RPMT_TICK_HZ;return 0;
        case 9:
            assert(moment.second==1); /* clock serviced between drawing batches */
            assert(star_checks==previous_stars&&sky_checks==previous_sky&&planet_checks==previous_planets);
            assert(trail_done==TRAIL_COUNT);++render_test;++key_index;return 'Q';
        }
        return 0;
    }
    if(key_index==0)assert(bearing==90&&moment.hour==12); /* real local time, not simulation */
    if(key_index==15)fake_time+=6*3600;
    if(key_index==16)assert(bearing==180&&elevation==24&&moment.hour==12);
    if(key_index==18)expect_busy(0); /* zone editor suspends calculations */
    if(key_index==20) {
        assert(++idle_steps<2000);
        if(!cache_test&&trail_done>=13) {
            cache_done=trail_done;cache_checks=trail_checks;++cache_test;return 'T';
        }
        if(cache_test==1) {
            assert(!trails&&trail_done==cache_done&&trail_checks==cache_checks);
            ++cache_test;return 'T';
        }
        if(rs_busy()||cs_busy()||(trails&&!trail_end&&trail_done<TRAIL_COUNT)){expect_busy(1);return 0;}
        expect_busy(0); /* Both jobs completed, including drawing their results. */
        if(cache_test==2){cache_checks=trail_checks;++cache_test;return 'T';}
        if(cache_test==3){assert(!trails&&trail_done==TRAIL_COUNT);++cache_test;return 'T';}
        if(cache_test==4){assert(trail_checks==cache_checks);++cache_test;return 'Z';}
        if(cache_test==5){strcpy(edit,"Etc/UTC");++cache_test;return '\r';}
        if(cache_test==6){assert(trail_done==TRAIL_COUNT&&trail_checks==cache_checks);++cache_test;}
        if(!replay){previous_stars=star_checks;replay=1;}
        if(replay<=5){++replay;return 'C';} /* full cycle, including off/on */
        if(replay==6)replay=8; /* The two white-label states were removed. */
        if(replay<=10)assert(star_checks==previous_stars);
        if(replay==8||replay==9){++replay;return 'P';}
        if(replay==10) { /* Enter LIVE at a new time, requiring a fresh catalogue. */
            struct tm utc={0};utc.tm_year=126;utc.tm_mon=10;utc.tm_mday=9;utc.tm_hour=12;utc.tm_min=15;
            fake_time=_mkgmtime(&utc);++replay;return 'N';
        }
        if(replay==11) {
            assert(star_checks==previous_stars+SIDERA_STAR_COUNT*4);
            previous_stars=star_checks;previous_sky=sky_checks;previous_planets=planet_checks;
            ++replay;fake_time+=60;ticks+=60*RPMT_TICK_HZ;return 0;
        }
        if(replay<=15) {
            assert(star_checks==previous_stars);
            assert(sky_checks==previous_sky+replay-11&&planet_checks==previous_planets+replay-11);
            ++replay;fake_time+=60;ticks+=60*RPMT_TICK_HZ;return 0;
        }
        if(replay==16) {
            assert(star_checks==previous_stars+SIDERA_STAR_COUNT*4);
            assert(sky_checks==previous_sky+5&&planet_checks==previous_planets+5);
            assert(bearing==180); /* automatic LIVE updates do not rotate the view */
            fake_time+=4*3600;++replay;return 'L';
        }
        if(replay==17){++replay;return '\r';}
        if(replay==18){assert(bearing==270&&elevation==24);++replay;return 'L';}
        if(replay==19){bearing=45;++replay;return K_ESC;}
        assert(replay==20&&bearing==45);
        if(!zoom_step) {
            assert(fov==120&&elevation==24);
            previous_stars=star_checks;previous_sky=sky_checks;previous_planets=planet_checks;
            memcpy(saved_trail,xram+XR_TRAILS,sizeof(saved_trail));
        } else {
            assert(fov==expected_fov&&elevation==24);
            assert(star_checks==previous_stars&&sky_checks==previous_sky&&planet_checks==previous_planets);
            assert(trail_done==TRAIL_COUNT&&!memcmp(saved_trail,xram+XR_TRAILS,sizeof(saved_trail)));
            if(zoom_noop)assert(portal_writes<30000); /* no full repaint at the limits */
            else assert(portal_writes>=30000);
        }
        if(zoom_step<24) {
            int step=zoom_step<8||zoom_step>=20?-10:10;
            zoom_noop=expected_fov+step<50||expected_fov+step>160;
            if(!zoom_noop)expected_fov+=step;
            ++zoom_step;portal_writes=0;
            return step<0?K_LEFT|K_SHIFT:K_RIGHT|K_SHIFT;
        }
        assert(fov==120);
        assert(!render_test);render_test=1;return K_RIGHT;
    }
    #ifdef SIDERA_OVERLAY_HOST
    if(key_index==16) {
        assert(ovl_loaded(OVL_CITIES));
        memcpy(saved_overlay,ovl_memory,OVL_BYTES);
    }
    if(key_index==17) {
        assert(ovl_loaded(OVL_CITIES));
        assert(!memcmp(saved_overlay,ovl_memory,OVL_BYTES));
    }
#endif
    if(key_index==16)assert(!strcmp(note,"Warsaw [PL] (press S to save location)"));
    if(key_index==17) {
        assert(!note[0]);
        assert(!strcmp(location_name,"Warsaw [PL]"));
        for(i=472*80;i<480*80;++i)assert(!xram[i]);
    }
    if(key_index>=4&&key_index<=6)assert(portal_writes<30000);
    if(key_index==7)assert(portal_writes>=30000);
    if(key_index>=4&&key_index<=7)for(i=156;i<162;++i)
        assert(!!(xram[462*80+i/8]&(128>>(i&7)))==!!grid);
    portal_writes=0;
    if(cities_active)++menu_seen;
    assert(key_index<sizeof(keys)/sizeof(keys[0]));return keys[key_index++];
}
int rpmt_open(const char *path,int flags)
{
    char full[512];int fd;
    if(!strncmp(path,"ROM:",4)) {
        assert(flags==O_RDONLY);++rom_reads;
#ifdef SIDERA_OVERLAY_HOST
        if(strstr(path,".ovl"))++overlay_reads;
#endif
        sprintf(full,"%s/%s",rom_dir,path+4);path=full;
    } else assert(!strcmp(path,save_path));
    fd=_open(path,flags|_O_BINARY,_S_IREAD|_S_IWRITE);
    if(fd>=0)++opened;return fd;
}
int rpmt_close(int fd){assert(opened);--opened;return _close(fd);}
int read_xram(unsigned a,unsigned n,int fd){assert(locked&&a+n<=XR_END);return _read(fd,xram+a,n);}
void xr_write0(unsigned v)
{
    assert(locked&&RIA.addr0<XR_END);
    if((RIA.addr0>=XR_STARS0&&RIA.addr0<XR_STARS0+SIDERA_STAR_COUNT*4)||
       (RIA.addr0>=XR_STARS1&&RIA.addr0<XR_STARS1+SIDERA_STAR_COUNT*4)){expect_busy(1);++star_checks;}
    ++portal_writes;xram[RIA.addr0]=(uint8_t)v;RIA.addr0+=RIA.step0;
}
unsigned char xr_read0(void){unsigned char v;assert(locked);v=xram[RIA.addr0];RIA.addr0+=RIA.step0;return v;}
unsigned char xr_read1(void){unsigned char v;assert(locked);v=xram[RIA.addr1];RIA.addr1+=RIA.step1;return v;}
int xreg(int a,int b,int c,...)
{
    int canvas;va_list ap;assert(locked);va_start(ap,c);
    if(c==0) {
        canvas=va_arg(ap,int);
        if(!canvas)++console_restored;
        if(canvas==1){assert(!cover_shown);cover_active=cover_shown=1;cover_start=ticks;}
        if(canvas==3&&cover_active){cover_duration=ticks-cover_start;cover_active=0;}
    }
    va_end(ap);return 0;
}
static void local_directions(void)
{
    static const unsigned hours[]={0,3,4,9,10,15,16,21,22,23};
    static const int directions[]={0,0,90,90,180,180,270,270,0,0};
    unsigned summer,i;Date saved=moment;
    struct tm utc={0};
    assert(tz_set("Europe/Warsaw"));live=0;
    moment.year=1900;moment.hour=0;saved=moment;
    for(summer=0;summer<2;++summer)for(i=0;i<10;++i) {
        utc.tm_year=126;utc.tm_mon=summer?6:0;utc.tm_mday=15;
        utc.tm_hour=hours[i];utc.tm_min=59;utc.tm_sec=59;
        fake_time=_mkgmtime(&utc)-(summer?2:1)*3600;
        location_view();assert(bearing==directions[i]&&elevation==24);
        assert(!memcmp(&moment,&saved,sizeof(moment)));
    }
    assert(tz_set("Asia/Tokyo"));
    utc.tm_mon=0;utc.tm_mday=15;utc.tm_hour=20;utc.tm_min=utc.tm_sec=0;
    fake_time=_mkgmtime(&utc);location_view();assert(bearing==90); /* next local day, 05:00 */
    bearing=45;fake_time=(time_t)-1;location_view();assert(bearing==45);
    tz_close();strcpy(asset_path,"ROM:missing/sidera.dat");
    /* An invalid selected zone leaves the direction unchanged. */
    strcpy(zone_name,"Invalid/Zone");fake_time=_mkgmtime(&utc);
    location_view();assert(bearing==45);tz_close();
}
int main(int argc,char **argv)
{
    char *help_args[]={"MSC0:/sidera.rp6502","/help"};
    char *run_args[]={"MSC0:/sidera.rp6502","/date","2026-11-09 12:00:00",
                      "/location","PL","/config",NULL};
    Observer saved;FILE *f;struct tm start={0};
    
    
    host_assert_stderr();
    
    assert(argc==3||argc==4);rom_dir=argv[1];save_path=argv[2];run_args[6]=argv[2];
    remove(save_path);
#ifdef SIDERA_OVERLAY_HOST
    if(argc==4&&(!strcmp(argv[3],"solar-loop")||!strcmp(argv[3],"solar-live"))) {
        solar_test=!strcmp(argv[3],"solar-live")?2:1;vga=1;start.tm_year=126;start.tm_mon=10;start.tm_mday=9;start.tm_hour=4;
        fake_time=_mkgmtime(&start);
        assert(!sidera_main(7,run_args));
        assert(solar_step==6&&!opened&&!locked&&console_restored==1);
        assert(_access(save_path,0)!=0); /* S was ignored. */
        puts("PASS: modal X/Q, clocks-only LIVE, frozen simulation/sky, ignored keys and recalculation on close");
        return 0;
    }
    if(argc==4) {
        char *fail_args[]={"sidera.rp6502","/config",argv[2]};
        failed_overlay=argv[3];
        start.tm_year=126;start.tm_mon=10;start.tm_mday=9;start.tm_hour=4;
        fake_time=_mkgmtime(&start);vga=1;
        assert(sidera_main(3,fail_args)==1);
        assert((ovl_failed||strstr(argv[3],".dat"))&&!opened&&!locked&&console_restored==1);
        assert(strstr(output,argv[3]));
        assert(strstr(output,strstr(argv[3],".dat")?"Cannot read":"short payload"));
        puts("PASS: overlay failure restores console, closes files, exits nonzero");
        return 0;
    }
#endif
    assert(sidera_main(2,help_args)==0);
    assert(strstr(output,"Q returns to the monitor")&&!strstr(output,"Alt+Tab"));
    assert(!opened&&rom_reads==overlay_reads);
    output[0]=0;
    assert(sidera_main(7,run_args)==1); /* No VSYNC: fail before touching files. */
    assert(strstr(output,"requires VGA")&&rom_reads==overlay_reads&&!opened);
    start.tm_year=126;start.tm_mon=10;start.tm_mday=9;start.tm_hour=4;
    fake_time=_mkgmtime(&start);vga=1;output[0]=0;
    assert(sidera_main(7,run_args)==0);
    assert(cover_shown&&!cover_keys&&cover_duration>=625&&cover_duration<=627);
    assert(key_index==21&&menu_seen==8&&console_restored==1&&!grid);
    assert(render_test==10&&cache_test==7&&gx_busy()); /* Q exited a partial frame. */
    assert(trail_checks>=TRAIL_COUNT&&sky_checks&&planet_checks&&star_checks&&idle_steps>1&&render_polls>1);
    assert(trail_done==TRAIL_COUNT&&!cs_busy());
    assert(!locked&&!opened&&rom_reads-overlay_reads==5&&planets&&constellation_mode==1);
    assert(!strcmp(asset_path,"ROM:sidera.dat"));
    assert(!note[0]);
#ifdef SIDERA_OVERLAY_HOST
    assert(overlay_reads==6); /* help INIT, startup cities/INIT/cover, UI cities/edit, L edit cached */
    assert(ovl_select(OVL_INIT));
#endif
    saved=observer;observer.latitude=0;location_name[0]=0;assert(config_load()==1);
    assert(observer.latitude==saved.latitude&&observer.longitude==saved.longitude);
    assert(!strcmp(location_name,"Warsaw [PL]"));
    tz_close();
    f=fopen(save_path,"rb");assert(f);fclose(f);remove(save_path);
    local_directions();assert(!opened&&!locked);
    puts("PASS: bare loop, Shift+arrows FOV/bounds without astronomy recalculation, cities, G/P/K/C/S, Q cleanup");
    return 0;
}
