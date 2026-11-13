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
    tz_close();return 0;
}
