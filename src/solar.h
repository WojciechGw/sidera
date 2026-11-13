#ifndef SIDERA_SOLAR_H
#define SIDERA_SOLAR_H
#include "astro.h"
#define SOLAR_SIZE 317
#define SOLAR_LEFT (628-10-SOLAR_SIZE)
/* Last upper-compass tick is at y=82; leave ten empty rows below it. */
#define SOLAR_TOP 93
#define SOLAR_RIGHT (SOLAR_LEFT+SOLAR_SIZE)
#define SOLAR_BOTTOM (SOLAR_TOP+SOLAR_SIZE)
#define SOLAR_ROW_BYTES ((SOLAR_RIGHT-1)/8-SOLAR_LEFT/8+1)
#define SOLAR_CX (SOLAR_LEFT+SOLAR_SIZE/2)
#define SOLAR_CY (SOLAR_TOP+(SOLAR_SIZE+14)/2)
#define XR_SOLAR_CFG 0xB096u /* Nine descriptors, ends at $B0F0. */
#define XR_SOLAR_IMAGES 0xE160u /* Earth and Neptune 8x8. */
#define XR_SOLAR_PAL 0xE200u /* Two 512-byte palettes, ends at $E600. */
extern unsigned char solar_visible;
extern Date solar_date;
unsigned char solar_hit(int x,int y,int w,int h);
void solar_hide(void);
int solar_draw(void); /* SOLAR overlay; renders the saved solar_date. */
typedef struct { int16_t x,y,left,top; } SolarLabel;
extern SolarLabel solar_labels[8];
extern int solar_moon_x,solar_moon_y; /* top-left of the 2x2 Moon marker */
extern const char *const solar_names[8];
unsigned char solar_place_labels(void);
/* Osculating Kepler ellipse projected onto the ecliptic; AU*256, Q14 axes. */
typedef struct {
    int16_t a,e,b,xx,xy,yx,yy;
    uint16_t phase;
} SolarOrbit;
void solar_orbit(unsigned char body,const Date *date,SolarOrbit *orbit);
void solar_point(const SolarOrbit *orbit,uint16_t phase,int16_t *x,int16_t *y);
#endif
