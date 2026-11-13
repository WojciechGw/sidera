static void map_expect(unsigned char region,const Observer *place)
{
    unsigned char raster[5850];unsigned x,y,mx=0,my=0,wanted;
    char path[256];FILE *file;
    sprintf(path,"%s/sidera.map",rom_dir);file=fopen(path,"rb");assert(file);
    assert(!fseek(file,16L+region*5850L,SEEK_SET));assert(fread(raster,1,sizeof(raster),file)==sizeof(raster));fclose(file);
    if(place) {
        mx=(unsigned)(((int64_t)place->longitude+6480000)*299+6480000)/12960000;
        my=(unsigned)(((int64_t)3240000-place->latitude)*149+3240000)/6480000;
        assert(word(XR_CITY_CFG)+2==334+mx&&word(XR_CITY_CFG+2)+2==77+my);
    } else assert((int16_t)word(XR_CITY_CFG)==-64);
    assert(word(XR_CITY_CFG+4)==XR_CITY_IMAGE&&word(XR_CITY_CFG+6)==XR_CITY_PAL);
    assert(word(XR_CITY_CFG+8)==0x11); /* 8x8, 1bpp, without scaling or flipping. */
    assert(!word(XR_CITY_PAL)&&word(XR_CITY_PAL+2)==0x3F);
    for(y=0;y<8;++y)for(x=0;x<8;++x)
        assert(!!(xram[XR_CITY_IMAGE+y]&(128>>x))==
               (x<5&&y<5&&(x==2||y==2))); /* Centered 5x5 red plus. */
    for(y=0;y<150;++y)for(x=0;x<300;++x) {
        wanted=!!(raster[y*39+(x+6)/8]&(128>>((x+6)&7)));
        assert(pixel(334+x,77+y)==wanted);
    }
}
static void world_map_tests(const char *output)
{
    static const int32_t coords[][2]={{522297L,210122L},{356895L,1396917L},{300444L,312357L},
        {407128L,-740060L},{-229068L,-431729L},{-338688L,1512093L},{-800000L,0}};
    Observer place;unsigned n,x,y,writes;char path[256];FILE *file;
    assert(cities_start());cities_draw();assert(gx_objects());city_map_settle();
    for(n=0;n<7;++n) {
        place.latitude=coords[n][0]*36/10;place.longitude=coords[n][1]*36/10;place.altitude=0;
        memcpy(before_clock,xram,sizeof(xram));
        world_select((unsigned char)n+1,&place);city_map_settle();map_expect((unsigned char)n+1,&place);
        for(y=0;y<480;++y)for(x=0;x<640;++x)if(x<334||x>=634||y<77||y>=227)
            assert(pixel(x,y)==!!(before_clock[y*80+x/8]&(128>>(x&7))));
    }
    world_select(0,0);city_map_settle();map_expect(0,0);
    place.latitude=1880272L;place.longitude=756424L;
    world_select(0,&place);city_map_settle();map_expect(1,&place); /* Legacy untagged catalogue. */
    file_reads=portal_writes=0;place.latitude=1956674L;place.longitude=671146L;
    world_select(1,&place);assert(!cities_step());writes=portal_writes;
    assert(!file_reads&&writes==4);map_expect(1,&place);
    /* Change the requested continent before its previous paint finishes. */
    world_select(2,&place);assert(cities_step());
    place.latitude=-825655L;place.longitude=-1554290L;
    world_select(5,&place);city_map_settle();map_expect(5,&place);
    place.latitude=-648000L;place.longitude=6476400L;
    world_select(6,&place);city_map_settle();map_expect(6,&place);
    place.latitude=3240000L;place.longitude=-6480000L;
    world_select(4,&place);city_map_settle();map_expect(4,&place);
    place.latitude=-3240000L;place.longitude=6480000L;
    world_select(7,&place);city_map_settle();map_expect(7,&place);
    assert(cities_key(K_ESC)==2);
    assert((int16_t)word(XR_CITY_CFG)==-64);
    assert(cities_start());cities_draw();type_city("Warsaw");city_map_settle();
    expect_text(334,240,"Warsaw",0);expect_text(334,254,"Poland",0);
    sprintf(path,"%s.world",output);file=fopen(path,"wb");assert(file);
    assert(fwrite(xram,1,sizeof(xram),file)==sizeof(xram));fclose(file);
    type_city("zz");city_map_settle();map_expect(0,0);expect_text(334,240,"      ",0);
    assert(cities_key(K_ESC)==2);
    fail_map_open=1;assert(cities_start());cities_draw();expect_text(334,77,"Map unavailable",0);
    assert(cities_key('\r')==1);fail_map_open=0; /* City use does not depend on the map. */
    assert(cities_start());cities_draw();fail_map_read=1;assert(!cities_step());
    expect_text(334,77,"Map unavailable",0);assert(cities_key(K_ESC)==2);
    assert((int16_t)word(XR_CITY_CFG)==-64);
    printf("PASS: seven map regions, 5x5 red cross, map edges, interrupted paint, no results, legacy tags; same-region move %u writes\n",writes);
}
