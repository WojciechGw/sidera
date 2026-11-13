/*
 * xr.h -- XRAM through the RIA's portals, under the OS lock (xram.c).
 */
#ifndef RPC_XR_H
#define RPC_XR_H

void xr_read(unsigned addr, void *dst, unsigned n);
void xr_write(unsigned addr, const void *src, unsigned n);
void xr_shift2(unsigned addr, unsigned n); /* n bytes two up */
/* The string at addr into dst, max bytes with its NUL; its length. The
 * next string is at addr + length + 1, if it fitted. */
unsigned char xr_gets(unsigned addr, char *dst, unsigned max);
/* n bytes from src to dst, which may overlap (xr_move.c). */
void xr_move(unsigned dst, unsigned src, unsigned n);

#endif
