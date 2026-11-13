/* Offline modal object finder. Scan and paint bounded batches between key polls. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sidera.h"

unsigned char find_active=0; /* Explicit DATA placement keeps this flag resident. */
#ifdef SIDERA_OVERLAYS
#pragma code-name ("FIND_CODE")
#pragma rodata-name ("FIND_RODATA")
#pragma bss-name ("FIND_BSS")
#pragma data-name ("FIND_DATA")
#endif
#define RECORD 256
#define COUNT FIND_COUNT
#define ROWS 20
#define ROW_Y 131
#define DETAILS_Y 432
#define XR_MATCHES 0xE200u
#if COUNT > 1024 || XR_MATCHES < XR_LABELS+96u*5 || XR_MATCHES+(COUNT*10u+7u)/8u > XR_PLANET_PAL
#error FIND match list overlaps VGA data
#endif
static int fd=-1;
static unsigned scanned,total,selected,page,clear_y,exact,changed,detail_y;
static unsigned char stage,row,valid,accept,side,alias_offset;
static int fit;
static const unsigned char zeros[40]={0};
static char query[40],line[106];
static unsigned char record[RECORD];
static Date at;
static StarFrame frame;
static Position position;

static unsigned word(const unsigned char *p){return p[0]|((unsigned)p[1]<<8);}
static void clear_rows(int y,int height)
{
    gx_clear(y,height);gx_line(0,y,0,y+height-1,0);gx_line(639,y,639,y+height-1,0);
}
static void message(const char *s){clear_rows(444,8);gx_text(6,444,s);}
/* Byte-aligned rectangles leave the other half of the panel untouched. */
static void clear_part(unsigned y,unsigned char height,unsigned char right)
{
    unsigned a=y*80u;unsigned char border=128;
    while(height--) {
        gx_write(a+(right?41:1),zeros,right?38:39);
        if(!right)gx_write(a,&border,1);
        a+=80;
    }
}
static void fail(void)
{
    stage=valid=accept=0;message("Cannot read object catalogue (sidera.find) or star vectors. Esc: cancel");
}
void find_close(void)
{
    if(fd>=0)close(fd);fd=-1;
    if(find_active)gx_panel_restore();
    find_active=0;
}
static int read_record(unsigned n)
{
    long offset=16L+(long)n*RECORD;
    if(n>=COUNT||lseek(fd,offset,SEEK_SET)!=offset||read(fd,record,RECORD)!=RECORD||
       word(record)>=COUNT||record[41]||record[191]||record[244]||!record[2]){fail();return 0;}
    return 1;
}
static unsigned match(unsigned n)
{
    unsigned bit=n*10u;unsigned char bytes[2];
    gx_read(XR_MATCHES+bit/8,bytes,2);return (word(bytes)>>(bit&7))&1023u;
}
/* Ten bits per result keep 855 indices in the existing XRAM scratch area. */
static void set_match(unsigned index,unsigned n)
{
    unsigned bit=index*10u,value,mask=1023u<<(bit&7);unsigned char bytes[2];
    gx_read(XR_MATCHES+bit/8,bytes,2);
    value=(word(bytes)&~mask)|(n<<(bit&7));
    bytes[0]=(unsigned char)value;bytes[1]=(unsigned char)(value>>8);
    gx_write(XR_MATCHES+bit/8,bytes,2);
}
static void search(void)
{
    scanned=total=selected=page=0;valid=accept=0;
    exact=0xFFFFu;
    detail_y=130;changed=platform_ticks();
    if(stage!=1&&stage!=4){stage=4;clear_y=130;}
    clear_rows(98,10);sprintf(line,"Search: %s",query);gx_text(6,99,line);
    message("Searching...");
}
int find_start(void)
{
    fd=open("ROM:sidera.find",O_RDONLY);
    if(fd<0){strcpy(note,"Cannot open object catalogue (sidera.find).");return 0;}
    if(read(fd,record,16)!=16||memcmp(record,"SIDFIND3",8)||word(record+8)!=COUNT||
       word(record+10)!=RECORD||lseek(fd,0,SEEK_END)!=16L+(long)COUNT*RECORD) {
        close(fd);fd=-1;strcpy(note,"Invalid object catalogue (sidera.find).");return 0;
    }
    at=moment;star_frame(&at,&observer,&frame);find_active=1;query[0]=0;
    gx_clear(71,409);
    gx_line(0,71,639,71,0);gx_line(0,479,639,479,0);
    gx_line(0,71,0,479,0);gx_line(639,71,639,479,0);
    gx_label(2,83," FIND ",1);
    gx_text(6,115,"Type HIP    HD     Designation     Name");
    gx_text(330,115,"DETAILS");
    gx_text(6,454,"Type: search (2+ chars)  Backspace: erase  Up/Down: select  PgUp/PgDn: page");
    gx_text(6,466,"Enter: locate object  Esc: cancel");
    stage=1;search();return 1;
}
static int paint_row(unsigned n,unsigned char inverse)
{
    unsigned y=ROW_Y+(n-page)*14u;
    if(!read_record(match(n)))return 0;
    gx_label(6,y,(char *)record+192,inverse);return 1;
}
static void invert_row(unsigned n)
{
    unsigned a=(ROW_Y-1+(n-page)*14u)*80u;unsigned char r,b;
    for(r=0;r<10;++r,a+=80) {
        gx_read(a,line,40);line[0]^=7;line[39]^=(unsigned char)254;
        for(b=1;b<39;++b)line[b]^=(unsigned char)255;
        gx_write(a,line,40);
    }
}
static void show_details(unsigned id)
{
    if(id>=FIND_FIGURE_FIRST){figure_result(&position,&fit);side=figure_side();}
    else {fit=0;side=position.alt>=0;}
    stage=6;row=alias_offset=0;valid=1;
}
static void paint_details(void)
{
    unsigned id=word(record);unsigned char n;int angle;char *p;
    line[0]=0;
    if(!row)strcpy(line,(char *)record+2);
    else if(row==1)strcpy(line,id>=FIND_FIGURE_FIRST?"Constellation":id>=8?"Star":id>=2?"Planet":id?"Moon":"Sun");
    else if(row==2)strcpy(line,"Names and identifiers:");
    else if(row<8) {
        p=(char *)record+42+alias_offset;n=(unsigned char)strlen(p);
        if(n>49){n=49;while(n&&p[n]!=' ')--n;if(!n)n=49;}
        memcpy(line,p,n);line[n]=0;alias_offset+=n+(p[n]==' ');
    } else if(row==8) {
        angle=as_degrees(position.az);sprintf(line,"AZ  %03d.%d\x01",angle/10,angle%10);
    } else if(row==9) {
        angle=(int)((int32_t)position.alt*900/16384);
        sprintf(line,"ALT %s%d.%d\x01",angle<0?"-":"+",abs(angle)/10,abs(angle)%10);
    } else if(row==10)sprintf(line,"%s horizon",side==2?"Crosses":side?"Above":"Below");
    else if(fit)strcpy(line,fit>160?"Figure centre; exceeds FOV 160.":"Figure centre; Enter fits the view.");
    gx_text(330,131+(unsigned)row*14u,line);
    if(++row<12)return;
    clear_rows(DETAILS_Y,8);
    sprintf(line,"%u / %u",selected+1,total);gx_text(634-(int)strlen(line)*6,DETAILS_Y,line);
    message("");stage=0;
}
static void details(void)
{
    unsigned id;
    int16_t vector[3];Sky bodies;Position ps[PLANET_COUNT];
    if(!total){message("No matching objects.");stage=valid=accept=0;return;}
    if(!read_record(match(selected)))return;
    id=word(record);
    if(id>=FIND_FIGURE_FIRST){figure_begin((unsigned char)(id-FIND_FIGURE_FIRST),&at);stage=5;return;}
    if(id>=8) {
        if(!assets_vectors(id-8,1,vector)){fail();return;}
        star_position(&frame,vector,&position);
    } else if(id<2){sky_positions(&at,&observer,&bodies);position=id?bodies.moon:bodies.sun;}
    else {planets_compute(&at,&observer,ps);position=ps[id-2];}
    show_details(id);
}
unsigned char find_step(void)
{
    unsigned char n;unsigned length=(unsigned)strlen(query),first;char *p;
    if(stage==7) {
        clear_part(clear_y,8,0);clear_y+=8;if(clear_y>=410){stage=2;row=0;}
    } else if(stage==4) {
        clear_rows(clear_y,clear_y+8>440?440-clear_y:8);clear_y+=8;
        if(clear_y>=440)stage=1;
    } else if(stage==1) {
        for(n=0;n<4&&scanned<COUNT;++n,++scanned) {
            if(!read_record(scanned))return 0;
            p=(char *)record+42;
            if(length>=2)while(*p&&strnicmp(p,query,length))++p;
            if(length<2||*p) {
                if(length>=2&&exact==0xFFFFu) {
                    if(!stricmp((char *)record+2,query))exact=total;
                    else if(length==3&&word(record)>=FIND_FIGURE_FIRST) {
                        p=strchr((char *)record+42,'|');
                        if(p&&strlen(p)>=6&&p[5]==' '&&!strnicmp(p+2,query,3))exact=total;
                    }
                }
                set_match(total++,scanned);
            }
        }
        if(scanned==COUNT) {
            if(exact!=0xFFFFu&&exact){first=match(0);set_match(0,match(exact));set_match(exact,first);}
            stage=2;row=0;
        }
    } else if(stage==2) {
        if(row<ROWS&&page+row<total){paint_row(page+row,page+row==selected);++row;}
        else {stage=3;}
    } else if(stage==3) {
        if(detail_y<298){clear_part(detail_y,8,1);detail_y+=8;}
        else if(accept||(unsigned)(platform_ticks()-changed)>=PLATFORM_TICK_HZ/4)details();
    }
    else if(stage==5) {
        n=figure_step();
        if(n==2)fail();
        else if(n==1&&read_record(match(selected)))show_details(word(record));
    } else if(stage==6)paint_details();
    if(accept&&valid)return find_key('\r');
    return 0;
}
unsigned char find_key(unsigned k)
{
    unsigned old=selected,n=(unsigned)strlen(query);
    if(k==K_ESC){find_close();return 2;}
    if(k==K_BS&&n){query[n-1]=0;search();return 0;}
    if(k>=32&&k<127&&n<sizeof(query)-1){query[n]=(char)k;query[n+1]=0;search();return 0;}
    if(k=='\r') {
        if(!valid){if(stage)accept=1;return 0;}
        if(!read_record(match(selected)))return 0;
        if(!target_select(word(record),(char *)record+2,&position)){fail();return 0;}
        find_close();note[0]=0;return 1;
    }
    if(stage==1||stage==4||!total||(!stage&&!valid))return 0;
    if(k==K_UP&&selected)--selected;
    if(k==K_DOWN&&selected+1<total)++selected;
    if(k==K_HOME)selected=0;
    if(k==K_END&&total)selected=total-1;
    if(k==K_PGUP)selected=selected>ROWS?selected-ROWS:0;
    if(k==K_PGDN&&total)selected=selected+ROWS<total?selected+ROWS:total-1;
    if(old==selected)return 0;
    valid=0;detail_y=130;
    if(old/ROWS!=selected/ROWS) {
        page=selected/ROWS*ROWS;clear_y=130;stage=7;
    } else if(stage!=7) {
        if(stage==2) {if(!paint_row(old,0)||!paint_row(selected,1))return 0;}
        else {invert_row(old);invert_row(selected);}
        if(stage!=2)stage=3;
    }
    changed=platform_ticks();
    return 0;
}
