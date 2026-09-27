#define main stage3_base_main
#include "stage3.c"
#undef main

#define EFIERR(x) (0x8000000000000000ULL | (x))
#define EFI_INVALID_PARAMETER EFIERR(2)
#define EFI_UNSUPPORTED EFIERR(3)
#define EFI_BUFFER_TOO_SMALL EFIERR(5)
#define EFI_OUT_OF_RESOURCES EFIERR(9)
#define EFI_NOT_FOUND EFIERR(14)
#define EFI_PAGE_SIZE 4096ULL
#define EFI_ALLOCATE_ANY_PAGES 0U
#define EFI_BOOT_SERVICES_DATA 4U
#define EFI_CONVENTIONAL_MEMORY 7U

typedef u64 EFI_PHYSICAL_ADDRESS;
typedef u64 EFI_VIRTUAL_ADDRESS;
typedef u64 UINTN;
typedef struct { u32 Data1; u16 Data2; u16 Data3; u8 Data4[8]; } EFI_GUID;
typedef struct { u32 Type; u32 Pad; EFI_PHYSICAL_ADDRESS PhysicalStart; EFI_VIRTUAL_ADDRESS VirtualStart; u64 NumberOfPages; u64 Attribute; } EFI_MEMORY_DESCRIPTOR;

typedef EFI_STATUS (EFIAPI *allocate_pages_fn)(u32,u32,UINTN,EFI_PHYSICAL_ADDRESS*);
typedef EFI_STATUS (EFIAPI *free_pages_fn)(EFI_PHYSICAL_ADDRESS,UINTN);
typedef EFI_STATUS (EFIAPI *get_memory_map_fn)(UINTN*,EFI_MEMORY_DESCRIPTOR*,UINTN*,UINTN*,u32*);
typedef EFI_STATUS (EFIAPI *allocate_pool_fn)(u32,UINTN,void**);
typedef EFI_STATUS (EFIAPI *free_pool_fn)(void*);
typedef EFI_STATUS (EFIAPI *handle_protocol_fn)(EFI_HANDLE,EFI_GUID*,void**);
typedef EFI_STATUS (EFIAPI *locate_protocol_fn)(EFI_GUID*,void*,void**);

typedef struct {
    EFI_TABLE_HEADER Hdr;
    void *RaiseTPL, *RestoreTPL;
    allocate_pages_fn AllocatePages;
    free_pages_fn FreePages;
    get_memory_map_fn GetMemoryMap;
    allocate_pool_fn AllocatePool;
    free_pool_fn FreePool;
    void *CreateEvent,*SetTimer,*WaitForEvent,*SignalEvent,*CloseEvent,*CheckEvent;
    void *InstallProtocolInterface,*ReinstallProtocolInterface,*UninstallProtocolInterface;
    handle_protocol_fn HandleProtocol;
    void *Reserved,*RegisterProtocolNotify,*LocateHandle,*LocateDevicePath,*InstallConfigurationTable;
    void *LoadImage,*StartImage,*Exit,*UnloadImage,*ExitBootServices;
    void *GetNextMonotonicCount,*Stall,*SetWatchdogTimer,*ConnectController,*DisconnectController;
    void *OpenProtocol,*CloseProtocol,*OpenProtocolInformation,*ProtocolsPerHandle,*LocateHandleBuffer;
    locate_protocol_fn LocateProtocol;
    void *InstallMultipleProtocolInterfaces,*UninstallMultipleProtocolInterfaces,*CalculateCrc32,*CopyMem,*SetMem,*CreateEventEx;
} EFI_BOOT_SERVICES_31;

typedef struct { u8 Type,SubType,Length[2]; } EFI_DEVICE_PATH_PROTOCOL_31;
typedef struct { u64 Revision; void *OpenVolume; } EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_31;
typedef struct {
    u32 Revision; EFI_HANDLE ParentHandle; EFI_SYSTEM_TABLE *SystemTable; EFI_HANDLE DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL_31 *FilePath; void *Reserved; u32 LoadOptionsSize; void *LoadOptions;
    void *ImageBase; u64 ImageSize; u32 ImageCodeType; u32 ImageDataType; void *Unload;
} EFI_LOADED_IMAGE_PROTOCOL_31;

static EFI_BOOT_SERVICES_31 bs31;
static EFI_DEVICE_PATH_PROTOCOL_31 end_path31;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_31 fs31;
static EFI_LOADED_IMAGE_PROTOCOL_31 loaded31;
static u64 image_handle31=0x3133474d49345350ULL;
static u64 device_handle31=0x3133564544345350ULL;
static u64 map_key31=1;
static void *arena31;

