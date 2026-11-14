/* Exercise reload through the real polling loop and overlay loader. */
static int settings_same_date(const Date *a,const Date *b)
{
    return a->year==b->year&&a->month==b->month&&a->day==b->day&&
        a->hour==b->hour&&a->minute==b->minute&&a->second==b->second;
}
static void message_timeout_checks(void)
{
    static const char *const messages[]={"[INFO] Configuration saved","[!] Failed to save configuration",
        "[INFO] Configuration reloaded","[!] Failed to reload configuration"};
    unsigned saved_ticks=ticks,i,j;
    for(i=0;i<4;++i) {
        gx_status(i&1);
        message_show(messages[i]);memcpy(frozen_frame,xram,480*80);
        /* Ending work must not dismiss the five-second result. */
        gx_status(1);gx_status(i&1);expect_text_at(8,472,messages[i],0);
        ticks=message_tick+4u*PLATFORM_TICK_HZ;message_step();
        assert(!strcmp(note,messages[i])&&!memcmp(frozen_frame,xram,480*80));
        message_show(messages[i]); /* An identical result gets another five seconds. */
        ticks=message_tick+5u*PLATFORM_TICK_HZ-2;message_step();
        assert(!strcmp(note,messages[i]));
        ticks=message_tick+5u*PLATFORM_TICK_HZ;message_step();
        assert(!note[0]&&!memcmp(frozen_frame,xram,472*80));
        if(i&1) {
            expect_text_at(8,472,"[INFO] Working, please wait...",0);
            ticks+=10u*PLATFORM_TICK_HZ;message_step();
            expect_text_at(8,472,"[INFO] Working, please wait...",0);
            gx_status(0);
        }
        for(j=472*80;j<480*80;++j)if(j%80<59)assert(xram[j]==(j%80==58?4:0));
        expect_text_at(473,472,"full ",0);expect_text_at(542,472,"navigational  ",0);
        portal_writes=0;message_step();assert(!portal_writes);
    }
    ticks=(unsigned)-100;message_show(messages[0]);
    ticks=message_tick+5u*PLATFORM_TICK_HZ;message_step();assert(!note[0]);
    message_show(messages[0]);strcpy(note,"New message");gx_footer();
    ticks=message_tick+5u*PLATFORM_TICK_HZ;message_step();assert(!strcmp(note,"New message"));
    message_show(messages[0]);solar_visible=1;memcpy(frozen_frame,xram,480*80);
    ticks=message_tick+5u*PLATFORM_TICK_HZ;message_step();
    assert(!note[0]&&!memcmp(frozen_frame,xram,480*80));
    solar_visible=0;gx_footer();ticks=saved_ticks;
}
static unsigned settings_keys(void)
{
    static Observer saved_observer;
    static Date saved_date;
    static time_t clock;
    static char file_text[224];
    static unsigned char frame[480*80];
    Date utc;FILE *file;unsigned n;
    if(!sky_checks)return 0;
    if((settings_step==0||settings_step==3||settings_step==4)&&
       (gx_busy()||cs_busy()||rs_busy()||(trails&&trail_done<TRAIL_COUNT)))return 0;
    if(settings_step>=2&&settings_step<=8) {
        assert(!memcmp(&observer,&saved_observer,sizeof(observer)));
        assert(settings_same_date(&moment,&saved_date));
        assert(bearing==135&&elevation==-14&&fov==70&&grid==3&&trails&&planets&&constellation_mode==4);
        assert(!strcmp(location_name,"Warsaw [PL]")&&!strcmp(zone_name,"Europe/Warsaw"));
    }
    switch(settings_step) {
    case 0:
        saved_observer=observer;saved_date=moment;clock=fake_time;
        ++settings_step;return CTRL('S');
    case 1:
        assert(!strcmp(note,"[INFO] Configuration saved"));expect_text_at(8,472,note,0);
        file=fopen(save_path,"rb");assert(file);n=(unsigned)fread(file_text,1,sizeof(file_text)-1,file);
        assert(n&&n<sizeof(file_text)-1);file_text[n]=0;fclose(file);
        observer.latitude=observer.longitude=observer.altitude=0;
        assert(tz_set("Etc/UTC"));location_name[0]=0;
        bearing=10;elevation=60;gx_zoom(50-fov);grid=trails=planets=constellation_mode=0;
        moment.year=2000;live=0;playing=1;
        ++settings_step;return CTRL('R');
    case 2:
        assert(!live&&!playing&&!strcmp(note,"[INFO] Configuration reloaded"));
        expect_text_at(8,472,note,0);
        assert(gx_busy()||cs_busy()); /* Reload again while the first redraw is pending. */
        ++settings_step;return CTRL('R');
    case 3:
        message_timeout_checks();
        memcpy(frame,xram,sizeof(frame));++settings_step;return 12;
    case 4:
        assert(!memcmp(frame,xram,sizeof(frame))); /* Same pixels as an explicit full refresh. */
        assert(!remove(save_path));++settings_step;return CTRL('R');
    case 5:
        assert(!strcmp(note,"[!] Failed to reload configuration"));expect_text_at(8,472,note,0);
        file=fopen(save_path,"wb");assert(file);fputs("broken",file);fclose(file);
        ++settings_step;return CTRL('R');
    case 6:
        assert(!strcmp(note,"[!] Failed to reload configuration"));
        file=fopen(save_path,"wb");assert(file);fputs(file_text,file);fclose(file);
        settings_read_fail=1;++settings_step;return CTRL('R');
    case 7:
        assert(!strcmp(note,"[!] Failed to reload configuration"));
        settings_read_fail=0;settings_write_fail=1;++settings_step;return CTRL('S');
    case 8:
        assert(!strcmp(note,"[!] Failed to save configuration"));expect_text_at(8,472,note,0);
        settings_write_fail=0;live=1;++settings_step;return CTRL('S');
    case 9:
        assert(!strcmp(note,"[INFO] Configuration saved"));
        live=0;playing=1;moment.year=2000;fake_time=(time_t)-1;
        ++settings_step;return CTRL('R');
    case 10:
        assert(!strcmp(note,"[!] Failed to reload configuration"));
        assert(!live&&playing&&moment.year==2000&&bearing==135);
        fake_time=clock+3600;++settings_step;return CTRL('R');
    case 11:
        assert(!strcmp(note,"[INFO] Configuration reloaded"));
        assert(read_clock(&utc)&&settings_same_date(&moment,&utc)&&live&&!playing);
        assert(bearing==135&&elevation==-14&&fov==70&&grid==3&&trails&&planets&&constellation_mode==4);
        ++settings_step;return CTRL('Q');
    default:assert(0);return CTRL('Q');
    }
}
