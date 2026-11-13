#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "solar.h"

static int radius[8]={30,42,54,66,83,100,117,134};
static void check_layout(void)
{
    int x,y,w,j,k,px,py,gapx,gapy;
    for(j=0;j<8;++j) {
        x=solar_labels[j].left-1;y=solar_labels[j].top-1;
        w=(int)strlen(solar_names[j])*6+2;
        assert(x>=SOLAR_LEFT+2&&x+w<=SOLAR_RIGHT-2);
        assert(y>=SOLAR_TOP+24&&y+10<=SOLAR_BOTTOM-14);
        assert(x+w<=solar_moon_x-1||x>=solar_moon_x+3||y+10<=solar_moon_y-1||y>=solar_moon_y+3);
        for(k=-1;k<8;++k) {
            int r=k<0?16:4;
            px=k<0?SOLAR_CX:solar_labels[k].x;py=k<0?SOLAR_CY:solar_labels[k].y;
            assert(x+w<=px-r||x>=px+r||y+10<=py-r||y>=py+r);
        }
        for(k=0;k<j;++k) {
            px=solar_labels[k].left-1;py=solar_labels[k].top-1;
            assert(x+w<=px||x>=px+(int)strlen(solar_names[k])*6+2||y+10<=py||y>=py+10);
        }
        px=solar_labels[j].x;py=solar_labels[j].y;
        gapx=x>px+4?x-px-4:x+w<px-4?px-4-x-w:0;
        gapy=y>py+4?y-py-4:y+10<py-4?py-4-y-10:0;
        assert(gapx+gapy<=10); /* directly adjacent, never a distant caption */
    }
}
int main(void)
{
#ifdef LABELS_6502_FIXED
    static const Date dates[]={
        {1900,1,1,0,0,0},{1911,9,14,12,34,56},{1913,3,22,12,34,56},
        {1913,3,24,12,34,56},{1945,9,1,2,45,0},{2026,11,10,14,47,0},
        {2044,4,2,12,34,56},{2093,10,11,12,34,56},{2200,12,31,23,59,59}
    };
#endif
    Date date={1900,1,1,12,34,56};
    SolarOrbit orbit;int j;int16_t x,y;uint16_t angle;
    int32_t n;unsigned long cases=0;
    do {
#ifdef LABELS_6502_FIXED
        date=dates[cases];
#endif
        for(j=0;j<8;++j) {
            solar_orbit((unsigned char)j,&date,&orbit);
            solar_point(&orbit,orbit.phase,&x,&y);angle=as_atan(y,x);
            n=(int32_t)as_cos(angle)*radius[j];solar_labels[j].x=SOLAR_CX+(int)((n+(n<0?-8192:8192))/16384);
            n=(int32_t)as_sin(angle)*radius[j];solar_labels[j].y=SOLAR_CY-(int)((n+(n<0?-8192:8192))/16384);
        }
        angle=moon_longitude(&date);
        n=(int32_t)as_cos(angle)*8;solar_moon_x=solar_labels[2].x+(int)((n+(n<0?-8192:8192))/16384)-1;
        n=(int32_t)as_sin(angle)*8;solar_moon_y=solar_labels[2].y-(int)((n+(n<0?-8192:8192))/16384)-1;
        if(!solar_place_labels()) {
            printf("FAIL: no layout for %04d-%02u-%02u\n",date.year,(unsigned)date.month,(unsigned)date.day);
            for(j=0;j<8;++j)printf("%s %d %d\n",solar_names[j],solar_labels[j].x,solar_labels[j].y);
            return 1;
        }
        check_layout();++cases;
    }
#ifdef LABELS_6502_FIXED
    while(cases<sizeof(dates)/sizeof(dates[0]));
#else
    while(date_step(&date,86400L));
#endif
    printf("PASS: all eight adjacent labels, no sprite/text overlap or overflow on %lu dates, 1900-2200\n",cases);
    return 0;
}