static const EFI_GUID loaded_guid31={0x5b1b31a1U,0x9562U,0x11d2U,{0x8e,0x3f,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static const EFI_GUID device_path_guid31={0x09576e91U,0x6d3fU,0x11d2U,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static const EFI_GUID simple_fs_guid31={0x964e5b22U,0x6459U,0x11d2U,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};

static int guid_eq31(const EFI_GUID *a,const EFI_GUID *b){ return a&&b&&mem_eq(a,b,sizeof(EFI_GUID)); }

struct pool_header31 { u64 magic,size; };
#define POOL_MAGIC31 0x31334c4f4f505350ULL

static EFI_STATUS EFIAPI alloc_pages31(u32 type,u32 memtype,UINTN pages,EFI_PHYSICAL_ADDRESS *out)
{
    void *p; (void)memtype;
    if(!out||!pages) return EFI_INVALID_PARAMETER;
    if(type!=EFI_ALLOCATE_ANY_PAGES) return EFI_UNSUPPORTED;
    p=mmap(0,(size_t)(pages*EFI_PAGE_SIZE),PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(p==(void*)-1) return EFI_OUT_OF_RESOURCES;
    *out=(EFI_PHYSICAL_ADDRESS)(unsigned long)p; ++map_key31; return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI free_pages31(EFI_PHYSICAL_ADDRESS addr,UINTN pages)
{
    if(!addr||!pages) return EFI_INVALID_PARAMETER;
    if(munmap((void*)(unsigned long)addr,(size_t)(pages*EFI_PAGE_SIZE))!=0) return EFI_INVALID_PARAMETER;
    ++map_key31; return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI alloc_pool31(u32 type,UINTN size,void **out)
{
    struct pool_header31 *h; size_t total=(size_t)size+sizeof(*h), map=(total+4095U)&~4095U; (void)type;
    if(!out||!size) return EFI_INVALID_PARAMETER;
    h=(struct pool_header31*)mmap(0,map,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(h==(struct pool_header31*)-1) return EFI_OUT_OF_RESOURCES;
    h->magic=POOL_MAGIC31; h->size=(u64)map; *out=(void*)(h+1); ++map_key31; return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI free_pool31(void *p)
{
    struct pool_header31 *h;
    if(!p) return EFI_INVALID_PARAMETER;
    h=((struct pool_header31*)p)-1;
    if(h->magic!=POOL_MAGIC31) return EFI_INVALID_PARAMETER;
    if(munmap(h,(size_t)h->size)!=0) return EFI_INVALID_PARAMETER;
    ++map_key31; return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI get_map31(UINTN *size,EFI_MEMORY_DESCRIPTOR *map,UINTN *key,UINTN *desc_size,u32 *ver)
{
    EFI_MEMORY_DESCRIPTOR d;
    if(!size||!key||!desc_size||!ver) return EFI_INVALID_PARAMETER;
    *desc_size=sizeof(EFI_MEMORY_DESCRIPTOR); *ver=1; *key=map_key31;
    if(!map||*size<sizeof(EFI_MEMORY_DESCRIPTOR)){ *size=sizeof(EFI_MEMORY_DESCRIPTOR); return EFI_BUFFER_TOO_SMALL; }
    mem_zero(&d,sizeof(d)); d.Type=EFI_CONVENTIONAL_MEMORY; d.PhysicalStart=(u64)(unsigned long)arena31; d.NumberOfPages=4096;
    mem_copy(map,&d,sizeof(d)); *size=sizeof(d); return EFI_SUCCESS;
}

static EFI_STATUS protocol31(EFI_HANDLE h,EFI_GUID *g,void **out)
{
    if(!g||!out) return EFI_INVALID_PARAMETER;
    if(h==(EFI_HANDLE)&image_handle31&&guid_eq31(g,&loaded_guid31)){*out=&loaded31;return EFI_SUCCESS;}
    if(h==(EFI_HANDLE)&device_handle31&&guid_eq31(g,&device_path_guid31)){*out=&end_path31;return EFI_SUCCESS;}
    if(h==(EFI_HANDLE)&device_handle31&&guid_eq31(g,&simple_fs_guid31)){*out=&fs31;return EFI_SUCCESS;}
    return EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI handle31(EFI_HANDLE h,EFI_GUID *g,void **out){return protocol31(h,g,out);}
static EFI_STATUS EFIAPI locate31(EFI_GUID *g,void *reg,void **out){(void)reg;if(guid_eq31(g,&loaded_guid31)){*out=&loaded31;return EFI_SUCCESS;}if(guid_eq31(g,&device_path_guid31)){*out=&end_path31;return EFI_SUCCESS;}if(guid_eq31(g,&simple_fs_guid31)){*out=&fs31;return EFI_SUCCESS;}return EFI_NOT_FOUND;}

static int build31(void)
{
    mem_zero(&bs31,sizeof(bs31)); mem_zero(&loaded31,sizeof(loaded31)); mem_zero(&end_path31,sizeof(end_path31)); mem_zero(&fs31,sizeof(fs31));
    arena31=mmap(0,16U*1024U*1024U,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(arena31==(void*)-1) return -1;
    bs31.Hdr.Signature=EFI_BOOT_SERVICES_SIGNATURE; bs31.Hdr.Revision=EFI_REVISION_2_0; bs31.Hdr.HeaderSize=(u32)sizeof(bs31);
    bs31.AllocatePages=alloc_pages31; bs31.FreePages=free_pages31; bs31.GetMemoryMap=get_map31; bs31.AllocatePool=alloc_pool31; bs31.FreePool=free_pool31; bs31.HandleProtocol=handle31; bs31.LocateProtocol=locate31;
    bs31.Hdr.CRC32=crc32_bytes(&bs31,sizeof(bs31));
    end_path31.Type=0x7f; end_path31.SubType=0xff; end_path31.Length[0]=4;
    fs31.Revision=0x00010000ULL;
    loaded31.Revision=0x1000U; loaded31.SystemTable=&g_system_table; loaded31.DeviceHandle=(EFI_HANDLE)&device_handle31; loaded31.FilePath=&end_path31;
    g_system_table.BootServices=&bs31;
    return 0;
}

int main(void)
{
    void *pool=0,*iface=0; EFI_PHYSICAL_ADDRESS pages=0; UINTN sz=0,key=0,ds=0; u32 ver=0; EFI_MEMORY_DESCRIPTOR d;
    notify("PS4 Windows Loader: Stage 3.1 started");
    build_efi_shim();
    if(build31()!=0){notify("Stage 3.1: Boot Services build FAILED");return 1;}
    notify("Stage 3.1: Boot Services table constructed");

    if(bs31.AllocatePool(EFI_BOOT_SERVICES_DATA,1234,&pool)!=EFI_SUCCESS||!pool||bs31.FreePool(pool)!=EFI_SUCCESS){notify("Stage 3.1: pool services FAILED");return 1;}
    notify("Stage 3.1: AllocatePool/FreePool OK");
    if(bs31.AllocatePages(EFI_ALLOCATE_ANY_PAGES,EFI_BOOT_SERVICES_DATA,2,&pages)!=EFI_SUCCESS||!pages||bs31.FreePages(pages,2)!=EFI_SUCCESS){notify("Stage 3.1: page services FAILED");return 1;}
    notify("Stage 3.1: AllocatePages/FreePages OK");
    if(bs31.GetMemoryMap(&sz,0,&key,&ds,&ver)!=EFI_BUFFER_TOO_SMALL){notify("Stage 3.1: memory map sizing FAILED");return 1;}
    sz=sizeof(d); if(bs31.GetMemoryMap(&sz,&d,&key,&ds,&ver)!=EFI_SUCCESS){notify("Stage 3.1: GetMemoryMap FAILED");return 1;}
    notify("Stage 3.1: GetMemoryMap semantics OK");
    if(bs31.HandleProtocol((EFI_HANDLE)&image_handle31,(EFI_GUID*)&loaded_guid31,&iface)!=EFI_SUCCESS||iface!=&loaded31){notify("Stage 3.1: LoadedImage FAILED");return 1;}
    notify("Stage 3.1: LoadedImage protocol OK");
    iface=0; if(bs31.HandleProtocol((EFI_HANDLE)&device_handle31,(EFI_GUID*)&device_path_guid31,&iface)!=EFI_SUCCESS||iface!=&end_path31){notify("Stage 3.1: DevicePath FAILED");return 1;}
    notify("Stage 3.1: DevicePath protocol OK");
    iface=0; if(bs31.LocateProtocol((EFI_GUID*)&simple_fs_guid31,0,&iface)!=EFI_SUCCESS||iface!=&fs31){notify("Stage 3.1: SimpleFS FAILED");return 1;}
    notify("Stage 3.1: SimpleFileSystem protocol published");
    notify("PS4 Windows Loader: Stage 3.1 EFI Boot Services self-test OK");
    munmap(arena31,16U*1024U*1024U);
    return 0;
}
