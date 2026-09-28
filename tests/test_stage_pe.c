/* Exercise the parser compiled into Stage 4.8 via its Stage 3 ancestry. */
#define main stage3_unused_main
#include "../payload/stage3.c"
#undef main

static void set16(u8 *p,u16 value){p[0]=(u8)value;p[1]=(u8)(value>>8);}
static void set32(u8 *p,u32 value){set16(p,(u16)value);set16(p+2,(u16)(value>>16));}

int main(void)
{
    u8 file[0x400]={0};struct pe_info pe;
    const size_t off=0x80U,opt=off+24U,sh=opt+0xb0U;
    file[0]='M';file[1]='Z';set32(file+0x3c,(u32)off);
    file[off]='P';file[off+1]='E';set16(file+off+4,IMAGE_FILE_MACHINE_AMD64);
    set16(file+off+6,1);set16(file+off+20,0xb0U);
    set16(file+opt,PE32_PLUS_MAGIC);
    set32(file+opt+0x10,0x1000U);
    set32(file+opt+0x38,0x2000U);
    set32(file+opt+0x3c,0x200U);
    set16(file+opt+0x44,IMAGE_SUBSYSTEM_EFI_APPLICATION);
    set32(file+sh+8,0x200U);set32(file+sh+12,0x1000U);
    set32(file+sh+16,0x200U);set32(file+sh+20,0x200U);
    set32(file+sh+36,0x20000000U);
    if(inspect_pe(file,sizeof(file),&pe)!=0)return 1;
    set16(file+off+6,5); /* The section table exceeds SizeOfHeaders. */
    if(inspect_pe(file,sizeof(file),&pe)==0)return 2;
    set16(file+off+6,1);
    set32(file+sh+20,0x300U); /* Raw bytes exceed the file. */
    if(inspect_pe(file,sizeof(file),&pe)==0)return 3;
    set32(file+sh+20,0x200U);
    set32(file+sh+12,0x1f00U); /* Virtual span exceeds SizeOfImage. */
    if(inspect_pe(file,sizeof(file),&pe)==0)return 4;
    set32(file+sh+12,0x1000U);
    set32(file+sh+36,0); /* Entry must be executable. */
    if(inspect_pe(file,sizeof(file),&pe)==0)return 5;
    set32(file+sh+36,0x20000000U);
    set32(file+opt+0x10,0x1800U); /* Entry is outside section. */
    if(inspect_pe(file,sizeof(file),&pe)==0)return 6;
    set32(file+opt+0x10,0x1000U);
    set32(file+opt+0x98,0x1f00U); /* Relocation directory exceeds image. */
    set32(file+opt+0x9c,0x200U);
    if(inspect_pe(file,sizeof(file),&pe)==0)return 7;
    set32(file+opt+0x98,0);set32(file+opt+0x9c,0);
    if(inspect_pe(file,sizeof(file),&pe)!=0)return 8;
    return 0;
}
