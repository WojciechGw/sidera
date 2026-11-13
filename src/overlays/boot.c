#include <stdio.h>
#include <string.h>
#include "../sidera.h"
#include "overlay.h"
#ifdef SIDERA_OVERLAYS
#pragma code-name ("INIT_CODE")
#pragma rodata-name ("INIT_RODATA")
#endif
static void help(void)
{
    platform_puts("sidera /config PATH /date \"YYYY-MM-DD HH:MM:SS\" /live\r\n"
        " /location \"CC/City\" /view AZ,ALT /fov 50..160\r\n"
        " /grid 0..3 /paths on|off /planets on|off /constellations 0..4\r\n"
        "All options optional. S saves settings.\r\n"
        "Standalone: resources embedded in ROM; Q returns to the monitor.\r\n");
}
/* INIT is allowed to disappear between these two calls. Keep their shared
 * results resident, never pointers into its temporary configuration buffer. */
const char *boot_location;
int boot_config;

/* 1: continue, 0: help printed, -1: startup error already printed. */
int boot_prepare(int argc,char **argv)
{
    int i;
    unsigned char option;
    boot_location=0;
    paths_init();
    for(i=1;i<argc;++i) {
        option=argv[i][0]=='/'?option_code(argv[i]+1):0;
        if(option==OPT_HELP){help();return 0;}
        if(option==OPT_LIVE)continue;
        if(!option||++i==argc||!option_value(option,argv[i],0)){help();return -1;}
        if(option==OPT_CONFIG)strcpy(config_path,argv[i]);
        if(option==OPT_ASSETS)strcpy(asset_path,argv[i]);
        if(option==OPT_LOCATION)boot_location=argv[i];
    }
    if(!platform_vga_ready()){platform_puts("sidera: requires VGA.\r\n");return -1;}
    boot_config=config_load();
    if(boot_config<0)strcpy(note,"Cannot read configuration. S saves current settings.");
    return 1;
}
void boot_apply(int argc,char **argv)
{
    int i;
    unsigned char option;
    for(i=1;i<argc;++i) {
        option=option_code(argv[i]+1);
        if(option==OPT_LIVE)option_value(option,"",1);
        else {++i;if(option<OPT_LOCATION)option_value(option,argv[i],1);}
    }
}
