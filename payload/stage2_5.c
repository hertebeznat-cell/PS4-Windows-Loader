#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>

void *dlopen(const char *path, int mode);
void *dlsym(void *handle, const char *name);

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long s64;
typedef int (*notify_fn_t)(int, const char *);

#define IMAGE_FILE_MACHINE_AMD64 0x8664U
#define PE32_PLUS_MAGIC 0x20bU
#define IMAGE_SUBSYSTEM_EFI_APPLICATION 10U
#define IMAGE_REL_BASED_ABSOLUTE 0U
#define IMAGE_REL_BASED_DIR64 10U
#define IMAGE_DIRECTORY_ENTRY_BASERELOC 5U
#define READ_CAP (16ULL * 1024ULL * 1024ULL)
#define MAX_IMAGE_SIZE (128ULL * 1024ULL * 1024ULL)

static void notify(const char *msg)
{
    static notify_fn_t fn = (notify_fn_t)0;
    if (!fn) {
        void *h = dlopen("/system/common/lib/libSceSysUtil.sprx", 0);
        if (!h) h = dlopen("libSceSysUtil.sprx", 0);
        if (h) fn = (notify_fn_t)dlsym(h, "sceSysUtilSendSystemNotificationWithText");
    }
    if (fn) fn(222, msg);
}

