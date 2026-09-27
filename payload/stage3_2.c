#define main stage3_base_main
#include "stage3.c"
#undef main

#define EFIERR(x) (0x8000000000000000ULL | (x))
#define EFI_INVALID_PARAMETER EFIERR(2)
#define EFI_UNSUPPORTED EFIERR(3)
#define EFI_BUFFER_TOO_SMALL EFIERR(5)
#define EFI_DEVICE_ERROR EFIERR(7)
#define EFI_OUT_OF_RESOURCES EFIERR(9)
#define EFI_NOT_FOUND EFIERR(14)
#define EFI_ACCESS_DENIED EFIERR(15)
#define EFI_PAGE_SIZE 4096ULL
#define EFI_ALLOCATE_ANY_PAGES 0U
#define EFI_BOOT_SERVICES_DATA 4U
#define EFI_CONVENTIONAL_MEMORY 7U
#define EFI_FILE_MODE_READ 0x0000000000000001ULL

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
    void *RaiseTPL,*RestoreTPL;
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
} EFI_BOOT_SERVICES_32;

typedef struct { u8 Type,SubType,Length[2]; } EFI_DEVICE_PATH_PROTOCOL_32;
typedef struct efi_file_protocol_32 EFI_FILE_PROTOCOL_32;

typedef EFI_STATUS (EFIAPI *efi_file_open_fn)(EFI_FILE_PROTOCOL_32*,EFI_FILE_PROTOCOL_32**,CHAR16*,u64,u64);
typedef EFI_STATUS (EFIAPI *efi_file_close_fn)(EFI_FILE_PROTOCOL_32*);
typedef EFI_STATUS (EFIAPI *efi_file_read_fn)(EFI_FILE_PROTOCOL_32*,UINTN*,void*);
typedef EFI_STATUS (EFIAPI *efi_file_get_position_fn)(EFI_FILE_PROTOCOL_32*,u64*);
typedef EFI_STATUS (EFIAPI *efi_file_set_position_fn)(EFI_FILE_PROTOCOL_32*,u64);

struct efi_file_protocol_32 {
    u64 Revision;
    efi_file_open_fn Open;
    efi_file_close_fn Close;
    void *Delete;
    efi_file_read_fn Read;
    void *Write;
    efi_file_get_position_fn GetPosition;
    efi_file_set_position_fn SetPosition;
    void *GetInfo;
    void *SetInfo;
    void *Flush;
    void *OpenEx;
    void *ReadEx;
    void *WriteEx;
    void *FlushEx;
};

typedef EFI_STATUS (EFIAPI *efi_open_volume_fn)(void*,EFI_FILE_PROTOCOL_32**);
typedef struct { u64 Revision; efi_open_volume_fn OpenVolume; } EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_32;

typedef struct {
    u32 Revision; EFI_HANDLE ParentHandle; EFI_SYSTEM_TABLE *SystemTable; EFI_HANDLE DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL_32 *FilePath; void *Reserved; u32 LoadOptionsSize; void *LoadOptions;
    void *ImageBase; u64 ImageSize; u32 ImageCodeType; u32 ImageDataType; void *Unload;
} EFI_LOADED_IMAGE_PROTOCOL_32;

typedef struct {
    EFI_FILE_PROTOCOL_32 proto;
    int fd;
    u64 position;
    int is_root;
    char path[384];
} FILE_HANDLE_32;

static EFI_BOOT_SERVICES_32 bs32;
static EFI_DEVICE_PATH_PROTOCOL_32 end_path32;
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_32 fs32;
static EFI_LOADED_IMAGE_PROTOCOL_32 loaded32;
static FILE_HANDLE_32 root32;
static u64 image_handle32=0x3233474d49345350ULL;
static u64 device_handle32=0x3233564544345350ULL;
static u64 map_key32=1;
static void *arena32;

