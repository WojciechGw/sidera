#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "sidera.h"

char asset_path[160];
static int locked;
void rpmt_os_lock(void){assert(!locked);locked=1;}
void rpmt_os_unlock(void){assert(locked);locked=0;}
long host_lseek(int fd,long offset,int whence){assert(locked);return _lseek(fd,offset,whence);}
int host_read(int fd,void *p,unsigned n){assert(!locked);return _read(fd,p,n);}
int rpmt_open(const char *p,int f){return _open(p,f|_O_BINARY,_S_IREAD|_S_IWRITE);}
int rpmt_close(int fd){return _close(fd);}
int main(int argc,char **argv)
{
    unsigned id,previous=65535;int y,mo,d,h,mi,s;Date utc,local;int32_t offset;
    
    assert(argc==2);sprintf(asset_path,"%s/sidera.dat",argv[1]);
    while(scanf("%u %d %d %d %d %d %d",&id,&y,&mo,&d,&h,&mi,&s)==7) {
        if(id!=previous){assert(tz_select(id));previous=id;}
        utc.year=y;utc.month=(unsigned char)mo;utc.day=(unsigned char)d;
        utc.hour=(unsigned char)h;utc.minute=(unsigned char)mi;utc.second=(unsigned char)s;
        assert(tz_local(&utc,&local,&offset));
        printf("%ld %04d-%02u-%02u %02u:%02u:%02u\n",(long)offset,local.year,
               local.month,local.day,local.hour,local.minute,local.second);
    }
    tz_close();return 0;
}