static void mem_zero(void *dst, size_t n){u8 *p=(u8*)dst;while(n--)*p++=0;}
static void mem_copy(void *dst,const void *src,size_t n){u8*d=(u8*)dst;const u8*s=(const u8*)src;while(n--)*d++=*s++;}
static u16 u16le(const u8*p){return(u16)p[0]|((u16)p[1]<<8);}
static u32 u32le(const u8*p){return(u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static u64 u64le(const u8*p){return(u64)u32le(p)|((u64)u32le(p+4)<<32);}
static void put_u64le(u8*p,u64 v){int i;for(i=0;i<8;i++)p[i]=(u8)(v>>(i*8));}

struct pe_info {
    const u8 *sections;
    u16 section_count;
    u32 entry_rva;
    u64 image_base;
    u32 image_size;
    u32 headers_size;
    u32 reloc_rva;
    u32 reloc_size;
};

static int inspect_pe(const u8 *data,size_t size,struct pe_info *pe)
{
    u32 peoff; const u8 *coff,*opt; u16 opt_size,section_count; size_t table_size;
    if(!data||!pe||size<0x80U)return-1;
    if(data[0]!='M'||data[1]!='Z')return-2;
    peoff=u32le(data+0x3c);
    if((size_t)peoff>size||size-(size_t)peoff<24U)return-3;
    coff=data+peoff;
    if(coff[0]!='P'||coff[1]!='E'||coff[2]||coff[3])return-4;
    if(u16le(coff+4)!=IMAGE_FILE_MACHINE_AMD64)return-5;
    section_count=u16le(coff+6); opt_size=u16le(coff+20);
    if(!section_count||section_count>96U||opt_size<0xb0U)return-6;
    if(size-(size_t)(coff-data)<24U+(size_t)opt_size)return-7;
    opt=coff+24;
    if(u16le(opt)!=PE32_PLUS_MAGIC)return-8;
    if(u16le(opt+0x44)!=IMAGE_SUBSYSTEM_EFI_APPLICATION)return-9;
    table_size=(size_t)section_count*40U;
    if((size_t)(opt+opt_size-data)>size||size-(size_t)(opt+opt_size-data)<table_size)return-10;
    mem_zero(pe,sizeof(*pe));
    pe->sections=opt+opt_size; pe->section_count=section_count;
    pe->entry_rva=u32le(opt+0x10); pe->image_base=u64le(opt+0x18);
    pe->image_size=u32le(opt+0x38); pe->headers_size=u32le(opt+0x3c);
    pe->reloc_rva=u32le(opt+0x70+IMAGE_DIRECTORY_ENTRY_BASERELOC*8U);
    pe->reloc_size=u32le(opt+0x74+IMAGE_DIRECTORY_ENTRY_BASERELOC*8U);
    if(!pe->image_size||pe->image_size>MAX_IMAGE_SIZE)return-11;
    if(!pe->headers_size||pe->headers_size>pe->image_size||pe->headers_size>size)return-12;
    if(pe->entry_rva>=pe->image_size)return-13;
    return 0;
}

static int map_sections(const u8 *file,size_t file_size,const struct pe_info *pe,u8 *image)
{
    u16 i; mem_zero(image,pe->image_size); mem_copy(image,file,pe->headers_size);
    for(i=0;i<pe->section_count;i++) {
        const u8 *sh=pe->sections+(size_t)i*40U;
        u32 vsize=u32le(sh+8),va=u32le(sh+12),raw_size=u32le(sh+16),raw_ptr=u32le(sh+20),copy_size=raw_size;
        if(va>=pe->image_size)return-1;
        if(vsize&&copy_size>vsize)copy_size=vsize;
        if(!copy_size)continue;
        if((u64)raw_ptr+copy_size>(u64)file_size)return-2;
        if((u64)va+copy_size>(u64)pe->image_size)return-3;
        mem_copy(image+va,file+raw_ptr,copy_size);
    }
    return 0;
}

static int apply_relocs(u8 *image,const struct pe_info *pe)
{
    u64 mapped=(u64)(unsigned long)image; s64 delta=(s64)(mapped-pe->image_base); u32 pos=0;
    if(!delta)return 0;
    if(!pe->reloc_rva||!pe->reloc_size)return-1;
    if((u64)pe->reloc_rva+pe->reloc_size>pe->image_size)return-2;
    while(pos<pe->reloc_size) {
        u8 *blk; u32 page,bsz,count,j;
        if(pe->reloc_size-pos<8U)return-3;
        blk=image+pe->reloc_rva+pos; page=u32le(blk); bsz=u32le(blk+4);
        if(bsz<8U||bsz>pe->reloc_size-pos)return-4;
        count=(bsz-8U)/2U;
        for(j=0;j<count;j++) {
            u16 e=u16le(blk+8U+j*2U),type=(u16)(e>>12),off=(u16)(e&0x0fffU); u64 rva=(u64)page+off;
            if(type==IMAGE_REL_BASED_ABSOLUTE)continue;
            if(type!=IMAGE_REL_BASED_DIR64)return-5;
            if(rva+8ULL>pe->image_size)return-6;
            put_u64le(image+(size_t)rva,(u64)((s64)u64le(image+(size_t)rva)+delta));
        }
        pos+=bsz;
    }
    return 0;
}

int main(void)
{
    const char *path="/mnt/usb0/EFI/Microsoft/Boot/bootmgfw.efi";
    int fd; u8 *file,*image; size_t total=0; struct pe_info pe;

    notify("PS4 Windows Loader: Stage 2.5 started");
    fd=open(path,O_RDONLY);
    if(fd<0){notify("Stage 2.5: direct usb0 open FAILED");return 1;}
    notify("Stage 2.5: direct usb0 open OK");

    file=(u8*)mmap((void*)0,(size_t)READ_CAP,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(file==(u8*)-1){close(fd);notify("Stage 2.5: read buffer mmap failed");return 1;}

    for(;;) {
        ssize_t got;
        if(total>=(size_t)READ_CAP){munmap(file,(size_t)READ_CAP);close(fd);notify("Stage 2.5: file exceeds 16 MiB cap");return 1;}
        got=read(fd,file+total,(size_t)READ_CAP-total);
        if(got<0){munmap(file,(size_t)READ_CAP);close(fd);notify("Stage 2.5: read FAILED");return 1;}
        if(got==0)break;
        total+=(size_t)got;
    }
    close(fd);
    if(total==0){munmap(file,(size_t)READ_CAP);notify("Stage 2.5: zero-byte file");return 1;}
    notify("Stage 2.5: stream read to EOF OK");

    if(inspect_pe(file,total,&pe)!=0){munmap(file,(size_t)READ_CAP);notify("Stage 2.5: PE32+ EFI validation FAILED");return 1;}
    notify("Stage 2.5: PE32+ EFI validation OK");

    image=(u8*)mmap((void*)0,pe.image_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(image==(u8*)-1){munmap(file,(size_t)READ_CAP);notify("Stage 2.5: image mmap failed");return 1;}
    if(map_sections(file,total,&pe,image)!=0){munmap(image,pe.image_size);munmap(file,(size_t)READ_CAP);notify("Stage 2.5: section map FAILED");return 1;}
    notify("Stage 2.5: sections mapped OK");
    if(apply_relocs(image,&pe)!=0){munmap(image,pe.image_size);munmap(file,(size_t)READ_CAP);notify("Stage 2.5: relocations FAILED");return 1;}
    notify("PS4 Windows Loader: Stage 2.5 PE map + relocations OK");
    munmap(image,pe.image_size); munmap(file,(size_t)READ_CAP); return 0;
}
