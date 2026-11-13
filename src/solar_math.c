/* Same low-precision Schlyter elements as astro.c; Earth and Neptune added.
 * https://stjarnhimlen.se/comp/ppcomp.html#4
 * Viewed from ecliptic north: +X is the equinox, +Y is 90 degrees longitude.
 * No geocentric subtraction: these are positions relative to the Sun.
 */
#include "solar.h"
#ifdef SIDERA_OVERLAYS
#pragma code-name ("SOLAR_CODE")
#pragma rodata-name ("SOLAR_RODATA")
#endif
#define ONE 16384
#define CIRCLE 360000000L
static int16_t mul(int16_t a, int16_t b) { return (int16_t)((int32_t)a * b / ONE); }
static uint16_t root(uint32_t n)
{
    uint32_t bit = 0x40000000UL, r = 0;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (uint16_t)r;
}
static int32_t modmul(int32_t a, int32_t b)
{
    int32_t r = 0;
    unsigned char neg = 0;
    if (a < 0) { a = -a; neg ^= 1; }
    if (b < 0) { b = -b; neg ^= 1; }
    while (b) {
        if (b & 1) { r += a; if (r >= CIRCLE) r -= CIRCLE; }
        a += a; if (a >= CIRCLE) a -= CIRCLE;
        b >>= 1;
    }
    return neg ? -r : r;
}
static uint16_t element(int32_t base, int32_t rate, int32_t day, uint16_t fraction)
{
    int32_t a = base + modmul(rate,day);
    /* Split the rate: rate*fraction would overflow a 32-bit product. */
    a += (rate / 65536L) * fraction + (rate % 65536L) * (int32_t)(fraction / 2) / 32768L;
    a %= CIRCLE; if (a < 0) a += CIRCLE;
    return as_angle(a / 1000L);
}
typedef struct { int32_t angle[8],a; int16_t e,de; } Orbit;
static const Orbit orbits[8]={
 {{48331300L,3246L,7004700L,5L,29124100L,1014L,168656200L,409233444L},1586L,3369,559},
 {{76679900L,2466L,3394600L,3L,54891000L,1384L,48005200L,160213022L},2963L,111,-1302},
 {{0L,0L,0L,0L,102940400L,4709L,356047000L,98560026L},4096L,274,-1151},
 {{49557400L,2111L,1849700L,-2L,286501600L,2930L,18602100L,52402078L},6241L,1530,2516},
 {{100454200L,2769L,1303000L,-16L,273877700L,1645L,19895000L,8308530L},21310L,795,4469},
 {{113663400L,2390L,2488600L,-11L,339393900L,2977L,316967000L,3344423L},39136L,910,-9499},
 {{74000500L,1398L,773300L,2L,96661200L,3057L,142590500L,1172581L},78568L,775,7450},
 {{131780600L,3017L,1770000L,-26L,272846100L,-603L,260247100L,599515L},123119L,141,2150}
};
static uint16_t planet_element(const int32_t *v,int32_t day,uint16_t f)
{
    return element(v[0],v[1]/100L,day,f)+as_angle(day*(v[1]%100L)/100000L);
}

void solar_orbit(unsigned char body,const Date *date,SolarOrbit *o)
{
    const Orbit *q=&orbits[body];
    int32_t day=date_day(date),ee,a=q->a;
    uint16_t f,n,w,inc,m;
    int16_t cn,sn,cw,sw,ci,er,delta;
    unsigned char k;
    f=(uint16_t)(((uint32_t)date->hour*3600UL+date->minute*60u+date->second)*32768UL/43200UL);
    n=planet_element(q->angle,day,f);inc=planet_element(q->angle+2,day,f);
    w=planet_element(q->angle+4,day,f);m=planet_element(q->angle+6,day,f);
    if(body==6)a-=day*635L/10000000L;
    if(body==7)a+=day*1357L/10000000L;
    o->a=(int16_t)((a+8)/16);
    o->e=q->e+(int16_t)(day*q->de/61035156L);
    o->b=(int16_t)root((uint32_t)ONE*ONE-(int32_t)o->e*o->e);
    cn=as_cos(n);sn=as_sin(n);cw=as_cos(w);sw=as_sin(w);ci=as_cos(inc);
    o->xx=mul(cn,cw)-mul(sn,mul(sw,ci));
    o->xy=-mul(cn,sw)-mul(sn,mul(cw,ci));
    o->yx=mul(sn,cw)+mul(cn,mul(sw,ci));
    o->yy=-mul(sn,sw)+mul(cn,mul(cw,ci));
    ee=m;er=(int16_t)((int32_t)o->e*10430L/ONE);
    for(k=0;k<4;++k) {
        delta=(int16_t)((int16_t)((uint16_t)ee-m)-mul(er,as_sin((uint16_t)ee)));
        ee-=(int32_t)delta*ONE/(ONE-mul(o->e,as_cos((uint16_t)ee)));
    }
    o->phase=(uint16_t)ee;
}
void solar_point(const SolarOrbit *o,uint16_t phase,int16_t *x,int16_t *y)
{
    int16_t a=mul(o->a,as_cos(phase)-o->e),b=mul(o->a,mul(o->b,as_sin(phase)));
    *x=mul(o->xx,a)+mul(o->xy,b);
    *y=mul(o->yx,a)+mul(o->yy,b);
}
