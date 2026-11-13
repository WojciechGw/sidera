/*
 * scr_ed.c -- the line editor (scr.h).
 */
#include <string.h>
#include "rpmt.h"
#include "key.h"
#include "scr.h"

ed_t ed;

void ed_set(char *buf, unsigned char max)
{
    ed.buf = buf;
    ed.max = max;
    ed.pos = (unsigned char)strlen(buf);
    ed.off = 0;
}

unsigned char ed_key(unsigned k)
{
    char *at = ed.buf + ed.pos;
    unsigned char len = (unsigned char)strlen(ed.buf);
    unsigned char tail = (unsigned char)(len - ed.pos);

    if (k >= 0x20 && k < 0x100 && k != 0x7F)
    {
        if (len + 1 < ed.max)
        {
            memmove(at + 1, at, tail + 1);
            *at = (char)k;
            ++ed.pos;
        }
        return 1;
    }
    switch (k)
    {
    case K_BS:
        if (ed.pos)
        {
            memmove(at - 1, at, tail + 1);
            --ed.pos;
        }
        break;
    case K_DEL:
        if (tail)
            memmove(at, at + 1, tail);
        break;
    case K_LEFT:
        if (ed.pos)
            --ed.pos;
        break;
    case K_RIGHT:
        if (tail)
            ++ed.pos;
        break;
    case K_HOME:
        ed.pos = 0;
        break;
    case K_END:
        ed.pos = len;
        break;
    case CTRL('Y'):
        ed.buf[0] = '\0';
        ed.pos = 0;
        break;
    default:
        return 0;
    }
    return 1;
}

void ed_draw(unsigned char row, unsigned char col, unsigned char w)
{
    if (ed.pos < ed.off)
        ed.off = ed.pos;
    if (ed.pos >= ed.off + w)
        ed.off = (unsigned char)(ed.pos - w + 1);
    o_at(row, col);
    o_fit(ed.buf + ed.off, w);
    o_at(row, (unsigned char)(col + ed.pos - ed.off));
}
