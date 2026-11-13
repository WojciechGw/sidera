#ifndef SIDERA_ASTRO_H
#define SIDERA_ASTRO_H
#include <stdint.h>

/* Angles: one turn = 65536. Vectors/trigonometry: signed Q14. Dates are
 * Gregorian UTC, independently of the platform's time_t range. */
typedef struct { int year; unsigned char month, day, hour, minute, second; } Date;
typedef struct { int32_t latitude, longitude; int altitude; } Observer; /* 0.1 arcsec, metres */
typedef struct { uint16_t az; int16_t alt; } Position;
typedef struct {
    Position sun, moon;
    uint16_t tilt;          /* clockwise rotation of an image lit from above */
    unsigned char phase;   /* 0..128, new to full (orientation is separate) */
    unsigned illumination; /* 0..1000 */
} Sky;

int date_valid(const Date *d);
int32_t date_day(const Date *d); /* 1999-12-31 = 0 */
int date_step(Date *d, int32_t seconds);
int16_t as_sin(uint16_t a);
int16_t as_cos(uint16_t a);
uint16_t as_atan(int16_t y, int16_t x);
uint16_t as_angle(int32_t millidegrees);
int as_degrees(uint16_t a); /* tenths of a degree, 0..3599 */
void sky_compute(const Date *d, const Observer *o, Sky *s);
void sky_positions(const Date *d,const Observer *o,Sky *s); /* no phase/tilt */
void sky_horizon(const Date *d,const Observer *o,Sky *s); /* upper limbs, standard refraction */
uint16_t moon_longitude(const Date *d); /* geocentric ecliptic longitude */
#define PLANET_COUNT 6
/* Geocentric distances in 1/1024 AU, planets then Sun; updated together. */
extern uint16_t body_distance[PLANET_COUNT+1];
void planets_compute(const Date *d,const Observer *o,Position p[PLANET_COUNT]);
/* J2000 unit vectors -> local north/east/up, with precession to date. */
typedef struct { int16_t m[9]; } StarFrame;
void star_frame(const Date *d, const Observer *o, StarFrame *frame);
void star_position(const StarFrame *frame, const int16_t v[3], Position *p);
#endif
