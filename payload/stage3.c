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
typedef u64 EFI_STATUS;
typedef void *EFI_HANDLE;
typedef u16 CHAR16;
typedef int (*notify_fn_t)(int, const char *);

#define EFIAPI __attribute__((ms_abi))
#define EFI_SUCCESS 0ULL
#define EFI_SYSTEM_TABLE_SIGNATURE 0x5453595320494249ULL
#define EFI_BOOT_SERVICES_SIGNATURE 0x56524553544f4f42ULL
#define EFI_REVISION_2_0 0x00020000U

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

static u32 crc32_bytes(const void *ptr, size_t len)
{
    const u8 *p=(const u8*)ptr;
    u32 crc=0xffffffffU;
    size_t i;
    for(i=0;i<len;i++) {
        u32 x=(crc^(u32)p[i])&0xffU;
        int b;
        for(b=0;b<8;b++) x=(x&1U)?((x>>1)^0xedb88320U):(x>>1);
        crc=(crc>>8)^x;
    }
    return crc^0xffffffffU;
}

typedef struct {
    u64 Signature;
    u32 Revision;
    u32 HeaderSize;
    u32 CRC32;
    u32 Reserved;
} EFI_TABLE_HEADER;

struct efi_simple_text_output_protocol;
typedef EFI_STATUS (EFIAPI *efi_output_string_t)(struct efi_simple_text_output_protocol *, CHAR16 *);

typedef struct efi_simple_text_output_protocol {
    void *Reset;
    efi_output_string_t OutputString;
    void *TestString;
    void *QueryMode;
    void *SetMode;
    void *SetAttribute;
    void *ClearScreen;
    void *SetCursorPosition;
    void *EnableCursor;
    void *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    EFI_TABLE_HEADER Hdr;
} EFI_BOOT_SERVICES_HEADER_ONLY;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    u32 FirmwareRevision;
    u32 Pad;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    void *RuntimeServices;
    void *BootServices;
    u64 NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL g_conout;
static EFI_BOOT_SERVICES_HEADER_ONLY g_boot_services;
static EFI_SYSTEM_TABLE g_system_table;
static u64 g_image_token=0x505334574c445231ULL;
static CHAR16 g_vendor[]={'P','S','4',' ','W','i','n','d','o','w','s',' ','L','o','a','d','e','r',0};
static CHAR16 g_test_message[]={'S','t','a','g','e',' ','3',' ','E','F','I',' ','O','u','t','p','u','t','S','t','r','i','n','g',' ','O','K',0};

static EFI_STATUS EFIAPI shim_output_string(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *self, CHAR16 *str)
{
    char out[192];
    size_t i=0;
    (void)self;
    if(!str) return 2;
    while(str[i] && i<sizeof(out)-1) {
        u16 c=str[i];
        out[i]=(c>=32U && c<=126U)?(char)c:'?';
        ++i;
    }
    out[i]=0;
    notify(out);
    return EFI_SUCCESS;
}

static void build_efi_shim(void)
{
    mem_zero(&g_conout,sizeof(g_conout));
    mem_zero(&g_boot_services,sizeof(g_boot_services));
    mem_zero(&g_system_table,sizeof(g_system_table));

    g_conout.OutputString=shim_output_string;

    g_boot_services.Hdr.Signature=EFI_BOOT_SERVICES_SIGNATURE;
    g_boot_services.Hdr.Revision=EFI_REVISION_2_0;
    g_boot_services.Hdr.HeaderSize=(u32)sizeof(g_boot_services);
    g_boot_services.Hdr.CRC32=0;
    g_boot_services.Hdr.CRC32=crc32_bytes(&g_boot_services,g_boot_services.Hdr.HeaderSize);

    g_system_table.Hdr.Signature=EFI_SYSTEM_TABLE_SIGNATURE;
    g_system_table.Hdr.Revision=EFI_REVISION_2_0;
    g_system_table.Hdr.HeaderSize=(u32)sizeof(g_system_table);
    g_system_table.FirmwareVendor=g_vendor;
    g_system_table.FirmwareRevision=1;
    g_system_table.ConsoleOutHandle=(EFI_HANDLE)&g_conout;
    g_system_table.ConOut=&g_conout;
    g_system_table.StandardErrorHandle=(EFI_HANDLE)&g_conout;
    g_system_table.StdErr=&g_conout;
    g_system_table.BootServices=&g_boot_services;
    g_system_table.Hdr.CRC32=0;
    g_system_table.Hdr.CRC32=crc32_bytes(&g_system_table,g_system_table.Hdr.HeaderSize);
}

