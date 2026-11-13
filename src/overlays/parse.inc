/* Shared source; INIT and EDIT each own their parser code and constants. */
static const char *space(const char *p) { while (*p==' ' || *p=='\t') ++p; return p; }
static int number(const char **p,int max)
{
    int n=0;
    const char *s=space(*p);
    if (*s<'0' || *s>'9') return -1;
    while (*s>='0' && *s<='9') {
        if (n > (max-(*s-'0'))/10) return -1;
        n=n*10+(*s++-'0');
    }
    *p=s; return n>max?-1:n;
}
static int symbol(const char **p,unsigned char which)
{
    static const char *const utf[]={"\xC2\xB0","\xE2\x80\xB2","\xE2\x80\xB3"};
    static const char ascii[]={'d','\'','"'};
    const char *s=space(*p);
    unsigned n=(unsigned)strlen(utf[which]);
    if (!strncmp(s,utf[which],n)) s+=n;
    else if (*s==ascii[which] || (unsigned char)*s==which+1) ++s;
    else return 0;
    *p=s;return 1;
}
static int coordinate(const char **p,unsigned char longitude,int32_t *v)
{
    int d,m,s,t;
    char c;
    d=number(p,longitude?180:90);
    if(d<0 || !symbol(p,0)) return 0;
    m=number(p,59);if(m<0 || !symbol(p,1)) return 0;
    s=number(p,59);if(s<0 || **p!='.') return 0;
    ++*p;
    c=**p;if(c<'0'||c>'9')return 0;t=c-'0';++*p;
    if(!symbol(p,2))return 0;
    *p=space(*p);c=(char)toupper(**p);
    if(longitude ? (c!='E'&&c!='W') : (c!='N'&&c!='S'))return 0;
    ++*p;
    if(d==(longitude?180:90) && (m||s||t))return 0;
    *v=(int32_t)d*36000L+m*600L+s*10+t;
    if(c=='W'||c=='S')*v=-*v;
    return 1;
}
PARSE_SCOPE int location_parse(const char *text,Observer *o)
{
    Observer temp;
    int sign=1,n;
    const char *p=text;
    if(!coordinate(&p,0,&temp.latitude))return 0;
    p=space(p);if(*p++!=',')return 0;
    if(!coordinate(&p,1,&temp.longitude))return 0;
    p=space(p);if(*p++!=',')return 0;
    p=space(p);if(*p=='-'){sign=-1;++p;}
    n=number(&p,sign<0?500:10000);if(n<0)return 0;
    p=space(p);if(strncmp(p,"m AMSL",6))return 0;
    if(*space(p+6))return 0;
    temp.altitude=sign*n;*o=temp;return 1;
}
PARSE_SCOPE int date_parse(const char *text,Date *d)
{
    Date t;
    int y,mo,dd,h,mi,se;
    const char *p=text;
    y=number(&p,2200);if(y<1900||*p++!='-')return 0;
    mo=number(&p,12);if(mo<1||*p++!='-')return 0;
    dd=number(&p,31);if(dd<1||*p++!=' ')return 0;
    h=number(&p,23);if(h<0||*p++!=':')return 0;
    mi=number(&p,59);if(mi<0||*p++!=':')return 0;
    se=number(&p,59);if(se<0||*space(p))return 0;
    t.year=y;t.month=(unsigned char)mo;t.day=(unsigned char)dd;t.hour=(unsigned char)h;
    t.minute=(unsigned char)mi;t.second=(unsigned char)se;
    if(!date_valid(&t))return 0;*d=t;return 1;
}