static const EFI_GUID loaded_guid32={0x5b1b31a1U,0x9562U,0x11d2U,{0x8e,0x3f,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static const EFI_GUID device_path_guid32={0x09576e91U,0x6d3fU,0x11d2U,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static const EFI_GUID simple_fs_guid32={0x964e5b22U,0x6459U,0x11d2U,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};

static int mem_eq32(const void *a,const void *b,size_t n){const u8*x=(const u8*)a,*y=(const u8*)b;while(n--){if(*x++!=*y++)return 0;}return 1;}
static int guid_eq32(const EFI_GUID *a,const EFI_GUID *b){return a&&b&&mem_eq32(a,b,sizeof(EFI_GUID));}
static size_t cstrlen32(const char*s){size_t n=0;while(s&&s[n])++n;return n;}
static void ccopy32(char*d,const char*s,size_t cap){size_t i=0;if(!cap)return;while(s&&s[i]&&i+1<cap){d[i]=s[i];++i;}d[i]=0;}

struct pool_header32{u64 magic,size;};
#define POOL_MAGIC32 0x32334c4f4f505350ULL

static EFI_STATUS EFIAPI alloc_pages32(u32 type,u32 memtype,UINTN pages,EFI_PHYSICAL_ADDRESS*out){void*p;(void)memtype;if(!out||!pages)return EFI_INVALID_PARAMETER;if(type!=EFI_ALLOCATE_ANY_PAGES)return EFI_UNSUPPORTED;p=mmap(0,(size_t)(pages*EFI_PAGE_SIZE),PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(p==(void*)-1)return EFI_OUT_OF_RESOURCES;*out=(u64)(unsigned long)p;++map_key32;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI free_pages32(EFI_PHYSICAL_ADDRESS addr,UINTN pages){if(!addr||!pages)return EFI_INVALID_PARAMETER;if(munmap((void*)(unsigned long)addr,(size_t)(pages*EFI_PAGE_SIZE))!=0)return EFI_INVALID_PARAMETER;++map_key32;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI alloc_pool32(u32 type,UINTN size,void**out){struct pool_header32*h;size_t total=(size_t)size+sizeof(*h),map=(total+4095U)&~4095U;(void)type;if(!out||!size)return EFI_INVALID_PARAMETER;h=(struct pool_header32*)mmap(0,map,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(h==(struct pool_header32*)-1)return EFI_OUT_OF_RESOURCES;h->magic=POOL_MAGIC32;h->size=(u64)map;*out=(void*)(h+1);++map_key32;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI free_pool32(void*p){struct pool_header32*h;if(!p)return EFI_INVALID_PARAMETER;h=((struct pool_header32*)p)-1;if(h->magic!=POOL_MAGIC32)return EFI_INVALID_PARAMETER;if(munmap(h,(size_t)h->size)!=0)return EFI_INVALID_PARAMETER;++map_key32;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI get_map32(UINTN*size,EFI_MEMORY_DESCRIPTOR*map,UINTN*key,UINTN*desc_size,u32*ver){EFI_MEMORY_DESCRIPTOR d;if(!size||!key||!desc_size||!ver)return EFI_INVALID_PARAMETER;*desc_size=sizeof(EFI_MEMORY_DESCRIPTOR);*ver=1;*key=map_key32;if(!map||*size<sizeof(EFI_MEMORY_DESCRIPTOR)){*size=sizeof(EFI_MEMORY_DESCRIPTOR);return EFI_BUFFER_TOO_SMALL;}mem_zero(&d,sizeof(d));d.Type=EFI_CONVENTIONAL_MEMORY;d.PhysicalStart=(u64)(unsigned long)arena32;d.NumberOfPages=4096;mem_copy(map,&d,sizeof(d));*size=sizeof(d);return EFI_SUCCESS;}

static EFI_STATUS protocol32(EFI_HANDLE h,EFI_GUID*g,void**out){if(!g||!out)return EFI_INVALID_PARAMETER;if(h==(EFI_HANDLE)&image_handle32&&guid_eq32(g,&loaded_guid32)){*out=&loaded32;return EFI_SUCCESS;}if(h==(EFI_HANDLE)&device_handle32&&guid_eq32(g,&device_path_guid32)){*out=&end_path32;return EFI_SUCCESS;}if(h==(EFI_HANDLE)&device_handle32&&guid_eq32(g,&simple_fs_guid32)){*out=&fs32;return EFI_SUCCESS;}return EFI_NOT_FOUND;}
static EFI_STATUS EFIAPI handle32(EFI_HANDLE h,EFI_GUID*g,void**out){return protocol32(h,g,out);}
static EFI_STATUS EFIAPI locate32(EFI_GUID*g,void*reg,void**out){(void)reg;if(!out)return EFI_INVALID_PARAMETER;if(guid_eq32(g,&loaded_guid32)){*out=&loaded32;return EFI_SUCCESS;}if(guid_eq32(g,&device_path_guid32)){*out=&end_path32;return EFI_SUCCESS;}if(guid_eq32(g,&simple_fs_guid32)){*out=&fs32;return EFI_SUCCESS;}return EFI_NOT_FOUND;}

static int make_orbis_path32(CHAR16*name,char*out,size_t cap){const char*prefix="/mnt/usb0";size_t p=0,i=0,j;if(!name||!out||cap<16)return-1;for(j=0;prefix[j]&&p+1<cap;j++)out[p++]=prefix[j];if(name[0]!=0&&p+1<cap)out[p++]='/';while(name[i]){u16 c=name[i++];if(c>127U)return-1;if(c=='\\')c='/';if(c==':')return-1;if(c=='.'&&name[i]=='.')return-1;if(p+1>=cap)return-1;out[p++]=(char)c;}out[p]=0;return 0;}

static EFI_STATUS EFIAPI file_close32(EFI_FILE_PROTOCOL_32*proto){FILE_HANDLE_32*h=(FILE_HANDLE_32*)proto;if(!h)return EFI_INVALID_PARAMETER;if(h->is_root)return EFI_SUCCESS;if(h->fd>=0)close(h->fd);munmap(h,4096);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI file_read32(EFI_FILE_PROTOCOL_32*proto,UINTN*size,void*buf){FILE_HANDLE_32*h=(FILE_HANDLE_32*)proto;ssize_t got;if(!h||!size||(!buf&&*size))return EFI_INVALID_PARAMETER;if(h->is_root)return EFI_UNSUPPORTED;got=read(h->fd,buf,(size_t)*size);if(got<0)return EFI_DEVICE_ERROR;*size=(UINTN)got;h->position+=(u64)got;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI file_get_position32(EFI_FILE_PROTOCOL_32*proto,u64*pos){FILE_HANDLE_32*h=(FILE_HANDLE_32*)proto;if(!h||!pos)return EFI_INVALID_PARAMETER;if(h->is_root)return EFI_UNSUPPORTED;*pos=h->position;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI file_set_position32(EFI_FILE_PROTOCOL_32*proto,u64 target){FILE_HANDLE_32*h=(FILE_HANDLE_32*)proto;u8 scratch[512];if(!h)return EFI_INVALID_PARAMETER;if(h->is_root)return EFI_UNSUPPORTED;if(target==0xffffffffffffffffULL)return EFI_UNSUPPORTED;if(target<h->position){close(h->fd);h->fd=open(h->path,O_RDONLY);if(h->fd<0)return EFI_DEVICE_ERROR;h->position=0;}while(h->position<target){u64 left=target-h->position;size_t want=left>sizeof(scratch)?sizeof(scratch):(size_t)left;ssize_t got=read(h->fd,scratch,want);if(got<=0)return EFI_DEVICE_ERROR;h->position+=(u64)got;}return EFI_SUCCESS;}

static EFI_STATUS EFIAPI file_open32(EFI_FILE_PROTOCOL_32*proto,EFI_FILE_PROTOCOL_32**new_handle,CHAR16*name,u64 mode,u64 attrs){FILE_HANDLE_32*parent=(FILE_HANDLE_32*)proto,*h;char path[384];int fd;(void)attrs;if(!parent||!new_handle||!name)return EFI_INVALID_PARAMETER;if((mode&EFI_FILE_MODE_READ)==0||mode!=EFI_FILE_MODE_READ)return EFI_ACCESS_DENIED;if(make_orbis_path32(name,path,sizeof(path))!=0)return EFI_INVALID_PARAMETER;fd=open(path,O_RDONLY);if(fd<0)return EFI_NOT_FOUND;h=(FILE_HANDLE_32*)mmap(0,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(h==(FILE_HANDLE_32*)-1){close(fd);return EFI_OUT_OF_RESOURCES;}mem_zero(h,4096);h->fd=fd;h->position=0;h->is_root=0;ccopy32(h->path,path,sizeof(h->path));h->proto.Revision=0x00010000ULL;h->proto.Open=file_open32;h->proto.Close=file_close32;h->proto.Read=file_read32;h->proto.GetPosition=file_get_position32;h->proto.SetPosition=file_set_position32;*new_handle=&h->proto;return EFI_SUCCESS;}

static EFI_STATUS EFIAPI open_volume32(void*self,EFI_FILE_PROTOCOL_32**root){(void)self;if(!root)return EFI_INVALID_PARAMETER;*root=&root32.proto;return EFI_SUCCESS;}

static int build32(void){mem_zero(&bs32,sizeof(bs32));mem_zero(&loaded32,sizeof(loaded32));mem_zero(&end_path32,sizeof(end_path32));mem_zero(&fs32,sizeof(fs32));mem_zero(&root32,sizeof(root32));arena32=mmap(0,16U*1024U*1024U,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(arena32==(void*)-1)return-1;bs32.Hdr.Signature=EFI_BOOT_SERVICES_SIGNATURE;bs32.Hdr.Revision=EFI_REVISION_2_0;bs32.Hdr.HeaderSize=(u32)sizeof(bs32);bs32.AllocatePages=alloc_pages32;bs32.FreePages=free_pages32;bs32.GetMemoryMap=get_map32;bs32.AllocatePool=alloc_pool32;bs32.FreePool=free_pool32;bs32.HandleProtocol=handle32;bs32.LocateProtocol=locate32;bs32.Hdr.CRC32=0;bs32.Hdr.CRC32=crc32_bytes(&bs32,sizeof(bs32));end_path32.Type=0x7f;end_path32.SubType=0xff;end_path32.Length[0]=4;fs32.Revision=0x00010000ULL;fs32.OpenVolume=open_volume32;root32.fd=-1;root32.is_root=1;root32.proto.Revision=0x00010000ULL;root32.proto.Open=file_open32;root32.proto.Close=file_close32;root32.proto.Read=file_read32;root32.proto.GetPosition=file_get_position32;root32.proto.SetPosition=file_set_position32;loaded32.Revision=0x1000U;loaded32.SystemTable=&g_system_table;loaded32.DeviceHandle=(EFI_HANDLE)&device_handle32;loaded32.FilePath=&end_path32;g_system_table.BootServices=&bs32;return 0;}

#ifndef STAGE3_2_NO_MAIN
int main(void){void*iface=0;EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_32*fs;EFI_FILE_PROTOCOL_32*root=0,*file=0;UINTN n;u8 sig[2];u64 pos=0;static CHAR16 boot_path[]={'\\','E','F','I','\\','M','i','c','r','o','s','o','f','t','\\','B','o','o','t','\\','b','o','o','t','m','g','f','w','.','e','f','i',0};notify("PS4 Windows Loader: Stage 3.2 started");build_efi_shim();if(build32()!=0){notify("Stage 3.2: EFI environment build FAILED");return 1;}notify("Stage 3.2: Boot Services + SimpleFS constructed");if(bs32.LocateProtocol((EFI_GUID*)&simple_fs_guid32,0,&iface)!=EFI_SUCCESS||iface!=&fs32){notify("Stage 3.2: LocateProtocol(SimpleFS) FAILED");return 1;}notify("Stage 3.2: LocateProtocol(SimpleFS) OK");fs=(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_32*)iface;if(fs->OpenVolume(fs,&root)!=EFI_SUCCESS||!root){notify("Stage 3.2: OpenVolume FAILED");return 1;}notify("Stage 3.2: OpenVolume OK");if(root->Open(root,&file,boot_path,EFI_FILE_MODE_READ,0)!=EFI_SUCCESS||!file){notify("Stage 3.2: EFI File Open FAILED");return 1;}notify("Stage 3.2: EFI File Open bootmgfw.efi OK");n=2;if(file->Read(file,&n,sig)!=EFI_SUCCESS||n!=2||sig[0]!='M'||sig[1]!='Z'){file->Close(file);notify("Stage 3.2: EFI File Read FAILED");return 1;}notify("Stage 3.2: EFI File Read returned MZ");if(file->GetPosition(file,&pos)!=EFI_SUCCESS||pos!=2){file->Close(file);notify("Stage 3.2: GetPosition FAILED");return 1;}if(file->SetPosition(file,0)!=EFI_SUCCESS){file->Close(file);notify("Stage 3.2: SetPosition FAILED");return 1;}n=2;sig[0]=sig[1]=0;if(file->Read(file,&n,sig)!=EFI_SUCCESS||n!=2||sig[0]!='M'||sig[1]!='Z'){file->Close(file);notify("Stage 3.2: rewind/read FAILED");return 1;}notify("Stage 3.2: GetPosition/SetPosition OK");if(file->Close(file)!=EFI_SUCCESS){notify("Stage 3.2: Close FAILED");return 1;}notify("Stage 3.2: EFI File Close OK");notify("PS4 Windows Loader: Stage 3.2 EFI file bridge self-test OK");munmap(arena32,16U*1024U*1024U);return 0;}
#endif