static EFI_STATUS EFIAPI mock_efi_entry(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *st)
{
    if(image_handle!=(EFI_HANDLE)&g_image_token) return 0x1001ULL;
    if(!st || st->Hdr.Signature!=EFI_SYSTEM_TABLE_SIGNATURE) return 0x1002ULL;
    if(!st->BootServices) return 0x1003ULL;
    if(!st->ConOut || !st->ConOut->OutputString) return 0x1004ULL;
    if(st->ConOut->OutputString(st->ConOut,g_test_message)!=EFI_SUCCESS) return 0x1005ULL;
    return EFI_SUCCESS;
}

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
    u32 peoff; const u8 *coff,*opt; u16 opt_size,section_count; size_t table_size,section_offset; u16 i,j; int executable_entry=0;
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
    section_offset=(size_t)(pe->sections-data);
    if(section_offset>pe->headers_size||table_size>pe->headers_size-section_offset)return-14;
    for(i=0;i<section_count;i++){
        const u8 *sh=pe->sections+(size_t)i*40U;
        u32 vsize=u32le(sh+8),va=u32le(sh+12),raw=u32le(sh+16),ptr=u32le(sh+20);
        u32 mapped=vsize>raw?vsize:raw;
        if(va>pe->image_size||mapped>pe->image_size-va)return-15;
        if(raw&&((size_t)ptr>size||(size_t)raw>size-(size_t)ptr))return-16;
        if(mapped&&va<pe->headers_size)return-19;
        for(j=0;j<i&&mapped;j++){
            const u8 *prev=pe->sections+(size_t)j*40U;
            u32 old_va=u32le(prev+12),old_virtual=u32le(prev+8),old_raw=u32le(prev+16);
            u32 old_size=old_virtual>old_raw?old_virtual:old_raw;
            if(old_size&&va<old_va+old_size&&old_va<va+mapped)return-20;
        }
        if(pe->entry_rva>=va&&pe->entry_rva-va<mapped&&
           (u32le(sh+36)&0x20000000U))executable_entry=1;
    }
    if(!executable_entry)return-17;
    if(pe->reloc_size&&(!pe->reloc_rva||pe->reloc_rva>pe->image_size||
       pe->reloc_size>pe->image_size-pe->reloc_rva))return-18;
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
    u64 mapped=(u64)(unsigned long)image,delta=mapped-pe->image_base; u32 pos; int pass;
    if(!delta)return 0;
    if(!pe->reloc_rva||!pe->reloc_size)return-1;
    if((u64)pe->reloc_rva+pe->reloc_size>pe->image_size)return-2;
    /* Validate the entire directory before changing a single image byte. */
    for(pass=0;pass<2;pass++) {
        pos=0;
        while(pos<pe->reloc_size) {
            u8 *blk; u32 page,bsz,count,j;
            if(pe->reloc_size-pos<8U)return-3;
            blk=image+pe->reloc_rva+pos; page=u32le(blk); bsz=u32le(blk+4);
            if(bsz<8U||bsz>pe->reloc_size-pos||(bsz&1U)||(page&0xfffU))return-4;
            count=(bsz-8U)/2U;
            for(j=0;j<count;j++) {
                u16 e=u16le(blk+8U+j*2U),type=(u16)(e>>12),off=(u16)(e&0x0fffU); u64 rva=(u64)page+off;
                if(type==IMAGE_REL_BASED_ABSOLUTE)continue;
                if(type!=IMAGE_REL_BASED_DIR64)return-5;
                if(rva+8ULL>pe->image_size)return-6;
                if(rva<(u64)pe->reloc_rva+pe->reloc_size&&
                   (u64)pe->reloc_rva<rva+8ULL)return-7;
                if(pass)put_u64le(image+(size_t)rva,u64le(image+(size_t)rva)+delta);
            }
            pos+=bsz;
        }
    }
    return 0;
}

