/* Fixed-size, disk-backed catalogue. Only one record and one page of IDs
 * are resident. Coordinates are WGS84, 0.1 arcsec; height is metres. */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sidera.h"

#define ROWS 20
#define ROW_Y (CITIES_TOP+60)
#define ROW_HEIGHT 14
#define TEXT_X (CITIES_LEFT+6)
#define DETAILS_Y (CITIES_BOTTOM-60)
#define NOTE_Y (CITIES_BOTTOM-46)
#define TEXT_CHARS ((CITIES_RIGHT-TEXT_X-2)/6)
#define LIST_CHARS 52
#define RECORD 96
unsigned char cities_active=0;
#ifdef SIDERA_OVERLAYS
#pragma code-name ("CITIES_CODE")
#pragma rodata-name ("CITIES_RODATA")
#endif
#ifdef SIDERA_OVERLAYS
#pragma bss-name ("CITIES_BSS")
#pragma data-name ("CITIES_DATA")
#endif
static int fd=-1;
static unsigned count,total,selected,page,ids[ROWS];
static unsigned char rows;
static char query[40],line[112];
static unsigned char record[RECORD];
static Observer candidate;

void cities_close(void)
{
    world_close();
    /* Erase the panel, including gaps outside the plot's normal redraw area.
     * gx_clear also invalidates the axes and footer for restoration. */
    if(cities_active)gx_clear(CITIES_TOP,CITIES_BOTTOM-CITIES_TOP);
    if(fd>=0)close(fd);fd=-1;cities_active=0;
}
static int bad(void)
{
    strcpy(note,"Cannot read city catalogue (sidera.cty).");return 0;
}
static int open_catalogue(void)
{
    cities_close();fd=open("ROM:sidera.cty",O_RDONLY);
    if(fd<0)fd=open("MSC0:/sidera.cty",O_RDONLY);
    if(fd<0)return bad();
    if(read(fd,record,16)!=16||memcmp(record,"SIDCITY2",8))goto invalid;
    count=record[8]|((unsigned)record[9]<<8);
    if(!count||count>1000||record[10]!=RECORD||record[11]||
       lseek(fd,0,SEEK_END)!=16L+(long)count*RECORD)goto invalid;
    return 1;
invalid:
    cities_close();return bad();
}
static int read_city(unsigned index)
{
    long offset=16L+(long)index*RECORD;
    int16_t height;
    if(index>=count||lseek(fd,offset,SEEK_SET)!=offset||read(fd,record,RECORD)!=RECORD)return bad();
    if(record[2]||record[42]||record[82]||record[0]<'A'||record[0]>'Z'||
       record[1]<'A'||record[1]>'Z'||!record[3]||!record[43])return bad();
    memcpy(&candidate.latitude,record+83,4);memcpy(&candidate.longitude,record+87,4);
    memcpy(&height,record+91,2);candidate.altitude=height;
    if(candidate.latitude< -3240000L||candidate.latitude>3240000L||
       candidate.longitude< -6480000L||candidate.longitude>6480000L||height< -500||height>10000)return bad();
    return 1;
}
static int apply_city(void)
{
    if(!tz_select(record[94]|((unsigned)record[95]<<8))) {
        strcpy(note,"Cannot read timezone data (sidera.tz).");return 0;
    }
    observer=candidate;sprintf(location_name,"%s [%s]",record+43,record);return 1;
}
/* Exact city, CC, or CC/City. Ambiguity never silently selects a country. */
int cities_lookup(const char *name)
{
    unsigned i,matches=0,n=(unsigned)strlen(name),found=0;
    unsigned char match;
    if(!open_catalogue())return 0;
    for(i=0;i<count;++i) {
        if(!read_city(i)){cities_close();return 0;}
        if(n==2)match=!stricmp(name,(char *)record)&&(record[93]&1);
        else if(n>3&&name[2]=='/')match=!strnicmp(name,(char *)record,2)&&!stricmp(name+3,(char *)record+43);
        else match=!stricmp(name,(char *)record+43);
        if(match){++matches;found=i;}
    }
    if(matches!=1) {
        cities_close();
        strcpy(note,matches?"Ambiguous city. Use CC/City, e.g. CA/St. John's.":"City not found. Use a country code or an English city name.");
        return 0;
    }
    match=read_city(found)&&apply_city();cities_close();
    if(match)note[0]=0;return match;
}
static int contains(const char *text)
{
    unsigned n=(unsigned)strlen(query);
    if(!n)return 1;
    for(;*text;++text)if(!strnicmp(text,query,n))return 1;
    return 0;
}
static int fill(void)
{
    unsigned i,n=(unsigned)strlen(query);
    unsigned char country=0,match;
    rows=0;total=0;page=selected/ROWS*ROWS;
    if(n<2) {
        total=count;
        while(rows<ROWS&&page+rows<count){ids[rows]=page+rows;++rows;}
        return 1;
    }
    for(i=0;i<count;++i) {
        if(!read_city(i)){rows=total=0;return 0;}
        match=n==2&&!stricmp(query,(char *)record);
        /* A valid country code supersedes earlier matches in unrelated names. */
        if(match&&!country){rows=total=0;country=1;}
        if(match||(!country&&(contains((char *)record+3)||contains((char *)record+43)))) {
            if(total>=page&&rows<ROWS)ids[rows++]=i;
            ++total;
        }
    }
    return 1;
}
int cities_start(void)
{
    if(!open_catalogue())return 0;
    query[0]=note[0]=0;selected=0;
    if(!fill()){cities_close();return 0;}
    cities_active=1;return 1;
}
/* Clear only the interior: preserve the frame and surrounding sky pixels,
 * including the partial bytes at the left and right edges. */
