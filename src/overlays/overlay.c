/* Resident loader. No code, state or error text here may live in an overlay. */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "overlay.h"
#ifdef SIDERA_OVERLAY_HOST
#include "platform.h"
#endif

extern char note[100];
unsigned char ovl_failed;
static unsigned char loaded;
/* Stamped, with the asset headers, by pack_overlays.py after linking. */
const unsigned char ovl_build_id[8]={0,0,0,0,0,0,0,0};
static const char *const names[]={
    "ROM:sidera-init.ovl", "ROM:sidera-cities.ovl",
    "ROM:sidera-edit.ovl", "ROM:sidera-cover.ovl",
    "ROM:sidera-solar.ovl"
};
static const uint32_t crc_table[16]={
    0x00000000UL,0x1DB71064UL,0x3B6E20C8UL,0x26D930ACUL,
    0x76DC4190UL,0x6B6B51F4UL,0x4DB26158UL,0x5005713CUL,
    0xEDB88320UL,0xF00F9344UL,0xD6D6A3E8UL,0xCB61B38CUL,
    0x9B64C2B0UL,0x86D3D2D4UL,0xA00AE278UL,0xBDBDF21CUL
};

static unsigned word(const unsigned char *p)
{
    return p[0]|((unsigned)p[1]<<8);
}
static unsigned char exact_read(int fd,void *buffer,unsigned length)
{
    int n;
    unsigned char *p=buffer;
    while(length) {
        n=read(fd,p,length);
        if(n<=0)return 0;
        p+=n;length-=(unsigned)n;
    }
    return 1;
}
unsigned char ovl_loaded(unsigned char id) { return !ovl_failed&&loaded==id; }

unsigned char ovl_select(unsigned char id)
{
    unsigned char header[OVL_HEADER_BYTES],extra;
    unsigned size,bss,zeros,i;
    uint32_t crc,expected;
    int fd=-1;
    const char *error;
    unsigned char *window=OVL_MEMORY;
    if(ovl_failed)return 0;
    if(id<OVL_INIT||id>OVL_SOLAR){error="invalid ID";goto fail;}
    if(id==loaded)return 1;
    fd=open(names[id-1],O_RDONLY);
    if(fd<0){error="cannot open";goto fail;}
    if(!exact_read(fd,header,sizeof(header))){error="short header";goto fail;}
    if(memcmp(header,"SOV1",4)||header[4]!=id||header[5]!=1||
       memcmp(header+12,ovl_build_id,8)){error="wrong build or format";goto fail;}
    size=word(header+6);bss=word(header+8);zeros=word(header+10);
    if(!size||size>OVL_BYTES||bss<size||bss>OVL_BYTES||zeros>OVL_BYTES-bss)
        {error="invalid memory range";goto fail;}
    /* From this point even a partial read destroys the previous overlay. */
    loaded=OVL_NONE;
    if(!exact_read(fd,window,size)){error="short payload";goto fail;}
    if(read(fd,&extra,1)!=0){error="invalid asset length";goto fail;}
    if(close(fd)<0){fd=-1;error="close failed";goto fail;}
    fd=-1;crc=0xFFFFFFFFUL;
    for(i=0;i<size;++i) {
        crc^=window[i];
        crc=(crc>>4)^crc_table[(unsigned char)crc&15];
        crc=(crc>>4)^crc_table[(unsigned char)crc&15];
    }
    memcpy(&expected,header+20,4);
    if((crc^0xFFFFFFFFUL)!=expected){error="checksum mismatch";goto fail;}
    memset(window+bss,0,zeros);
    loaded=id;return 1;
fail:
    if(fd>=0)close(fd);
    loaded=OVL_NONE;ovl_failed=1;
    sprintf(note,"sidera: %s: %s",id>=OVL_INIT&&id<=OVL_SOLAR?names[id-1]:"overlay",error);
    return 0;
}
