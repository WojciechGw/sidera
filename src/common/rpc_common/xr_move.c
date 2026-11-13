/*
 * xr_move.c -- n bytes from src to dst inside XRAM, which may overlap
 * (xr.h): portal 0 reads and portal 1 writes, both up, or both down from
 * the end when dst is above src. The OS lock is held for 512 bytes at a
 * time, so the other tasks do not wait long.
 */
#include "rpmt.h"
#include "xr.h"

#define CHUNK 512

void xr_move(unsigned dst, unsigned src, unsigned n)
{
    unsigned k;
    signed char step = 1;
    if (!n || dst == src)
        return;
    if (dst > src)
    {
        step = -1;
        src += n - 1;
        dst += n - 1;
    }
    while (n)
    {
        k = n > CHUNK ? CHUNK : n;
        n -= k;
        rpmt_os_lock();
        RIA.step0 = step;
        RIA.addr0 = src;
        RIA.step1 = step;
        RIA.addr1 = dst;
        if (step > 0)
        {
            src += k;
            dst += k;
        }
        else
        {
            src -= k;
            dst -= k;
        }
        while (k--)
            RIA.rw1 = RIA.rw0;
        rpmt_os_unlock();
    }
}
