#include <time.h>
#define STAGE3_2_NO_MAIN 1
#include "stage3_2.c"

#define EFI_NOT_READY       EFIERR(6)
#define EFI_WRITE_PROTECTED EFIERR(8)
#define EFI_ABORTED         EFIERR(21)
#define EFI_RUNTIME_SERVICES_SIGNATURE 0x56524553544e5552ULL
#define EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL 0x00000001U
#define EFI_OPEN_PROTOCOL_GET_PROTOCOL       0x00000002U
#define EFI_OPEN_PROTOCOL_TEST_PROTOCOL      0x00000004U
#define EFI_LOCATE_BY_PROTOCOL 2U
#define TIMER_CANCEL   0U
#define TIMER_PERIODIC 1U
#define TIMER_RELATIVE 2U
#define EVENT_MAGIC35 0x3533544e45564550ULL

typedef short s16_35;
typedef void *EFI_EVENT_35;
typedef UINTN EFI_TPL_35;

typedef struct {
    u16 Year; u8 Month; u8 Day; u8 Hour; u8 Minute; u8 Second; u8 Pad1;
    u32 Nanosecond; s16_35 TimeZone; u8 Daylight; u8 Pad2;
} EFI_TIME_35;

typedef struct { u32 Resolution; u32 Accuracy; u8 SetsToZero; } EFI_TIME_CAPABILITIES_35;

typedef struct {
    u64 Size;
    u64 FileSize;
    u64 PhysicalSize;
    EFI_TIME_35 CreateTime;
    EFI_TIME_35 LastAccessTime;
    EFI_TIME_35 ModificationTime;
    u64 Attribute;
    CHAR16 FileName[1];
} EFI_FILE_INFO_35;

typedef void (EFIAPI *event_notify35_fn)(EFI_EVENT_35, void *);
typedef EFI_STATUS (EFIAPI *create_event35_fn)(u32, EFI_TPL_35, event_notify35_fn, void *, EFI_EVENT_35 *);
typedef EFI_STATUS (EFIAPI *set_timer35_fn)(EFI_EVENT_35, u32, u64);
typedef EFI_STATUS (EFIAPI *wait_for_event35_fn)(UINTN, EFI_EVENT_35 *, UINTN *);
typedef EFI_STATUS (EFIAPI *signal_event35_fn)(EFI_EVENT_35);
typedef EFI_STATUS (EFIAPI *close_event35_fn)(EFI_EVENT_35);
typedef EFI_STATUS (EFIAPI *check_event35_fn)(EFI_EVENT_35);
typedef EFI_STATUS (EFIAPI *create_event_ex35_fn)(u32, EFI_TPL_35, event_notify35_fn, const void *, const EFI_GUID *, EFI_EVENT_35 *);
typedef EFI_TPL_35 (EFIAPI *raise_tpl35_fn)(EFI_TPL_35);
typedef void (EFIAPI *restore_tpl35_fn)(EFI_TPL_35);
typedef EFI_STATUS (EFIAPI *exit_boot_services35_fn)(EFI_HANDLE, UINTN);
typedef EFI_STATUS (EFIAPI *monotonic35_fn)(u64 *);
typedef EFI_STATUS (EFIAPI *watchdog35_fn)(UINTN, u64, UINTN, CHAR16 *);

