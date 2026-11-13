#ifndef SIDERA_HOST_PLATFORM_API
#define SIDERA_HOST_PLATFORM_API
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include "host_platform.h"
#define open host_open
#define close host_close
#define read host_read
#define write _write
#define lseek host_lseek
int host_open(const char *,int);
int host_close(int);
long host_lseek(int,long,int);
int host_read(int,void *,unsigned);
#define stricmp _stricmp
#define strnicmp _strnicmp
#define PLATFORM_TICK_HZ 125u
void platform_puts(const char *);
int platform_getc_nb(void);
void platform_sleep_ticks(unsigned);
unsigned platform_ticks(void);
unsigned char platform_vga_ready(void);
unsigned char platform_keyboard_start(void);
void platform_keyboard_stop(void);
typedef struct { uint16_t addr0,addr1; uint8_t step0,step1,vsync; } HostRIA;
extern HostRIA RIA;
extern uint8_t xram[65536];
int read_xram(unsigned,unsigned,int);
int xreg(int,int,int,...);
void xr_write0(unsigned v);
unsigned char xr_read0(void);
unsigned char xr_read1(void);
#endif
