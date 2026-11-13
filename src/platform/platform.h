#ifndef SIDERA_PLATFORM_H
#define SIDERA_PLATFORM_H
#include <rp6502.h>
#include <fcntl.h>
#include <unistd.h>

/* Preserve the 8 ms time base and unsigned wrap used by the UI. */
#define PLATFORM_TICK_HZ 125u
unsigned platform_ticks(void);
void __fastcall__ platform_sleep_ticks(unsigned ticks);
void __fastcall__ platform_puts(const char *text);
int platform_getc_nb(void);
unsigned char platform_vga_ready(void);
unsigned char platform_keyboard_start(void);
void platform_keyboard_stop(void);
unsigned char platform_keyboard_modifiers(void);
#endif
