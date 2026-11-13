/*
 * ftype.c -- rpc's file info (Alt+I): sizes and what a file is, see util.h.
 * Apart from util.c, which every program of rpc's links: only rpc uses it.
 */
#include <string.h>
#include "util.h"

void fmt_bytes(char *dst, unsigned long v)
{
    unsigned long k = v >> 10;
    dst += fmt_ul(dst, v);
    strcpy(dst, " B");
    if (!k)
        return;
    dst += 2;
    strcpy(dst, ", ");
    dst += 2;
    if (k < 1024)
    {
        dst += fmt_ul(dst, k);
        strcpy(dst, " KB");
        return;
    }
    /* In tenths of a MB, through KB: v * 10 does not fit in 32 bits. */
    k = k * 10 >> 10;
    dst += fmt_ul(dst, k / 10);
    *dst++ = '.';
    *dst++ = (char)('0' + (unsigned char)(k % 10));
    strcpy(dst, " MB");
}

static unsigned char has_ext(const char *n, const char *ext)
{
    unsigned char l = (unsigned char)strlen(n);
    unsigned char e = (unsigned char)strlen(ext);
    const char *a;
    if (l <= e)
        return 0;
    /* ext is lower case: | 0x20 leaves '.' and digits as they are. */
    a = n + l - e;
    while (*ext)
    {
        if ((*a | 0x20) != *ext)
            return 0;
        ++a;
        ++ext;
    }
    return 1;
}

static char *put_hex4(char *p, unsigned v)
{
    static const char hx[] = "0123456789ABCDEF";
    *p++ = '$';
    *p++ = hx[v >> 12];
    *p++ = hx[(v >> 8) & 15];
    *p++ = hx[(v >> 4) & 15];
    *p++ = hx[v & 15];
    return p;
}

static unsigned long le32(const unsigned char *b)
{
    return b[0] | (unsigned)b[1] << 8 | (unsigned long)b[2] << 16 | (unsigned long)b[3] << 24;
}

static unsigned long be32(const unsigned char *b)
{
    return (unsigned long)b[0] << 24 | (unsigned long)b[1] << 16 | (unsigned)b[2] << 8 | b[3];
}

/* "what WxH, B bit" */
static void picture(char *dst, const char *what, unsigned long w, unsigned long h, unsigned char bits)
{
    strcpy(dst, what);
    dst += 4;
    dst += fmt_ul(dst, w);
    *dst++ = 'x';
    dst += fmt_ul(dst, h);
    strcpy(dst, ", ");
    dst += 2;
    dst += fmt_ul(dst, bits);
    strcpy(dst, " bit");
}

void file_type(char *dst, const char *name, const unsigned char *h, unsigned n)
{
    static const unsigned char png_sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    static const unsigned char png_ch[7] = {1, 0, 3, 1, 2, 0, 4}; /* by colour type */
    unsigned long v;
    unsigned i;
    unsigned char c;

    if (!n)
    {
        strcpy(dst, "empty");
        return;
    }
    if (has_ext(name, ".com"))
    {
        if (n >= 6 && !memcmp(h, "RPM", 3) && (h[3] == 'L' || h[3] == 'T'))
        {
            /* The header's end is the first byte after the program. */
            strcpy(dst, h[3] == 'L' ? "rpmt program, $0800-" : "rpmt program (old), $8000-");
            dst += strlen(dst);
            *put_hex4(dst, (unsigned)(h[4] | h[5] << 8) - 1) = '\0';
        }
        else
            strcpy(dst, ".com, not rpmt");
        return;
    }
    if (has_ext(name, ".rp6502") || (n >= 8 && !memcmp(h, "#!RP6502", 8)))
    {
        strcpy(dst, "RP6502 ROM");
        return;
    }
    if (n >= 30 && h[0] == 'B' && h[1] == 'M')
    {
        v = le32(h + 22);
        if (v & 0x80000000UL)
            v = 0 - v; /* top-down */
        picture(dst, "BMP ", le32(h + 18), v, h[28]);
        return;
    }
    if (n >= 26 && !memcmp(h, png_sig, 8) && h[25] < 7)
    {
        picture(dst, "PNG ", be32(h + 16), be32(h + 20), (unsigned char)(h[24] * png_ch[h[25]]));
        return;
    }
    for (i = 0; i < n; ++i)
    {
        c = h[i];
        if (c < 0x20 && c != '\t' && c != '\r' && c != '\n' && c != 0x1A)
        {
            strcpy(dst, "binary");
            return;
        }
    }
    strcpy(dst, "text");
}
