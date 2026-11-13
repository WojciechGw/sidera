/*
 * util.c -- rpc's helpers without I/O (see util.h).
 */
#include <string.h>
#include "util.h"

static char upc(char c)
{
    return c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
}

/* One mask, p up to pe, against the whole of s. Backtracks only to the last
 * star, which is enough for '*' and '?'. */
static unsigned char match1(const char *p, const char *pe, const char *s)
{
    const char *star_p = 0;
    const char *star_s = 0;

    if (pe - p == 3 && p[0] == '*' && p[1] == '.' && p[2] == '*')
        return 1;
    while (*s)
    {
        if (p < pe && *p == '*')
        {
            star_p = ++p;
            star_s = s;
        }
        else if (p < pe && (*p == '?' || upc(*p) == upc(*s)))
        {
            ++p;
            ++s;
        }
        else if (star_p)
        {
            p = star_p;
            s = ++star_s;
        }
        else
            return 0;
    }
    while (p < pe && *p == '*')
        ++p;
    return p == pe;
}

static unsigned char is_sep(char c)
{
    return c == ';' || c == ',' || c == ' ';
}

unsigned char mask_match(const char *masks, const char *name)
{
    const char *e;
    unsigned char any = 0;
    for (;;)
    {
        while (is_sep(*masks))
            ++masks;
        if (!*masks)
            return !any;
        any = 1;
        for (e = masks; *e && !is_sep(*e); ++e)
            ;
        if (match1(masks, e, name))
            return 1;
        masks = e;
    }
}

unsigned char path_root_len(const char *path)
{
    const char *c = strchr(path, ':');
    const char *p = c ? c + 1 : path;
    /* Another computer's: "NET:<host>:<port>/" (netc.h). */
    if (!strncmp(path, "NET:", 4))
    {
        c = strchr(path, '/');
        return (unsigned char)(c ? c - path + 1 : strlen(path));
    }
    return (unsigned char)(p - path + (*p == '/'));
}

unsigned char path_is_root(const char *path)
{
    return path[path_root_len(path)] == '\0';
}

unsigned char path_join(char *dst, const char *dir, const char *name)
{
    unsigned dl = strlen(dir);
    unsigned nl = strlen(name);
    unsigned char slash = dl && dir[dl - 1] != '/' && dir[dl - 1] != ':';
    if (dl + slash + nl >= RPC_PATH)
        return 0;
    if (dst != dir)
        memcpy(dst, dir, dl);
    if (slash)
        dst[dl++] = '/';
    memcpy(dst + dl, name, nl + 1);
    return 1;
}

unsigned char path_parent(char *path, char *child)
{
    unsigned char root = path_root_len(path);
    unsigned char i = (unsigned char)strlen(path);
    if (i <= root)
    {
        *child = '\0';
        return 0;
    }
    while (i > root && path[i - 1] != '/')
        --i;
    strcpy(child, path + i);
    /* The slash before the child goes, unless it is the root's. Not as
     * path[i > root ? i - 1 : root]: cc65 2.19 takes the index for the
     * root case from the wrong stack slot. */
    if (i > root)
        --i;
    path[i] = '\0';
    return 1;
}

unsigned char fmt_ul(char *dst, unsigned long v)
{
    char tmp[10];
    unsigned char n = 0;
    unsigned char i = 0;
    do
    {
        tmp[n++] = (char)('0' + (unsigned char)(v % 10));
        v /= 10;
    } while (v);
    while (n)
        dst[i++] = tmp[--n];
    dst[i] = '\0';
    return i;
}

void fmt_size(char *dst, unsigned long v, unsigned char width)
{
    char num[12];
    unsigned char n = fmt_ul(num, v);
    unsigned char i = 0;
    if (n > width)
    {
        n = fmt_ul(num, v >> 10);
        num[n++] = 'K';
        num[n] = '\0';
    }
    if (n > width)
    {
        n = fmt_ul(num, v >> 20);
        num[n++] = 'M';
        num[n] = '\0';
    }
    while (n + i < width)
        dst[i++] = ' ';
    strcpy(dst + i, num);
}

static char *put2(char *p, unsigned char v)
{
    *p++ = (char)('0' + v / 10 % 10);
    *p++ = (char)('0' + v % 10);
    return p;
}

void fmt_date(char *dst, unsigned fdate, unsigned char century)
{
    unsigned year = 1980 + (fdate >> 9);
    if (century)
        dst = put2(dst, (unsigned char)(year / 100));
    dst = put2(dst, (unsigned char)(year % 100));
    *dst++ = '-';
    dst = put2(dst, (unsigned char)((fdate >> 5) & 15));
    *dst++ = '-';
    dst = put2(dst, (unsigned char)(fdate & 31));
    *dst = '\0';
}

void fmt_time(char *dst, unsigned ftime)
{
    dst = put2(dst, (unsigned char)(ftime >> 11));
    *dst++ = ':';
    dst = put2(dst, (unsigned char)((ftime >> 5) & 63));
    *dst = '\0';
}
