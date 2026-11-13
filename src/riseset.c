/* Local civil-day events, independent of time_t. One ephemeris per step. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sidera.h"

static int32_t day;
static Observer place;
static char zone[40];
static Date scan;
static Sky sample;
static int previous[2],current[2],events[4];
static unsigned span,lo,hi;
static unsigned char state,have_previous,root_body,seen[2],partial,previous_local;

unsigned char rs_busy(void) { return state==1; }
static int altitude(unsigned char body) { return body?sample.moon.alt:sample.sun.alt; }
static void finish(void) { state=partial?3:2; }
unsigned char rs_sync(void)
{
    Date local;int32_t offset,d;
    unsigned char i;
    if(!tz_local(&moment,&local,&offset)){i=state!=3;state=3;zone[0]=0;return i;}
    d=date_day(&local);
    if(state&&day==d&&!memcmp(&place,&observer,sizeof(place))&&!strcmp(zone,zone_name))return 0;
    day=d;place=observer;strcpy(zone,zone_name);
    for(i=0;i<4;++i)events[i]=-1;
    seen[0]=seen[1]=have_previous=previous_local=partial=0;root_body=2;state=1;
    scan=local;scan.hour=scan.minute=scan.second=0;
    if(!date_valid(&scan)){state=3;return 1;}
    if(!date_step(&scan,-86400L)) {
        if(!tz_local(&scan,&local,&offset)){state=3;return 1;}
        partial=date_day(&local)==day&&(local.hour||local.minute||local.second);
    }
    return 1;
}
static void advance(void)
{
    previous[0]=current[0];previous[1]=current[1];have_previous=1;
    /* Resolve short polar excursions at one-minute spacing near the horizon. */
    span=abs(current[0])<92||abs(current[1])<92?60:900;
    if(!date_step(&scan,span)||date_day(&scan)>=day+2)finish();
}
static void next_root(void)
{
    while(root_body<2) {
        if((previous[root_body]<0)!=(current[root_body]<0)) {lo=0;hi=span;return;}
        ++root_body;
    }
    advance();
}
unsigned char rs_step(void)
{
    Date at,local;
    int32_t offset,d;
    unsigned mid;
    unsigned char i;
    if(!rs_busy())return 0;
    at=scan;
    if(root_body<2) {
        mid=(lo+hi)/2;
        date_step(&at,(int32_t)mid-span);
        sky_horizon(&at,&place,&sample);
        if((altitude(root_body)<0)==(previous[root_body]<0))lo=mid;else hi=mid;
        if(hi-lo<=30) {
            if(!tz_local(&at,&local,&offset)){state=3;return 1;}
            if(date_day(&local)==day) {
                i=root_body*2+(previous[root_body]>=0);
                /* Keep the first occurrence if an exceptional civil day has two. */
                if(events[i]<0)events[i]=local.hour*60u+local.minute;
            }
            ++root_body;next_root();
        }
    } else {
        if(!tz_local(&at,&local,&offset)){state=3;return 1;}
        d=date_day(&local);
        /* Skip ephemerides outside the requested day, retaining both endpoints
         * of its boundary intervals. UTC offsets can change during the day. */
        if(d!=day&&!previous_local) {
            date_step(&at,900);
            if(tz_local(&at,&local,&offset)&&date_day(&local)!=day) {
                if(!date_step(&scan,900)||date_day(&scan)>=day+2)finish();
                have_previous=0;return !rs_busy();
            }
        }
        previous_local=d==day;
        /* A clipped first/last UTC day cannot certify a complete local day. */
        if(scan.year==2200&&scan.month==12&&scan.day==31&&scan.hour==23&&d==day)partial=1;
        sky_horizon(&scan,&place,&sample);
        for(i=0;i<2;++i) {
            current[i]=altitude(i);
            if(d==day)seen[i]|=current[i]>=0?1:2;
        }
        root_body=have_previous?0:2;next_root();
    }
    return !rs_busy();
}
unsigned char rs_daylight(void)
{
    Date local;
    int32_t offset;
    int minute,rise=events[0],set=events[1];
    /* Use the same local minute boundaries as the displayed Rise/Set times.
     * Pending, incomplete or stale results must not colour a different day. */
    if(state!=2||memcmp(&place,&observer,sizeof(place))||strcmp(zone,zone_name)||
       !tz_local(&moment,&local,&offset)||date_day(&local)!=day)return 0;
    minute=local.hour*60u+local.minute;
    if(rise<0&&set<0)return seen[0]==1;
    if(rise<0)return minute<set;
    if(set<0)return minute>=rise;
    if(rise<=set)return minute>=rise&&minute<set;
    return minute>=rise||minute<set; /* Daylight spanning local midnight. */
}
void rs_text(unsigned char body,char *text)
{
    unsigned char i;
    int value;
    strcpy(text,state==1?"Rise ..:.. Set ..:..":"Rise --:-- Set --:--");
    if(state==2) {
        for(i=0;i<2;++i) {
            value=events[body*2+i];
            if(value>=0)sprintf(text+5+i*10,"%02d:%02d",value/60,value%60);
        }
        text[10]=' ';text[20]=0;
        if(events[body*2]<0&&events[body*2+1]<0)
            strcat(text,seen[body]==1?" UP":seen[body]==2?" DOWN":"");
    } else if(state==3)strcat(text," N/A");
}
