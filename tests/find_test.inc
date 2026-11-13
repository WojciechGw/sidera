/* Exercise the real modal finder, including its bounded scan and persistent target. */
static void find_settle(void)
{
    unsigned n;
    for(n=0;n<400;++n)assert(!find_step());
}
static void find_query(const char *s)
{
    assert(find_start());assert(gx_objects());
    while(*s){assert(!find_key((unsigned char)*s++));assert(!find_step());}
    find_settle();
}
static void find_tests(const char *output)
{
    unsigned n,id;int x,y,old_fov=fov,old_bearing;
    Position p;StarFrame frame;int16_t vector[3];
    char name[256];FILE *file;
    target_id=0xFFFFu;
    gx_clear(0,480);assert(gx_objects());gx_scene();gx_header();gx_footer();
    memcpy(before_clock,xram,sizeof(xram));
    assert(find_start());assert(gx_objects());
    /* Cancellation is available before any scan completes. */
    assert(find_key(K_ESC)==2&&!find_active&&target_id==0xFFFFu);
    gx_scene();assert(gx_objects());gx_footer();
    assert(!memcmp(before_clock,xram,480*80));
    assert(find_start());assert(gx_objects());find_settle();
    assert(!memcmp(before_clock,xram,71*80));
    for(n=71;n<480;++n)assert(pixel(0,n)&&pixel(639,n));
    expect_text(2,83," FIND ",255);
    expect_text(6,466,"Enter: locate object  Esc: cancel",0);
    expect_text(634-7*6,432,"1 / 855",0);
    expect_text(6,115,"Type",0);expect_text(36,115,"HIP",0);
    expect_text(78,115,"HD",0);expect_text(120,115,"Designation",0);expect_text(216,115,"Name",0);
    expect_text(6,131,"Sun",255);expect_text(216,131,"Sun",255);
    for(x=6;x<318;++x)assert(pixel(x,130));
    expect_text(36,131,"             ",255);
    sprintf(name,"%s.find",output);file=fopen(name,"wb");assert(file);
    assert(fwrite(xram,1,sizeof(xram),file)==sizeof(xram));fclose(file);
    /* Repeated arrows are accepted before details settle; only two left rows change. */
    memcpy(before_clock,xram,sizeof(xram));
    for(n=1;n<=4;++n) {
        portal_writes=0;file_reads=0;
        assert(!find_key(K_DOWN));expect_text(6,131+n*14,n==1?"Moon":"Pl",255);
        assert(!file_reads&&portal_writes==800);
        for(y=130;y<410;++y)assert(!memcmp(xram+y*80+40,before_clock+y*80+40,40));
    }
    assert(!find_key('\r')); /* Enter chooses Mars, not stale Sun/Moon details. */
    for(n=0;n<100&&find_active;++n)find_step();
    assert(!find_active&&target_id==4);
    assert(find_start());find_settle();
    assert(!find_key(K_PGDN));find_settle();expect_text(634-8*6,432,"21 / 855",0);
    assert(!find_key(K_END));find_settle();expect_text(634-9*6,432,"855 / 855",0);
    assert(!find_key(K_UP));find_settle();expect_text(634-9*6,432,"854 / 855",0);
    assert(!find_key(K_PGUP));find_settle();expect_text(634-9*6,432,"834 / 855",0);
    /* A new page/arrow replaces a pending page clear; Enter uses the latest row. */
    assert(!find_key(K_END));assert(!find_key(K_HOME));assert(!find_key(K_DOWN));
    assert(!find_key('\r'));
    for(n=0;n<150&&find_active;++n)find_step();
    assert(!find_active&&target_id==1);
    assert(find_start());find_settle();
    assert(find_key(K_ESC)==2);
    find_query("zzzzzz");expect_text(6,444,"No matching objects.",0);
    assert(!find_key('\r')&&find_active);assert(find_key(K_ESC)==2);
    constellation_mode=0;
    find_query("sIrIuS");expect_text(634-5*6,432,"1 / 1",0);
    expect_text(6,131,"St",255);expect_text(216,131,"Sirius",255);
    expect_text(120,131,"Alpha CMa",255);expect_text(36,131,"32349",255);
    expect_text(78,131,"48915",255);expect_text(330,173,"Sirius | Alpha CMa | 9 CMa | HIP 32349 | HD 48915",0);
    sprintf(name,"%s.find-columns",output);file=fopen(name,"wb");assert(file);
    assert(fwrite(xram,1,sizeof(xram),file)==sizeof(xram));fclose(file);
    assert(find_key('\r')==1&&!find_active&&constellation_mode==1&&fov==old_fov);
    id=target_id;assert(id>=8&&id<SIDERA_STAR_COUNT+8);
    assert(assets_vectors(id-8,1,vector));star_frame(&moment,&observer,&frame);
    star_position(&frame,vector,&p);assert(gx_project(&p,&x,&y));
    assert(abs(x-VIEW_CENTER)<=gx_circle/360+1&&abs(y-VIEW_MIDDLE)<=gx_circle/360+1);
    constellation_mode=4;target_update();gx_clear(71,377);target_draw();
    assert(pixel(x,y)); /* Sirius is not in the five C4 navigation groups. */
    sprintf(name,"%s.target",output);file=fopen(name,"wb");assert(file);
    assert(fwrite(xram,1,sizeof(xram),file)==sizeof(xram));fclose(file);
    old_bearing=bearing;assert(date_step(&moment,3600));target_update();
    assert(bearing==old_bearing&&target_id==id); /* Position moves; camera does not. */
    star_frame(&moment,&observer,&frame);star_position(&frame,vector,&p);
    gx_clear(71,377);target_draw();if(gx_project(&p,&x,&y))assert(pixel(x,y));
    find_query("HIP 32349");assert(find_key('\r')==1&&target_id==id&&constellation_mode==4);
    assert(find_start());assert(find_key(K_ESC)==2&&target_id==id);
    planets=0;find_query("Mercury");assert(find_key('\r')==1&&target_id==2&&planets);
    /* Enter during scanning applies the final query once its exact result is ready. */
    assert(find_start());assert(!find_key('M'));assert(!find_key('o'));assert(!find_key('o'));
    assert(!find_key('n'));assert(!find_key('\r'));
    for(n=0;n<260&&find_active;++n)find_step();
    assert(!find_active&&target_id==1);
    assert(find_start());fail_find_read=1;find_step();
    expect_text(6,444,"Cannot read object catalogue",0);
    assert(!find_key('\r')&&find_active&&target_id==1);
    assert(find_key(K_ESC)==2&&target_id==1);
    fail_find_open=1;assert(!find_start()&&!find_active&&target_id==1);fail_find_open=0;
    /* Zenith/nadir retain FOV and fit exactly at the view centre. */
    p.az=0;p.alt=16384;assert(target_select(0,"Sun",&p)&&elevation==90);
    assert(gx_project(&p,&x,&y)&&x==VIEW_CENTER&&y==VIEW_MIDDLE&&fov==old_fov);
    p.alt=-16384;assert(target_select(0,"Sun",&p)&&elevation==-90);
    assert(gx_project(&p,&x,&y)&&x==VIEW_CENTER&&y==VIEW_MIDDLE);
    target_id=0xFFFFu;
    puts("PASS: FIND catalogue, pages, query replacement, cancel, queued Enter, C0/C4, target motion, poles and I/O failures");
}
