/* Compare optimized integer primitives with the former division-based rules.
 * Include the implementation to exercise private Q14/Kepler helpers on cc65. */
#include <assert.h>
#include <stdio.h>
#include "astro.c"

static int16_t reference_mul(int16_t a,int16_t b)
{
    return (int16_t)((int32_t)a*b/16384L);
}
static uint16_t reference_anomaly(uint16_t m,int16_t e,int16_t *radius)
{
    int32_t ee=m;
    int16_t er=(int16_t)((int32_t)e*10430L/16384L),x,y,delta;
    unsigned char k;
    for(k=0;k<3;++k) {
        delta=(int16_t)((int16_t)((uint16_t)ee-m)-reference_mul(er,as_sin((uint16_t)ee)));
        ee-=(int32_t)delta*16384L/(16384-reference_mul(e,as_cos((uint16_t)ee)));
    }
    x=as_cos((uint16_t)ee)-e;
    y=reference_mul((int16_t)root(268435456UL-(int32_t)e*e),as_sin((uint16_t)ee));
    *radius=(int16_t)root((int32_t)x*x+(int32_t)y*y);
    return as_atan(y,x);
}
int main(void)
{
    static const int16_t factors[]={-16384,-1,0,1,16384};
    static const int16_t edges[]={-32768,-32767,-16385,-16384,-16383,-2,-1,0,1,2,16383,16384,16385,32766,32767};
    static const int16_t eccentricities[]={0,274,899,3370};
    uint16_t a,p,i;
    int16_t expected,r0,r1;
    unsigned char j,k;
    assert(MOON_SIN_I==as_sin(as_angle(5145)));
    assert(MOON_COS_I==as_cos(as_angle(5145)));
    for(i=0;i<256;++i)assert(sine[i+1]>=sine[i]&&sine[i+1]-sine[i]<=101);
    a=0;
    do {
        p=a&32767u;if(p>16384u)p=32768u-p;i=p>>6;
        expected=sine[i];
        if(i<256)expected+=(int16_t)((int32_t)(sine[i+1]-expected)*(p&63)/64);
        if(a&32768u)expected=-expected;
        assert(as_sin(a)==expected);
        for(j=0;j<5;++j)assert(mul((int16_t)a,factors[j])==reference_mul((int16_t)a,factors[j]));
    } while(++a);
    for(j=0;j<15;++j)for(k=0;k<15;++k)
        assert(mul(edges[j],edges[k])==reference_mul(edges[j],edges[k]));
    for(j=0;j<4;++j)for(i=0;i<256;++i) {
        a=i*257u;
        assert(anomaly(a,eccentricities[j],&r0)==reference_anomaly(a,eccentricities[j],&r1));
        assert(r0==r1);
    }
    puts("PASS: all 65536 sine angles, 327905 signed Q14 pairs and 1024 Kepler cases agree exactly");
    return 0;
}
