#define _GNU_SOURCE
#include "resident_test_image.h"
#include "pwl_graphics.h"
typedef uint64_t (EFI *query_t)(void *,uint32_t,size_t *,pwl_graphics_info_t **);
typedef uint64_t (EFI *set_t)(void *,uint32_t);
typedef uint64_t (EFI *blt_t)(void *,uint32_t *,unsigned,size_t,size_t,size_t,size_t,size_t,size_t,size_t);
typedef uint64_t (EFI *text_t)(void *,const uint16_t *);
typedef uint64_t (EFI *cursor_t)(void *,unsigned char);
typedef uint64_t (EFI *position_t)(void *,size_t,size_t);
typedef uint64_t (EFI *attribute_t)(void *,size_t);
static void run(unsigned format)
{
    resident_test_image_t t=resident_test_open();
    size_t frame_bytes=648*400*4,frame_pages=(frame_bytes+4095)&~(size_t)4095;
    unsigned char *allocation=mmap(NULL,frame_pages+8192,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(allocation!=MAP_FAILED);uint32_t *frame=(void *)(allocation+4096);
    assert(mprotect(frame,frame_pages,PROT_READ|PROT_WRITE)==0);
    memset(frame,0x5a,frame_pages);
    pwl_graphics_spec_t s={(uintptr_t)frame,frame_bytes,640,400,648,format,0};
    pwl_efi_table_spec_t code={0};code.code_pa=(uintptr_t)t.code;code.code_bytes=t.image.size;
    for (size_t i=0;i<PWL_EFI_PREPARED_CALLBACKS;i++) code.callback_offsets[i]=t.image.callbacks[i];
    pwl_resident_graphics_t original=t.data->graphics;
    s.pitch=639;assert(pwl_graphics_prepare(&s,&code,(uintptr_t)&t.data->graphics,&t.data->graphics)!=0);
    assert(!memcmp(&original,&t.data->graphics,sizeof(original)));s.pitch=648;
    assert(pwl_graphics_prepare(&s,&code,(uintptr_t)&t.data->graphics,&t.data->graphics)==0);
    assert(frame[0]==0x5a5a5a5a); /* Preparation must not touch video bytes. */
    void *heap=mmap(NULL,65536,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(heap!=MAP_FAILED);pwl_phys_region_t region={(uintptr_t)heap,65536,PWL_MEMORY_FREE};uint64_t cache=8;
    assert(pwl_fw_memory_init(&t.data->memory,&region,&cache,1,1)==0);
    pwl_resident_graphics_t *g=&t.data->graphics;
    LOAD_FROM(g->gop[0],query_t,query);LOAD_FROM(g->gop[1],set_t,set);LOAD_FROM(g->gop[2],blt_t,blt);
    size_t bytes=0;pwl_graphics_info_t *info=NULL;
    assert(query(g->gop,1,&bytes,&info)==PWL_EFI_INVALID_PARAMETER);
    assert(query(g->gop,0,&bytes,&info)==0 && bytes==36 && info->pitch==648 && info->pixel_format==format);
    assert(info!=&g->info && pwl_fw_free_pool(&t.data->memory,(uintptr_t)info)==0);
    assert(set(g->gop,1)==PWL_EFI_UNSUPPORTED && set(g->gop,0)==0);
    assert(frame[0]==0 && frame[640]==0x5a5a5a5a && frame[399*648+647]==0x5a5a5a5a);
    uint32_t color=0x00ff0000;
    assert(blt(g->gop,&color,0,0,0,3,4,2,2,0)==0);
    assert(frame[4*648+3]==(format ? 0xff0000 : 0xff));
    uint32_t buffer[32];for (unsigned i=0;i<32;i++) buffer[i]=0x5a5a5a5a;
    assert(blt(g->gop,buffer,1,3,4,1,2,2,2,4*4)==0);
    assert(buffer[9]==color && buffer[10]==color && buffer[8]==0x5a5a5a5a);
    assert(blt(g->gop,buffer,2,1,2,8,9,2,2,4*4)==0);
    assert(frame[9*648+8]==frame[4*648+3]);
    assert(blt(g->gop,&color,0,0,0,639,0,2,1,0)==PWL_EFI_INVALID_PARAMETER);
    assert(blt(g->gop,buffer,1,0,0,SIZE_MAX,0,2,1,16)==PWL_EFI_INVALID_PARAMETER);
    assert(blt(g->gop,buffer,2,1,0,0,0,2,1,8)==PWL_EFI_INVALID_PARAMETER);
    for (unsigned i=0;i<6;i++) frame[i]=i+1;
    assert(blt(g->gop,NULL,3,0,0,1,0,5,1,0)==0);
    for (unsigned i=1;i<6;i++) assert(frame[i]==i);
    for (unsigned y=0;y<5;y++) for (unsigned x=0;x<5;x++) frame[y*648+x]=y*10+x;
    assert(blt(g->gop,NULL,3,0,0,0,1,5,4,0)==0);
    for (unsigned y=1;y<5;y++) for (unsigned x=0;x<5;x++) assert(frame[y*648+x]==(y-1)*10+x);
    LOAD_FROM(g->text[1],text_t,output);LOAD_FROM(g->text[2],text_t,test);
    LOAD_FROM(g->text[7],position_t,position);LOAD_FROM(g->text[8],cursor_t,cursor);
    LOAD_FROM(g->text[5],attribute_t,attribute);
    typedef uint64_t (EFI *clear_t)(void *);LOAD_FROM(g->text[6],clear_t,clear);
    assert(cursor(g->text,0)==0 && clear(g->text)==0);
    const uint16_t string[]={'A',13,10,'B',0},unknown[]={0x410,0};
    assert(test(g->text,string)==0 && test(g->text,unknown)==PWL_EFI_UNSUPPORTED);
    assert(output(g->text,string)==0 && g->text_mode.row==1 && g->text_mode.column==1);
    unsigned drawn=0;for (unsigned y=0;y<16;y++) for (unsigned x=0;x<8;x++) if (frame[y*648+x]) drawn++;
    assert(drawn && output(g->text,unknown)==1);
    assert(attribute(g->text,128)==PWL_EFI_UNSUPPORTED && attribute(g->text,0x1f)==0);
    assert(position(g->text,80,0)==PWL_EFI_UNSUPPORTED && position(g->text,79,24)==0);
    const uint16_t last[]={'Z',0};assert(output(g->text,last)==0 && g->text_mode.row==24 && g->text_mode.column==0);
    uint32_t prior=frame[399*648];
    assert(cursor(g->text,1)==0 && frame[399*648]==(prior^0xffffff));
    assert(cursor(g->text,0)==0 && frame[399*648]==prior);
    assert(frame[640]==0x5a5a5a5a);
    t.data->memory.exited=1;assert(output(g->text,string)!=0 && blt(g->gop,&color,0,0,0,0,0,1,1,0)!=0);
    assert(munmap(heap,65536)==0 && munmap(allocation,frame_pages+8192)==0);resident_test_close(&t);
}
int main(void)
{ run(0);run(1);puts("resident graphics: RX callbacks, RGB/BGR, strided/overlapping BLT, text, cursor, scroll and bounds passed"); }
