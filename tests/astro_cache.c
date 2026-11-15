/* Golden results predate the day/observer caches. Interleave unrelated callers
 * to exercise keys across midnight, calendar limits and observer changes. */
#include <assert.h>
#include <stdio.h>
#include "astro.h"

static void position(const Position *p) { printf(" %u %d",p->az,p->alt); }
int main(void)
{
    static const Date dates[]={
        {1999,12,31,0,0,0},{2000,1,1,0,0,0},
        {1900,1,1,0,0,0},{1900,2,28,23,59,59},
        {2000,2,29,12,34,56},{2026,3,29,1,0,0},
        {2026,11,17,0,0,0},{2026,11,17,23,59,59},
        {2026,11,18,0,0,0},{2100,3,1,0,0,0},
        {2200,12,31,23,59,59},{2026,11,17,12,0,0}
    };
    static const Observer places[]={
        {0,0,0},{1880272L,756424L,113},{1880272L,756424L,8849},
        {1880272L,-6480000L,-430},{3240000L,6480000L,0},
        {-3240000L,-6480000L,2835}
    };
    Sky full,plain,horizon,discard;
    Position planets[PLANET_COUNT];StarFrame frame;
    unsigned char i,j,k;
    for(i=0;i<12;++i)for(j=0;j<6;++j) {
        sky_compute(&dates[i],&places[j],&full);
        printf("%u %u",i,j);position(&full.sun);position(&full.moon);
        printf(" %u %u %u",full.tilt,full.phase,full.illumination);
        /* Perturb both caches, including the longitude-only call without an
         * observer. Then request the original instant through another API. */
        planets_compute(&dates[(i+5)%12],&places[(j+1)%6],planets);
        printf(" %u",moon_longitude(&dates[(i+7)%12]));
        sky_positions(&dates[i],&places[j],&plain);
        assert(full.sun.az==plain.sun.az&&full.sun.alt==plain.sun.alt);
        assert(full.moon.az==plain.moon.az&&full.moon.alt==plain.moon.alt);
        sky_horizon(&dates[i],&places[j],&horizon);
        position(&horizon.sun);position(&horizon.moon);
        sky_compute(&dates[(i+1)%12],&places[(j+3)%6],&discard);
        star_frame(&dates[i],&places[j],&frame);
        for(k=0;k<9;++k)printf(" %d",frame.m[k]);
        planets_compute(&dates[i],&places[j],planets);
        for(k=0;k<PLANET_COUNT;++k){position(&planets[k]);printf(" %u",body_distance[k]);}
        printf(" %u\n",body_distance[PLANET_COUNT]);
    }
    return 0;
}
