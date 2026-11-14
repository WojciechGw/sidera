#ifndef SIDERA_SOLAR_H
#define SIDERA_SOLAR_H
#include "astro.h"
#define SOLAR_LEFT 0
#define SOLAR_TOP 71
#define SOLAR_RIGHT 640 /* exclusive; same panel bounds as CITIES */
#define SOLAR_BOTTOM 480 /* exclusive */
#define SOLAR_CX ((SOLAR_LEFT+SOLAR_RIGHT)/2)
#define SOLAR_CY ((SOLAR_TOP+24+SOLAR_BOTTOM-14)/2)
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
