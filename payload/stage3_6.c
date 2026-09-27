#define STAGE3_5_NO_MAIN 1
#include "stage3_5.c"

#define EFI_LOADER_CODE 1U
#define EFI_LOADER_DATA 2U
#define EFI_NATIVE_INTERFACE 0U
#define EFI_LOCATE_ALL_HANDLES 0U
#define EFI_LOCATE_BY_REGISTER_NOTIFY 1U
#define MAX_ALLOCS36 64U
#define MAX_CONFIG36 8U

typedef struct { int used; int pool; void *base; UINTN pages; u32 type; } ALLOC36;
typedef struct { EFI_GUID VendorGuid; void *VendorTable; } EFI_CONFIGURATION_TABLE_36;
typedef EFI_STATUS (EFIAPI *locate_handle36_fn)(u32,EFI_GUID*,void*,UINTN*,EFI_HANDLE*);
typedef EFI_STATUS (EFIAPI *locate_device_path36_fn)(EFI_GUID*,EFI_DEVICE_PATH_PROTOCOL_32**,EFI_HANDLE*);
typedef EFI_STATUS (EFIAPI *install_config36_fn)(EFI_GUID*,void*);
typedef EFI_STATUS (EFIAPI *register_notify36_fn)(EFI_GUID*,EFI_EVENT_35,void**);
typedef EFI_STATUS (EFIAPI *open_info36_fn)(EFI_HANDLE,EFI_GUID*,void**,UINTN*);

static ALLOC36 alloc36[MAX_ALLOCS36];
static EFI_CONFIGURATION_TABLE_36 config36[MAX_CONFIG36];
static UINTN config_count36=0;
static UINTN arena_pages36=4096;
static union { u64 align; u8 bytes[256]; } loaded_path36;

static ALLOC36 *free_slot36(void){UINTN i;for(i=0;i<MAX_ALLOCS36;i++)if(!alloc36[i].used)return &alloc36[i];return 0;}
static ALLOC36 *find_alloc36(void *base,int pool){UINTN i;for(i=0;i<MAX_ALLOCS36;i++)if(alloc36[i].used&&alloc36[i].base==base&&alloc36[i].pool==pool)return &alloc36[i];return 0;}

