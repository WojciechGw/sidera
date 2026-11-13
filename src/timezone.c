/* Disk-backed UTC intervals: dates remain independent of 32-bit time_t. */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sidera.h"

char zone_name[40]="Europe/Warsaw";
static int zone_fd=-1;
static unsigned zone_count,intervals;
static long table;
static unsigned char record[48],cached;
static int32_t first_day,first_sec,last_day,last_sec,utc_offset;

void tz_close(void)
{
    if(zone_fd>=0)close(zone_fd);zone_fd=-1;table=0;cached=0;
}
static int fetch(long at,void *to,unsigned size)
{
    long pos;
    pos=lseek(zone_fd,at,SEEK_SET);
    return pos==at&&read(zone_fd,to,size)==(int)size;
}
static int open_zones(void)
{
    char path[160],*p;
    if(zone_fd>=0)return 1;
    strcpy(path,asset_path);p=strrchr(path,'/');if(!p)p=strchr(path,':');
    strcpy(p?p+1:path,"sidera.tz");
    zone_fd=open(path,O_RDONLY);
    if(zone_fd<0)zone_fd=open("ROM:sidera.tz",O_RDONLY);
    if(zone_fd<0)zone_fd=open("MSC0:/sidera.tz",O_RDONLY);
    if(zone_fd<0)return 0;
    if(!fetch(0,record,24)||memcmp(record,"SIDTZ001",8)||record[10]!=48||record[11])goto bad;
    zone_count=record[8]|((unsigned)record[9]<<8);
    if(zone_count&&zone_count<1024)return 1;
bad:
    tz_close();return 0;
}
int tz_select(unsigned id)
{
    int32_t pos;unsigned n;
    if(!open_zones()||id>=zone_count||!fetch(24L+(long)id*48,record,48)||record[39])return 0;
    memcpy(&pos,record+40,4);n=record[44]|((unsigned)record[45]<<8);
    if(!n||pos<24L+(long)zone_count*48)return 0;
    table=pos;intervals=n;strcpy(zone_name,(char *)record);cached=0;return 1;
}
int tz_set(const char *name)
{
    unsigned lo=0,hi,mid;int cmp;
    if(!open_zones())return 0;hi=zone_count;
    while(lo<hi) {
        mid=lo+(hi-lo)/2;
        if(!fetch(24L+(long)mid*48,record,48)||record[39])return 0;
        cmp=strcmp(name,(char *)record);
        if(!cmp)return tz_select(mid);
        if(cmp<0)hi=mid;else lo=mid+1;
    }
    return 0;
}
static int before(int32_t day,int32_t sec,int32_t d,int32_t s)
{
    return day<d||(day==d&&sec<s);
}
int tz_local(const Date *utc,Date *local,int32_t *offset)
{
    int32_t day=date_day(utc),sec=(int32_t)utc->hour*3600+utc->minute*60u+utc->second;
    int32_t row[3],s;
    unsigned lo,hi,mid;
    if(!table&&!tz_set(zone_name))return 0;
    if(!cached||before(day,sec,first_day,first_sec)||!before(day,sec,last_day,last_sec)) {
        cached=0;
        lo=0;hi=intervals;
        while(lo<hi) {
            mid=lo+(hi-lo)/2;
            if(!fetch(table+(long)mid*12,row,12))return 0;
            if(before(day,sec,row[0],row[1]))hi=mid;else lo=mid+1;
        }
        if(!lo||!fetch(table+(long)(lo-1)*12,row,12))return 0;
        first_day=row[0];first_sec=row[1];utc_offset=row[2];
        if(utc_offset<=-86400L||utc_offset>=86400L)return 0;
        last_day=1000000L;last_sec=0;
        if(lo<intervals) {
            if(!fetch(table+(long)lo*12,row,12))return 0;
            last_day=row[0];last_sec=row[1];
        }
        cached=1;
    }
    *local=*utc;*offset=utc_offset;
    if(!date_step(local,utc_offset)) {
        /* Local time may cross the UTC simulation's 1900/2200 boundary. */
        s=sec+utc_offset;
        local->year=utc_offset<0?1899:2201;local->month=utc_offset<0?12:1;
        local->day=utc_offset<0?31:1;s=(s+86400L)%86400L;
        local->hour=(unsigned char)(s/3600);local->minute=(unsigned char)(s/60%60);
        local->second=(unsigned char)(s%60);
    }
    return 1;
}
