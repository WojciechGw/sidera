/* Exercise the actual resumable calculation and timezone files. */
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "host_platform.h"
#include "sidera.h"
Date moment;Observer observer;
char asset_path[160];
static unsigned calls;
static void counted(const Date *d,const Observer *o,Sky *s) {++calls;sky_horizon(d,o,s);}
#define sky_horizon counted
#include "../src/riseset.c"
#undef sky_horizon

long host_lseek(int fd,long pos,int whence){return _lseek(fd,pos,whence);}
int host_read(int fd,void *p,unsigned n){return _read(fd,p,n);}
int host_open(const char *p,int f){return _open(p,f|_O_BINARY,_S_IREAD|_S_IWRITE);}
int host_close(int fd){return _close(fd);}
static void daylight_boundaries(void)
{
    Date test_date={2026,11,10,0,0,0};
    int i;
    /* Synthetic local event boundaries isolate clock classification from
     * astronomy. Test normal, midnight-spanning and single-event days. */
    assert(tz_set("Etc/UTC"));
    moment=test_date;rs_sync();
    state=2;events[0]=360;events[1]=1080;
    moment.hour=5;moment.minute=59;moment.second=59;assert(!rs_daylight());
    moment.hour=6;moment.minute=moment.second=0;assert(rs_daylight());
    moment.hour=17;moment.minute=moment.second=59;assert(rs_daylight());
    moment.hour=18;moment.minute=moment.second=0;assert(!rs_daylight());
    /* Reversing simulation time must reverse the transition immediately. */
    assert(date_step(&moment,-1));assert(rs_daylight());
    moment.hour=6;moment.minute=moment.second=0;
    assert(date_step(&moment,-1));assert(!rs_daylight());
    events[0]=1080;events[1]=360;
    moment.hour=0;assert(rs_daylight());moment.hour=12;assert(!rs_daylight());
    moment.hour=18;assert(rs_daylight());
    events[0]=-1;moment.hour=0;assert(rs_daylight());moment.hour=12;assert(!rs_daylight());
    events[0]=360;events[1]=-1;assert(rs_daylight());moment.hour=0;assert(!rs_daylight());
    events[0]=events[1]=360;moment.hour=6;assert(!rs_daylight());
    moment.hour=12;assert(!rs_daylight());
    events[0]=events[1]=-1;
    for(i=1;i<=2;++i) {
        seen[0]=i;moment.hour=0;assert(rs_daylight()==(i==1));
        moment.hour=23;assert(rs_daylight()==(i==1));
    }
    seen[0]=1;state=1;assert(!rs_daylight());state=3;assert(!rs_daylight());
    state=2;assert(rs_daylight());observer.longitude++;assert(!rs_daylight());observer.longitude--;
    assert(tz_set("Europe/Warsaw"));assert(!rs_daylight());
    assert(tz_set("Etc/UTC"));assert(date_step(&moment,86400L));assert(!rs_daylight());
    /* Discard all synthetic cache state before any subsequent use. */
    state=0;
}
int main(int argc,char **argv)
{
    char name[40],sun[40],moon[40];int y,m,d,h,mi;long lat,lon;
    unsigned steps,previous_calls;Date saved;
    host_assert_stderr();
    assert(argc==2);sprintf(asset_path,"%s/sidera.dat",argv[1]);
    while(scanf("%39s %d %d %d %d %d %ld %ld",name,&y,&m,&d,&h,&mi,&lat,&lon)==8) {
        assert(tz_set(name));moment.year=y;moment.month=m;moment.day=d;
        moment.hour=h;moment.minute=mi;moment.second=0;
        observer.latitude=lat;observer.longitude=lon;observer.altitude=0;
        rs_sync();saved=moment;steps=calls=0;
        while(rs_busy()) {
            previous_calls=calls;rs_step();assert(calls-previous_calls<=1);
            assert(++steps<5000);assert(!rs_sync()); /* no restart for the same civil day */
            assert(!memcmp(&moment,&saved,sizeof(moment)));
        }
        rs_text(0,sun);rs_text(1,moon);
        printf("%s|%s|%u|%u|%u\n",sun,moon,steps,calls,state);
        assert(!rs_sync());
    }
    daylight_boundaries();tz_close();return 0;
}