static void clear_rows(int y,int height)
{
    unsigned char row[(CITIES_RIGHT-2)/8-(CITIES_LEFT+1)/8+1];
    unsigned a;
    memset(row,0,sizeof(row));
    for(;height>0;--height,++y) {
        a=(unsigned)y*80u+(CITIES_LEFT+1)/8;
        gx_read(a,row,1);row[0]&=0xFFu<<(8-((CITIES_LEFT+1)&7));
        gx_read(a+sizeof(row)-1,row+sizeof(row)-1,1);
        row[sizeof(row)-1]&=0xFFu>>( (CITIES_RIGHT-1)&7);
        gx_write(a,row,sizeof(row));
    }
}
static void clear_list_rows(unsigned y,unsigned char height)
{
    unsigned char row[41];memset(row,0,sizeof(row));row[0]=128;
    while(height--)gx_write(y++*80u,row,sizeof(row));
}
static void fit_line(unsigned char width)
{
    if(strlen(line)>width)strcpy(line+width-3,"...");
}
static void draw_search(void)
{
    clear_list_rows(CITIES_TOP+28,8);
    sprintf(line,"Search: %s",query);gx_text(TEXT_X,CITIES_TOP+28,line);
}
static int draw_row(unsigned char i)
{
    clear_list_rows(ROW_Y-1+i*ROW_HEIGHT,10);
    if(!read_city(ids[i]))return 0;
    sprintf(line,"%s %s - %s%s",record,record+3,record+43,
            record[93]&2?" [Capital]":record[93]&4?" [Seat]":"");
    fit_line(LIST_CHARS);gx_label(TEXT_X,ROW_Y+i*ROW_HEIGHT,line,selected==page+i);return 1;
}
static void draw_list(void)
{
    unsigned char i;
    clear_list_rows(ROW_Y-1,140);clear_list_rows(ROW_Y-1+140,140);
    for(i=0;i<rows;++i)if(!draw_row(i))break;
    if(!total)gx_text(TEXT_X,ROW_Y,"No matching cities.");
}
static void draw_details(void)
{
    unsigned char n,ok=0;
    clear_rows(DETAILS_Y,8);
    if(rows&&read_city(ids[selected-page])) {
        ok=1;
        location_format(line,&candidate,0);gx_text(TEXT_X,DETAILS_Y,line);
        world_select(record[93]>>4,&candidate);
    } else world_select(0,0);
    for(n=0;n<2;++n) {
        memset(line,' ',39);line[39]=0;
        if(ok)memcpy(line,record+(n?3:43),strlen((char *)record+(n?3:43)));
        gx_label(334,240+n*14,line,0);
    }
    sprintf(line,"%u / %u",total?selected+1:0,total);
    gx_text(CITIES_RIGHT-6-(int)strlen(line)*6,DETAILS_Y,line);
}
static void draw_note(void)
{
    clear_rows(NOTE_Y,8);strcpy(line,note);fit_line(TEXT_CHARS);gx_text(TEXT_X,NOTE_Y,line);
}
void cities_draw(void)
{
    clear_rows(CITIES_TOP+1,CITIES_BOTTOM-CITIES_TOP-2);
    gx_line(CITIES_LEFT,CITIES_TOP,CITIES_RIGHT-1,CITIES_TOP,0);
    gx_line(CITIES_LEFT,CITIES_TOP,CITIES_LEFT,CITIES_BOTTOM-1,0);
    gx_line(CITIES_RIGHT-1,CITIES_TOP,CITIES_RIGHT-1,CITIES_BOTTOM-1,0);
    gx_line(CITIES_LEFT,CITIES_BOTTOM-1,CITIES_RIGHT-1,CITIES_BOTTOM-1,0);
    gx_label(CITIES_LEFT+2,CITIES_TOP+12," CITIES ",1);
    world_begin();
    gx_text(TEXT_X,CITIES_TOP+44,"Country - City  [Capital / Seat of government]");
    draw_search();draw_list();draw_details();
    gx_text(TEXT_X,CITIES_BOTTOM-26,"Type: search (2+ chars)  Backspace: erase  Up/Down: select  PgUp/PgDn: page");
    gx_text(TEXT_X,CITIES_BOTTOM-14,"Enter: use city  Esc: cancel");
    draw_note();
}
/* Same results as the coordinate editor: 1 commit, 2 cancel, 0 continue. */
unsigned char cities_key(unsigned k)
{
    unsigned n=(unsigned)strlen(query),old=selected;
    unsigned char search=0;
    if(k==K_ESC){cities_close();note[0]=0;return 2;}
    if(k=='\r'&&rows) {
        if(!read_city(ids[selected-page])){cities_close();return 2;}
        if(!apply_city()){draw_note();return 0;}
        cities_close();
        sprintf(note,"%s (press Ctrl+S to save location)",location_name);return 1;
    }
    if(k==K_UP&&selected)--selected;
    else if(k==K_DOWN&&selected+1<total)++selected;
    else if(k==K_PGUP)selected=selected>ROWS?selected-ROWS:0;
    else if(k==K_PGDN&&total)selected=selected+ROWS<total?selected+ROWS:total-1;
    else if(k==K_HOME)selected=0;
    else if(k==K_END&&total)selected=total-1;
    else if(k==K_BS&&n){query[n-1]=0;search=1;}
    else if(k>=32&&k<127&&n<sizeof(query)-1){query[n]=(char)k;query[n+1]=0;search=1;}
    if(search) {
        draw_search();note[0]=0;
        if(n>=2||strlen(query)>=2){selected=0;fill();draw_list();draw_details();}
    } else if(selected!=old) {
        if(selected/ROWS!=old/ROWS){fill();draw_list();}
        else {draw_row((unsigned char)(old-page));draw_row((unsigned char)(selected-page));}
        draw_details();
    } else return 0;
    draw_note();return 0;
}