static int load_bootmgfw(u8 **out_file,size_t *out_size)
{
    const char *path="/mnt/usb0/EFI/Microsoft/Boot/bootmgfw.efi";
    int fd; u8 *file; size_t total=0;
    fd=open(path,O_RDONLY);
    if(fd<0)return-1;
    file=(u8*)mmap((void*)0,(size_t)READ_CAP,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(file==(u8*)-1){close(fd);return-2;}
    for(;;) {
        ssize_t got;
        if(total>=(size_t)READ_CAP){munmap(file,(size_t)READ_CAP);close(fd);return-3;}
        got=read(fd,file+total,(size_t)READ_CAP-total);
        if(got<0){munmap(file,(size_t)READ_CAP);close(fd);return-4;}
        if(got==0)break;
        total+=(size_t)got;
    }
    close(fd);
    if(total==0){munmap(file,(size_t)READ_CAP);return-5;}
    *out_file=file; *out_size=total; return 0;
}

int main(void)
{
    EFI_STATUS efi_rc;
    u8 *file,*image;
    size_t file_size;
    struct pe_info pe;
    void *entry;

    notify("PS4 Windows Loader: Stage 3 started");

    build_efi_shim();
    notify("Stage 3: EFI SystemTable constructed");
    if(g_system_table.Hdr.Signature!=EFI_SYSTEM_TABLE_SIGNATURE) {
        notify("Stage 3: SystemTable signature FAILED");
        return 1;
    }
    notify("Stage 3: SystemTable signature OK");

    efi_rc=mock_efi_entry((EFI_HANDLE)&g_image_token,&g_system_table);
    if(efi_rc!=EFI_SUCCESS) {
        notify("Stage 3: Microsoft x64 EFI ABI test FAILED");
        return 1;
    }
    notify("Stage 3: Microsoft x64 EFI ABI test OK");

    if(load_bootmgfw(&file,&file_size)!=0) {
        notify("Stage 3: bootmgfw.efi read FAILED");
        return 1;
    }
    notify("Stage 3: bootmgfw.efi read OK");

    if(inspect_pe(file,file_size,&pe)!=0) {
        munmap(file,(size_t)READ_CAP);
        notify("Stage 3: PE32+ EFI validation FAILED");
        return 1;
    }
    notify("Stage 3: PE32+ EFI validation OK");

    image=(u8*)mmap((void*)0,pe.image_size,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(image==(u8*)-1) {
        munmap(file,(size_t)READ_CAP);
        notify("Stage 3: executable image mmap FAILED");
        return 1;
    }
    if(map_sections(file,file_size,&pe,image)!=0) {
        munmap(image,pe.image_size); munmap(file,(size_t)READ_CAP);
        notify("Stage 3: PE section map FAILED");
        return 1;
    }
    if(apply_relocs(image,&pe)!=0) {
        munmap(image,pe.image_size); munmap(file,(size_t)READ_CAP);
        notify("Stage 3: relocations FAILED");
        return 1;
    }
    notify("Stage 3: bootmgfw executable image ready");

    entry=(void*)(image+pe.entry_rva);
    if(!entry) {
        munmap(image,pe.image_size); munmap(file,(size_t)READ_CAP);
        notify("Stage 3: entry point FAILED");
        return 1;
    }
    notify("PS4 Windows Loader: Stage 3 EFI shim + bootmgfw entry ready");

    /* Intentionally do not call Microsoft's entry point yet. Stage 3.1 will
       add the first real Boot Services/protocol implementations before that. */
    munmap(image,pe.image_size);
    munmap(file,(size_t)READ_CAP);
    return 0;
}
