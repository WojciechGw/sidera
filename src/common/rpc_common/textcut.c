/*
 * textcut.c -- a row of text for rpc's quick view and file info (Alt+Q,
 * Alt+I), see util.h.
 * Apart from util.c, which every program of rpc's links: only rpc uses it.
 */
#include "util.h"

void text_cut(char *dst, const unsigned char *buf, unsigned len, unsigned *pos,
              unsigned char w, unsigned char ascii)
{
    unsigned p = *pos;
    unsigned char col = 0;
    unsigned char c;

    while (p < len)
    {
        c = buf[p++];
        if (c == '\n')
            break;
        if (c == '\r' || col >= w)
            continue;
        if (c == '\t')
        {
            do
                dst[col++] = ' ';
            while ((col & 7) && col < w);
            continue;
        }
        if (c < 0x20 || c == 0x7F || (c >= 0x80 && ascii))
            c = '.';
        dst[col++] = (char)c;
    }
    dst[col] = '\0';
    *pos = p;
}
