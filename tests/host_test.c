#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include "host_platform.h"
#include "sidera.h"
#include "solar.h"

Observer observer={1787346L,689597L,395};
Date moment={2026,11,17,14,0,0};
Sky sky;
int bearing=180,elevation=24;
unsigned char grid=1,trails=0,live=0,playing=0,trail_done=0;
Position trail[2][TRAIL_COUNT];
char note[100];
uint8_t xram[65536];
static uint8_t before_clock[65536];
HostRIA RIA;
static unsigned ticks;
static char rom_dir[512];
static const char *city_file;
static unsigned char fail_rom_zones;
static int fail_star_seek;
static unsigned file_reads;
static int checking_cities;
static unsigned char city_allowed[480];
long host_lseek(int fd,long offset,int whence)
{
    if(fail_star_seek&&offset>=DATA_VECTORS&&offset<DATA_EDGES){fail_star_seek=0;return -1;}
    return _lseek(fd,offset,whence);
}
int host_read(int fd,void *p,unsigned n){++file_reads;return _read(fd,p,n);}
static unsigned planet_mode;
static unsigned portal_writes;
static unsigned vector_reads;
static unsigned header_writes;
static int checking_header;
static unsigned char preserved_axes;

unsigned platform_ticks(void){++ticks;++RIA.vsync;return ticks;}
void xr_write0(unsigned v)
{
    assert(RIA.addr0<XR_END);
    if(preserved_axes&&RIA.addr0<480*80) {
        unsigned row=RIA.addr0/80,col=RIA.addr0%80,mask=0;
        if((preserved_axes&1)&&row>=71&&row<84&&col>=3&&col<=78)
            mask|=col==3?0x0F:col==78?0xF0:255;
        if((preserved_axes&2)&&row>=420&&row<448)mask=255;
        if((preserved_axes&4)&&row>=78&&row<418)
            mask|=col<3||col>78?255:col==3?0xF0:col==78?0x0F:0;
        /* Shared bytes may be updated, but protected pixels must never blink.
         * Fully protected bytes must not even be rewritten identically. */
        assert(mask!=255);
        assert(!((xram[RIA.addr0]^v)&mask));
    }
    if(checking_cities) {
        unsigned a=RIA.addr0,bit;
        assert(a<480*80&&city_allowed[a/80]);
        for(bit=0;bit<8;++bit)if((xram[a]^v)&(128>>bit))
            assert((a%80)*8+bit>CITIES_LEFT&&(a%80)*8+bit<CITIES_RIGHT-1);
    }
    if(checking_header && RIA.addr0<63*80) {
        assert(xram[XR_SUN_HEADER_CFG]==0xC0&&xram[XR_SUN_HEADER_CFG+1]==0xFF);
        assert(xram[XR_MOON_HEADER_CFG+12]==0xC0&&xram[XR_MOON_HEADER_CFG+13]==0xFF);
        ++header_writes;
    }
    ++portal_writes;xram[RIA.addr0]=(uint8_t)v;RIA.addr0+=RIA.step0;
}
unsigned char xr_read0(void){unsigned char v;v=xram[RIA.addr0];RIA.addr0+=RIA.step0;return v;}
unsigned char xr_read1(void){unsigned char v;v=xram[RIA.addr1];RIA.addr1+=RIA.step1;return v;}
int host_open(const char *p,int flags)
{
    char path[1024];
    if(!strncmp(p,"ROM:",4)) {
        if(!strcmp(p,"ROM:sidera.tz")&&fail_rom_zones)return -1;
        if(!strcmp(p,"ROM:sidera.cty")&&city_file)p=city_file;
        else {sprintf(path,"%s/%s",rom_dir,p+4);p=path;}
    }
    return _open(p,flags|_O_BINARY,_S_IREAD|_S_IWRITE);
}
int host_close(int fd){return _close(fd);}
void platform_puts(const char *p){}
int read_xram(unsigned a,unsigned n,int fd){assert(a+n<=XR_END);if(a==XR_VECTORS)++vector_reads;return _read(fd,xram+a,n);}
int xreg(int a,int b,int c,...)
{
    va_list ap;int mode,options,length,plane,top,bottom;unsigned cfg;
    if(c==1) {
        va_start(ap,c);mode=va_arg(ap,int);options=va_arg(ap,int);cfg=va_arg(ap,unsigned);
        if(cfg==XR_BODY_CFG) {
            length=va_arg(ap,int);plane=va_arg(ap,int);top=va_arg(ap,int);bottom=va_arg(ap,int);
            assert(mode==5&&options==56&&length==16&&plane==1&&top==VIEW_TOP&&bottom==416);
            ++planet_mode;
        }
        if(cfg==XR_MOON_CFG) {
            length=va_arg(ap,int);plane=va_arg(ap,int);top=va_arg(ap,int);bottom=va_arg(ap,int);
            assert(mode==4&&options==1&&length==1&&plane==2&&top==VIEW_TOP&&bottom==416);
        }
        va_end(ap);
    }
    return 0;
}
static unsigned word(unsigned a){return xram[a]|((unsigned)xram[a+1]<<8);}
static unsigned chart_sprite(unsigned image)
{
    unsigned a;
    for(a=XR_BODY_CFG;a<XR_BODY_CFG+7*10;a+=10)if(word(a+4)==image)return a;
    assert(!"Missing chart sprite");return 0;
}
#define XR_SUN_CFG chart_sprite(XR_SUN)
static int pixel(int x,int y){return !!(xram[y*80+x/8]&(0x80u>>(x&7)));}
static void gx_scene(void) { gx_scene_begin();while(gx_busy())gx_scene_step(); }
#include "editor_host.inc"
/* The full loop test uses a controlled clock; CLI-only tests use east. */
void location_view(void){bearing=90;}
static int command_line(int argc,char **argv) { return startup(argc,argv)<0; }
static void corrected_date(void)
{
    Date original=moment;unsigned i;
    unsigned char old_live=live,old_playing=playing;
    live=playing=1;edit_start(2);
    edit_key(K_HOME);for(i=0;i<6;++i)edit_key(K_RIGHT);
    edit_key(K_DEL);edit_key('3'); /* November -> invalid month 13. */
    assert(!edit_key('\r')&&editing==2);
    assert(!strcmp(note,"Invalid Gregorian date or time."));
    assert(moment.year==original.year&&moment.month==original.month&&moment.day==original.day);
    edit_key(K_BS);edit_key('1');
    assert(edit_key('\r')==1&&!editing);
    assert(!note[0]&&!live&&!playing);
    assert(moment.year==original.year&&moment.month==original.month&&moment.day==original.day);
    gx_footer();for(i=472*80;i<480*80;++i)assert(!xram[i]);
    live=old_live;playing=old_playing;
}
static void expect_text(int x,int y,const char *p,unsigned inverse)
{
    unsigned c,row,col;unsigned char bits;
    while(*p) {
        c=(unsigned char)*p++;
        for(row=0;row<8;++row) {
            bits=xram[XR_FONT+c*8+row];if(inverse)bits^=0xFC;
            for(col=0;col<6;++col) {
                if(pixel(x+col,y+row)!=!!(bits&(0x80u>>col)))fprintf(stderr,"Text mismatch at %d,%d: char %u\n",x,y,c);
                assert(pixel(x+col,y+row)==!!(bits&(0x80u>>col)));
            }
        }
        x+=6;if(inverse)--inverse;
    }
}
static void timezone_settings(void)
{
    unsigned i;char original_asset[160],line[120];Observer saved=observer;
    FILE *f;
    assert(tz_set("Europe/Warsaw"));edit_start(3);
    edit_key(K_HOME);while(edit[0])edit_key(K_DEL);
    strcpy(edit,"Unknown/Zone");cursor=(unsigned char)strlen(edit);
    assert(!edit_key('\r')&&editing==3&&!strcmp(zone_name,"Europe/Warsaw"));
    strcpy(edit,"Asia/Tokyo");cursor=(unsigned char)strlen(edit);
    assert(edit_key('\r')==1&&!strcmp(zone_name,"Asia/Tokyo"));
    edit_start(1);assert(edit_key('\r')==1&&!strcmp(zone_name,"Asia/Tokyo"));
    strcpy(config_path,"sidera-zone-test.cfg");assert(config_save());
    assert(tz_set("Etc/UTC"));assert(config_load()==1&&!strcmp(zone_name,"Asia/Tokyo"));
    location_format(line,&saved,1);
    f=fopen(config_path,"wb");assert(f);fprintf(f,"%s\r\nZone Invalid/Zone\r\n",line);fclose(f);
    assert(config_load()==-1&&!strcmp(zone_name,"Asia/Tokyo")&&observer.latitude==saved.latitude);
    f=fopen(config_path,"wb");assert(f);fprintf(f,"%s\r\n",line);fclose(f);
    assert(tz_set("Europe/Warsaw"));assert(config_load()==-1);remove(config_path);
    strcpy(original_asset,asset_path);strcpy(asset_path,"missing-zone-folder/sidera.dat");tz_close();fail_rom_zones=1;
    gx_clear(0,63);gx_clock();expect_text(38,31,"---- -- -- --:--:-- LOCAL",0);
    for(i=188;i<196;++i)assert(!pixel(i,31));
    fail_rom_zones=0;strcpy(asset_path,original_asset);assert(tz_set("Europe/Warsaw"));note[0]=0;
}
static void reload_assets(void)
{
    constellation_mode=3;cs_begin();while(cs_busy())cs_step();gx_scene();
    memcpy(before_clock,xram,sizeof(xram));
    memset(xram+0xC000,0,XR_END-0xC000); /* Simulate lost resource bytes before an explicit reload. */
    assert(assets_base());cs_begin();while(cs_busy())cs_step();gx_scene();
    assert(!memcmp(before_clock,xram,480*80));
    assert(!memcmp(before_clock+XR_PLANETS,xram+XR_PLANETS,XR_END-XR_PLANETS));
    constellation_mode=0;
}
static void catalogue_recovery(void)
{
    vector_reads=0;
    constellation_mode=3;cs_begin();while(cs_busy())cs_step();assert(vector_reads==12);gx_scene();
    memcpy(before_clock,xram,480*80);
    fail_star_seek=1;cs_begin();assert(cs_step()&&!cs_busy());
    assert(!strcmp(note,"Cannot read star catalogue."));
    gx_scene();assert(!memcmp(before_clock,xram,480*80)); /* keep last complete sky */
    cs_begin();assert(!cs_step());assert(note[0]); /* do not hide an unresolved error */
    while(cs_busy())cs_step();assert(!note[0]);
    gx_footer();
    {unsigned i;for(i=472*80;i<480*80;++i)assert(!xram[i]);}
    strcpy(note,"Location/zone changed (press S to save location)");
    cs_begin();while(cs_busy())cs_step();
    assert(!strcmp(note,"Location/zone changed (press S to save location)"));
    note[0]=0;constellation_mode=0;
}
static void catalogue_freshness(void)
{
    Date saved=moment;Observer place=observer;unsigned char was_live=live;
    live=1;assert(date_parse("2026-12-31 23:58:00",&moment));
    assert(!cs_current());cs_begin();assert(cs_current()&&cs_busy());
    while(cs_busy())cs_step();assert(cs_current());
    assert(date_step(&moment,299)&&cs_current()); /* across midnight and year */
    live=0;assert(!cs_current());live=1;
    assert(date_step(&moment,1)&&!cs_current());
    assert(date_step(&moment,-300)&&cs_current());
    assert(date_step(&moment,-1)&&!cs_current());
    assert(date_step(&moment,1));
    ++observer.longitude;assert(!cs_current());observer=place;
    ++observer.latitude;assert(!cs_current());observer=place;
    ++observer.altitude;assert(!cs_current());observer=place;
    live=0;assert(cs_current());assert(date_step(&moment,1)&&!cs_current());
    assert(date_parse("1900-01-01 00:00:00",&moment)&&!cs_current());
    assert(date_parse("2200-12-31 23:59:59",&moment)&&!cs_current());
    moment=saved;live=was_live;assert(!cs_current());
    cs_begin();while(cs_busy())cs_step();
}
static void header_layout(void)
{
    unsigned i,y;Observer saved=observer;Sky saved_sky=sky;char location[80];
    live=1;gx_header();expect_text(604,1," LIVE ",6);
    for(i=604;i<640;++i)assert(pixel(i,0));
    live=0;playing=1;gx_header();expect_text(580,1,"SIM / PLAY",0);
    for(i=604;i<640;++i)assert(!pixel(i,0));
    playing=0;gx_header();expect_text(574,1,"SIM / PAUSE",0);
    expect_text(0,1," SIDERA  The Celestial Observatory",8);
    for(i=0;i<48;++i)assert(pixel(i,0));assert(!pixel(48,0));
    expect_text(8,21,"Time 2026-11-17 14:00:00 UTC",0);
    expect_text(38,31,"2026-11-17 15:00:00 LOCAL",0);
    expect_text(38,41,"UTC+01:00",0);
    expect_text(532,21,"View 180\x01 / +24\x01",0);
    expect_text(580,31,"FOV 120\x01",0);
    expect_text(202,21,"Location ",0);
    expect_text(226,43,"Sun ",0);expect_text(412,43,"Moon ",0);
    sky.sun.az=sky.moon.az=0;sky.sun.alt=16384;sky.moon.alt=-16384;sky.illumination=1000;
    gx_header();
    expect_text(226,43,"Sun AZ 000.0\x01 ALT +90.0\x01",0);
    expect_text(412,43,"Moon AZ 000.0\x01 ALT -90.0\x01 LIT 100.0%",0);
    sky.sun.alt=sky.moon.alt=0;sky.illumination=8;gx_header();
    expect_text(226,43,"Sun AZ 000.0\x01 ALT +00.0\x01",0);
    expect_text(412,43,"Moon AZ 000.0\x01 ALT +00.0\x01 LIT   0.8%",0);
    for(y=43;y<51;++y) {
        for(i=370;i<379;++i)assert(!pixel(i,y));
        for(i=628;i<640;++i)assert(!pixel(i,y));
    }
    sky=saved_sky;
    observer.latitude=3240000L;observer.longitude=6480000L;observer.altitude=10000;
    gx_header();strcpy(location,"Location ");location_format(location+9,&observer,0);
    assert(202+strlen(location)*6<=526);
    expect_text(202,21,location,0);
    expect_text(8,21,"Time 2026-11-17 14:00:00 UTC",0);
    expect_text(532,21,"View 180\x01 / +24\x01",0);
    observer=saved;gx_header();
}
static void location_names(void)
{
    int saved_bearing=bearing;
    Observer saved=observer;char coords[100],contents[256],long_name[46],original_asset[160];
    char *args[3];
    FILE *f;unsigned i,n;
    strcpy(config_path,"sidera-name-test.cfg");
    strcpy(location_name,"Sosnowiec [PL]");gx_header();
    expect_text(256,31,"Sosnowiec [PL]",0);
    edit_start(1);assert(edit_key('\r')==1&&!strcmp(location_name,"Sosnowiec [PL]"));
    edit_start(1);assert(edit_key(K_ESC)==2&&location_name[0]);
    edit_start(1);strcpy(edit,"bad location");assert(!edit_key('\r')&&location_name[0]);
    assert(edit_key(K_ESC)==2);
    assert(config_save());location_name[0]=0;assert(config_load()==1);
    assert(!strcmp(location_name,"Sosnowiec [PL]"));
    f=fopen(config_path,"rb");assert(f);n=(unsigned)fread(contents,1,255,f);contents[n]=0;fclose(f);
    assert(strstr(contents,"m AMSL\r\nZone Europe/Warsaw\r\nCity Sosnowiec [PL]\r\n"));
    edit_start(1);edit[0]=edit[0]=='4'?'3':'4';assert(edit_key('\r')==1&&!location_name[0]);
    gx_header();expect_text(256,31,"             ",0);observer=saved;
    location_format(coords,&observer,1);
    /* Empty name, LF endings and longest catalogue name. */
    f=fopen(config_path,"wb");assert(f);fprintf(f,"%s\nZone Europe/Warsaw\nCity \nView 180,24 120 1 off off 0\nTime 2026-11-17 14:00:00\n",coords);fclose(f);
    strcpy(location_name,"old name");assert(config_load()==1&&!location_name[0]);
    memset(location_name,'X',39);strcpy(location_name+39," [PL]");
    assert(config_save());location_name[0]=0;assert(config_load()==1&&strlen(location_name)==44);
    gx_header();expect_text(256,31,location_name,0); /* deliberately under the Moon sprite */
    memset(long_name,'Y',45);long_name[45]=0;
    f=fopen(config_path,"wb");assert(f);fprintf(f,"%s\nZone Etc/UTC\nCity %s\n",coords,long_name);fclose(f);
    assert(config_load()==-1&&strlen(location_name)==44&&!strcmp(zone_name,"Europe/Warsaw"));
    /* Reject legacy notation without changing state or rewriting the file. */
    strcpy(strstr(coords,"m AMSL"),"m n.p.m.");
    f=fopen(config_path,"wb");assert(f);fprintf(f,"%s\nZone Etc/UTC\nCity Old\n",coords);fclose(f);
    assert(config_load()==-1&&strlen(location_name)==44&&!strcmp(zone_name,"Europe/Warsaw"));
    assert(observer.latitude==saved.latitude&&observer.longitude==saved.longitude);
    f=fopen(config_path,"rb");assert(f);n=(unsigned)fread(contents,1,255,f);contents[n]=0;fclose(f);
    assert(strstr(contents,"m n.p.m."));
    strcpy(original_asset,asset_path);args[0]=original_asset;args[1]="/config";args[2]=config_path;
    location_name[0]=0;
    assert(command_line(3,args)==0&&strstr(note,"Cannot read configuration"));
    assert(observer.latitude==saved.latitude&&observer.longitude==saved.longitude&&!location_name[0]);
    strcpy(asset_path,original_asset);
    assert(config_save()); /* Explicit S replaces the legacy file. */
    f=fopen(config_path,"rb");assert(f);n=(unsigned)fread(contents,1,255,f);contents[n]=0;fclose(f);
    assert(strstr(contents,"m AMSL")&&!strstr(contents,"n.p.m."));
    remove(config_path);bearing=saved_bearing;location_name[0]=note[0]=0;gx_header();gx_footer();
    for(i=472*80;i<480*80;++i)assert(!xram[i]);
}
static void header_sprites(void)
{
    unsigned char chart[70],moon[20];
    memcpy(chart,xram+XR_BODY_CFG,sizeof(chart));memcpy(moon,xram+XR_MOON_CFG,sizeof(moon));
    checking_header=1;header_writes=0;gx_header();checking_header=0;
    assert(header_writes>63*80&&!memcmp(chart,xram+XR_BODY_CFG,sizeof(chart)));
    assert(!memcmp(moon,xram+XR_MOON_CFG,sizeof(moon)));
    assert(word(XR_SUN_HEADER_CFG)==195&&word(XR_SUN_HEADER_CFG+2)==35);
    assert(!memcmp(xram+XR_SUN_CFG+4,xram+XR_SUN_HEADER_CFG+4,4));
    assert(word(XR_MOON_HEADER_CFG+12)==379&&word(XR_MOON_HEADER_CFG+14)==35);
    assert(!memcmp(xram+XR_MOON_CFG,xram+XR_MOON_HEADER_CFG,12));
    assert(!memcmp(xram+XR_MOON_CFG+16,xram+XR_MOON_HEADER_CFG+16,4));
}
static void calculation_status(void)
{
    unsigned mode,i,writes;int x;
    for(mode=0;mode<3;++mode) {
        live=mode==0;playing=mode==1;gx_status(0);gx_header();
        memcpy(before_clock,xram,sizeof(xram));
        portal_writes=0;gx_status(1);assert(portal_writes<2000);
        x=(mode==0?604:mode==1?580:574)-66;
        expect_text(x,1,"Working...",0);
        expect_text(x+60,1," ",0);
        expect_text(x+66,1,mode==0?" LIVE ":mode==1?"SIM / PLAY":"SIM / PAUSE",mode==0?6:0);
        for(i=0;i<sizeof(xram);++i)
            if(i>=9*80||i%80<60)assert(xram[i]==before_clock[i]);
        writes=portal_writes;gx_status(1);assert(writes==portal_writes);
        gx_header();expect_text(x,1,"Working...",0); /* redraw keeps busy state */
        gx_status(0);assert(!memcmp(before_clock,xram,sizeof(xram)));
    }
    live=1;gx_status(1);live=0;playing=1;gx_status(1);playing=0;gx_status(1);
    expect_text(508,1,"Working...",0);expect_text(574,1,"SIM / PAUSE",0);
    for(i=480;i<640;++i)assert(!pixel(i,0)); /* LIVE bar is erased */
    gx_status(0);gx_header();
}
static void footer_states(void)
{
    unsigned g,c,flags;
    unsigned char saved_grid=grid,saved_c=constellation_mode,saved_t=trails,saved_p=planets;
    for(g=0;g<4;++g)for(c=0;c<5;++c)for(flags=0;flags<4;++flags) {
        grid=g;constellation_mode=c;trails=flags&1;planets=flags&2;gx_footer();
        expect_text(114,462," grid ",g?255:0);
        expect_text(180,462," paths ",trails?255:0);
        expect_text(330,462," constellations ",c?255:0);
        expect_text(246,462," Planets ",planets?255:0);
        expect_text(102,462,"G:",0);expect_text(168,462,"T:",0);
        expect_text(318,462,"C:",0);expect_text(234,462,"P:",0);
        expect_text(498,450,"Z: Timezone",0);expect_text(438,462,"K: Cities",0);
    }
    grid=saved_grid;constellation_mode=saved_c;trails=saved_t;planets=saved_p;gx_footer();
}
static void depth_order(void)
{
    static const uint16_t distances[][7]={
        {1500,500,2000,5000,9000,19000,1024}, /* Venus in front, Mercury behind Sun */
        {500,1500,2000,5000,9000,19000,1024}, /* Mercury in front, Venus behind Sun */
        {1024,1024,1024,1024,1024,1024,1024} /* stable ties */
    };
    static const unsigned order[][7]={
        {5,4,3,2,0,6,1},{5,4,3,2,1,6,0},{0,1,2,3,4,5,6}
    };
    unsigned i,n,a,image;int x,y;
    unsigned char saved=planets;Sky saved_sky=sky;
    Position saved_positions[6];uint16_t saved_distances[7];
    memcpy(saved_positions,planet_positions,sizeof(saved_positions));
    memcpy(saved_distances,body_distance,sizeof(saved_distances));
    sky.sun.az=as_angle(bearing*1000L);sky.sun.alt=(int16_t)as_angle(elevation*1000L);
    sky.moon=sky.sun;
    for(i=0;i<6;++i)planet_positions[i]=sky.sun;
    assert(gx_project(&sky.sun,&x,&y));planets=1;
    for(n=0;n<3;++n) {
        memcpy(body_distance,distances[n],sizeof(body_distance));
        assert(gx_objects());assert(gx_objects()); /* stable across repeated redraws */
        for(i=0;i<7;++i) {
            a=XR_BODY_CFG+i*10;
            image=order[n][i]==6?XR_SUN:XR_PLANETS+order[n][i]*PLANET_BYTES;
            assert(word(a+4)==image);
            assert(word(a+6)==(order[n][i]==6?XR_PAL:XR_PLANET_PAL+order[n][i]*512u));
            assert((int16_t)word(a)==x-(order[n][i]==6?16:4)&&(int16_t)word(a+2)==y-(order[n][i]==6?16:4));
            assert(xram[a+8]==(order[n][i]==6?0x77:0x11)&&xram[a+9]==3);
        }
        assert((int16_t)word(XR_MOON_CFG+12)==x-16&&(int16_t)word(XR_MOON_CFG+14)==y-16);
        header_sprites(); /* ordering never moves the two header icons */
    }
    planets=0;assert(gx_objects());
    for(i=0;i<6;++i)assert((int16_t)word(chart_sprite(XR_PLANETS+i*PLANET_BYTES))<0);
    assert((int16_t)word(XR_SUN_CFG)==x-16);
    planets=saved;sky=saved_sky;
    memcpy(planet_positions,saved_positions,sizeof(saved_positions));
    memcpy(body_distance,saved_distances,sizeof(saved_distances));assert(gx_objects());
    puts("PASS: depth order, Mars/Jupiter, both inner-planet/Sun orders, ties, Moon layer, P off, header icons");
}
static void planet_display(const char *path)
{
    static const char *const names[]={"Mercury","Venus","Mars","Jupiter","Saturn","Uranus"};
    unsigned i,c;int x,y;FILE *f;char file[180];
    assert(!planets&&planet_mode);
    constellation_mode=0;grid=0;bearing=180;elevation=24;
    sky.sun.az=sky.moon.az=0;
    for(i=0;i<6;++i) {
        planet_positions[i].az=as_angle(135000L+i*18000L);
        planet_positions[i].alt=(int16_t)as_angle(i?30000L:-5000L);
    }
    assert(gx_objects());gx_scene();gx_header();gx_footer();
    memcpy(before_clock,xram,sizeof(xram));
    assert(planet_key('p')&&planets);assert(gx_objects());gx_scene();
    for(i=0;i<6;++i) {
        assert(gx_project(&planet_positions[i],&x,&y));
        c=chart_sprite(XR_PLANETS+i*PLANET_BYTES);
        assert((int16_t)word(c)==x-4&&(int16_t)word(c+2)==y-4);
        assert(xram[c+8]==0x11&&xram[c+9]==3);
        assert(word(c+4)==XR_PLANETS+i*PLANET_BYTES&&word(c+6)==XR_PLANET_PAL+i*512);
        expect_text(x+6,y-4,names[i],0);
        assert(gx_body_hit(x-4,y-4,8,8));
        assert(!gx_body_hit(x+6,y-4,(int)strlen(names[i])*6,8));
        assert(word(XR_PLANET_PAL+i*512)==0); /* transparent background */
    }
    assert(!memcmp(before_clock,xram,64*80));
    assert(!memcmp(before_clock+416*80,xram+416*80,(480-416)*80));
    sprintf(file,"%s.planet-icons",path);f=fopen(file,"wb");assert(f);
    assert(fwrite(xram,1,65536,f)==65536);fclose(f);
    assert(planet_key('P')&&!planets);assert(gx_objects());gx_scene();
    assert(!memcmp(before_clock,xram,sizeof(xram))); /* hides icons AND labels */
    /* Real sky, with moving planets, stars and below-horizon display. */
    observer.latitude=1787346L;observer.longitude=689597L;observer.altitude=395;
    assert(date_parse("2026-11-17 22:00:00",&moment));bearing=90;grid=2;
    assert(planet_key('P'));planets_compute(&moment,&observer,planet_positions);
    sky_compute(&moment,&observer,&sky);cs_begin();while(cs_busy())cs_step();
    constellation_mode=3;
    assert(gx_objects());gx_scene();gx_header();gx_footer();
    sprintf(file,"%s.planets",path);f=fopen(file,"wb");assert(f);
    assert(fwrite(xram,1,65536,f)==65536);fclose(f);
    for(i=XR_END;i<65536;++i)assert(xram[i]==0xA5);
}
static void label_styles(void)
{
    unsigned char black[10][8];int x,y;
    memset(xram,255,38400);gx_label(101,101,"A",0);
    for(y=0;y<10;++y)for(x=0;x<8;++x) {
        black[y][x]=(unsigned char)pixel(100+x,100+y);
        if(!x||x==7||!y||y==9)assert(!black[y][x]);
    }
    assert(pixel(99,100)&&pixel(108,100));
    gx_label(101,101,"A",1);
    for(y=0;y<10;++y)for(x=0;x<8;++x)assert(pixel(100+x,100+y)!=black[y][x]);
}
static void grid_modes(const char *path)
{
    unsigned char horizon[80];unsigned mode;FILE *f;char name[180];
    constellation_mode=0;grid=0;
    for(mode=0;mode<4;++mode) {
        assert(grid==mode);gx_scene();
        if(!mode)memcpy(horizon,xram+370*80,80);
        else {
            assert(!memcmp(horizon,xram+370*80,80));
            assert(pixel(28,120)&&pixel(29,120)&&pixel(30,120)&&!pixel(31,120));
            assert(pixel(628,120)&&pixel(629,120)&&pixel(630,120)&&!pixel(631,120));
            assert(pixel(328,VIEW_TOP-4)&&pixel(328,VIEW_TOP-3)&&pixel(328,VIEW_TOP-2));
            expect_text(316,71,"180\x01",0);
            assert(!pixel(328,VIEW_TOP-5));
            expect_text(0,114,"+50",0);
        }
        expect_text(12,364,"0",0);
        assert(pixel(19,364)&&pixel(20,364)); /* degree immediately after zero */
        assert(!pixel(6,367)); /* no leading plus sign */
        expect_text(28,361,"horizon",0);
        assert(!pixel(28,369));
        assert(!pixel(20,370)&&pixel(28,370)&&pixel(623,370)&&!pixel(628,370));
        if(mode<2)assert(!pixel(328,120));
        if(mode>=2)assert(pixel(328,120)&&pixel(328,118)&&pixel(328,122)&&
                         pixel(326,120)&&pixel(330,120)&&!pixel(329,119));
        if(mode==3)assert(pixel(338,120)&&pixel(328,130)&&!pixel(328,129)&&!pixel(338,130));
        assert(pixel(378,428)&&pixel(378,430)&&!pixel(378,431));
        assert(pixel(328,426)&&pixel(328,431)&&!pixel(328,432));
        assert(!pixel(328,432)&&pixel(326,433));
        assert(pixel(328,424)&&!pixel(328,425));
        gx_footer();sprintf(name,"%s.g%u",path,mode);f=fopen(name,"wb");assert(f);
        assert(fwrite(xram,1,65536,f)==65536);fclose(f);
        gx_grid_cycle();
    }
    assert(!grid);grid=1;
}
static void grid_incremental(void)
{
    static unsigned char initial[65536],incremental[65536];
    static const int views[]={180,185,0},tilts[]={24,54,-54};
    unsigned v,c,mode;int zoom;
    int old_bearing=bearing,old_elevation=elevation;
    unsigned char old_planets=planets;
    planets=1;planets_compute(&moment,&observer,planet_positions);
    for(zoom=50;zoom<=160;zoom+=10) {
        assert(gx_zoom(zoom-fov));
        for(v=0;v<3;++v)for(c=0;c<5;++c) {
            bearing=views[v];elevation=tilts[v];constellation_mode=c;
            grid=0;gx_scene();gx_header();memcpy(initial,xram,sizeof(xram));
            for(mode=1;mode<=3;++mode) {
                gx_grid_cycle();assert(grid==mode);portal_writes=0;gx_grid_add();
                assert(portal_writes<30000); /* Clearing the plot alone needs >30 KB writes. */
                memcpy(incremental,xram,sizeof(xram));
                gx_scene();assert(!memcmp(incremental,xram,sizeof(xram)));
                gx_header();assert(!memcmp(incremental,xram,sizeof(xram))); /* top scale survives */
            }
            gx_grid_cycle();assert(!grid);gx_scene();assert(!memcmp(initial,xram,sizeof(xram)));
        }
    }
    assert(gx_zoom(120-fov));
    bearing=old_bearing;elevation=old_elevation;planets=old_planets;constellation_mode=0;grid=1;
}
static void zoom_projection(const char *path)
{
    int zoom,view,tilt,az,alt,x,y,dx,sx,sy;
    Position p;Sky saved_sky=sky;int saved_bearing=bearing,saved_elevation=elevation;
    Position saved_trail[2];
    unsigned i;char name[180],label[16];FILE *file;
    memcpy(saved_trail,trail[0],sizeof(saved_trail));
    assert(fov==120);
    for(zoom=50;zoom<=160;zoom+=10) {
        assert(gx_zoom(zoom-fov)&&fov==zoom);
        for(view=0;view<360;view+=5)for(tilt=-54;tilt<=54;tilt+=54) {
            bearing=view;elevation=tilt;
            for(az=0;az<360;az+=10)for(alt=-90;alt<=90;alt+=10) {
                p.az=as_angle(az*1000L);p.alt=(int16_t)as_angle(alt*1000L);
                dx=(az-view+540)%360-180;
                gx_project(&p,&x,&y);
                assert(abs(x-(VIEW_CENTER+dx*600/zoom))<=2);
                assert(abs(y-(VIEW_MIDDLE+(tilt-alt)*600/zoom))<=2);
            }
        }
        /* The same angular separation has the same horizontal/vertical scale. */
        bearing=0;elevation=0;p.az=0;p.alt=0;
        assert(gx_project(&p,&sx,&sy)&&sx==VIEW_CENTER&&sy==VIEW_MIDDLE);
        p.az=as_angle(10000);p.alt=(int16_t)p.az;
        assert(gx_project(&p,&x,&y)&&x-sx==sy-y);
        p.az=as_angle(350000);p.alt=0;
        assert(gx_project(&p,&x,&y)&&x<VIEW_CENTER); /* north seam */
        sky.sun=sky.moon=p;assert(gx_objects());
        assert((int16_t)word(XR_SUN_CFG)==x-16);
        assert((int16_t)word(XR_MOON_CFG+12)==x-16);
        assert(word(XR_SUN_CFG+4)==XR_SUN&&xram[XR_MOON_CFG+18]==5);
        p.az=as_angle(180000);assert(!gx_project(&p,&x,&y));
        sky.sun=sky.moon=p;assert(gx_objects());
        assert((int16_t)word(XR_SUN_CFG)<0&&(int16_t)word(XR_MOON_CFG+12)<0);
        /* Trail discontinuity is an angular limit, independent of pixel scale. */
        trail[0][0].az=as_angle(-14500);trail[0][1].az=as_angle(14500);
        trail[0][0].alt=trail[0][1].alt=0;
        gx_write(XR_TRAILS,&trail[0][0],4);gx_write(XR_TRAILS+8,&trail[0][1],4);
        gx_clear(VIEW_TOP,VIEW_BOTTOM-VIEW_TOP);gx_trail(0,1);
        assert(pixel(VIEW_CENTER,VIEW_MIDDLE));
        trail[0][0].az=as_angle(-15500);trail[0][1].az=as_angle(15500);
        gx_write(XR_TRAILS,&trail[0][0],4);gx_write(XR_TRAILS+8,&trail[0][1],4);
        gx_clear(VIEW_TOP,VIEW_BOTTOM-VIEW_TOP);portal_writes=0;gx_trail(0,1);
        assert(!portal_writes);
        /* The plot, header icons and fixed-width FOV field keep their positions. */
        sky=saved_sky;bearing=saved_bearing;elevation=saved_elevation;grid=3;
        gx_scene();gx_header();assert(gx_objects());
        sprintf(label,"FOV %3d\x01",zoom);expect_text(580,31,label,0);
        assert(word(XR_SUN_HEADER_CFG)==195&&word(XR_MOON_HEADER_CFG+12)==379);
        if(zoom==50||zoom==120||zoom==160) {
            sprintf(name,"%s.fov%d",path,zoom);file=fopen(name,"wb");assert(file);
            assert(fwrite(xram,1,sizeof(xram),file)==sizeof(xram));fclose(file);
        }
        for(i=XR_END;i<sizeof(xram);++i)assert(xram[i]==0xA5);
    }
    memcpy(before_clock,xram,sizeof(xram));portal_writes=0;
    assert(!gx_zoom(10)&&fov==160&&!portal_writes);
    assert(!memcmp(before_clock,xram,sizeof(xram)));
    assert(gx_zoom(-110)&&fov==50);assert(!gx_zoom(-10)&&fov==50);
    assert(gx_zoom(70)&&fov==120);grid=1;
    memcpy(trail[0],saved_trail,sizeof(saved_trail));
    puts("PASS: all 12 FOVs, angular scale, north seam, sprite size, fixed header and zoom bounds");
}
static void projected_catalogue(void)
{
    Date saved_date=moment;
    int saved_bearing=bearing,saved_elevation=elevation,saved_fov=fov;
    unsigned char saved_c=constellation_mode;
    unsigned i,angles,points;int zoom,x,y;Position p;
    constellation_mode=3;
    for(zoom=50;zoom<=160;zoom+=10) {
        assert(date_step(&moment,300));cs_begin();
        memcpy(before_clock,xram,sizeof(xram));
        assert(cs_project_step()); /* Inactive buffer belongs to the calculation. */
        assert(!memcmp(before_clock,xram,sizeof(xram)));
        while(cs_busy())cs_step();
        assert(gx_zoom(zoom-fov));bearing=zoom%20?359:0;elevation=zoom%20?54:-54;
        memcpy(before_clock,xram,sizeof(xram));gx_scene_begin();gx_scene_step();
        assert(!memcmp(before_clock,xram,480*80)); /* Prepare before clearing. */
        while(gx_busy())gx_scene_step();
        angles=!memcmp(before_clock+XR_STARS0,xram+XR_STARS0,SIDERA_STAR_COUNT*4)?XR_STARS0:XR_STARS1;
        points=angles==XR_STARS0?XR_STARS1:XR_STARS0;
        assert(!memcmp(before_clock+angles,xram+angles,SIDERA_STAR_COUNT*4));
        for(i=0;i<SIDERA_STAR_COUNT;++i) {
            memcpy(&p,xram+angles+i*4,4);gx_project(&p,&x,&y);
            assert((int16_t)word(points+i*4)==x&&(int16_t)word(points+i*4+2)==y);
        }
    }
    moment=saved_date;bearing=saved_bearing;elevation=saved_elevation;gx_zoom(saved_fov-fov);
    cs_begin();while(cs_busy())cs_step();constellation_mode=saved_c;gx_scene();
    puts("PASS: exact projected cache, 12 FOVs, horizon/wrap, time invalidation, inactive-buffer ownership");
}
static void view_header(void)
{
    int saved_bearing=bearing,saved_elevation=elevation,saved_fov=fov;
    unsigned i,bit;int x,y;
    elevation=24;gx_header();memcpy(before_clock,xram,sizeof(xram));
    bearing=123;elevation=9;gx_zoom(110-fov);gx_view();
    expect_text(532,21," View 123\x01 / +9\x01",0);
    expect_text(580,31,"FOV 110\x01",0);
    for(i=0;i<sizeof(xram);++i)for(bit=0;bit<8;++bit)
        if((before_clock[i]^xram[i])&(0x80u>>bit)) {
            x=(int)(i%80)*8+bit;y=(int)(i/80);
            assert((x>=532&&x<628&&y>=21&&y<29)||(x>=580&&x<628&&y>=31&&y<39));
        }
    bearing=saved_bearing;elevation=saved_elevation;gx_zoom(saved_fov-fov);gx_header();
}
static void interactive_scene(void)
{
    int saved_bearing=bearing;
    unsigned char saved_c=constellation_mode;
    constellation_mode=3;bearing=(bearing+5)%360;
    cs_begin();assert(!cs_step()&&cs_busy());
    memcpy(before_clock,xram,sizeof(xram));
    gx_scene_interactive();while(gx_busy())gx_scene_step();
    assert(cs_busy()); /* Drawing must not consume/overwrite the pending catalogue. */
    assert(!memcmp(before_clock+XR_STARS0,xram+XR_STARS0,SIDERA_STAR_COUNT*4));
    assert(!memcmp(before_clock+XR_STARS1,xram+XR_STARS1,SIDERA_STAR_COUNT*4));
    while(cs_busy())cs_step();
    gx_scene_interactive();while(gx_busy())gx_scene_step();
    memcpy(before_clock,xram,480*80);gx_scene();
    assert(!memcmp(before_clock,xram,480*80)); /* Same final pixels, without prepass. */
    bearing=saved_bearing;constellation_mode=saved_c;gx_scene();
    puts("PASS: immediate drawing preserves pending catalogue and matches prepared rendering");
}
static void staged_scene(void)
{
    unsigned steps,writes;int saved_bearing=bearing;
    unsigned char saved_c=constellation_mode,saved_grid=grid;
    constellation_mode=3;grid=3;
    gx_scene_begin();assert(gx_busy());
    for(steps=0;steps<3;++steps)gx_scene_step();
    assert(gx_busy()); /* Interrupt and restart for the newest view. */
    bearing=(bearing+45)%360;gx_scene_begin();
    steps=0;
    while(gx_busy()) {
        portal_writes=0;gx_scene_step();writes=portal_writes;
        assert(writes<20000);assert(++steps<1800); /* Includes 759 cached projections. */
    }
    assert(steps>10);
    memcpy(before_clock,xram,sizeof(xram));gx_scene();
    assert(!memcmp(before_clock,xram,sizeof(xram)));
    bearing=saved_bearing;constellation_mode=saved_c;grid=saved_grid;
    puts("PASS: bounded drawing batches; interrupted/restarted frame equals a fresh frame");
}
static void check_axes_scene(unsigned char keep)
{
    preserved_axes=keep;gx_scene();preserved_axes=0;
    memcpy(before_clock,xram,480*80);
    gx_clear(71,377);gx_scene();
    assert(!memcmp(before_clock,xram,480*80));
}
static void preserved_scales(void)
{
    int zoom,step,saved_bearing=bearing,saved_elevation=elevation,saved_fov=fov;
    unsigned char saved_c=constellation_mode,saved_g=grid,saved_p=planets;
    unsigned char saved_live=live,saved_playing=playing;
    Date saved_date=moment;
    constellation_mode=3;planets=1;
    for(zoom=50;zoom<=160;zoom+=10) {
        gx_zoom(zoom-fov);grid=(zoom/10)%4;bearing=355;elevation=24;gx_scene();
        elevation=29;check_axes_scene(3); /* Both compasses survive tilt. */
        bearing=0;check_axes_scene(4); /* Side scales survive wraparound. */
        live=1;playing=0;assert(date_step(&moment,60));
        sky_compute(&moment,&observer,&sky);planets_compute(&moment,&observer,planet_positions);
        cs_begin();while(cs_busy())cs_step();check_axes_scene(7);
        live=0;playing=1;assert(date_step(&moment,300));
        sky_compute(&moment,&observer,&sky);planets_compute(&moment,&observer,planet_positions);
        cs_begin();while(cs_busy())cs_step();check_axes_scene(7);
        /* Repeated keys can interrupt clearing, grid drawing or catalogue work.
         * Return to a formerly cached view before finishing the last frame. */
        for(step=1;step<=140;step+=13) {
            unsigned n;
            bearing=step%360;elevation=step%109-54;
            gx_zoom((step&1?50:160)-fov);grid=step%4;
            gx_scene_interactive();
            for(n=0;n<(unsigned)step&&gx_busy();++n)gx_scene_step();
        }
        bearing=0;elevation=29;gx_zoom(zoom-fov);grid=(zoom/10)%4;
        check_axes_scene(0);
        gx_axes_invalidate();check_axes_scene(0); /* Explicit Ctrl+L refresh. */
    }
    moment=saved_date;live=saved_live;playing=saved_playing;
    bearing=saved_bearing;elevation=saved_elevation;gx_zoom(saved_fov-fov);
    constellation_mode=saved_c;grid=saved_g;planets=saved_p;
    sky_compute(&moment,&observer,&sky);planets_compute(&moment,&observer,planet_positions);
    cs_begin();while(cs_busy())cs_step();gx_scene();
    puts("PASS: untouched compass/scale pixels and bytes; LIVE/SIM, 12 FOVs, G modes, interrupted redraws");
}
static void daylight_compass(void)
{
    Date saved=moment;
    unsigned i;
    int zoom,saved_fov=fov,saved_bearing=bearing;
    static unsigned char daytime[480*80];
    assert(rs_daylight());
    assert(!pixel(0,425)&&pixel(0,426)&&pixel(639,440)&&!pixel(639,441)&&pixel(VIEW_CENTER,424));
    expect_text(VIEW_CENTER-3,433,"S",1);
    memcpy(daytime,xram,sizeof(daytime));
    /* Exact minute boundaries from the Warsaw fixture, expressed in UTC.
     * Updating daylight alone must touch only the 15-row compass band. */
    moment.hour=14;moment.minute=50;moment.second=59;
    portal_writes=0;gx_daylight();assert(!portal_writes);
    assert(date_step(&moment,1));assert(!rs_daylight());gx_daylight();
    assert(portal_writes==15*80);
    for(i=0;i<sizeof(daytime);++i)
        assert(xram[i]==(i>=426*80&&i<441*80?(daytime[i]^255):daytime[i]));
    expect_text(VIEW_CENTER-3,433,"S",0);
    assert(date_step(&moment,-1));gx_daylight();assert(!memcmp(daytime,xram,sizeof(daytime)));
    moment.hour=5;moment.minute=46;moment.second=59;gx_daylight();
    assert(!pixel(0,426)&&pixel(VIEW_CENTER,424));
    assert(date_step(&moment,1));gx_daylight();assert(!memcmp(daytime,xram,sizeof(daytime)));
    /* Daytime view changes and interrupted clears must never double-invert. */
    for(zoom=50;zoom<=160;zoom+=10) {
        gx_zoom(zoom-fov);bearing=zoom%20?359:0;
        gx_scene_interactive();gx_scene_step();
        moment.hour=20;gx_daylight();moment.hour=12;gx_daylight();
        gx_scene_interactive();while(gx_busy())gx_scene_step();
        assert(!pixel(0,425)&&pixel(0,426)&&pixel(639,440)&&!pixel(639,441));
        memcpy(daytime,xram,sizeof(daytime));gx_clear(71,377);gx_scene();
        assert(!memcmp(daytime,xram,sizeof(daytime)));
        gx_clear(440,1);gx_scene(); /* The white padding is cached content too. */
        assert(!memcmp(daytime,xram,sizeof(daytime)));
    }
    moment=saved;bearing=saved_bearing;gx_zoom(saved_fov-fov);gx_scene();
    puts("PASS: sunrise/sunset band inversion, backward time, unchanged V/plot, 12 FOVs and interrupted clears");
}
static void trail_label_repair(unsigned char mode)
{
    static unsigned char initial[38400],incremental[38400];
    unsigned n,b,partial=0,full=0;
    int x,y,px,py,saved_bearing=bearing,saved_elevation=elevation;
    unsigned char saved_c=constellation_mode,saved_grid=grid,saved_p=planets;
    Position p[2];
    grid=0;constellation_mode=mode;planets=1;bearing=mode==4?0:90;elevation=54;
    planets_compute(&moment,&observer,planet_positions);gx_scene();
    cs_labels(0);gx_planet_labels();
    for(n=0;n<97;++n)for(b=0;b<2;++b) {
        p[b].az=as_angle((int32_t)(bearing-48+(int)n)*1000);
        p[b].alt=(int16_t)as_angle((int32_t)(30+b*20+(n%5)*4)*1000);
        gx_write(XR_TRAILS+n*8+b*4,&p[b],4);
    }
    for(n=1;n<97;++n) {
        memcpy(initial,xram,sizeof(initial));portal_writes=0;
        gx_trail(0,n);gx_trail(1,n);partial+=portal_writes;
        memcpy(incremental,xram,sizeof(incremental));memcpy(xram,initial,sizeof(initial));
        portal_writes=0;
        for(b=0;b<2;++b) {
            gx_read(XR_TRAILS+n*8+b*4,&p[0],4);gx_read(XR_TRAILS+(n-1)*8+b*4,&p[1],4);
            if(gx_project(&p[0],&x,&y)&&gx_project(&p[1],&px,&py))gx_line(px,py,x,y,b);
        }
        cs_labels(0);gx_planet_labels();full+=portal_writes;
        assert(!memcmp(incremental,xram,sizeof(incremental)));
    }
    assert(partial<full/2);
    printf("PASS: C%u selective trail label repair matches full repaint; %u vs %u portal writes\n",mode,partial,full);
    bearing=saved_bearing;elevation=saved_elevation;constellation_mode=saved_c;grid=saved_grid;planets=saved_p;
}
static void type_city(const char *p)
{
    while(*p)assert(!cities_key((unsigned char)*p++));
}
static void city_count(const char *text)
{
    expect_text(CITIES_RIGHT-6-(int)strlen(text)*6,CITIES_BOTTOM-60,text,0);
}
static void city_bounds(void)
{
    unsigned a,bit;
    for(a=0;a<480*80u;++a)for(bit=0;bit<8;++bit)
        if((before_clock[a]^xram[a])&(128>>bit)) {
            int x=(a%80)*8+bit,y=a/80;
            assert(x>=CITIES_LEFT&&x<CITIES_RIGHT&&y>=CITIES_TOP&&y<CITIES_BOTTOM);
        }
    for(a=CITIES_LEFT;a<CITIES_RIGHT;++a)
        assert(pixel(a,CITIES_TOP)&&pixel(a,CITIES_BOTTOM-1));
    for(a=CITIES_TOP;a<CITIES_BOTTOM;++a)
        assert(pixel(CITIES_LEFT,a)&&pixel(CITIES_RIGHT-1,a));
    expect_text(CITIES_LEFT+2,CITIES_TOP+12," CITIES ",255);
}
static void city_key_checked(unsigned key,unsigned kind,unsigned old_row,unsigned new_row)
{
    unsigned writes;
    memset(city_allowed,0,sizeof(city_allowed));
    if(kind)memset(city_allowed+CITIES_BOTTOM-46,1,8);
    if(kind==1||kind==3)memset(city_allowed+CITIES_TOP+28,1,8);
    if(kind>=2)memset(city_allowed+CITIES_BOTTOM-60,1,8);
    if(kind==2) {
        memset(city_allowed+CITIES_TOP+59+old_row*14,1,10);memset(city_allowed+CITIES_TOP+59+new_row*14,1,10);
    }
    if(kind==3)memset(city_allowed+CITIES_TOP+59,1,14*14);
    checking_cities=1;portal_writes=file_reads=0;
    assert(!cities_key(key));checking_cities=0;writes=portal_writes;
    if(!kind)assert(!writes&&!file_reads);
    if(kind==1)assert(!file_reads&&writes<3000);
    if(kind==2)assert(file_reads<=3&&writes<8000);
    memcpy(before_clock,xram,sizeof(xram));cities_draw();
    assert(!memcmp(before_clock,xram,sizeof(xram))); /* identical to a fresh full draw */
}
static void city_incremental(void)
{
    memcpy(before_clock,xram,sizeof(xram));
    assert(cities_start());cities_draw();city_bounds();
    city_key_checked(K_UP,0,0,0);city_key_checked(K_BS,0,0,0);
    city_key_checked(K_LEFT,0,0,0);
    city_key_checked(K_DOWN,2,0,1);
    city_key_checked('s',1,0,0);
    city_count("2 / 345");
    city_key_checked('o',3,0,0);
    city_key_checked('s',3,0,0);city_count("1 / 1");
    expect_text(CITIES_LEFT+6,CITIES_TOP+60,"PL Poland - Sosnowiec",255);
    city_key_checked(K_BS,3,0,0);city_key_checked(K_BS,3,0,0);city_count("1 / 345");
    city_key_checked(K_BS,1,0,0);
    city_key_checked('p',1,0,0);city_key_checked('l',3,0,0);city_count("1 / 20");
    city_key_checked(K_BS,3,0,0);city_key_checked(K_BS,1,0,0);
    city_key_checked('u',1,0,0);city_key_checked('s',3,0,0);city_count("1 / 8");
    expect_text(CITIES_LEFT+6,CITIES_TOP+60,"US United States - Washington [Capital]",255);
    city_key_checked(K_BS,3,0,0);city_key_checked(K_BS,1,0,0);
    city_key_checked('l',1,0,0);city_key_checked('o',3,0,0); /* non-code: match city/country names */
    expect_text(CITIES_LEFT+6,CITIES_TOP+60,"BR Brazil - Sao Paulo",255);
    city_key_checked(K_BS,3,0,0);city_key_checked(K_BS,1,0,0);
    city_key_checked('z',1,0,0);city_key_checked('z',3,0,0);city_key_checked('z',3,0,0);
    expect_text(CITIES_LEFT+6,CITIES_TOP+60,"No matching cities.",0);city_count("0 / 0");
    city_key_checked('\r',0,0,0);city_key_checked(K_DOWN,0,0,0);
    city_key_checked(K_BS,3,0,0);city_key_checked(K_BS,3,0,0);city_key_checked(K_BS,1,0,0);
    city_key_checked(K_PGDN,3,0,0);city_count("15 / 345");
    city_key_checked(K_HOME,3,0,0);city_key_checked(K_END,3,0,0);
    city_count("345 / 345");city_key_checked(K_DOWN,0,0,0);
    city_key_checked(K_UP,2,8,7);
    assert(cities_key(K_ESC)==2);
}
static void city_catalogue(const char *asset,const char *output)
{
    Observer saved,chosen,albania;
    unsigned modes,i;
    char name[180],config[]="sidera-cities-test.cfg";
    char *args[5];FILE *f;
    city_file=NULL;
    city_incremental();
    assert(cities_lookup("pl"));saved=observer;
    assert(!strcmp(location_name,"Warsaw [PL]"));
    assert(observer.latitude>52*36000L&&observer.latitude<53*36000L);
    assert(observer.longitude>21*36000L&&observer.longitude<22*36000L);
    assert(cities_lookup("warsaw")&&observer.latitude==saved.latitude);
    assert(cities_lookup("PL/Sosnowiec")&&observer.latitude==1810326L&&observer.longitude==687739L&&observer.altitude==256);
    assert(cities_lookup("PL/Radziechowy")&&observer.latitude==1787274L&&observer.longitude==688721L&&observer.altitude==437);
    assert(!strcmp(zone_name,"Europe/Warsaw"));
    assert(cities_lookup("Torun")&&observer.latitude==1908495L);
    assert(cities_lookup("Zielona Gora")&&observer.latitude==1869677L);
    assert(cities_lookup("US")&&observer.longitude< -77*36000L&&observer.longitude> -78*36000L);
    assert(cities_lookup("New York")&&observer.latitude>40*36000L&&observer.latitude<41*36000L);
    assert(cities_lookup("Los Angeles")&&observer.longitude< -118*36000L);
    assert(!strcmp(location_name,"Los Angeles [US]"));
    chosen=observer;
    assert(!cities_lookup("St. John's")&&strstr(note,"Ambiguous"));
    assert(!strcmp(location_name,"Los Angeles [US]"));
    assert(observer.latitude==chosen.latitude&&observer.longitude==chosen.longitude);
    assert(cities_lookup("ca/St. John's")&&observer.latitude>47*36000L);
    assert(!cities_lookup("not a city"));
    assert(cities_lookup("AL"));albania=observer;
    strcpy(config_path,config);observer=saved;assert(config_save());
    args[0]=(char *)asset;args[1]="/location";args[2]="Los Angeles";
    args[3]="/config";args[4]=config;
    assert(command_line(5,args)==0&&observer.latitude==chosen.latitude);
    args[1]="/config";args[2]=config;args[3]="/location";args[4]="pl";
    assert(command_line(5,args)==0&&observer.latitude==saved.latitude);
    args[4]="St. John's";assert(command_line(5,args)==1&&strstr(note,"Ambiguous"));
    args[4]="PL/Radziechowy";assert(command_line(5,args)==0&&observer.altitude==437);
    assert(cities_start());cities_draw();type_city("Sosnowiec");
    assert(cities_key('\r')==1&&observer.latitude==1810326L&&observer.altitude==256);
    assert(!strcmp(note,"Sosnowiec [PL] (press S to save location)"));
    assert(!strcmp(location_name,"Sosnowiec [PL]"));
    assert(cities_start());cities_draw();type_city("Poland");
    assert(!cities_key(K_END));city_count("20 / 20");
    sprintf(name,"%s.poland",output);f=fopen(name,"wb");assert(f);
    assert(fwrite(xram,1,65536,f)==65536);fclose(f);
    assert(cities_key(K_ESC)==2);
    assert(!strcmp(location_name,"Sosnowiec [PL]"));
    args[1]="/location";assert(command_line(2,args)==1);
    city_file=NULL;observer=saved;
    assert(cities_start()&&cities_active);modes=planet_mode;
    assert(gx_objects()&&planet_mode==modes); /* retain VGA planes and header sprites */
    cities_draw();assert(!cities_key(K_PGDN));city_count("15 / 345");
    assert(!cities_key(K_END));city_count("345 / 345");
    assert(!cities_key(K_HOME));assert(!cities_key(K_DOWN));
    assert(cities_key('\r')==1&&!cities_active&&observer.latitude==albania.latitude);
    assert(!strcmp(note,"Tirana [AL] (press S to save location)"));
    assert(gx_objects()&&planet_mode==modes); /* restore sky sprites without resetting VGA */
    assert(config_load()==1&&observer.latitude==saved.latitude); /* no implicit save */
    assert(cities_start());assert(gx_objects());cities_draw();type_city("United States");
    sprintf(name,"%s.cities",output);f=fopen(name,"wb");assert(f);
    assert(fwrite(xram,1,65536,f)==65536);fclose(f);
    assert(cities_key(K_ESC)==2&&observer.latitude==saved.latitude);
    assert(cities_start());cities_draw();type_city("zzzzzz");assert(!cities_key('\r')&&cities_active);
    expect_text(CITIES_LEFT+6,CITIES_TOP+60,"No matching cities.",0);
    assert(cities_key(K_ESC)==2&&observer.latitude==saved.latitude);
    assert(cities_start());cities_draw();type_city("Los Angeleq");assert(!cities_key(K_BS));type_city("s");
    assert(cities_key('\r')==1&&observer.latitude==chosen.latitude);
    assert(!strcmp(note,"Los Angeles [US] (press S to save location)"));
    assert(config_save());observer=saved;location_name[0]=0;
    assert(config_load()==1&&observer.latitude==chosen.latitude&&!strcmp(location_name,"Los Angeles [US]"));
    sky_compute(&moment,&observer,&sky);planets_compute(&moment,&observer,planet_positions);
    cs_begin();while(cs_busy())cs_step();assert(gx_canvas());assert(gx_objects());
    gx_scene();gx_header();gx_footer();expect_text(256,31,"Los Angeles [US]",0);
    sprintf(name,"%s.location",output);f=fopen(name,"wb");assert(f);
    assert(fwrite(xram,1,65536,f)==65536);fclose(f);
    remove(config);
    /* Missing and corrupt catalogues must not alter the observer. */
    _mkdir("cities-test-bad");city_file="cities-test-bad/sidera.cty";
    assert(!cities_start()&&!cities_active);
    f=fopen("cities-test-bad/sidera.cty","wb");assert(f);
    assert(fwrite("broken",1,6,f)==6);fclose(f);
    assert(!cities_lookup("PL")&&!cities_active&&observer.latitude==chosen.latitude);
    remove("cities-test-bad/sidera.cty");_rmdir("cities-test-bad");
    city_file=NULL;assert(gx_canvas());
    for(i=XR_END;i<65536;++i)assert(xram[i]==0xA5);
    puts("PASS: Cities CLI/config precedence, ambiguity, paging/search, apply/cancel/save, missing/corrupt files, VGA menu");
}
#include "settings_test.inc"
static void solar_layout(void)
{
    static const int radii[]={134,117,100,83,66,54,42,30};
    char title[48];unsigned i,j,cfg;int x[9],y[9],size[9],dx,dy,rr;
    expect_text(SOLAR_LEFT+2,SOLAR_TOP+12," SOLAR SYSTEM ",255);
    strcpy(title,"snapshot ");date_format(title+strlen(title),&solar_date);strcat(title," UTC");
    expect_text(SOLAR_RIGHT-2-(int)strlen(title)*6,SOLAR_TOP+4,title,0);
    expect_text(SOLAR_LEFT+2,SOLAR_BOTTOM-11,"Schematic view (not in AU scale)",0);
    expect_text(SOLAR_RIGHT-2-48,SOLAR_BOTTOM-11,"X: close",0);
    for(i=0;i<8;++i)expect_text(solar_labels[i].left,solar_labels[i].top,solar_names[i],0);
    for(i=0;i<4;++i)for(j=0;j<4;++j)
        assert(pixel(solar_moon_x-1+i,solar_moon_y-1+j)==(i>0&&i<3&&j>0&&j<3));
    dx=solar_moon_x+1-solar_labels[2].x;dy=solar_moon_y+1-solar_labels[2].y;
    assert(dx*dx+dy*dy>=7*7&&dx*dx+dy*dy<=9*9);
    assert(solar_labels[2].left-1+32<=solar_moon_x-1||solar_labels[2].left-1>=solar_moon_x+3||
           solar_labels[2].top-1+10<=solar_moon_y-1||solar_labels[2].top-1>=solar_moon_y+3);
    for(i=0;i<9;++i) {
        cfg=XR_SOLAR_CFG+i*10;x[i]=(int16_t)word(cfg);y[i]=(int16_t)word(cfg+2);
        size[i]=i?8:32;
        assert(x[i]>=SOLAR_LEFT&&x[i]+size[i]<=SOLAR_RIGHT&&y[i]>=SOLAR_TOP&&y[i]+size[i]<=SOLAR_BOTTOM);
        if(!i)assert(word(cfg+4)==XR_SUN&&xram[cfg+8]==0x77);
        else {
            dx=x[i]+4-SOLAR_CX;dy=y[i]+4-SOLAR_CY;rr=radii[i-1];
            assert(dx*dx+dy*dy>=(rr-1)*(rr-1)&&dx*dx+dy*dy<=(rr+1)*(rr+1));
            /* Opaque names may cover orbit pixels, but never sprites. */
        }
        for(j=0;j<i;++j)assert(x[i]+size[i]<=x[j]||x[j]+size[j]<=x[i]||y[i]+size[i]<=y[j]||y[j]+size[j]<=y[i]);
    }
}
static void reference_views(const char *output)
{
    static const char *const cities[]={"PL/Gdansk","US/Los Angeles","BR/Rio de Janeiro"};
    static const char *const names[]={"Gdansk [PL]","Los Angeles [US]","Rio de Janeiro [BR]"};
    static const char *const views[]={"65,14","330,9","195,34"};
    static const int azimuths[]={65,330,195},altitudes[]={14,9,34};
    unsigned i,n;Date time;Sky path;FILE *file;char name[256],program[160];
    char *args[]={program,"/config",name,"/date","1945-09-01 02:45:00",
        "/location",NULL,"/view",NULL,"/fov","160","/grid","1",
        "/paths","on","/planets","on","/constellations","4"};
    strcpy(program,asset_path);
    for(i=0;i<3;++i) {
        solar_hide();
        snprintf(name,sizeof(name),"%s.test%c.cfg",output,'A'+i);remove(name);
        args[4]=i==2?"2026-11-10 14:47:00":"1945-09-01 02:45:00";
        args[6]=(char *)cities[i];args[8]=(char *)views[i];args[10]=i==2?"70":"160";
        assert(command_line(19,args)==0);assert(config_save());
        bearing=elevation=0;grid=trails=planets=constellation_mode=0;
        gx_zoom(50-fov);live=playing=1;moment.year=2000;
        assert(command_line(3,args)==0&&!live&&!playing);
        assert(!strcmp(location_name,names[i]));
        assert(bearing==azimuths[i]&&elevation==altitudes[i]&&fov==(i==2?70:160));note[0]=0;
        sky_compute(&moment,&observer,&sky);planets_compute(&moment,&observer,planet_positions);
        rs_sync();while(rs_busy())rs_step();
        cs_begin();while(cs_busy())cs_step();
        time=moment;time.hour=time.minute=time.second=0;
        for(n=0;n<TRAIL_COUNT;++n) {
            sky_positions(&time,&observer,&path);gx_write(XR_TRAILS+n*8,&path,8);
            if(n+1<TRAIL_COUNT)assert(date_step(&time,900));
        }
        trail_done=TRAIL_COUNT;
        gx_clear(0,480);assert(gx_canvas());assert(gx_objects());
        gx_scene();gx_status(0);gx_header();gx_footer();
        expect_text(38,21,i==2?"2026-11-10 14:47:00 UTC":"1945-09-01 02:45:00 UTC",0);
        expect_text(256,31,names[i],0);expect_text(580,31,i==2?"FOV  70\x01":"FOV 160\x01",0);
        assert(grid==1&&trails&&planets&&constellation_mode==4&&!gx_busy()&&!cs_busy()&&!rs_busy());
        snprintf(name,sizeof(name),"%s.test%c",output,'A'+i);
        file=fopen(name,"wb");assert(file);assert(fwrite(xram,1,sizeof(xram),file)==sizeof(xram));fclose(file);
        if(i==0) {
            memcpy(before_clock,xram,sizeof(xram));
            assert(cities_start());assert(gx_objects());cities_draw();city_bounds();
            snprintf(name,sizeof(name),"%s.cities-panel",output);
            file=fopen(name,"wb");assert(file);assert(fwrite(xram,1,sizeof(xram),file)==sizeof(xram));fclose(file);
            assert(cities_key(K_ESC)==2);gx_scene();assert(gx_objects());
            assert(!memcmp(before_clock,xram,480*80));
        }
        memcpy(before_clock,xram,sizeof(xram));
        solar_date=moment;assert(solar_draw());assert(solar_visible);
        for(n=0;n<480*80u;++n)if(xram[n]!=before_clock[n]) {
            unsigned bit;
            for(bit=0;bit<8;++bit)if((xram[n]^before_clock[n])&(128>>bit)) {
                int x=(n%80)*8+bit,y=n/80;
                assert(x>=SOLAR_LEFT&&x<SOLAR_RIGHT&&y>=SOLAR_TOP&&y<SOLAR_BOTTOM);
            }
        }
        solar_layout();
        assert(gx_objects());
        snprintf(name,sizeof(name),"%s.solar%c",output,'A'+i);
        file=fopen(name,"wb");assert(file);assert(fwrite(xram,1,sizeof(xram),file)==sizeof(xram));fclose(file);
        solar_hide();gx_scene();assert(gx_objects());
    }
    puts("PASS: reference views A/B/C, Gdansk/Los Angeles/Rio de Janeiro, G1 T1 P1 C4");
}
int main(int argc,char **argv)
{
    char buf[160];Observer o;Date d;FILE *f;unsigned i,bit,changes=0,below;int x,y;
    assert(argc==3);
    strcpy(rom_dir,argv[1]);*strrchr(rom_dir,'/')=0;
    solar_hide();
    /* Shared line renderer preserves Moon dashes and continuous five-pixel
       constellation spacing even after crossing an 8-bit counter boundary. */
    for(bit=0;bit<3;++bit) {
        gx_clear(100,1);gx_line(28,100,627,100,(unsigned char)bit);
        for(x=28;x<628;++x)assert(pixel(x,100)==
            (bit==0?1:bit==1?!((x-28)&4):(x-28)%5==0));
    }
    strcpy(asset_path,argv[1]);assert(assets_open());assert(assets_base());
    {int16_t vectors[4][3];assert(assets_vectors(0,4,vectors));}
    timezone_settings();
    label_styles();
    location_format(buf,&observer,1);
    assert(!strcmp(buf,"49\xC2\xB0" "38\xE2\x80\xB2" "54.6\xE2\x80\xB3 N, 19\xC2\xB0" "09\xE2\x80\xB2" "19.7\xE2\x80\xB3 E, 395 m AMSL"));
    assert(location_parse(buf,&o));assert(o.latitude==observer.latitude&&o.longitude==observer.longitude&&o.altitude==395);
    assert(location_parse("49d38'54.6\" S, 19d09'19.7\" W, -100 m AMSL",&o));
    assert(o.latitude==-1787346L&&o.longitude==-689597L&&o.altitude==-100);
    assert(!location_parse("90d00'00.1\" N, 19d09'19.7\" E, 395 m AMSL",&o));
    assert(!location_parse("49d60'00.0\" N, 19d09'19.7\" E, 395 m AMSL",&o));
    assert(!location_parse("49d38'54.6\" N, 19d09'19.7\" E, 99999999 m AMSL",&o));
    assert(!date_parse("2100-02-29 12:00:00",&d));assert(date_parse("2000-02-29 12:00:00",&d));
    assert(date_parse("2200-12-31 23:59:59",&d));assert(!date_parse("2201-01-01 00:00:00",&d));
    assert(!date_parse("2026-11-08 24:00:00",&d));
    strcpy(config_path,"sidera-test.cfg");assert(config_save());observer.altitude=0;
    assert(config_load()==1&&observer.altitude==395);remove(config_path);
    /* North wrapping: NE and NW are on opposite sides of the same view. */
    bearing=0;sky.sun.az=as_angle(45000);sky.sun.alt=0;
    assert(gx_project(&sky.sun,&x,&y)&&x==553);
    sky.sun.az=as_angle(315000);assert(gx_project(&sky.sun,&x,&y)&&x==103);
    corrected_date();
    bearing=180;sky_compute(&moment,&observer,&sky);
    memset(xram+XR_END,0xA5,sizeof(xram)-XR_END);
    gx_clear(0,480);assert(gx_objects());assert(gx_canvas());
    assert(word(XR_SUN_HEADER_CFG)==65472&&word(XR_MOON_HEADER_CFG+12)==65472);
    assert(gx_objects()); /* Updating positions cannot reveal an unfinished header. */
    assert(word(XR_SUN_HEADER_CFG)==65472&&word(XR_MOON_HEADER_CFG+12)==65472);
    gx_scene();gx_header();gx_footer();
    header_layout();
    location_names();
    header_sprites();
    calculation_status();
    footer_states();
    assert(word(XR_CFG+6)==640&&word(XR_CFG+8)==480);
    assert(word(XR_SUN_CFG+4)==XR_SUN);
    assert(word(XR_MOON_CFG+16)==XR_MOON0);
    assert(xram[XR_MOON_CFG+18]==5);
    for(i=XR_END;i<65536;++i)assert(xram[i]==0xA5);
    f=fopen(argv[2],"wb");assert(f);assert(fwrite(xram,1,65536,f)==65536);fclose(f);
    /* Five consecutive presses return to the original scene; stars also
     * appear below the horizon but cannot touch the compass/header/assets. */
    assert(!constellation_mode);cs_begin();while(cs_busy())cs_step();
    /* Populate the scratch projection buffer before comparing mode-only
     * changes; its bytes are working data, outside the visible bitmap. */
    constellation_mode=1;gx_scene();constellation_mode=0;gx_scene();
    memcpy(before_clock,xram,sizeof(xram));
    for(bit=1;bit<=4;++bit) {
        cs_cycle();assert(constellation_mode==bit);gx_scene();
        changes=below=0;
        for(i=0;i<sizeof(xram);++i)if(before_clock[i]!=xram[i]) {
            if(i>=XR_LABELS&&i<XR_LABELS+96*5)continue; /* Label layout cache. */
            ++changes;y=(int)(i/80);assert(y>=VIEW_TOP&&y<VIEW_BOTTOM);
            if(y>VIEW_MIDDLE+elevation*5+11)++below;
        }
        /* UMa/UMi/Cru may all be outside this southern view. */
        if(bit<4)assert(changes);
        if(bit==1)assert(below);
        gx_footer();
        sprintf(buf,"%s.%u",argv[2],bit);f=fopen(buf,"wb");assert(f);
        assert(fwrite(xram,1,65536,f)==65536);fclose(f);
        memcpy(xram+448*80,before_clock+448*80,32*80);
    }
    cs_cycle();assert(!constellation_mode);gx_scene();assert(!memcmp(before_clock,xram,sizeof(xram)));
    changes=0;
    /* A one-second update must leave every pixel outside the clock intact,
     * including the other header fields, and all sprite data/descriptors. */
    memcpy(before_clock,xram,sizeof(xram));++moment.second;gx_clock();
    for(i=0;i<sizeof(xram);++i)for(bit=0;bit<8;++bit)
        if((before_clock[i]^xram[i])&(0x80u>>bit)) {
            ++changes;x=(int)(i%80)*8+bit;y=(int)(i/80);
            assert((y>=21&&y<29&&x>=38&&x<176)||
                   (y>=31&&y<39&&x>=38&&x<188));
        }
    assert(changes);
    grid_modes(argv[2]);
    grid_incremental();
    zoom_projection(argv[2]);
    reload_assets();
    catalogue_recovery();
    catalogue_freshness();
    projected_catalogue();view_header();interactive_scene();staged_scene();preserved_scales();
    trail_label_repair(3);trail_label_repair(4);
    /* Real northern and southern examples of the restricted mode. */
    constellation_mode=4;bearing=0;
    assert(gx_objects());gx_scene();gx_header();gx_footer();
    header_sprites();
    sprintf(buf,"%s.north",argv[2]);f=fopen(buf,"wb");assert(f);
    assert(fwrite(xram,1,65536,f)==65536);fclose(f);
    observer.latitude=-1220400L;observer.longitude=662400L;
    assert(date_parse("2026-05-01 20:00:00",&moment));bearing=180;elevation=34;
    sky_compute(&moment,&observer,&sky);cs_begin();
    assert(cs_busy());assert(!cs_step());while(cs_busy())cs_step();
    assert(gx_objects());gx_scene();gx_header();gx_footer();
    header_sprites();
    sprintf(buf,"%s.south",argv[2]);f=fopen(buf,"wb");assert(f);
    assert(fwrite(xram,1,65536,f)==65536);fclose(f);
    planet_display(argv[2]);
    depth_order();
    city_catalogue(argv[1],argv[2]);
    observer.latitude=1880272L;observer.longitude=756424L;observer.altitude=113;
    strcpy(location_name,"Warsaw [PL]");assert(tz_set("Europe/Warsaw"));
    assert(date_parse("2026-11-10 12:00:00",&moment));
    assert(rs_sync());gx_riseset();expect_text(250,53,"Rise ..:.. Set ..:..",0);
    while(rs_busy())rs_step();
    sky_compute(&moment,&observer,&sky);bearing=180;elevation=24;grid=1;
    constellation_mode=planets=trails=0;note[0]=0;gx_clear(0,480);
    assert(gx_objects());gx_scene();gx_header();gx_footer();
    expect_text(250,53,"Rise 06:47 Set 15:51",0);
    expect_text(442,53,"Rise 08:29 Set 15:43",0);
    header_sprites();
    sprintf(buf,"%s.riseset",argv[2]);f=fopen(buf,"wb");assert(f);
    assert(fwrite(xram,1,65536,f)==65536);fclose(f);
    /* Local midnight is detected even while the displayed UTC date stays put. */
    daylight_compass();
    assert(date_parse("2026-11-10 23:00:00",&moment));assert(rs_sync());gx_riseset();
    expect_text(250,53,"Rise ..:.. Set ..:..",0);assert(rs_busy());
    startup_settings(argv[1]);reference_views(argv[2]);
    assets_close();
    puts("PASS: DMS/UTF-8, date/config round-trip, XRAM bounds, projection, sprite layout, drawing, clock-only update");
    return 0;
}

unsigned char platform_vga_ready(void){return 1;}