typedef EFI_STATUS (EFIAPI *get_time35_fn)(EFI_TIME_35 *, EFI_TIME_CAPABILITIES_35 *);
typedef EFI_STATUS (EFIAPI *set_time35_fn)(EFI_TIME_35 *);
typedef EFI_STATUS (EFIAPI *get_wakeup35_fn)(u8 *, u8 *, EFI_TIME_35 *);
typedef EFI_STATUS (EFIAPI *set_wakeup35_fn)(u8, EFI_TIME_35 *);
typedef EFI_STATUS (EFIAPI *set_virtual_map35_fn)(UINTN, UINTN, u32, EFI_MEMORY_DESCRIPTOR *);
typedef EFI_STATUS (EFIAPI *convert_pointer35_fn)(UINTN, void **);
typedef EFI_STATUS (EFIAPI *get_variable35_fn)(CHAR16 *, EFI_GUID *, u32 *, UINTN *, void *);
typedef EFI_STATUS (EFIAPI *get_next_variable35_fn)(UINTN *, CHAR16 *, EFI_GUID *);
typedef EFI_STATUS (EFIAPI *set_variable35_fn)(CHAR16 *, EFI_GUID *, u32, UINTN, void *);
typedef EFI_STATUS (EFIAPI *get_next_high_monotonic35_fn)(u32 *);
typedef void (EFIAPI *reset_system35_fn)(u32, EFI_STATUS, UINTN, void *);
typedef EFI_STATUS (EFIAPI *update_capsule35_fn)(void **, UINTN, EFI_PHYSICAL_ADDRESS);
typedef EFI_STATUS (EFIAPI *query_capsule35_fn)(void **, UINTN, u64 *, u32 *);
typedef EFI_STATUS (EFIAPI *query_variable_info35_fn)(u32, u64 *, u64 *, u64 *);

typedef EFI_STATUS (EFIAPI *file_get_info35_fn)(EFI_FILE_PROTOCOL_32 *, EFI_GUID *, UINTN *, void *);
typedef EFI_STATUS (EFIAPI *open_protocol35_fn)(EFI_HANDLE, EFI_GUID *, void **, EFI_HANDLE, EFI_HANDLE, u32);
typedef EFI_STATUS (EFIAPI *close_protocol35_fn)(EFI_HANDLE, EFI_GUID *, EFI_HANDLE, EFI_HANDLE);
typedef EFI_STATUS (EFIAPI *locate_handle_buffer35_fn)(u32, EFI_GUID *, void *, UINTN *, EFI_HANDLE **);
typedef EFI_STATUS (EFIAPI *protocols_per_handle35_fn)(EFI_HANDLE, EFI_GUID ***, UINTN *);
typedef EFI_STATUS (EFIAPI *calculate_crc32_35_fn)(void *, UINTN, u32 *);
typedef void (EFIAPI *copy_mem35_fn)(void *, void *, UINTN);
typedef void (EFIAPI *set_mem35_fn)(void *, UINTN, u8);
typedef EFI_STATUS (EFIAPI *stall35_fn)(UINTN);

typedef struct {
    EFI_TABLE_HEADER Hdr;
    get_time35_fn GetTime;
    set_time35_fn SetTime;
    get_wakeup35_fn GetWakeupTime;
    set_wakeup35_fn SetWakeupTime;
    set_virtual_map35_fn SetVirtualAddressMap;
    convert_pointer35_fn ConvertPointer;
    get_variable35_fn GetVariable;
    get_next_variable35_fn GetNextVariableName;
    set_variable35_fn SetVariable;
    get_next_high_monotonic35_fn GetNextHighMonotonicCount;
    reset_system35_fn ResetSystem;
    update_capsule35_fn UpdateCapsule;
    query_capsule35_fn QueryCapsuleCapabilities;
    query_variable_info35_fn QueryVariableInfo;
} EFI_RUNTIME_SERVICES_35;

typedef struct {
    u64 magic;
    int signaled;
    int timer_mode;
    u64 trigger_time_100ns;
    event_notify35_fn notify_fn;
    void *notify_ctx;
} EVENT35;

static const EFI_GUID file_info_guid35={0x09576e92U,0x6d3fU,0x11d2U,{0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}};
static EFI_RUNTIME_SERVICES_35 rt35;
static EFI_TPL_35 current_tpl35 = 4;
static u64 monotonic35 = 1;
static int boot_services_exited35 = 0;

static int guid_eq35(const EFI_GUID *a,const EFI_GUID *b){return a&&b&&mem_eq32(a,b,sizeof(EFI_GUID));}
static const char *basename35(const char *p){const char*b=p;while(p&&*p){if(*p=='/')b=p+1;++p;}return b;}

