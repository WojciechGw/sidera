/* Fixed-point adaptation of Paul Schlyter's solar/lunar orbital model:
 * https://stjarnhimlen.se/comp/ppcomp.html (elements and lunar perturbations).
 * All arithmetic here runs on the 6502. No floating point, ephemeris files,
 * or coprocessor. Angular rates use microdegrees/day with modular products
 * to avoid overflow over 1900..2200. UT approximates dynamical time; this is
 * a sky display, not an occultation or navigation ephemeris.
 */
#include "astro.h"
#include "tables/sine.h"
#include "tables/atan.h"
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
int16_t as_sin(uint16_t a)
{
    uint16_t p = a & 32767u, i;
    int16_t v;
    if (p > 16384u) p = 32768u - p;
    i = p >> 6;
    v = sine[i];
    if (i < 256) v += (int16_t)((int32_t)(sine[i + 1] - v) * (p & 63) / 64);
    return (a & 32768u) ? -v : v;
}
int16_t as_cos(uint16_t a) { return as_sin((uint16_t)(a + 16384u)); }
uint16_t as_atan(int16_t y, int16_t x)
{
    /* One ratio division and an interpolated octant lookup for every body.
     * Unsigned magnitudes also handle INT16_MIN without signed negation. */
    uint16_t ax=x<0?0u-(uint16_t)x:(uint16_t)x;
    uint16_t ay=y<0?0u-(uint16_t)y:(uint16_t)y,q,i,a;
    if(!(ax|ay))return 0;
    q=(uint16_t)((uint32_t)(ay<ax?ay:ax)*4096UL/(ay<ax?ax:ay));
    i=q>>4;a=atan_octant[i];
    if(i<256)a+=(atan_octant[i+1]-a)*(q&15)/16;
    if(ay>ax)a=16384u-a;
    if(x<0)a=32768u-a;
    return y<0?0u-a:a;
}
uint16_t as_angle(int32_t md) { return (uint16_t)(md * 4096L / 22500L); }
int as_degrees(uint16_t a) { return (int)((uint32_t)a * 3600UL / 65536UL); }
static unsigned char leap(int y) { return !(y % 4) && (y % 100 || !(y % 400)); }
static unsigned char days(int y, unsigned char m)
{
    static const unsigned char n[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return n[m - 1] + (m == 2 && leap(y));
}
int date_valid(const Date *d)
{
    return d->year >= 1900 && d->year <= 2200 && d->month >= 1 && d->month <= 12 &&
        d->day >= 1 && d->day <= days(d->year,d->month) && d->hour < 24 &&
        d->minute < 60 && d->second < 60;
}
int32_t date_day(const Date *d)
{
    int32_t y = d->year - 1;
    int32_t n = y * 365L + y / 4 - y / 100 + y / 400;
    unsigned char m;
    for (m = 1; m < d->month; ++m) n += days(d->year,m);
    return n + d->day - 730119L;
}
int date_step(Date *d, int32_t seconds)
{
    Date t;
    int32_t s;
    t = *d;
    s = (int32_t)t.hour * 3600 + t.minute * 60u + t.second + seconds;
    while (s >= 86400L) {
        s -= 86400L;
        if (++t.day > days(t.year,t.month)) {
            t.day = 1;
            if (++t.month == 13) { t.month = 1; ++t.year; }
        }
        if (t.year > 2200) return 0;
    }
    while (s < 0) {
        s += 86400L;
        if (!--t.day) {
            if (!--t.month) { t.month = 12; --t.year; }
            if (t.year < 1900) return 0;
            t.day = days(t.year,t.month);
        }
    }
    t.hour = (unsigned char)(s / 3600); s %= 3600;
    t.minute = (unsigned char)(s / 60); t.second = (unsigned char)(s % 60);
    *d = t;
    return 1;
}
/* Modular multiplication: every intermediate is below two full circles. */
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
static int16_t correction(int amplitude, uint16_t a)
{
    return (int16_t)as_angle((int32_t)amplitude * as_sin(a) / ONE);
}
static uint16_t anomaly(uint16_t m, int16_t e, int16_t *radius)
{
    int32_t ee = m;
    int16_t er = (int16_t)((int32_t)e * 10430L / ONE), x,y,delta;
    unsigned char k;
    for (k = 0; k < 3; ++k) {
        delta = (int16_t)((int16_t)((uint16_t)ee - m) - mul(er,as_sin((uint16_t)ee)));
        ee -= (int32_t)delta * ONE / (ONE - mul(e,as_cos((uint16_t)ee)));
    }
    x = as_cos((uint16_t)ee) - e;
    y = mul((int16_t)root((uint32_t)ONE * ONE - (int32_t)e * e),as_sin((uint16_t)ee));
    *radius = (int16_t)root((int32_t)x*x + (int32_t)y*y);
    return as_atan(y,x);
}
typedef struct { int16_t x,y,z; } Vec;
static void equatorial(uint16_t lon, uint16_t lat, uint16_t ob, Vec *v)
{
    int16_t y = mul(as_sin(lon),as_cos(lat)), z = as_sin(lat);
    v->x = mul(as_cos(lon),as_cos(lat));
    v->y = mul(y,as_cos(ob)) - mul(z,as_sin(ob));
    v->z = mul(y,as_sin(ob)) + mul(z,as_cos(ob));
}
static void horizontal(Vec *v, uint16_t sid, uint16_t lat, int16_t par,
                       int altitude, Position *p)
{
    int16_t x = mul(v->x,as_cos(sid)) + mul(v->y,as_sin(sid));
    int16_t east = mul(v->y,as_cos(sid)) - mul(v->x,as_sin(sid));
    int16_t sl = as_sin(lat), cl = as_cos(lat), north, up, rho;
    /* WGS84 geodetic observer, with height, divided by body distance. */
    rho = ONE + mul(55,mul(sl,sl)) + (int16_t)((int32_t)altitude * ONE / 6378137L);
    x -= mul(par,mul(rho,cl));
    v->z -= mul(par,mul(rho - 110,sl));
    north = mul(v->z,cl) - mul(x,sl);
    up = mul(x,cl) + mul(v->z,sl);
    p->az = as_atan(east,north);
    p->alt = (int16_t)as_atan(up,(int16_t)root((int32_t)east*east + (int32_t)north*north));
}
static uint16_t compute_sky(const Date *d,const Observer *o,Sky *s,unsigned char shading)
{
    int32_t day = date_day(d), seconds = (int32_t)d->hour * 3600 + d->minute * 60u + d->second;
    uint16_t f = (uint16_t)((uint32_t)seconds * 32768UL / 43200UL);
    uint16_t ms,ws,ls,mm,wm,nm,lm,dd,ff,ob,lon,lat,sid,v;
    int16_t radius,x,y,z,si,ci,sl,cl,proj,illum,tx,ty;
    int32_t dist;
    Vec sun,moon;
    ms = element(356047000L,985600L,day,f);
    ws = element(282940400L,47L,day,f);
    ls = ms + ws;
    ob = element(23439300L,0,0,0) - (uint16_t)(day * 65L / 1000000L);
    v = anomaly(ms,(int16_t)(274L - day / 53000L),&radius);
    equatorial(v + ws,0,ob,&sun);
    mm = element(115365400L,13064993L,day,f);
    wm = element(318063400L,164357L,day,f);
    nm = element(125122800L,-52954L,day,f);
    lm = mm + wm + nm; dd = lm - ls; ff = lm - nm;
    v = anomaly(mm,899,&radius) + wm;
    si = as_sin(as_angle(5145)); ci = as_cos(as_angle(5145));
    x = mul(as_cos(nm),as_cos(v)) - mul(as_sin(nm),mul(as_sin(v),ci));
    y = mul(as_sin(nm),as_cos(v)) + mul(as_cos(nm),mul(as_sin(v),ci));
    z = mul(as_sin(v),si);
    lon = as_atan(y,x);
    lat = as_atan(z,(int16_t)root((int32_t)x*x + (int32_t)y*y));
    lon += correction(-1274,mm-2u*dd) + correction(658,2u*dd) + correction(-186,ms)
        + correction(-59,2u*mm-2u*dd) + correction(-57,mm-2u*dd+ms)
        + correction(53,mm+2u*dd) + correction(46,2u*dd-ms) + correction(41,mm-ms)
        + correction(-35,dd) + correction(-31,mm+ms) + correction(-15,2u*ff-2u*dd)
        + correction(11,mm-4u*dd);
    if(shading==3)return lon;
    lat += correction(-173,ff-2u*dd) + correction(-55,mm-ff-2u*dd)
        + correction(-46,mm+ff-2u*dd) + correction(33,ff+2u*dd) + correction(17,2u*mm+ff);
    equatorial(lon,lat,ob,&moon);
    /* Distance in km. Split multiplication keeps it within int32. */
    dist = (int32_t)radius * 23462L / 1000L;
    dist -= (int32_t)3699 * as_cos(mm-2u*dd) / ONE + (int32_t)2934 * as_cos(2u*dd) / ONE;
    if(shading==1) {
    proj = mul(sun.x,moon.x) + mul(sun.y,moon.y) + mul(sun.z,moon.z);
    /* Finite Sun distance is negligible for icon shading; 129 samples use
     * uniform illumination steps, at most 0.4 percentage point error. */
    if (proj > ONE) proj = ONE; if (proj < -ONE) proj = -ONE;
    illum = (int16_t)((ONE - (int32_t)proj) / 2);
    s->illumination = (unsigned)((int32_t)illum * 1000 / ONE);
    s->phase = (unsigned char)(((int32_t)illum * 128 + ONE/2) / ONE);
    }
    sid = ls + 32768u + f + (uint16_t)((o->longitude / 100L) * 4096L / 8100L);
    lat = (uint16_t)((o->latitude / 100L) * 4096L / 8100L);
    horizontal(&sun,sid,lat,0,o->altitude,&s->sun);
    horizontal(&moon,sid,lat,(int16_t)(104499446L / dist),o->altitude,&s->moon);
    if(shading==2) {
        s->sun.alt+=152; /* 16 arcmin solar radius + 34 arcmin refraction. */
        s->moon.alt+=103+(int16_t)(18122000L/dist); /* topocentric: no second parallax correction */
        return 0;
    }
    if(!shading)return 0;
    /* Project Sun direction on Moon's local horizontal tangent plane.
     * tx is toward increasing azimuth, ty toward the zenith. This is the
     * bright-limb position angle minus the parallactic angle directly. */
    v = s->sun.az - s->moon.az;
    sl = as_sin((uint16_t)s->sun.alt); cl = as_cos((uint16_t)s->sun.alt);
    tx = mul(cl,as_sin(v));
    ty = mul(sl,as_cos((uint16_t)s->moon.alt)) -
         mul(mul(cl,as_cos(v)),as_sin((uint16_t)s->moon.alt));
    s->tilt = as_atan(tx,ty);
    return 0;
}
void sky_compute(const Date *d,const Observer *o,Sky *s) { compute_sky(d,o,s,1); }
void sky_positions(const Date *d,const Observer *o,Sky *s) { compute_sky(d,o,s,0); }
void sky_horizon(const Date *d,const Observer *o,Sky *s) { compute_sky(d,o,s,2); }
uint16_t moon_longitude(const Date *d) { return compute_sky(d,0,0,3); }

void star_frame(const Date *d, const Observer *o, StarFrame *frame)
{
    int32_t day=date_day(d),jday=day-1;
    uint16_t f,sid,lat,z,theta;
    int16_t cz,sz,ct,st,cs,ss,cl,sl,x,y,vz,a,b,c,h;
    unsigned char i;
    f=(uint16_t)(((uint32_t)d->hour*3600UL+d->minute*60u+d->second)*32768UL/43200UL);
    sid=element(356047000L,985600L,day,f)+element(282940400L,47L,day,f)+32768u+f+
        (uint16_t)((o->longitude/100L)*4096L/8100L);
    lat=(uint16_t)((o->latitude/100L)*4096L/8100L);
    /* Linear terms of the Lieske IAU 1976 precession angles from J2000:
     * zeta=z=2306.2181 arcsec/century, theta=2004.3109. Higher orders
     * contribute <5 arcsec over 1900..2200, below our angle quantum.
     * Constants below convert days straight to 1/65536-turn units. */
    z=(uint16_t)(jday*3193L/1000000L);
    theta=(uint16_t)(jday*2775L/1000000L);
    cz=as_cos(z);sz=as_sin(z);ct=as_cos(theta);st=as_sin(theta);
    cs=as_cos(sid);ss=as_sin(sid);cl=as_cos(lat);sl=as_sin(lat);
    for(i=0;i<3;++i) {
        x=i==0?ONE:0;y=i==1?ONE:0;vz=i==2?ONE:0;
        a=mul(cz,x)-mul(sz,y);b=mul(sz,x)+mul(cz,y);
        c=mul(st,a)+mul(ct,vz);a=mul(ct,a)-mul(st,vz);
        x=mul(cz,a)-mul(sz,b);y=mul(sz,a)+mul(cz,b);
        h=mul(x,cs)+mul(y,ss);
        frame->m[i]=mul(c,cl)-mul(h,sl);
        frame->m[3+i]=mul(y,cs)-mul(x,ss);
        frame->m[6+i]=mul(h,cl)+mul(c,sl);
    }
}
void star_position(const StarFrame *frame,const int16_t v[3],Position *p)
{
    int16_t n,e,u;
    const int16_t *m=frame->m;
    n=(int16_t)(((int32_t)m[0]*v[0]+(int32_t)m[1]*v[1]+(int32_t)m[2]*v[2])/ONE);
    e=(int16_t)(((int32_t)m[3]*v[0]+(int32_t)m[4]*v[1]+(int32_t)m[5]*v[2])/ONE);
    u=(int16_t)(((int32_t)m[6]*v[0]+(int32_t)m[7]*v[1]+(int32_t)m[8]*v[2])/ONE);
    p->az=as_atan(e,n);
    p->alt=(int16_t)as_atan(u,(int16_t)root((int32_t)e*e+(int32_t)n*n));
}

/* Schlyter elements, equinox of date. Angles are microdegrees at epoch,
 * rates are 1e-8 degrees/day; a is AU*4096, e is Q14 and de is 1e-12/day.
 * Splitting the angular rates preserves the full 1900..2200 range without
 * 64-bit arithmetic. See https://stjarnhimlen.se/comp/ppcomp.html . */
typedef struct { int32_t angle[8],a; int16_t e,de; } Orbit;
static const Orbit orbits[PLANET_COUNT]={
 {{48331300L,3246L,7004700L,5L,29124100L,1014L,168656200L,409233444L},1586L,3369,559},
 {{76679900L,2466L,3394600L,3L,54891000L,1384L,48005200L,160213022L},2963L,111,-1302},
 {{49557400L,2111L,1849700L,-2L,286501600L,2930L,18602100L,52402078L},6241L,1530,2516},
 {{100454200L,2769L,1303000L,-16L,273877700L,1645L,19895000L,8308530L},21310L,795,4469},
 {{113663400L,2390L,2488600L,-11L,339393900L,2977L,316967000L,3344423L},39136L,910,-9499},
 {{74000500L,1398L,773300L,2L,96661200L,3057L,142590500L,1172581L},78568L,775,7450}
};
static uint16_t planet_element(const int32_t *v,int32_t day,uint16_t f)
{
    return element(v[0],v[1]/100L,day,f)+as_angle(day*(v[1]%100L)/100000L);
}
typedef struct { unsigned char planet,latitude; signed char j,s,u; int16_t phase,amp; } Perturb;
/* Phases in tenths of a degree; cosine is encoded as sine(angle+90). */
static const Perturb perturb[]={
 {3,0,2,-5,0,-676,-332},{3,0,2,-2,0,210,-56},{3,0,3,-5,0,210,42},
 {3,0,1,-2,0,0,-36},{3,0,1,-1,0,900,22},{3,0,2,-3,0,520,23},{3,0,1,-5,0,-690,-16},
 {4,0,2,-5,0,-676,812},{4,0,2,-4,0,880,-229},{4,0,1,-2,0,-30,119},
 {4,0,2,-6,0,-690,46},{4,0,1,-3,0,320,14},
 {4,1,2,-4,0,880,-20},{4,1,2,-6,0,-490,18},
 {5,0,0,1,-2,60,40},{5,0,0,1,-3,330,35},{5,0,1,0,-1,200,-15}
};
uint16_t body_distance[PLANET_COUNT+1];
void planets_compute(const Date *d,const Observer *o,Position p[PLANET_COUNT])
{
    int32_t day=date_day(d),xx,yy,zz,a;
    uint16_t f,ms,ws,sid,ob,lat,n,inc,w,m[PLANET_COUNT],v,lon,beta,arg;
    int16_t radius,sx,sy,x,y,z,e;
    unsigned char i,k;
    Vec eq;
    const Orbit *q;
    const Perturb *t;
    f=(uint16_t)(((uint32_t)d->hour*3600UL+d->minute*60u+d->second)*32768UL/43200UL);
    ms=element(356047000L,985600L,day,f);ws=element(282940400L,47L,day,f);
    v=anomaly(ms,(int16_t)(274L-day/53000L),&radius)+ws;
    body_distance[PLANET_COUNT]=(uint16_t)radius/16u;
    sx=mul(radius,as_cos(v));sy=mul(radius,as_sin(v));
    sid=ms+ws+32768u+f+(uint16_t)((o->longitude/100L)*4096L/8100L);
    lat=(uint16_t)((o->latitude/100L)*4096L/8100L);
    ob=as_angle(23439L)-(uint16_t)(day*65L/1000000L);
    for(i=0;i<PLANET_COUNT;++i)m[i]=planet_element(orbits[i].angle+6,day,f);
    for(i=0;i<PLANET_COUNT;++i) {
        q=&orbits[i];n=planet_element(q->angle,day,f);
        inc=planet_element(q->angle+2,day,f);w=planet_element(q->angle+4,day,f);
        e=q->e+(int16_t)(day*q->de/61035156L);
        v=anomaly(m[i],e,&radius)+w;
        x=mul(as_cos(n),as_cos(v))-mul(as_sin(n),mul(as_sin(v),as_cos(inc)));
        y=mul(as_sin(n),as_cos(v))+mul(as_cos(n),mul(as_sin(v),as_cos(inc)));
        z=mul(as_sin(v),as_sin(inc));
        lon=as_atan(y,x);beta=as_atan(z,(int16_t)root((int32_t)x*x+(int32_t)y*y));
        for(k=0;k<sizeof(perturb)/sizeof(perturb[0]);++k) {
            t=&perturb[k];if(t->planet!=i)continue;
            arg=(uint16_t)((int32_t)t->j*m[3]+(int32_t)t->s*m[4]+(int32_t)t->u*m[5])+as_angle((int32_t)t->phase*100L);
            if(t->latitude)beta+=correction(t->amp,arg);else lon+=correction(t->amp,arg);
        }
        a=q->a;if(i==5)a-=day*635L/10000000L;
        xx=(int32_t)mul(radius,mul(as_cos(lon),as_cos(beta)))*a/4096L+sx;
        yy=(int32_t)mul(radius,mul(as_sin(lon),as_cos(beta)))*a/4096L+sy;
        zz=(int32_t)mul(radius,as_sin(beta))*a/4096L;
        /* Normalize before Q14 rotations and squared lengths. Planetary
         * parallax (<1 arcmin) is below this display's precision. */
        k=0;
        while(xx>8192L||xx< -8192L||yy>8192L||yy< -8192L||zz>8192L||zz< -8192L){xx/=2;yy/=2;zz/=2;++k;}
        body_distance[i]=(uint16_t)(((uint32_t)root(xx*xx+yy*yy+zz*zz)<<k)/16u);
        eq.x=(int16_t)xx;
        eq.y=mul((int16_t)yy,as_cos(ob))-mul((int16_t)zz,as_sin(ob));
        eq.z=mul((int16_t)yy,as_sin(ob))+mul((int16_t)zz,as_cos(ob));
        horizontal(&eq,sid,lat,0,o->altitude,&p[i]);
    }
}
