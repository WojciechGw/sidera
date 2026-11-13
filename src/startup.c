/* Resident startup orchestration; INIT and CITIES may replace each other. */
#include "sidera.h"
#include "overlays/overlay.h"

/* 1: ready, 0: help, -1: error. The caller performs overlay-error cleanup. */
int startup(int argc,char **argv)
{
    int r;
    if(!ovl_select(OVL_INIT))return -1;
    r=boot_prepare(argc,argv);
    if(r<=0)return r;
    if(boot_location) {
        if(!ovl_select(OVL_CITIES))return -1;
        if(!cities_lookup(boot_location)){platform_puts(note);platform_puts("\r\n");return -1;}
    }
    if(boot_config!=1||boot_location)location_view();
    if(!ovl_select(OVL_INIT))return -1;
    boot_apply(argc,argv);
    return 1;
}
