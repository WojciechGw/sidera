/*
 * xr_gets.c -- a NUL-terminated string from XRAM (xr.h).
 */
#include "rpmt.h"
#include "xr.h"

unsigned char xr_gets(unsigned addr, char *dst, unsigned max)
{
    unsigned n = 0;
    char c;
    rpmt_os_lock();
    RIA.step1 = 1;
    RIA.addr1 = addr;
    while (n + 1 < max && (c = RIA.rw1) != '\0')
        dst[n++] = c;
    rpmt_os_unlock();
    dst[n] = '\0';
    return (unsigned char)n;
}
