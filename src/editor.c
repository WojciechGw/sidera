#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "sidera.h"
#include "overlays/overlay.h"
#ifdef SIDERA_OVERLAYS
#pragma bss-name ("EDIT_BSS")
#pragma data-name ("EDIT_DATA")
#endif
static char edit[112];
static unsigned char cursor=0;
#ifdef SIDERA_OVERLAYS
#pragma bss-name ("BSS")
#pragma data-name ("DATA")
#endif
extern unsigned char editing;

#ifdef SIDERA_OVERLAYS
#pragma code-name ("EDIT_CODE")
#pragma rodata-name ("EDIT_RODATA")
#endif
#ifdef SIDERA_OVERLAYS
#define PARSE_SCOPE static
#define location_parse edit_location_parse
#define date_parse edit_date_parse
#include "overlays/parse.inc"
#undef PARSE_SCOPE
#endif
static void edit_draw(void)
{
    gx_clear(448,32);
    gx_text(20,450,editing==1?"Location: D = degrees, ' = minutes, \" = seconds; Enter: apply, Esc: cancel":
        editing==3?"Zone: IANA name (e.g. Europe/Warsaw, Etc/UTC); Enter: apply, Esc: cancel":
        "UTC date: YYYY-MM-DD HH:MM:SS (1900-2200); Enter: apply, Esc: cancel");
    gx_text(20,462,edit);gx_line(20+cursor*6,471,24+cursor*6,471,0);
    gx_text(20,472,note);
}
void edit_start(unsigned char type)
{
    gx_status(0); /* Calculations pause while the editor owns the keyboard. */
    editing=type;note[0]=0;
    if(type==1)location_format(edit,&observer,0);
    else if(type==3)strcpy(edit,zone_name);else date_format(edit,&moment);
    cursor=(unsigned char)strlen(edit);edit_draw();
}
/* Returns 1 on committed changes, 2 on cancellation, 0 while editing. */
unsigned char edit_key(unsigned k)
{
    unsigned n=(unsigned)strlen(edit);
    char c;
    Observer previous;
    if(k==K_ESC){editing=0;return 2;}
    if(k=='\r') {
        previous=observer;
        if(editing==1?location_parse(edit,&observer):editing==3?tz_set(edit):date_parse(edit,&moment)) {
            if(editing==1&&memcmp(&previous,&observer,sizeof(observer)))location_name[0]=0;
            if(editing==2){live=0;playing=0;note[0]=0;}
            else strcpy(note,"Location/zone changed (press S to save location)");
            editing=0;return 1;
        }
        strcpy(note,editing==1?"Invalid DMS location or height (-500..10000 m).":
            editing==3?"Unknown zone (case-sensitive IANA name) or missing sidera.tz.":"Invalid Gregorian date or time.");
    }
    else if(k==K_LEFT&&cursor)--cursor;
    else if(k==K_RIGHT&&cursor<n)++cursor;
    else if(k==K_HOME)cursor=0;
    else if(k==K_END)cursor=(unsigned char)n;
    else if(k==K_BS&&cursor){--cursor;memmove(edit+cursor,edit+cursor+1,n-cursor);}
    else if(k==K_DEL&&cursor<n)memmove(edit+cursor,edit+cursor+1,n-cursor);
    else if(k>=32&&k<127&&n<(editing==3?39:95)) {
        c=(char)k;
        if(editing==1) {
            if(c=='d'||c=='D')c=1;
            else if(c=='\'')c=2;
            else if(c=='"')c=3;
        }
        memmove(edit+cursor+1,edit+cursor,n-cursor+1);edit[cursor++]=c;
    }
    edit_draw();return 0;
}
#ifdef SIDERA_OVERLAYS
#undef location_parse
#undef date_parse
#endif
