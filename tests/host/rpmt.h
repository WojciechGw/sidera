#ifndef SIDERA_HOST_RPMT
#define SIDERA_HOST_RPMT
#include <stdint.h>
#include "host_platform.h"
#define read host_read
#define write _write
#define lseek host_lseek
long host_lseek(int,long,int);
int host_read(int,void *,unsigned);
#define stricmp _stricmp
#define strnicmp _strnicmp
#define RPMT_GFX 1
#define rpmt_out_tty() 1
#define rpmt_in_tty() 1
void rpmt_con_puts(const char *);
int rpmt_getc_nb(void);
void rpmt_sleep_ticks(unsigned);
unsigned char rpmt_fg_gen(void);
unsigned char rpmt_sigterm(void);
#ifdef RPMT_BARE
#define RPMT_TICK_HZ 125
void rpmt_puts(const char *);
void rpmt_sleep_ticks(unsigned);
#endif
typedef struct { uint16_t addr0,addr1; uint8_t step0,step1,vsync; } HostRIA;
extern HostRIA RIA;
extern uint8_t xram[65536];
void rpmt_os_lock(void);
void rpmt_os_unlock(void);
void rpmt_yield(void);
unsigned rpmt_ticks(void);
int rpmt_open(const char *,int);
int rpmt_close(int);
int read_xram(unsigned,unsigned,int);
int xreg(int,int,int,...);
void xr_write0(unsigned v);
unsigned char xr_read0(void);
unsigned char xr_read1(void);
#endif
