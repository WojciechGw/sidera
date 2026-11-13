/*
 * xram.c -- the way into XRAM for rpc and its programs: the RIA's portals,
 * whose addresses and steps every task shares, so under the OS lock.
 */
#include "rpmt.h"
#include "xr.h"

void xr_read(unsigned addr, void *dst, unsigned n)
{
    unsigned char *p = (unsigned char *)dst;
    rpmt_os_lock();
    RIA.step1 = 1;
    RIA.addr1 = addr;
    while (n--)
        *p++ = RIA.rw1;
    rpmt_os_unlock();
}

/* n bytes from addr two bytes up, the last first so they may overlap:
 * portal 0 reads and portal 1 writes, both stepping down. */
void xr_shift2(unsigned addr, unsigned n)
{
    if (!n)
        return;
    rpmt_os_lock();
    RIA.step0 = -1;
    RIA.addr0 = addr + n - 1;
    RIA.step1 = -1;
    RIA.addr1 = addr + n + 1;
    while (n--)
        RIA.rw1 = RIA.rw0;
    rpmt_os_unlock();
}

void xr_write(unsigned addr, const void *src, unsigned n)
{
    const unsigned char *p = (const unsigned char *)src;
    rpmt_os_lock();
    RIA.step1 = 1;
    RIA.addr1 = addr;
    while (n--)
        RIA.rw1 = *p++;
    rpmt_os_unlock();
}
