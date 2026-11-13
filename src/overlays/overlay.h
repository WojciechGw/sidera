#ifndef SIDERA_OVERLAY_H
#define SIDERA_OVERLAY_H

#define OVL_BASE 0xC800u
#define OVL_BYTES 0x1700u
#ifdef SIDERA_OVERLAY_HOST
extern unsigned char ovl_memory[6144];
#define OVL_MEMORY ovl_memory
#define OVL_SCRATCH (ovl_memory+OVL_BYTES)
#else
#define OVL_MEMORY ((unsigned char *)OVL_BASE)
#define OVL_SCRATCH 0xDF00u
#endif
#define OVL_HEADER_BYTES 24u
enum { OVL_NONE, OVL_INIT, OVL_CITIES, OVL_EDIT, OVL_COVER, OVL_SOLAR, OVL_FIND };

extern const char *boot_location;
extern int boot_config;
int boot_prepare(int argc,char **argv);
void boot_apply(int argc,char **argv);

#ifdef SIDERA_OVERLAYS
extern unsigned char ovl_failed;
extern const unsigned char ovl_build_id[8];
unsigned char ovl_select(unsigned char id);
unsigned char ovl_loaded(unsigned char id);
#else
#define ovl_select(id) 1
#define ovl_loaded(id) 1
#define ovl_failed 0
#endif
#endif
