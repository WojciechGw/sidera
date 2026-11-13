/*
 * pathx.c -- more about paths, for the jobs (util.h). No I/O: the host
 * test builds it natively.
 */
#include <string.h>
#include "util.h"

static char upc(char c)
{
    return c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
}

/* How much of a is b, case ignored: b's length if a starts with it, else
 * 0. */
static unsigned char prefix(const char *a, const char *b)
{
    unsigned char n = 0;
    while (b[n])
    {
        if (upc(a[n]) != upc(b[n]))
            return 0;
        ++n;
    }
    return n;
}

unsigned char path_same(const char *a, const char *b)
{
    unsigned char n = prefix(a, b);
    return (n || !b[0]) && !a[n];
}

unsigned char path_is_inside(const char *a, const char *b)
{
    unsigned char n = prefix(a, b);
    if (!n)
        return 0;
    /* "MSC0:/" and "MSC0:" contain all of their drive */
    if (b[n - 1] == '/' || b[n - 1] == ':')
        return 1;
    return a[n] == '/' || !a[n];
}

void path_up(char *path)
{
    unsigned char root = path_root_len(path);
    unsigned char i = (unsigned char)strlen(path);
    while (i > root && path[i - 1] != '/')
        --i;
    /* The slash before the last name goes, unless it is the root's. */
    if (i > root)
        --i;
    path[i] = '\0';
}

void path_tail(char *dst, const char *path, unsigned char w)
{
    unsigned char n = (unsigned char)strlen(path);
    if (n < w)
    {
        strcpy(dst, path);
        return;
    }
    dst[0] = '~';
    strcpy(dst + 1, path + n - (w - 2));
}
