"""Schematic geographic regions for SIDERA's locator, not political borders.

Europe/Asia follows a simplified Urals–Caspian–Caucasus–Turkish Straits line.
Oceania includes New Guinea and Pacific islands, including Hawaii. Madeira and
the Canaries are grouped with Africa; Greenland with North America. Boundary
choices are conventional, especially for islands and transcontinental cities.
"""
NAMES=('Europe','Asia','Africa','North America','South America','Oceania','Antarctica')

def inside(lon,lat,points):
    yes=False
    for (x,y),(xx,yy) in zip(points,points[1:]+points[:1]):
        if (y>lat)!=(yy>lat) and lon<(xx-x)*(lat-y)/(yy-y)+x:yes=not yes
    return yes

def region(lon,lat):
    if lat < -60:return 7
    if -60<lon<-10 and lat>67:return 4 # Eastern Greenland, not Europe.
    if lon>170 and 50<lat<60:return 4 # Western Aleutians across the date line.
    if inside(lon,lat,[(105,-60),(181,-60),(181,30),(140,30),(135,25),(129,10),(129,0),(129,-10),(105,-10)]) or \
       inside(lon,lat,[(-181,-60),(-100,-60),(-100,0),(-130,0),(-130,30),(-181,30)]):return 6
    if lon < -169 and lat>60:return 2
    if lon < -30:
        if -62<=lon<=-60 and 9.9<=lat<=12:return 4
        if inside(lon,lat,[(-100,-60),(-30,-60),(-30,9.5),(-60,9.5),(-62,11.5),
                          (-71,12.5),(-75,11.5),(-77,8.5),(-77.5,7.5),(-80,7),(-100,0)]):return 5
        return 4
    if inside(lon,lat,[(-30,-60),(65,-60),(65,-10),(50,15),(43,12),
                      (34,28),(32.6,30),(32.3,31.3),(34,34),(-10,35),(-30,35)]):return 3
    if inside(lon,lat,[(-30,35),(-10,35),(10,36),(26,34.5),(27.5,40),
                      (29.05,40.98),(29.15,41.25),(28,42),(35,44),(39,43.3),(40,43.5),(43,43),
                      (46.5,41.9),(48,42),(51.8,47),(55.5,51),(60,55),(60,60),
                      (65,67),(68,75),(68,91),(-30,91)]):return 1
    return 2
