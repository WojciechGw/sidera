static void figure_tests(const char *output)
{
    unsigned i,j,k,site,ori=0,uma=0,erid=0,limited=0,visible=0;
    unsigned char mode,label[5];uint16_t meta[2],edge[2];
    char title[28],path[256];int zoom,x,y,az,alt;
    Position p,center;StarFrame frame;int16_t vector[3];FILE *file;
    figure_reset();p.az=as_angle(350000L);p.alt=as_angle(-10000L);figure_add(&p);
    p.az=as_angle(10000L);p.alt=as_angle(10000L);figure_add(&p);
    figure_finish();figure_result(&center,&zoom);az=(as_degrees(center.az)+5)/10%360;
    assert((az<=1||az>=359)&&zoom==50&&figure_side()==2);
    figure_reset();p.az=0;p.alt=as_angle(80000L);figure_add(&p);
    p.alt=as_angle(-80000L);figure_add(&p);figure_finish();figure_result(&center,&zoom);
    assert(zoom>160&&center.alt==0);
    for(i=0;i<88;++i) {
        gx_read(XR_META+764u+i*32u,title,28);
        if(strstr(title,"[Ori]"))ori=i;
        if(strstr(title,"[UMa]"))uma=i;
        if(strstr(title,"[Eri]"))erid=i;
    }
    assert(ori&&uma&&erid);
    /* Exact constellation codes precede stellar aliases containing that code. */
    find_query("oRi");assert(find_key('\r')==1&&target_figure==ori);
    assert(find_start());assert(find_key(K_ESC)==2&&target_figure==ori);
    /* Navigation can replace a pending constellation preview, including its geometry. */
    find_query("Constellation");assert(!find_key(K_DOWN));
    for(i=0;i<54;++i)assert(!find_step());
    assert(!find_key(K_DOWN));assert(!find_key('\r'));
    for(i=0;i<120&&find_active;++i)find_step();
    assert(!find_active&&target_figure==2);
    constellation_mode=0;find_query("Ursa Major");assert(find_key('\r')==1&&target_figure==uma&&!constellation_mode);
    assert(cs_enabled());
    for(site=0;site<2;++site) {
        observer.latitude=site?-1220400L:1880272L;observer.longitude=site?662400L:756424L;
        assert(date_parse(site?"2026-05-01 20:00:00":"2026-11-12 20:00:00",&moment));
        star_frame(&moment,&observer,&frame);cs_begin();while(cs_busy())cs_step();
        for(i=0;i<88;++i) {
            gx_read(XR_META+760u+i*32u,meta,4);gx_read(XR_META+764u+i*32u,title,28);
            assert(target_select(FIND_FIGURE_FIRST+i,title,&center));
            assert(cs_focus(&center,&zoom));target_center();
            assert(fov==(zoom>160?160:zoom)&&!constellation_mode);
            if(zoom>160)++limited;
            else for(j=0;j<meta[1];++j) {
                gx_read(XR_EDGES+(meta[0]+j)*4u,edge,4);
                for(k=0;k<2;++k) {
                    assert(assets_vectors(edge[k],1,vector));star_position(&frame,vector,&p);
                    assert(gx_project(&p,&x,&y));
                    assert(x>=VIEW_LEFT+8&&x<VIEW_RIGHT-8&&y>=VIEW_TOP+8&&y<VIEW_BOTTOM-8);
                }
            }
        }
    }
    assert(limited); /* Includes near-zenith and very extended figures. */
    observer.latitude=1880272L;observer.longitude=756424L;
    assert(date_parse("2026-11-12 20:00:00",&moment));
    cs_begin();while(cs_busy())cs_step();
    grid=trails=planets=0;note[0]=0;
    find_query("Ori");assert(find_key('\r')==1&&target_figure==ori);
    target_center();az=bearing;alt=elevation;zoom=fov;
    for(mode=0;mode<=4;mode+=4) {
        constellation_mode=mode;gx_clear(0,480);assert(gx_objects());gx_scene();gx_header();gx_footer();
        gx_read(XR_LABELS+(ori+8)*5u,label,5);assert(label[4]);
        x=label[0]|((unsigned)label[1]<<8);y=label[2]|((unsigned)label[3]<<8);
        gx_read(XR_META+764u+ori*32u,title,28);expect_text(x+1,y+1,title,255);
        if(!mode)for(j=VIEW_TOP*80u;j<VIEW_BOTTOM*80u;++j)visible+=xram[j]!=0;
        sprintf(path,"%s.figure%u",output,mode);file=fopen(path,"wb");assert(file);
        assert(fwrite(xram,1,sizeof(xram),file)==sizeof(xram));fclose(file);
    }
    assert(visible>100);
    assert(date_step(&moment,300));cs_begin();while(cs_busy())cs_step();gx_scene();
    assert(bearing==az&&elevation==alt&&fov==zoom&&target_figure==ori);
    live=1;assert(date_step(&moment,60)&&!cs_current());live=0;
    find_query("Sirius");assert(find_key('\r')==1&&target_figure==255);
    target_id=0xFFFFu;
    puts("PASS: 88 figure fits at two sites, north wrap, FOV limits, code priority, C0/C4 label/lines and no camera tracking");
}