static EFI_STATUS scan_file_size35(FILE_HANDLE_32 *h,u64 *out)
{
    int fd; u8 buf[4096]; u64 total=0; ssize_t n;
    if(!h||!out||h->is_root)return EFI_INVALID_PARAMETER;
    fd=open(h->path,O_RDONLY); if(fd<0)return EFI_NOT_FOUND;
    for(;;){n=read(fd,buf,sizeof(buf));if(n<0){close(fd);return EFI_DEVICE_ERROR;}if(!n)break;total+=(u64)n;}
    close(fd);*out=total;return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI file_get_info35(EFI_FILE_PROTOCOL_32 *proto,EFI_GUID *type,UINTN *size,void *buffer)
{
    FILE_HANDLE_32*h=(FILE_HANDLE_32*)proto; EFI_FILE_INFO_35*info; const char*name; size_t len=0,i; UINTN need; u64 fsize=0; EFI_STATUS rc;
    if(!h||!type||!size)return EFI_INVALID_PARAMETER;
    if(!guid_eq35(type,&file_info_guid35))return EFI_UNSUPPORTED;
    if(h->is_root)return EFI_UNSUPPORTED;
    name=basename35(h->path);while(name[len])++len;
    need=(UINTN)offsetof(EFI_FILE_INFO_35,FileName)+(UINTN)((len+1)*sizeof(CHAR16));
    if(!buffer||*size<need){*size=need;return EFI_BUFFER_TOO_SMALL;}
    rc=scan_file_size35(h,&fsize);if(rc!=EFI_SUCCESS)return rc;
    mem_zero(buffer,(size_t)need);info=(EFI_FILE_INFO_35*)buffer;info->Size=need;info->FileSize=fsize;info->PhysicalSize=fsize;
    for(i=0;i<len;++i)info->FileName[i]=(CHAR16)(u8)name[i];info->FileName[len]=0;*size=need;return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI file_open35(EFI_FILE_PROTOCOL_32 *p,EFI_FILE_PROTOCOL_32 **n,CHAR16 *name,u64 mode,u64 attrs)
{
    EFI_STATUS rc=file_open32(p,n,name,mode,attrs);
    if(rc==EFI_SUCCESS&&n&&*n){(*n)->Open=file_open35;(*n)->GetInfo=(void*)file_get_info35;}
    return rc;
}

static EFI_STATUS EFIAPI open_protocol35(EFI_HANDLE h,EFI_GUID*g,void**iface,EFI_HANDLE agent,EFI_HANDLE controller,u32 attr)
{
    void*found=0;EFI_STATUS rc;(void)agent;(void)controller;
    if(!h||!g)return EFI_INVALID_PARAMETER;
    if(attr!=EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL&&attr!=EFI_OPEN_PROTOCOL_GET_PROTOCOL&&attr!=EFI_OPEN_PROTOCOL_TEST_PROTOCOL)return EFI_UNSUPPORTED;
    if(attr!=EFI_OPEN_PROTOCOL_TEST_PROTOCOL&&!iface)return EFI_INVALID_PARAMETER;
    rc=protocol32(h,g,&found);if(rc!=EFI_SUCCESS)return rc;if(attr!=EFI_OPEN_PROTOCOL_TEST_PROTOCOL)*iface=found;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI close_protocol35(EFI_HANDLE h,EFI_GUID*g,EFI_HANDLE a,EFI_HANDLE c){void*x=0;(void)a;(void)c;if(!h||!g)return EFI_INVALID_PARAMETER;return protocol32(h,g,&x);}

static EFI_STATUS EFIAPI locate_handle_buffer35(u32 type,EFI_GUID*g,void*key,UINTN*count,EFI_HANDLE**buffer)
{
    EFI_HANDLE match;EFI_HANDLE*arr=0;EFI_STATUS rc;(void)key;
    if(!count||!buffer)return EFI_INVALID_PARAMETER;*count=0;*buffer=0;
    if(type!=EFI_LOCATE_BY_PROTOCOL||!g)return EFI_UNSUPPORTED;
    if(guid_eq35(g,&loaded_guid32))match=(EFI_HANDLE)&image_handle32;
    else if(guid_eq35(g,&device_path_guid32)||guid_eq35(g,&simple_fs_guid32))match=(EFI_HANDLE)&device_handle32;
    else return EFI_NOT_FOUND;
    rc=alloc_pool32(EFI_BOOT_SERVICES_DATA,sizeof(EFI_HANDLE),(void**)&arr);if(rc!=EFI_SUCCESS)return rc;arr[0]=match;*count=1;*buffer=arr;return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI protocols_per_handle35(EFI_HANDLE h,EFI_GUID***buffer,UINTN*count)
{
    EFI_GUID**arr=0;UINTN n;EFI_STATUS rc;if(!h||!buffer||!count)return EFI_INVALID_PARAMETER;*buffer=0;*count=0;
    if(h==(EFI_HANDLE)&image_handle32)n=1;else if(h==(EFI_HANDLE)&device_handle32)n=2;else return EFI_NOT_FOUND;
    rc=alloc_pool32(EFI_BOOT_SERVICES_DATA,n*sizeof(EFI_GUID*),(void**)&arr);if(rc!=EFI_SUCCESS)return rc;
    if(n==1)arr[0]=(EFI_GUID*)&loaded_guid32;else{arr[0]=(EFI_GUID*)&device_path_guid32;arr[1]=(EFI_GUID*)&simple_fs_guid32;}
    *buffer=arr;*count=n;return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI calculate_crc32_35(void*d,UINTN n,u32*out){if((!d&&n)||!out)return EFI_INVALID_PARAMETER;*out=crc32_bytes(d,(size_t)n);return EFI_SUCCESS;}
static void EFIAPI copy_mem35(void*d,void*s,UINTN n){if(n)mem_copy(d,s,(size_t)n);}
static void EFIAPI set_mem35(void*d,UINTN n,u8 v){u8*p=(u8*)d;UINTN i;for(i=0;i<n;++i)p[i]=v;}
static EFI_STATUS EFIAPI stall35(UINTN us){struct timespec ts;ts.tv_sec=(time_t)(us/1000000ULL);ts.tv_nsec=(long)((us%1000000ULL)*1000ULL);return nanosleep(&ts,0)==0?EFI_SUCCESS:EFI_DEVICE_ERROR;}

static void install_preboot35(void)
{
    root32.proto.Open=file_open35;
    bs32.OpenProtocol=(void*)open_protocol35;bs32.CloseProtocol=(void*)close_protocol35;
    bs32.LocateHandleBuffer=(void*)locate_handle_buffer35;bs32.ProtocolsPerHandle=(void*)protocols_per_handle35;
    bs32.CalculateCrc32=(void*)calculate_crc32_35;bs32.CopyMem=(void*)copy_mem35;bs32.SetMem=(void*)set_mem35;bs32.Stall=(void*)stall35;
}

static EVENT35 *event_ptr35(EFI_EVENT_35 event){EVENT35*e=(EVENT35*)event;if(!e||e->magic!=EVENT_MAGIC35)return 0;return e;}
static EFI_STATUS EFIAPI create_event35(u32 type,EFI_TPL_35 tpl,event_notify35_fn fn,void*ctx,EFI_EVENT_35*out)
{EVENT35*e;(void)type;(void)tpl;if(!out)return EFI_INVALID_PARAMETER;e=(EVENT35*)mmap(0,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(e==(EVENT35*)-1)return EFI_OUT_OF_RESOURCES;mem_zero(e,4096);e->magic=EVENT_MAGIC35;e->notify_fn=fn;e->notify_ctx=ctx;*out=e;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI set_timer35(EFI_EVENT_35 event,u32 type,u64 trigger){EVENT35*e=event_ptr35(event);if(!e)return EFI_INVALID_PARAMETER;if(type>TIMER_RELATIVE)return EFI_INVALID_PARAMETER;e->timer_mode=(int)type;e->trigger_time_100ns=trigger;if(type==TIMER_CANCEL)e->signaled=0;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI signal_event35(EFI_EVENT_35 event){EVENT35*e=event_ptr35(event);if(!e)return EFI_INVALID_PARAMETER;e->signaled=1;if(e->notify_fn)e->notify_fn(event,e->notify_ctx);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI check_event35(EFI_EVENT_35 event){EVENT35*e=event_ptr35(event);if(!e)return EFI_INVALID_PARAMETER;return e->signaled?EFI_SUCCESS:EFI_NOT_READY;}
static EFI_STATUS EFIAPI wait_for_event35(UINTN count,EFI_EVENT_35*events,UINTN*index){UINTN i;if(!count||!events||!index)return EFI_INVALID_PARAMETER;for(i=0;i<count;++i){EVENT35*e=event_ptr35(events[i]);if(!e)return EFI_INVALID_PARAMETER;if(e->signaled){*index=i;return EFI_SUCCESS;}}return EFI_NOT_READY;}
static EFI_STATUS EFIAPI close_event35(EFI_EVENT_35 event){EVENT35*e=event_ptr35(event);if(!e)return EFI_INVALID_PARAMETER;e->magic=0;return munmap(e,4096)==0?EFI_SUCCESS:EFI_DEVICE_ERROR;}
static EFI_STATUS EFIAPI create_event_ex35(u32 t,EFI_TPL_35 p,event_notify35_fn f,const void*c,const EFI_GUID*g,EFI_EVENT_35*e){(void)g;return create_event35(t,p,f,(void*)c,e);}
static EFI_TPL_35 EFIAPI raise_tpl35(EFI_TPL_35 n){EFI_TPL_35 o=current_tpl35;current_tpl35=n;return o;}
static void EFIAPI restore_tpl35(EFI_TPL_35 o){current_tpl35=o;}
static EFI_STATUS EFIAPI exit_boot_services35(EFI_HANDLE image,UINTN key){if(image!=(EFI_HANDLE)&image_handle32||key!=map_key32)return EFI_INVALID_PARAMETER;boot_services_exited35=1;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI get_next_monotonic35(u64*c){if(!c)return EFI_INVALID_PARAMETER;*c=monotonic35++;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI set_watchdog35(UINTN t,u64 c,UINTN n,CHAR16*d){(void)t;(void)c;(void)n;(void)d;return EFI_SUCCESS;}

static EFI_STATUS EFIAPI rt_get_time35(EFI_TIME_35*t,EFI_TIME_CAPABILITIES_35*c){(void)t;(void)c;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_set_time35(EFI_TIME_35*t){(void)t;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_get_wakeup35(u8*e,u8*p,EFI_TIME_35*t){(void)e;(void)p;(void)t;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_set_wakeup35(u8 e,EFI_TIME_35*t){(void)e;(void)t;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_set_virtual_map35(UINTN a,UINTN b,u32 c,EFI_MEMORY_DESCRIPTOR*d){(void)a;(void)b;(void)c;(void)d;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_convert_pointer35(UINTN d,void**a){(void)d;(void)a;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_get_variable35(CHAR16*n,EFI_GUID*v,u32*a,UINTN*s,void*d){(void)d;if(!n||!v||!s)return EFI_INVALID_PARAMETER;if(a)*a=0;return EFI_NOT_FOUND;}
static EFI_STATUS EFIAPI rt_get_next_variable35(UINTN*s,CHAR16*n,EFI_GUID*v){if(!s||!n||!v)return EFI_INVALID_PARAMETER;return EFI_NOT_FOUND;}
static EFI_STATUS EFIAPI rt_set_variable35(CHAR16*n,EFI_GUID*v,u32 a,UINTN s,void*d){(void)a;(void)s;(void)d;if(!n||!v)return EFI_INVALID_PARAMETER;return EFI_WRITE_PROTECTED;}
static EFI_STATUS EFIAPI rt_get_next_high_monotonic35(u32*h){if(!h)return EFI_INVALID_PARAMETER;*h=(u32)(monotonic35>>32);return EFI_SUCCESS;}
static void EFIAPI rt_reset_system35(u32 t,EFI_STATUS s,UINTN n,void*d){(void)t;(void)s;(void)n;(void)d;notify("Stage 3.5: ResetSystem requested");}
static EFI_STATUS EFIAPI rt_update_capsule35(void**c,UINTN n,EFI_PHYSICAL_ADDRESS s){(void)c;(void)n;(void)s;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_query_capsule35(void**c,UINTN n,u64*m,u32*r){(void)c;(void)n;(void)m;(void)r;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_query_variable_info35(u32 a,u64*m,u64*r,u64*v){(void)a;(void)m;(void)r;(void)v;return EFI_UNSUPPORTED;}

static void install_stage35_services(void)
{
    install_preboot35();
    mem_zero(&rt35,sizeof(rt35));rt35.Hdr.Signature=EFI_RUNTIME_SERVICES_SIGNATURE;rt35.Hdr.Revision=EFI_REVISION_2_0;rt35.Hdr.HeaderSize=(u32)sizeof(rt35);
    rt35.GetTime=rt_get_time35;rt35.SetTime=rt_set_time35;rt35.GetWakeupTime=rt_get_wakeup35;rt35.SetWakeupTime=rt_set_wakeup35;
    rt35.SetVirtualAddressMap=rt_set_virtual_map35;rt35.ConvertPointer=rt_convert_pointer35;rt35.GetVariable=rt_get_variable35;rt35.GetNextVariableName=rt_get_next_variable35;
    rt35.SetVariable=rt_set_variable35;rt35.GetNextHighMonotonicCount=rt_get_next_high_monotonic35;rt35.ResetSystem=rt_reset_system35;
    rt35.UpdateCapsule=rt_update_capsule35;rt35.QueryCapsuleCapabilities=rt_query_capsule35;rt35.QueryVariableInfo=rt_query_variable_info35;
    rt35.Hdr.CRC32=0;rt35.Hdr.CRC32=crc32_bytes(&rt35,sizeof(rt35));

    bs32.RaiseTPL=(void*)raise_tpl35;bs32.RestoreTPL=(void*)restore_tpl35;bs32.CreateEvent=(void*)create_event35;bs32.SetTimer=(void*)set_timer35;
    bs32.WaitForEvent=(void*)wait_for_event35;bs32.SignalEvent=(void*)signal_event35;bs32.CloseEvent=(void*)close_event35;bs32.CheckEvent=(void*)check_event35;
    bs32.ExitBootServices=(void*)exit_boot_services35;bs32.GetNextMonotonicCount=(void*)get_next_monotonic35;bs32.SetWatchdogTimer=(void*)set_watchdog35;bs32.CreateEventEx=(void*)create_event_ex35;
    bs32.Hdr.CRC32=0;bs32.Hdr.CRC32=crc32_bytes(&bs32,sizeof(bs32));
    g_system_table.RuntimeServices=&rt35;g_system_table.BootServices=&bs32;g_system_table.Hdr.CRC32=0;g_system_table.Hdr.CRC32=crc32_bytes(&g_system_table,g_system_table.Hdr.HeaderSize);
}

#ifndef STAGE3_5_NO_MAIN
int main(void)
{
    create_event35_fn create_event;signal_event35_fn signal_event;check_event35_fn check_event;wait_for_event35_fn wait_event;close_event35_fn close_event;exit_boot_services35_fn exit_bs;
    EFI_EVENT_35 event=0;UINTN index=99,map_size=0,key_before=0,desc_size=0;u32 version=0;void*pool=0;
    static CHAR16 variable_name[]={'S','e','c','u','r','e','B','o','o','t',0};
    static EFI_GUID global_variable_guid={0x8be4df61U,0x93caU,0x11d2U,{0xaa,0x0d,0x00,0xe0,0x98,0x03,0x2b,0x8c}};
    UINTN variable_size=1;u8 variable_data=0;

    notify("PS4 Windows Loader: Stage 3.5 started");build_efi_shim();if(build32()!=0){notify("Stage 3.5: EFI environment build FAILED");return 1;}install_stage35_services();
    notify("Stage 3.5: event + Runtime Services installed");
    if(g_system_table.RuntimeServices!=&rt35||g_system_table.BootServices!=&bs32||rt35.Hdr.Signature!=EFI_RUNTIME_SERVICES_SIGNATURE){notify("Stage 3.5: SystemTable/RuntimeServices FAILED");return 1;}
    notify("Stage 3.5: RuntimeServices table published");

    create_event=(create_event35_fn)bs32.CreateEvent;signal_event=(signal_event35_fn)bs32.SignalEvent;check_event=(check_event35_fn)bs32.CheckEvent;wait_event=(wait_for_event35_fn)bs32.WaitForEvent;close_event=(close_event35_fn)bs32.CloseEvent;
    if(!create_event||!signal_event||!check_event||!wait_event||!close_event||create_event(0,4,0,0,&event)!=EFI_SUCCESS||!event){notify("Stage 3.5: CreateEvent FAILED");return 1;}
    if(check_event(event)!=EFI_NOT_READY||signal_event(event)!=EFI_SUCCESS||check_event(event)!=EFI_SUCCESS||wait_event(1,&event,&index)!=EFI_SUCCESS||index!=0){close_event(event);notify("Stage 3.5: event signal/wait semantics FAILED");return 1;}
    if(close_event(event)!=EFI_SUCCESS){notify("Stage 3.5: CloseEvent FAILED");return 1;}
    notify("Stage 3.5: Create/Signal/Wait/Check/CloseEvent OK");

    if(rt35.GetVariable(variable_name,&global_variable_guid,0,&variable_size,&variable_data)!=EFI_NOT_FOUND||rt35.SetVariable(variable_name,&global_variable_guid,0,0,0)!=EFI_WRITE_PROTECTED){notify("Stage 3.5: UEFI variable semantics FAILED");return 1;}
    notify("Stage 3.5: Runtime variable fallback semantics OK");

    if(bs32.GetMemoryMap(&map_size,0,&key_before,&desc_size,&version)!=EFI_BUFFER_TOO_SMALL||!key_before){notify("Stage 3.5: initial MapKey FAILED");return 1;}
    if(bs32.AllocatePool(EFI_BOOT_SERVICES_DATA,64,&pool)!=EFI_SUCCESS||!pool){notify("Stage 3.5: MapKey allocation test FAILED");return 1;}
    map_size=0;
    {
        UINTN key_after=0;
        if(bs32.GetMemoryMap(&map_size,0,&key_after,&desc_size,&version)!=EFI_BUFFER_TOO_SMALL||key_after==key_before){bs32.FreePool(pool);notify("Stage 3.5: MapKey did not change");return 1;}
        bs32.FreePool(pool);notify("Stage 3.5: GetMemoryMap MapKey tracking OK");
        exit_bs=(exit_boot_services35_fn)bs32.ExitBootServices;map_size=0;
        if(!exit_bs||bs32.GetMemoryMap(&map_size,0,&key_after,&desc_size,&version)!=EFI_BUFFER_TOO_SMALL||exit_bs((EFI_HANDLE)&image_handle32,key_after)!=EFI_SUCCESS||!boot_services_exited35){notify("Stage 3.5: ExitBootServices key validation FAILED");return 1;}
    }
    notify("Stage 3.5: ExitBootServices MapKey validation OK");
    notify("PS4 Windows Loader: Stage 3.5 pre-entry firmware self-test OK");
    munmap(arena32,16U*1024U*1024U);return 0;
}
#endif
