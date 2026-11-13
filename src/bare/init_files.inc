/* Compiled in INIT for the ROM build and in files.c for host checks. */
void paths_init(const char *argv0)
{
#ifdef RPMT_BARE
    (void)argv0;
    strcpy(asset_path,"ROM:sidera.dat");
#else
    const char *p = strrchr(argv0,'/');
    unsigned n;
    if (!p) p = strchr(argv0,':');
    n = p ? (unsigned)(p-argv0)+1 : 0;
    if (n < sizeof(asset_path)-12) memcpy(asset_path,argv0,n);
    else n=0;
    strcpy(asset_path+n,"sidera.dat");
#endif
    tz_close();strcpy(zone_name,"Europe/Warsaw");
}
#define PARSE_SCOPE
#include "parse.inc"
#undef PARSE_SCOPE
static char *next_line(char *p)
{
    p+=strcspn(p,"\r\n");
    if(*p){*p++=0;p+=strspn(p,"\r\n");}
    return p;
}
unsigned char option_code(const char *name)
{
    static const char *const names[]={"view","fov","grid","paths","planets",
        "constellations","date","live","location","config","assets","help"};
    unsigned char i;
    if(!strcmp(name,"?"))return OPT_HELP;
    for(i=0;i<12;++i)if(!stricmp(name,names[i]))return i+1;
    return 0;
}
int option_value(unsigned char code,const char *value,unsigned char apply)
{
    const char *p=value;
    int a,b,sign=1;Date d;
    if(code==OPT_DATE) {
        if(!date_parse(value,&d))return 0;
        if(apply){moment=d;live=playing=0;}return 1;
    }
    if(code==OPT_LIVE){if(apply){live=1;playing=0;}return 1;}
    if(code>=OPT_LOCATION)return *value&&strlen(value)<160;
    if(code==OPT_PATHS||code==OPT_PLANETS) {
        if(!stricmp(value,"on"))a=1;
        else if(!stricmp(value,"off"))a=0;else return 0;
    } else {
        a=number(&p,code==OPT_VIEW?359:code==OPT_FOV?160:code==OPT_GRID?3:4);
        if(a<0)return 0;
        if(code==OPT_VIEW) {
            if(*p++!=',')return 0;
            if(*p=='-'){sign=-1;++p;}else if(*p=='+')++p;
            b=number(&p,54);if(b<0)return 0;
        }
        if(*p)return 0;
    }
    if(code==OPT_FOV&&(a<50||a%10))return 0;
    if(apply)switch(code) {
        case OPT_VIEW:bearing=a;elevation=b*sign;break;
        case OPT_FOV:gx_zoom(a-fov);break;
        case OPT_GRID:grid=(unsigned char)a;break;
        case OPT_PATHS:trails=(unsigned char)a;break;
        case OPT_PLANETS:planets=(unsigned char)a;break;
        case OPT_CONSTELLATIONS:constellation_mode=(unsigned char)a;break;
    }
    return 1;
}
int config_load(void)
{
    int fd,n;char *line[5],*values[6],*p;
    unsigned char i,mode;
    Observer parsed;
    fd=rpmt_open(config_path,O_RDONLY);
    if(fd<0)return errno==ENOENT?0:-1;
    n=read(fd,buffer,CONFIG_BYTES-1);rpmt_close(fd);
    if(n<=0||n==(int)CONFIG_BYTES-1)return -1;
    buffer[n]=0;p=buffer;
    for(i=0;i<5;++i){line[i]=p;p=next_line(p);}
    if(*p||!location_parse(line[0],&parsed)||strncmp(line[1],"Zone ",5)||
       strncmp(line[2],"City ",5)||strlen(line[2]+5)>=sizeof(location_name)||
       strncmp(line[3],"View ",5)||strncmp(line[4],"Time ",5))return -1;
    p=line[3]+5;
    for(i=0;i<6;++i) {
        values[i]=p;p=strchr(p,' ');
        if(i<5){if(!p)return -1;*p++=0;}else if(p)return -1;
        if(!option_value(i+1,values[i],0))return -1;
    }
    mode=!strcmp(line[4]+5,"LIVE")?OPT_LIVE:OPT_DATE;
    if(!option_value(mode,line[4]+5,0)||!tz_set(line[1]+5))return -1;
    observer=parsed;strcpy(location_name,line[2]+5);
    for(i=0;i<6;++i)option_value(i+1,values[i],1);
    option_value(mode,line[4]+5,1);return 1;
}
int assets_open(void)
{
    static const char magic[8]={'S','I','D','E','R','A','5',0};
    asset_fd=rpmt_open(asset_path,O_RDONLY);
    if(asset_fd<0)asset_fd=rpmt_open("ROM:sidera.dat",O_RDONLY);
    if(asset_fd<0)asset_fd=rpmt_open("MSC0:/sidera.dat",O_RDONLY);
    if(asset_fd<0)return 0;
    if(read(asset_fd,buffer,8)!=8||memcmp(buffer,magic,8)) {assets_close();return 0;}
    return 1;
}