static EFI_STATUS EFIAPI alloc_pages36(u32 at,u32 mt,UINTN pages,EFI_PHYSICAL_ADDRESS*out)
{
    ALLOC36*r;void*p;size_t bytes;
    if(!out||!pages)return EFI_INVALID_PARAMETER;if(at!=EFI_ALLOCATE_ANY_PAGES)return EFI_UNSUPPORTED;
    r=free_slot36();if(!r)return EFI_OUT_OF_RESOURCES;bytes=(size_t)(pages*EFI_PAGE_SIZE);
    p=mmap(0,bytes,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(p==(void*)-1)return EFI_OUT_OF_RESOURCES;
    r->used=1;r->pool=0;r->base=p;r->pages=pages;r->type=mt;*out=(u64)(unsigned long)p;++map_key32;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI free_pages36(EFI_PHYSICAL_ADDRESS addr,UINTN pages)
{
    ALLOC36*r=find_alloc36((void*)(unsigned long)addr,0);if(!r||r->pages!=pages)return EFI_INVALID_PARAMETER;
    if(munmap(r->base,(size_t)(r->pages*EFI_PAGE_SIZE))!=0)return EFI_INVALID_PARAMETER;mem_zero(r,sizeof(*r));++map_key32;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI alloc_pool36(u32 mt,UINTN size,void**out)
{
    struct pool_header32*h;ALLOC36*r;size_t total,map;
    if(!out||!size)return EFI_INVALID_PARAMETER;r=free_slot36();if(!r)return EFI_OUT_OF_RESOURCES;
    total=(size_t)size+sizeof(*h);map=(total+4095U)&~4095U;h=(struct pool_header32*)mmap(0,map,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(h==(struct pool_header32*)-1)return EFI_OUT_OF_RESOURCES;h->magic=POOL_MAGIC32;h->size=(u64)map;
    r->used=1;r->pool=1;r->base=h;r->pages=(UINTN)(map/4096U);r->type=mt;*out=(void*)(h+1);++map_key32;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI free_pool36(void*p)
{
    struct pool_header32*h;ALLOC36*r;if(!p)return EFI_INVALID_PARAMETER;h=((struct pool_header32*)p)-1;
    if(h->magic!=POOL_MAGIC32)return EFI_INVALID_PARAMETER;r=find_alloc36(h,1);if(!r)return EFI_INVALID_PARAMETER;
    if(munmap(h,(size_t)h->size)!=0)return EFI_INVALID_PARAMETER;mem_zero(r,sizeof(*r));++map_key32;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI get_map36(UINTN*size,EFI_MEMORY_DESCRIPTOR*map,UINTN*key,UINTN*desc_size,u32*ver)
{
    UINTN i,n=1,need,pos=0;EFI_MEMORY_DESCRIPTOR*d;
    if(!size||!key||!desc_size||!ver)return EFI_INVALID_PARAMETER;for(i=0;i<MAX_ALLOCS36;i++)if(alloc36[i].used)++n;
    need=n*(UINTN)sizeof(EFI_MEMORY_DESCRIPTOR);*desc_size=sizeof(EFI_MEMORY_DESCRIPTOR);*ver=1;*key=map_key32;
    if(!map||*size<need){*size=need;return EFI_BUFFER_TOO_SMALL;}mem_zero(map,(size_t)need);
    d=(EFI_MEMORY_DESCRIPTOR*)((u8*)map+pos);d->Type=EFI_CONVENTIONAL_MEMORY;d->PhysicalStart=(u64)(unsigned long)arena32;d->NumberOfPages=arena_pages36;pos+=sizeof(*d);
    for(i=0;i<MAX_ALLOCS36;i++)if(alloc36[i].used){d=(EFI_MEMORY_DESCRIPTOR*)((u8*)map+pos);d->Type=alloc36[i].type;d->PhysicalStart=(u64)(unsigned long)alloc36[i].base;d->NumberOfPages=alloc36[i].pages;pos+=sizeof(*d);}
    *size=need;return EFI_SUCCESS;
}

static EFI_HANDLE match_handle36(EFI_GUID*g)
{
    if(!g)return 0;if(guid_eq35(g,&loaded_guid32))return (EFI_HANDLE)&image_handle32;
    if(guid_eq35(g,&device_path_guid32)||guid_eq35(g,&simple_fs_guid32))return (EFI_HANDLE)&device_handle32;return 0;
}
static EFI_STATUS EFIAPI locate_handle36(u32 type,EFI_GUID*g,void*key,UINTN*sz,EFI_HANDLE*buf)
{
    EFI_HANDLE h;(void)key;if(!sz)return EFI_INVALID_PARAMETER;
    if(type==EFI_LOCATE_ALL_HANDLES)h=(EFI_HANDLE)&device_handle32;else if(type==EFI_LOCATE_BY_PROTOCOL)h=match_handle36(g);else return EFI_UNSUPPORTED;
    if(!h)return EFI_NOT_FOUND;if(!buf||*sz<sizeof(EFI_HANDLE)){*sz=sizeof(EFI_HANDLE);return EFI_BUFFER_TOO_SMALL;}buf[0]=h;*sz=sizeof(EFI_HANDLE);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI locate_handle_buffer36(u32 type,EFI_GUID*g,void*key,UINTN*count,EFI_HANDLE**buffer)
{
    EFI_HANDLE h;EFI_STATUS rc;if(!count||!buffer)return EFI_INVALID_PARAMETER;*count=0;*buffer=0;if(type!=EFI_LOCATE_BY_PROTOCOL)return EFI_UNSUPPORTED;h=match_handle36(g);if(!h)return EFI_NOT_FOUND;
    rc=alloc_pool36(EFI_BOOT_SERVICES_DATA,sizeof(EFI_HANDLE),(void**)buffer);if(rc!=EFI_SUCCESS)return rc;(*buffer)[0]=h;*count=1;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI protocols_per_handle36(EFI_HANDLE h,EFI_GUID***buffer,UINTN*count)
{
    UINTN n;EFI_STATUS rc;if(!h||!buffer||!count)return EFI_INVALID_PARAMETER;*buffer=0;*count=0;
    if(h==(EFI_HANDLE)&image_handle32)n=1;else if(h==(EFI_HANDLE)&device_handle32)n=2;else return EFI_NOT_FOUND;
    rc=alloc_pool36(EFI_BOOT_SERVICES_DATA,n*sizeof(EFI_GUID*),(void**)buffer);if(rc!=EFI_SUCCESS)return rc;
    if(n==1)(*buffer)[0]=(EFI_GUID*)&loaded_guid32;else{(*buffer)[0]=(EFI_GUID*)&device_path_guid32;(*buffer)[1]=(EFI_GUID*)&simple_fs_guid32;}*count=n;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI locate_device_path36(EFI_GUID*g,EFI_DEVICE_PATH_PROTOCOL_32**path,EFI_HANDLE*dev)
{if(!g||!path||!*path||!dev)return EFI_INVALID_PARAMETER;if(guid_eq35(g,&simple_fs_guid32)||guid_eq35(g,&device_path_guid32)){*dev=(EFI_HANDLE)&device_handle32;return EFI_SUCCESS;}return EFI_NOT_FOUND;}
static EFI_STATUS EFIAPI register_notify36(EFI_GUID*g,EFI_EVENT_35 e,void**reg){if(!g||!e||!reg)return EFI_INVALID_PARAMETER;*reg=(void*)g;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI open_info36(EFI_HANDLE h,EFI_GUID*g,void**entries,UINTN*count){void*x=0;if(!h||!g||!entries||!count)return EFI_INVALID_PARAMETER;if(protocol32(h,g,&x)!=EFI_SUCCESS)return EFI_NOT_FOUND;*entries=0;*count=0;return EFI_SUCCESS;}

static void refresh_system_crc36(void){g_system_table.Hdr.CRC32=0;g_system_table.Hdr.CRC32=crc32_bytes(&g_system_table,g_system_table.Hdr.HeaderSize);}
static EFI_STATUS EFIAPI install_config36(EFI_GUID*g,void*table)
{
    UINTN i;if(!g)return EFI_INVALID_PARAMETER;
    for(i=0;i<config_count36;i++)if(guid_eq35(g,&config36[i].VendorGuid)){
        if(!table){UINTN j;for(j=i+1;j<config_count36;j++)config36[j-1]=config36[j];--config_count36;}
        else config36[i].VendorTable=table;g_system_table.NumberOfTableEntries=config_count36;g_system_table.ConfigurationTable=config_count36?config36:0;refresh_system_crc36();return EFI_SUCCESS;
    }
    if(!table)return EFI_NOT_FOUND;if(config_count36>=MAX_CONFIG36)return EFI_OUT_OF_RESOURCES;config36[config_count36].VendorGuid=*g;config36[config_count36].VendorTable=table;++config_count36;
    g_system_table.NumberOfTableEntries=config_count36;g_system_table.ConfigurationTable=config36;refresh_system_crc36();return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI unsupported_install_protocol36(EFI_HANDLE*h,EFI_GUID*g,u32 t,void*i){(void)h;(void)g;(void)t;(void)i;notify("Stage 3.6 trace: InstallProtocolInterface");return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI unsupported_reinstall36(EFI_HANDLE h,EFI_GUID*g,void*o,void*n){(void)h;(void)g;(void)o;(void)n;notify("Stage 3.6 trace: ReinstallProtocolInterface");return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI unsupported_uninstall36(EFI_HANDLE h,EFI_GUID*g,void*i){(void)h;(void)g;(void)i;notify("Stage 3.6 trace: UninstallProtocolInterface");return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI unsupported_load_image36(u8 b,EFI_HANDLE p,EFI_DEVICE_PATH_PROTOCOL_32*d,void*s,UINTN z,EFI_HANDLE*i){(void)b;(void)p;(void)d;(void)s;(void)z;(void)i;notify("Stage 3.6 trace: LoadImage");return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI unsupported_start_image36(EFI_HANDLE i,UINTN*s,CHAR16**d){(void)i;(void)s;(void)d;notify("Stage 3.6 trace: StartImage");return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI image_exit36(EFI_HANDLE i,EFI_STATUS st,UINTN n,CHAR16*d){(void)n;(void)d;if(i!=(EFI_HANDLE)&image_handle32)return EFI_INVALID_PARAMETER;return st;}
static EFI_STATUS EFIAPI unsupported_unload36(EFI_HANDLE i){(void)i;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI unsupported_connect36(EFI_HANDLE c,EFI_HANDLE*d,EFI_DEVICE_PATH_PROTOCOL_32*p,u8 r){(void)c;(void)d;(void)p;(void)r;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI unsupported_disconnect36(EFI_HANDLE c,EFI_HANDLE d,EFI_HANDLE h){(void)c;(void)d;(void)h;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI unsupported_multi_install36(EFI_HANDLE*h,void*first){(void)h;(void)first;notify("Stage 3.6 trace: InstallMultipleProtocolInterfaces");return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI unsupported_multi_uninstall36(EFI_HANDLE h,void*first){(void)h;(void)first;notify("Stage 3.6 trace: UninstallMultipleProtocolInterfaces");return EFI_UNSUPPORTED;}

static void install_stage36_services(void)
{
    void*big=mmap(0,128U*1024U*1024U,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    mem_zero(alloc36,sizeof(alloc36));config_count36=0;
    if(big!=(void*)-1){munmap(arena32,16U*1024U*1024U);arena32=big;arena_pages36=32768;}else arena_pages36=4096;
    bs32.AllocatePages=alloc_pages36;bs32.FreePages=free_pages36;bs32.GetMemoryMap=get_map36;bs32.AllocatePool=alloc_pool36;bs32.FreePool=free_pool36;
    bs32.InstallProtocolInterface=(void*)unsupported_install_protocol36;bs32.ReinstallProtocolInterface=(void*)unsupported_reinstall36;bs32.UninstallProtocolInterface=(void*)unsupported_uninstall36;
    bs32.RegisterProtocolNotify=(void*)register_notify36;bs32.LocateHandle=(void*)locate_handle36;bs32.LocateDevicePath=(void*)locate_device_path36;bs32.InstallConfigurationTable=(void*)install_config36;
    bs32.LoadImage=(void*)unsupported_load_image36;bs32.StartImage=(void*)unsupported_start_image36;bs32.Exit=(void*)image_exit36;bs32.UnloadImage=(void*)unsupported_unload36;
    bs32.ConnectController=(void*)unsupported_connect36;bs32.DisconnectController=(void*)unsupported_disconnect36;bs32.OpenProtocolInformation=(void*)open_info36;
    bs32.LocateHandleBuffer=(void*)locate_handle_buffer36;bs32.ProtocolsPerHandle=(void*)protocols_per_handle36;
    bs32.InstallMultipleProtocolInterfaces=(void*)unsupported_multi_install36;bs32.UninstallMultipleProtocolInterfaces=(void*)unsupported_multi_uninstall36;
    bs32.Hdr.CRC32=0;bs32.Hdr.CRC32=crc32_bytes(&bs32,sizeof(bs32));g_system_table.BootServices=&bs32;refresh_system_crc36();
}

static int map_sections36(const u8*file,size_t file_size,const struct pe_info*pe,u8*image)
{
    u16 i;mem_zero(image,pe->image_size);mem_copy(image,file,pe->headers_size);
    for(i=0;i<pe->section_count;i++){const u8*sh=pe->sections+(size_t)i*40U;u32 va=u32le(sh+12),raw=u32le(sh+16),ptr=u32le(sh+20);if(va>=pe->image_size)return-1;if(!raw)continue;if((u64)ptr+raw>(u64)file_size)return-2;if((u64)va+raw>(u64)pe->image_size)return-3;mem_copy(image+va,file+ptr,raw);}return 0;
}
static void build_loaded_path36(void)
{
    static CHAR16 p[]={'\\','E','F','I','\\','M','i','c','r','o','s','o','f','t','\\','B','o','o','t','\\','b','o','o','t','m','g','f','w','.','e','f','i',0};
    size_t n=0,i,node;u8*b=loaded_path36.bytes;while(p[n])++n;node=4+(n+1)*2;mem_zero(b,sizeof(loaded_path36.bytes));b[0]=4;b[1]=4;b[2]=(u8)node;b[3]=(u8)(node>>8);
    for(i=0;i<=n;i++){b[4+i*2]=(u8)p[i];b[5+i*2]=(u8)(p[i]>>8);}b[node]=0x7f;b[node+1]=0xff;b[node+2]=4;b[node+3]=0;
}
static int map_contains36(EFI_PHYSICAL_ADDRESS addr,u32 type)
{
    UINTN sz=0,key=0,ds=0,i,n;u32 v=0;EFI_MEMORY_DESCRIPTOR*m;
    if(get_map36(&sz,0,&key,&ds,&v)!=EFI_BUFFER_TOO_SMALL||!sz)return 0;m=(EFI_MEMORY_DESCRIPTOR*)mmap(0,(size_t)sz,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(m==(EFI_MEMORY_DESCRIPTOR*)-1)return 0;
    if(get_map36(&sz,m,&key,&ds,&v)!=EFI_SUCCESS){munmap(m,(size_t)sz);return 0;}n=sz/ds;for(i=0;i<n;i++){EFI_MEMORY_DESCRIPTOR*d=(EFI_MEMORY_DESCRIPTOR*)((u8*)m+i*ds);if(d->PhysicalStart==addr&&d->Type==type){munmap(m,(size_t)sz);return 1;}}munmap(m,(size_t)sz);return 0;
}

int main(void)
{
    UINTN sz=0,key=0,ds=0;u32 ver=0;EFI_PHYSICAL_ADDRESS page=0,imgaddr=0;void*pool=0;EFI_HANDLE h=0;EFI_DEVICE_PATH_PROTOCOL_32*dp=&end_path32;EFI_GUID**plist=0;UINTN pc=0;void*oi=0;UINTN oc=99;
    static EFI_GUID dummy_guid={0x36335350U,0x4c57U,0x3633U,{0x80,0,0,0,0,0,0,1}};u64 dummy_table=0x3636ULL;
    u8*file=0,*image=0;size_t file_size=0;struct pe_info pe;UINTN pages;void*entry;

    notify("PS4 Windows Loader: Stage 3.6 started");build_efi_shim();if(build32()!=0){notify("Stage 3.6: EFI environment build FAILED");return 1;}install_stage35_services();install_stage36_services();
    notify("Stage 3.6: Stage 3.5 firmware + extended Boot Services installed");

    if(get_map36(&sz,0,&key,&ds,&ver)!=EFI_BUFFER_TOO_SMALL||sz<sizeof(EFI_MEMORY_DESCRIPTOR)||alloc_pages36(EFI_ALLOCATE_ANY_PAGES,EFI_LOADER_DATA,2,&page)!=EFI_SUCCESS||alloc_pool36(EFI_BOOT_SERVICES_DATA,100,&pool)!=EFI_SUCCESS){notify("Stage 3.6: tracked memory setup FAILED");return 1;}
    sz=0;if(get_map36(&sz,0,&key,&ds,&ver)!=EFI_BUFFER_TOO_SMALL||sz<3*sizeof(EFI_MEMORY_DESCRIPTOR)||!map_contains36(page,EFI_LOADER_DATA)){free_pool36(pool);free_pages36(page,2);notify("Stage 3.6: tracked GetMemoryMap FAILED");return 1;}free_pool36(pool);free_pages36(page,2);notify("Stage 3.6: tracked EFI allocation memory map OK");

    sz=0;if(((locate_handle36_fn)bs32.LocateHandle)(EFI_LOCATE_BY_PROTOCOL,(EFI_GUID*)&simple_fs_guid32,0,&sz,0)!=EFI_BUFFER_TOO_SMALL||sz!=sizeof(EFI_HANDLE)){notify("Stage 3.6: LocateHandle sizing FAILED");return 1;}
    if(((locate_handle36_fn)bs32.LocateHandle)(EFI_LOCATE_BY_PROTOCOL,(EFI_GUID*)&simple_fs_guid32,0,&sz,&h)!=EFI_SUCCESS||h!=(EFI_HANDLE)&device_handle32||((locate_device_path36_fn)bs32.LocateDevicePath)((EFI_GUID*)&simple_fs_guid32,&dp,&h)!=EFI_SUCCESS){notify("Stage 3.6: LocateHandle/LocateDevicePath FAILED");return 1;}
    if(protocols_per_handle36((EFI_HANDLE)&device_handle32,&plist,&pc)!=EFI_SUCCESS||pc!=2||!plist){notify("Stage 3.6: protocol enumeration FAILED");return 1;}free_pool36(plist);
    if(((open_info36_fn)bs32.OpenProtocolInformation)((EFI_HANDLE)&device_handle32,(EFI_GUID*)&simple_fs_guid32,&oi,&oc)!=EFI_SUCCESS||oi||oc!=0){notify("Stage 3.6: OpenProtocolInformation FAILED");return 1;}notify("Stage 3.6: protocol discovery extensions OK");

    if(((install_config36_fn)bs32.InstallConfigurationTable)(&dummy_guid,&dummy_table)!=EFI_SUCCESS||g_system_table.NumberOfTableEntries!=1||g_system_table.ConfigurationTable!=config36||((install_config36_fn)bs32.InstallConfigurationTable)(&dummy_guid,0)!=EFI_SUCCESS||g_system_table.NumberOfTableEntries!=0){notify("Stage 3.6: ConfigurationTable semantics FAILED");return 1;}notify("Stage 3.6: ConfigurationTable semantics OK");

    if(load_bootmgfw(&file,&file_size)!=0||inspect_pe(file,file_size,&pe)!=0){if(file)munmap(file,(size_t)READ_CAP);notify("Stage 3.6: bootmgfw read/PE validation FAILED");return 1;}pages=(pe.image_size+4095U)/4096U;
    if(alloc_pages36(EFI_ALLOCATE_ANY_PAGES,EFI_LOADER_CODE,pages,&imgaddr)!=EFI_SUCCESS){munmap(file,(size_t)READ_CAP);notify("Stage 3.6: bootmgfw AllocatePages FAILED");return 1;}image=(u8*)(unsigned long)imgaddr;
    if(map_sections36(file,file_size,&pe,image)!=0||apply_relocs(image,&pe)!=0){free_pages36(imgaddr,pages);munmap(file,(size_t)READ_CAP);notify("Stage 3.6: corrected PE map/relocations FAILED");return 1;}entry=(void*)(image+pe.entry_rva);if(!entry||!map_contains36(imgaddr,EFI_LOADER_CODE)){free_pages36(imgaddr,pages);munmap(file,(size_t)READ_CAP);notify("Stage 3.6: executable image memory descriptor FAILED");return 1;}
    notify("Stage 3.6: corrected bootmgfw PE mapping + memory descriptor OK");

    build_loaded_path36();loaded32.SystemTable=&g_system_table;loaded32.DeviceHandle=(EFI_HANDLE)&device_handle32;loaded32.FilePath=(EFI_DEVICE_PATH_PROTOCOL_32*)loaded_path36.bytes;loaded32.ImageBase=image;loaded32.ImageSize=pe.image_size;loaded32.ImageCodeType=EFI_LOADER_CODE;loaded32.ImageDataType=EFI_LOADER_DATA;
    if(loaded32.ImageBase!=image||loaded32.ImageSize!=pe.image_size||loaded32.FilePath!=(EFI_DEVICE_PATH_PROTOCOL_32*)loaded_path36.bytes){free_pages36(imgaddr,pages);munmap(file,(size_t)READ_CAP);notify("Stage 3.6: LoadedImage metadata FAILED");return 1;}
    notify("Stage 3.6: LoadedImage bootmgfw metadata + FilePath OK");
    notify("Stage 3.6: bootmgfw entry preflight ready - Microsoft code NOT called yet");
    notify("PS4 Windows Loader: Stage 3.6 final pre-entry self-test OK");

    free_pages36(imgaddr,pages);munmap(file,(size_t)READ_CAP);munmap(arena32,(size_t)(arena_pages36*EFI_PAGE_SIZE));return 0;
}
