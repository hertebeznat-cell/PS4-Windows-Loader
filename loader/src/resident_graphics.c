#include "pwl_resident.h"
#include "console_font.h"
#define EFI __attribute__((ms_abi))
extern const uint64_t pwl_resident_binding __attribute__((visibility("hidden")));
static pwl_resident_data_t *state(void)
{ return (pwl_resident_data_t *)(uintptr_t)pwl_resident_binding; }
static pwl_resident_graphics_t *graphics(void *self,unsigned text)
{
    pwl_resident_data_t *d=state();
    if (!d || d->memory.exited || !d->graphics.enabled ||
        self!=(text ? (void *)d->graphics.text : (void *)d->graphics.gop)) return NULL;
    return &d->graphics;
}
static uint32_t read_pixel(const pwl_resident_graphics_t *g,size_t x,size_t y)
{
    volatile uint32_t *p=(void *)(uintptr_t)g->mode.framebuffer;
    uint32_t v=p[y*g->info.pitch+x]&0xffffff;
    return g->info.pixel_format ? v : ((v&255)<<16)|(v&0xff00)|((v>>16)&255);
}
static void write_pixel(const pwl_resident_graphics_t *g,size_t x,size_t y,uint32_t v)
{
    volatile uint32_t *p=(void *)(uintptr_t)g->mode.framebuffer;
    p[y*g->info.pitch+x]=g->info.pixel_format ? v : ((v&255)<<16)|(v&0xff00)|((v>>16)&255);
}
static int video_rect(const pwl_resident_graphics_t *g,size_t x,size_t y,size_t w,size_t h)
{ return x<g->info.width && y<g->info.height && w<=g->info.width-x && h<=g->info.height-y; }
static void flush_pixels(void)
{ __asm__ volatile("sfence":::"memory"); }
uint64_t EFI pwl_resident_gop_query(void *self,uint32_t mode,size_t *bytes,pwl_graphics_info_t **info)
{
    pwl_resident_graphics_t *g=graphics(self,0);pwl_resident_data_t *d=state();
    if (!g || !bytes || !info) return PWL_EFI_INVALID_PARAMETER;
    if (mode) return PWL_EFI_INVALID_PARAMETER;
    uint64_t buffer=0;uint64_t status=pwl_fw_allocate_pool(&d->memory,4,sizeof(g->info),&buffer);
    if (status) return status;
    *(pwl_graphics_info_t *)(uintptr_t)buffer=g->info;
    *bytes=sizeof(g->info);*info=(void *)(uintptr_t)buffer;return PWL_EFI_SUCCESS;
}
static void cursor(pwl_resident_graphics_t *g,unsigned draw)
{
    if (g->cursor_drawn==draw) return;
    size_t x=(size_t)g->text_mode.column*8,y=(size_t)g->text_mode.row*16+15;
    for (size_t i=0;i<8;i++) write_pixel(g,x+i,y,read_pixel(g,x+i,y)^0xffffff);
    g->cursor_drawn=draw;
    flush_pixels();
}
static void clear(pwl_resident_graphics_t *g,uint32_t color)
{
    for (size_t y=0;y<g->info.height;y++) for (size_t x=0;x<g->info.width;x++) write_pixel(g,x,y,color);
    g->text_mode.column=0;g->text_mode.row=0;g->cursor_drawn=0;
    flush_pixels();
}
uint64_t EFI pwl_resident_gop_set(void *self,uint32_t mode)
{
    pwl_resident_graphics_t *g=graphics(self,0);
    if (!g) return PWL_EFI_INVALID_PARAMETER;
    if (mode) return PWL_EFI_UNSUPPORTED;
    clear(g,0);return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_gop_blt(void *self,uint32_t *buffer,unsigned operation,
    size_t sx,size_t sy,size_t dx,size_t dy,size_t w,size_t h,size_t delta)
{
    pwl_resident_graphics_t *g=graphics(self,0);
    if (!g || operation>3 || !w || !h) return PWL_EFI_INVALID_PARAMETER;
    if ((operation==0 || operation==2 || operation==3) && !video_rect(g,dx,dy,w,h)) return PWL_EFI_INVALID_PARAMETER;
    if ((operation==1 || operation==3) && !video_rect(g,sx,sy,w,h)) return PWL_EFI_INVALID_PARAMETER;
    if (operation!=3 && !buffer) return PWL_EFI_INVALID_PARAMETER;
    size_t stride=0;
    if (operation==1 || operation==2) {
        if (w>SIZE_MAX/4) return PWL_EFI_INVALID_PARAMETER;
        if (!delta) delta=w*4;
        size_t x=operation==1 ? dx : sx,y=operation==1 ? dy : sy;
        if (delta&3 || x>SIZE_MAX/4 || x*4>delta || w*4>delta-x*4 ||
            y>SIZE_MAX-h || y+h>SIZE_MAX/delta) return PWL_EFI_INVALID_PARAMETER;
        size_t bytes=(y+h)*delta;
        if (bytes>UINTPTR_MAX-(uintptr_t)buffer ||
            ((uintptr_t)buffer<g->mode.framebuffer+g->mode.framebuffer_bytes &&
             g->mode.framebuffer<(uintptr_t)buffer+bytes)) return PWL_EFI_INVALID_PARAMETER;
        stride=delta/4;
    }
    if (operation==0) {
        uint32_t color=*buffer&0xffffff;
        for (size_t y=0;y<h;y++) for (size_t x=0;x<w;x++) write_pixel(g,dx+x,dy+y,color);
    } else if (operation==1) {
        for (size_t y=0;y<h;y++) for (size_t x=0;x<w;x++) buffer[(dy+y)*stride+dx+x]=read_pixel(g,sx+x,sy+y);
    } else if (operation==2) {
        for (size_t y=0;y<h;y++) for (size_t x=0;x<w;x++) write_pixel(g,dx+x,dy+y,buffer[(sy+y)*stride+sx+x]&0xffffff);
    } else {
        /* Direction preserves overlapping source rectangles, including rows. */
        for (size_t j=0;j<h;j++) {
            size_t y=dy>sy ? h-1-j : j;
            for (size_t i=0;i<w;i++) {
                size_t x=(dy==sy && dx>sx) ? w-1-i : i;
                write_pixel(g,dx+x,dy+y,read_pixel(g,sx+x,sy+y));
            }
        }
    }
    if (operation!=1) flush_pixels();
    return PWL_EFI_SUCCESS;
}
static const uint32_t colors[16]={0,0x0000aa,0x00aa00,0x00aaaa,0xaa0000,0xaa00aa,0xaaaa00,0xaaaaaa,
    0x555555,0x5555ff,0x55ff55,0x55ffff,0xff5555,0xff55ff,0xffff55,0xffffff};
uint64_t EFI pwl_resident_text_query(void *self,size_t mode,size_t *columns,size_t *rows)
{
    if (!graphics(self,1) || !columns || !rows) return PWL_EFI_INVALID_PARAMETER;
    if (mode) return PWL_EFI_UNSUPPORTED;
    *columns=80;*rows=25;return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_text_clear(void *self)
{
    pwl_resident_graphics_t *g=graphics(self,1);
    if (!g) return PWL_EFI_INVALID_PARAMETER;
    clear(g,colors[(unsigned)g->text_mode.attribute>>4]);
    if (g->text_mode.cursor_visible) cursor(g,1);
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_text_set(void *self,size_t mode)
{
    if (!graphics(self,1)) return PWL_EFI_INVALID_PARAMETER;
    if (mode) return PWL_EFI_UNSUPPORTED;
    return pwl_resident_text_clear(self);
}
uint64_t EFI pwl_resident_text_reset(void *self,unsigned char extended)
{
    (void)extended;pwl_resident_graphics_t *g=graphics(self,1);
    if (!g) return PWL_EFI_INVALID_PARAMETER;
    g->text_mode.attribute=7;
    return pwl_resident_text_clear(self);
}
uint64_t EFI pwl_resident_text_attribute(void *self,size_t attribute)
{
    pwl_resident_graphics_t *g=graphics(self,1);
    if (!g || attribute>127) return PWL_EFI_UNSUPPORTED;
    g->text_mode.attribute=(int32_t)attribute;return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_text_position(void *self,size_t column,size_t row)
{
    pwl_resident_graphics_t *g=graphics(self,1);
    if (!g) return PWL_EFI_INVALID_PARAMETER;
    if (column>=80 || row>=25) return PWL_EFI_UNSUPPORTED;
    cursor(g,0);g->text_mode.column=(int32_t)column;g->text_mode.row=(int32_t)row;
    if (g->text_mode.cursor_visible) cursor(g,1);
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_text_cursor(void *self,unsigned char visible)
{
    pwl_resident_graphics_t *g=graphics(self,1);
    if (!g || visible>1) return PWL_EFI_INVALID_PARAMETER;
    cursor(g,visible);g->text_mode.cursor_visible=visible;return PWL_EFI_SUCCESS;
}
static int supported(uint16_t c)
{ return (c>=32 && c<127) || c==8 || c==10 || c==13; }
uint64_t EFI pwl_resident_text_test(void *self,const uint16_t *string)
{
    if (!graphics(self,1) || !string) return PWL_EFI_INVALID_PARAMETER;
    for (size_t i=0;string[i];i++) if (!supported(string[i])) return PWL_EFI_UNSUPPORTED;
    return PWL_EFI_SUCCESS;
}
static void scroll(pwl_resident_graphics_t *g)
{
    for (size_t y=0;y<384;y++) for (size_t x=0;x<640;x++) write_pixel(g,x,y,read_pixel(g,x,y+16));
    uint32_t bg=colors[(unsigned)g->text_mode.attribute>>4];
    for (size_t y=384;y<400;y++) for (size_t x=0;x<640;x++) write_pixel(g,x,y,bg);
    g->text_mode.row=24;
    flush_pixels();
}
uint64_t EFI pwl_resident_text_output(void *self,const uint16_t *string)
{
    pwl_resident_graphics_t *g=graphics(self,1);
    if (!g || !string) return PWL_EFI_INVALID_PARAMETER;
    uint64_t result=0;cursor(g,0);
    for (size_t i=0;string[i];i++) {
        uint16_t c=string[i];
        if (c==13) g->text_mode.column=0;
        else if (c==10) g->text_mode.row++;
        else if (c==8) {
            if (g->text_mode.column) g->text_mode.column--;
            else if (g->text_mode.row) { g->text_mode.row--;g->text_mode.column=79; }
        } else {
            if (!supported(c)) { c='?';result=1; } /* EFI_WARN_UNKNOWN_GLYPH. */
            uint32_t fg=colors[(unsigned)g->text_mode.attribute&15],bg=colors[(unsigned)g->text_mode.attribute>>4];
            size_t px=(size_t)g->text_mode.column*8,py=(size_t)g->text_mode.row*16;
            for (size_t y=0;y<16;y++) for (size_t x=0;x<8;x++)
                write_pixel(g,px+x,py+y,(pwl_console_font[c][y/2]&(1U<<x)) ? fg : bg);
            g->text_mode.column++;
            if (g->text_mode.column==80) { g->text_mode.column=0;g->text_mode.row++; }
        }
        if (g->text_mode.row==25) scroll(g);
    }
    flush_pixels();
    if (g->text_mode.cursor_visible) cursor(g,1);
    return result;
}
