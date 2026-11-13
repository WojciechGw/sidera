#ifndef SIDERA_H
#define SIDERA_H
#include "astro.h"
#include "platform.h"
#include "key.h"
#include "tables/catalogue.h"

int startup(int argc,char **argv);
void location_view(void);

extern char zone_name[40];
int tz_set(const char *name);
int tz_select(unsigned id);
int tz_local(const Date *utc,Date *local,int32_t *offset);
void tz_close(void);

/* XRAM layout for VGA resources and astronomical working data. */
#define XR_FONT 0x9600u
#define XR_PAL 0x9A00u
#define XR_SUN 0x9C00u
#define XR_MOON0 0xA000u
#define XR_MOON1 0xA800u
#define XR_CFG 0xB000u
#define XR_MOON_CFG 0xB018u
#define XR_SUN_HEADER_CFG 0xB030u
#define XR_MOON_HEADER_CFG 0xB038u
#define XR_BODY_CFG 0xB050u /* Seven 10-byte custom Mode 5 descriptors, farthest first. */
#define XR_STARS0 0xB100u
#define XR_STARS1 0xBD00u
#define XR_VECTORS 0xC900u /* 64 catalogue vectors, in the gap before planets. */
#define XR_TRAILS 0xCA80u /* 97 pairs of Sun/Moon positions, eight bytes each. */
#define XR_PLANETS 0xD000u
#define XR_META 0xD180u /* Star sizes and 88 fixed-size constellation records. */
#define XR_LABELS 0xDF78u /* 96 label rectangles, five bytes each. */
#define XR_PLANET_PAL 0xE800u
#define XR_EDGES 0xF400u
#define CATALOGUE_RECORD_BYTES (SIDERA_RECORD_COUNT*4u)
#define XR_END (XR_EDGES+CATALOGUE_RECORD_BYTES)
#if XR_END > KEYBOARD_XRAM || KEYBOARD_XRAM > 0xFFE0u
#error Keyboard bitmap overlaps graphics data or exceeds XRAM
#endif
#define DATA_PHASES 2568L
#define DATA_PLANETS 266760L
#define PLANET_BYTES 64u
#define DATA_VECTORS (DATA_PLANETS+PLANET_COUNT*(64L+512L))
#define DATA_EDGES (DATA_VECTORS+(long)SIDERA_STAR_COUNT*6)
#define DATA_COVER (DATA_EDGES+CATALOGUE_RECORD_BYTES)
#define DATA_COVER_PAL (DATA_COVER+57600L)
#define DATA_META (DATA_COVER_PAL+33L*512)
int asset_read(int32_t offset,unsigned addr,unsigned count);
void show_cover(void);
#define VIEW_LEFT 28
#define VIEW_RIGHT 628 /* exclusive */
#define VIEW_CENTER 328
#define VIEW_TOP 84
#define VIEW_BOTTOM 416
#define VIEW_MIDDLE 250
#define CITIES_LEFT (VIEW_LEFT+5)
#define CITIES_RIGHT (VIEW_RIGHT-5) /* exclusive */
#define CITIES_TOP (VIEW_TOP+5)
#define CITIES_BOTTOM (VIEW_BOTTOM-5) /* exclusive */
#define TRAIL_COUNT 97

extern Observer observer;
extern Date moment;
extern Sky sky;
extern int bearing, elevation;
extern int fov,gx_circle;
int gx_zoom(int step);
extern unsigned char grid, trails, live, playing;
extern unsigned char constellation_mode;
extern unsigned char planets;
extern Position planet_positions[PLANET_COUNT];
extern unsigned char trail_done;
extern char note[100];
extern char location_name[45];
extern char config_path[160], asset_path[160];
unsigned char rs_sync(void);
unsigned char rs_busy(void);
unsigned char rs_step(void);
void rs_text(unsigned char body,char *text);
void gx_riseset(void);

void paths_init(void);
int config_load(void); /* 0 missing, 1 read, -1 invalid/I/O */
int config_save(void);
enum { OPT_VIEW=1,OPT_FOV,OPT_GRID,OPT_PATHS,OPT_PLANETS,OPT_CONSTELLATIONS,
       OPT_DATE,OPT_LIVE,OPT_LOCATION,OPT_CONFIG,OPT_ASSETS,OPT_HELP };
unsigned char option_code(const char *name);
int option_value(unsigned char code,const char *value,unsigned char apply);
int location_parse(const char *text, Observer *o);
void location_format(char *text, const Observer *o, unsigned char utf8);
int date_parse(const char *text, Date *d);
void date_format(char *text, const Date *d);
int assets_open(void);
void assets_close(void);
int assets_base(void);
int assets_phase(unsigned char phase, unsigned target);
int assets_vectors(unsigned first,unsigned char count,void *vectors);
void assets_forget_vectors(void);
extern unsigned char editing;
void edit_start(unsigned char type);
unsigned char edit_key(unsigned k);
extern unsigned char cities_active;
int cities_lookup(const char *query);
int cities_start(void);
unsigned char cities_key(unsigned k);
void cities_draw(void);
void cities_close(void);

void gx_clear(int y, int rows);
void gx_text(int x, int y, const char *text);
void gx_label(int x,int y,const char *text,unsigned char white);
void gx_line(int x0,int y0,int x1,int y1,unsigned char dashed);
void gx_pixel(int x,int y);
void gx_dot(int x,int y); /* white 2x2 pixels, with a one-pixel black border */
void gx_read(unsigned addr,void *data,unsigned char count);
void gx_write(unsigned addr,const void *data,unsigned char count);
int gx_canvas(void);
void gx_console(void);
int gx_objects(void);
int gx_project(const Position *p,int *x,int *y);
void gx_scene_begin(void);
void gx_axes_invalidate(void);
void gx_scene_interactive(void);
void gx_view(void);
void gx_scene_step(void);
unsigned char gx_busy(void);
extern int gx_body_x[PLANET_COUNT+2],gx_body_y[PLANET_COUNT+2];
int gx_body_hit(int x,int y,int w,int h);
int gx_label_damaged(int x,int y,int w);
void gx_grid_cycle(void);
void gx_grid_add(void);
void gx_clock(void);
void gx_status(unsigned char busy);
void gx_header(void);
void gx_footer(void);
void gx_planet_labels(void);
void gx_trail(unsigned char body,unsigned char n);
void cs_begin(void);
unsigned char cs_step(void);
unsigned char cs_busy(void);
unsigned char cs_current(void);
void cs_draw_begin(void);
unsigned char cs_project_step(void);
unsigned char cs_draw_step(void);
void cs_labels(unsigned char repair);
void cs_cycle(void);
#endif
