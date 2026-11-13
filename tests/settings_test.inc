/* End-to-end configuration and command-line precedence using the real parser. */
static void startup_settings(const char *asset)
{
    char config[]="sidera-settings-test.cfg",text[256],coords[96],saved_name[45];
    char *args[]={(char *)asset,"/config",config,"/location","PL/Gdansk",
        "/date","1945-09-01 02:45:00","/view","65,+14","/fov","160",
        "/grid","1","/paths","on","/planets","on","/constellations","4"};
    char *override[]={(char *)asset,"/config",config,"/view","330,9","/location","US/Los Angeles"};
    static const char *bad[][2]={{"/view","360,0"},{"/view","0,-55"},{"/view","0,55"},
        {"/view","0,0junk"},{"/view","0"},{"/fov","49"},{"/fov","161"},{"/fov","55"},
        {"/grid","4"},{"/grid","9"},{"/constellations","5"},{"/paths","1"},
        {"/planets","yes"},{"/date","1945-02-29 00:00:00"},{"/unknown","1"}};
    char *invalid[]={(char *)asset,NULL,NULL};
    Observer saved;Date date;unsigned i,n;FILE *file;
    remove(config);note[0]=0;
    assert(command_line(19,args)==0);
    assert(!strcmp(location_name,"Gdansk [PL]")&&!strcmp(zone_name,"Europe/Warsaw"));
    assert(bearing==65&&elevation==14&&fov==160&&grid==1&&trails&&planets&&constellation_mode==4);
    assert(!live&&!playing&&moment.year==1945&&moment.month==9&&moment.day==1&&moment.hour==2&&moment.minute==45);
    saved=observer;date=moment;playing=1;assert(config_save());
    file=fopen(config,"rb");assert(file);n=(unsigned)fread(text,1,255,file);text[n]=0;fclose(file);
    assert(strstr(text,"View 65,14 160 1 on on 4\r\nTime 1945-09-01 02:45:00\r\n"));
    /* Reload restores all settings, always paused, regardless of the prior state. */
    bearing=0;elevation=0;gx_zoom(50-fov);grid=trails=planets=constellation_mode=0;live=1;
    observer.latitude=0;moment.year=2000;
    assert(command_line(3,args)==0&&bearing==65&&elevation==14&&fov==160);
    assert(observer.latitude==saved.latitude&&grid==1&&trails&&planets&&constellation_mode==4);
    assert(!live&&!playing&&moment.year==1945);
    /* Explicit location gets the local-hour direction; explicit view wins in either order. */
    assert(command_line(7,override)==0&&bearing==330&&elevation==9);
    assert(!strcmp(location_name,"Los Angeles [US]"));
    override[3]="/location";override[4]="US/Los Angeles";override[5]="/view";override[6]="65,14";
    assert(command_line(7,override)==0&&bearing==65);
    assert(command_line(5,override)==0&&bearing==90&&elevation==14);
    override[3]="/live";assert(command_line(4,override)==0&&live&&!playing);
    override[3]="/date";override[4]="2200-12-31 23:59:59";
    assert(command_line(5,override)==0&&!live&&moment.year==2200);
    live=1;playing=1;assert(config_save());live=0;
    assert(command_line(3,args)==0&&live&&!playing);
    override[3]="/paths";override[4]="off";
    assert(command_line(5,override)==0&&!trails);
    /* Invalid CLI values never partially apply settings. Missing values are errors too. */
    for(i=0;i<sizeof(bad)/sizeof(*bad);++i) {
        invalid[1]=(char *)bad[i][0];invalid[2]=(char *)bad[i][1];
        assert(command_line(3,invalid)==1);
    }
    invalid[1]="/view";assert(command_line(2,invalid)==1);
    /* Failed config validation leaves even the location, date and zone untouched. */
    observer=saved;moment=date;strcpy(location_name,"Gdansk [PL]");assert(tz_set("Europe/Warsaw"));
    bearing=65;elevation=14;grid=1;trails=planets=1;constellation_mode=4;live=playing=0;
    strcpy(config_path,config);assert(config_save());location_format(coords,&observer,1);
    for(i=0;i<3;++i) {
        file=fopen(config,"wb");assert(file);
        fprintf(file,"%s\nZone Etc/UTC\nCity Wrong city\nView 330,9 160 %s on on 4\nTime %s\n",
            coords,i==0?"4":"1",i==1?"1945-02-29 00:00:00":"1945-09-01 02:45:00");
        if(i==2)fputs("garbage\n",file);fclose(file);
        assert(config_load()==-1&&bearing==65&&elevation==14&&grid==1&&!live);
        assert(!strcmp(location_name,"Gdansk [PL]")&&!strcmp(zone_name,"Europe/Warsaw"));
    }
    /* Longest names/coordinates, numeric limits and the compact output buffer. */
    strcpy(saved_name,location_name);memset(location_name,'X',44);location_name[44]=0;
    observer.latitude=3240000L;observer.longitude=-6480000L;observer.altitude=10000;
    bearing=359;elevation=-54;gx_zoom(160-fov);grid=3;trails=planets=0;constellation_mode=4;
    assert(config_save());bearing=0;assert(config_load()==1&&bearing==359&&elevation==-54);
    assert(strlen(location_name)==44&&observer.altitude==10000);
    observer=saved;moment=date;strcpy(location_name,saved_name);note[0]=0;remove(config);
    puts("PASS: startup settings, CLI precedence/ranges, complete save/reload, LIVE and paused simulation");
}
