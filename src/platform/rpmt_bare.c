/*
 * rpmt_bare.c -- what standalone programs call of rpmt, on a
 * Picocomputer without mtRAM and without rpmt: the program is alone, task
 * 0 and in the foreground, and talks to the RIA itself. Built with
 * -DRPMT_BARE (rpmt.h), beside lib/rpmt_os_file.c and lib/rpmt_os_dir.c;
 * read() and write() are rp6502.lib's. rpmt.cmake uses this for the network
 * programs and ss-saver.rp6502.
 */
#include <stddef.h>
#include <time.h>
#include "rpmt.h"

#define RIA_TX_READY 0x80
#define RIA_RX_READY 0x40
#define ARGV_MAX 512 /* the XSTACK, all the RIA gives */

static rpmt_task_t me;
static char argv_buf[ARGV_MAX];

/* main() gets the monitor's arguments only through this (cc65's mainargs,
 * the RIA's ARGV call): rp6502.lib's own gives no memory, and so none. No
 * API call in here -- the argv is still on the XSTACK. */
void *__fastcall__ __argv_mem(size_t size)
{
    return size <= ARGV_MAX ? argv_buf : NULL;
}

void rpmt_os_lock(void)
{
}

void rpmt_os_unlock(void)
{
}

void rpmt_yield(void)
{
}

unsigned char rpmt_getpid(void)
{
    return 0;
}

volatile rpmt_task_t *__fastcall__ rpmt_task_info(unsigned char id)
{
    (void)id;
    return &me;
}

/* The RIA's clock in rpmt's ticks: wraps as rpmt's does, every 2^16. */
unsigned rpmt_ticks(void)
{
    return (unsigned)(clock() * RPMT_TICK_HZ / CLOCKS_PER_SEC);
}

void __fastcall__ rpmt_sleep_ticks(unsigned ticks)
{
    unsigned start = rpmt_ticks();
    while ((unsigned)(rpmt_ticks() - start) < ticks)
        ;
}

void __fastcall__ rpmt_putc(char c)
{
    while (!(RIA.ready & RIA_TX_READY))
        ;
    RIA.tx = c;
}

void __fastcall__ rpmt_puts(const char *s)
{
    while (*s)
        rpmt_putc(*s++);
}

int rpmt_getc_nb(void)
{
    if (!(RIA.ready & RIA_RX_READY))
        return -1;
    return RIA.rx;
}

/* Only for a program in the background, which here none is. */
unsigned char __fastcall__ rpmt_notify(const char *text, unsigned char seconds)
{
    (void)text;
    (void)seconds;
    return 0;
}
