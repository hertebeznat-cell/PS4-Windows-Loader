#define main stage3_4_base_main
#include "stage3_4.c"
#undef main

#define EFI_NOT_READY       EFIERR(6)
#define EFI_WRITE_PROTECTED EFIERR(8)
#define EFI_ABORTED         EFIERR(21)
#define EFI_RUNTIME_SERVICES_SIGNATURE 0x56524553544e5552ULL
#define TIMER_CANCEL   0U
#define TIMER_PERIODIC 1U
#define TIMER_RELATIVE 2U
#define EVENT_MAGIC35 0x3533544e45564550ULL

typedef void *EFI_EVENT_35;
typedef UINTN EFI_TPL_35;

typedef struct {
    u16 Year;
    u8 Month;
    u8 Day;
    u8 Hour;
    u8 Minute;
    u8 Second;
    u8 Pad1;
    u32 Nanosecond;
    s16_34 TimeZone;
    u8 Daylight;
    u8 Pad2;
} EFI_TIME_35;

typedef struct {
    u32 Resolution;
    u32 Accuracy;
    u8 SetsToZero;
} EFI_TIME_CAPABILITIES_35;

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

static EFI_RUNTIME_SERVICES_35 rt35;
static EFI_TPL_35 current_tpl35 = 4;
static u64 monotonic35 = 1;
static int boot_services_exited35 = 0;

static EVENT35 *event_ptr35(EFI_EVENT_35 event)
{
    EVENT35 *e = (EVENT35 *)event;
    if (!e || e->magic != EVENT_MAGIC35) return 0;
    return e;
}

static EFI_STATUS EFIAPI create_event35(u32 type, EFI_TPL_35 notify_tpl, event_notify35_fn notify_fn,
                                        void *notify_ctx, EFI_EVENT_35 *event)
{
    EVENT35 *e;
    (void)type;
    (void)notify_tpl;
    if (!event) return EFI_INVALID_PARAMETER;
    e = (EVENT35 *)mmap(0, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (e == (EVENT35 *)-1) return EFI_OUT_OF_RESOURCES;
    mem_zero(e, 4096);
    e->magic = EVENT_MAGIC35;
    e->notify_fn = notify_fn;
    e->notify_ctx = notify_ctx;
    *event = (EFI_EVENT_35)e;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI set_timer35(EFI_EVENT_35 event, u32 type, u64 trigger_time)
{
    EVENT35 *e = event_ptr35(event);
    if (!e) return EFI_INVALID_PARAMETER;
    if (type > TIMER_RELATIVE) return EFI_INVALID_PARAMETER;
    e->timer_mode = (int)type;
    e->trigger_time_100ns = trigger_time;
    if (type == TIMER_CANCEL) e->signaled = 0;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI signal_event35(EFI_EVENT_35 event)
{
    EVENT35 *e = event_ptr35(event);
    if (!e) return EFI_INVALID_PARAMETER;
    e->signaled = 1;
    if (e->notify_fn) e->notify_fn(event, e->notify_ctx);
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI check_event35(EFI_EVENT_35 event)
{
    EVENT35 *e = event_ptr35(event);
    if (!e) return EFI_INVALID_PARAMETER;
    return e->signaled ? EFI_SUCCESS : EFI_NOT_READY;
}

static EFI_STATUS EFIAPI wait_for_event35(UINTN count, EFI_EVENT_35 *events, UINTN *index)
{
    UINTN i;
    if (!count || !events || !index) return EFI_INVALID_PARAMETER;
    for (i = 0; i < count; ++i) {
        EVENT35 *e = event_ptr35(events[i]);
        if (!e) return EFI_INVALID_PARAMETER;
        if (e->signaled) {
            *index = i;
            return EFI_SUCCESS;
        }
    }
    return EFI_NOT_READY;
}

static EFI_STATUS EFIAPI close_event35(EFI_EVENT_35 event)
{
    EVENT35 *e = event_ptr35(event);
    if (!e) return EFI_INVALID_PARAMETER;
    e->magic = 0;
    return munmap(e, 4096) == 0 ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}

static EFI_STATUS EFIAPI create_event_ex35(u32 type, EFI_TPL_35 notify_tpl, event_notify35_fn notify_fn,
                                           const void *notify_ctx, const EFI_GUID *group, EFI_EVENT_35 *event)
{
    (void)group;
    return create_event35(type, notify_tpl, notify_fn, (void *)notify_ctx, event);
}

static EFI_TPL_35 EFIAPI raise_tpl35(EFI_TPL_35 new_tpl)
{
    EFI_TPL_35 old = current_tpl35;
    current_tpl35 = new_tpl;
    return old;
}

static void EFIAPI restore_tpl35(EFI_TPL_35 old_tpl)
{
    current_tpl35 = old_tpl;
}

static EFI_STATUS EFIAPI exit_boot_services35(EFI_HANDLE image, UINTN map_key)
{
    if (image != (EFI_HANDLE)&image_handle32) return EFI_INVALID_PARAMETER;
    if (map_key != map_key32) return EFI_INVALID_PARAMETER;
    boot_services_exited35 = 1;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI get_next_monotonic35(u64 *count)
{
    if (!count) return EFI_INVALID_PARAMETER;
    *count = monotonic35++;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI set_watchdog35(UINTN timeout, u64 code, UINTN data_size, CHAR16 *data)
{
    (void)timeout; (void)code; (void)data_size; (void)data;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI rt_get_time35(EFI_TIME_35 *time, EFI_TIME_CAPABILITIES_35 *caps)
{
    (void)time; (void)caps;
    return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI rt_set_time35(EFI_TIME_35 *time){(void)time;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_get_wakeup35(u8 *enabled,u8 *pending,EFI_TIME_35 *time){(void)enabled;(void)pending;(void)time;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_set_wakeup35(u8 enabled,EFI_TIME_35 *time){(void)enabled;(void)time;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_set_virtual_map35(UINTN a,UINTN b,u32 c,EFI_MEMORY_DESCRIPTOR*d){(void)a;(void)b;(void)c;(void)d;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_convert_pointer35(UINTN debug,void **addr){(void)debug;(void)addr;return EFI_UNSUPPORTED;}

static EFI_STATUS EFIAPI rt_get_variable35(CHAR16 *name, EFI_GUID *vendor, u32 *attrs, UINTN *data_size, void *data)
{
    (void)data;
    if (!name || !vendor || !data_size) return EFI_INVALID_PARAMETER;
    if (attrs) *attrs = 0;
    return EFI_NOT_FOUND;
}

static EFI_STATUS EFIAPI rt_get_next_variable35(UINTN *name_size, CHAR16 *name, EFI_GUID *vendor)
{
    if (!name_size || !name || !vendor) return EFI_INVALID_PARAMETER;
    return EFI_NOT_FOUND;
}

static EFI_STATUS EFIAPI rt_set_variable35(CHAR16 *name, EFI_GUID *vendor, u32 attrs, UINTN data_size, void *data)
{
    (void)attrs; (void)data_size; (void)data;
    if (!name || !vendor) return EFI_INVALID_PARAMETER;
    return EFI_WRITE_PROTECTED;
}

static EFI_STATUS EFIAPI rt_get_next_high_monotonic35(u32 *high)
{
    if (!high) return EFI_INVALID_PARAMETER;
    *high = (u32)(monotonic35 >> 32);
    return EFI_SUCCESS;
}

static void EFIAPI rt_reset_system35(u32 type, EFI_STATUS status, UINTN data_size, void *data)
{
    (void)type; (void)status; (void)data_size; (void)data;
    notify("Stage 3.5: ResetSystem requested");
}

static EFI_STATUS EFIAPI rt_update_capsule35(void **capsules, UINTN count, EFI_PHYSICAL_ADDRESS scatter)
{(void)capsules;(void)count;(void)scatter;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_query_capsule35(void **capsules, UINTN count, u64 *max_size, u32 *reset_type)
{(void)capsules;(void)count;(void)max_size;(void)reset_type;return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI rt_query_variable_info35(u32 attrs, u64 *max_storage, u64 *remaining, u64 *max_variable)
{(void)attrs;(void)max_storage;(void)remaining;(void)max_variable;return EFI_UNSUPPORTED;}

static void install_stage35_services(void)
{
    mem_zero(&rt35, sizeof(rt35));
    rt35.Hdr.Signature = EFI_RUNTIME_SERVICES_SIGNATURE;
    rt35.Hdr.Revision = EFI_REVISION_2_0;
    rt35.Hdr.HeaderSize = (u32)sizeof(rt35);
    rt35.GetTime = rt_get_time35;
    rt35.SetTime = rt_set_time35;
    rt35.GetWakeupTime = rt_get_wakeup35;
    rt35.SetWakeupTime = rt_set_wakeup35;
    rt35.SetVirtualAddressMap = rt_set_virtual_map35;
    rt35.ConvertPointer = rt_convert_pointer35;
    rt35.GetVariable = rt_get_variable35;
    rt35.GetNextVariableName = rt_get_next_variable35;
    rt35.SetVariable = rt_set_variable35;
    rt35.GetNextHighMonotonicCount = rt_get_next_high_monotonic35;
    rt35.ResetSystem = rt_reset_system35;
    rt35.UpdateCapsule = rt_update_capsule35;
    rt35.QueryCapsuleCapabilities = rt_query_capsule35;
    rt35.QueryVariableInfo = rt_query_variable_info35;
    rt35.Hdr.CRC32 = 0;
    rt35.Hdr.CRC32 = crc32_bytes(&rt35, sizeof(rt35));

    bs32.RaiseTPL = (void *)raise_tpl35;
    bs32.RestoreTPL = (void *)restore_tpl35;
    bs32.CreateEvent = (void *)create_event35;
    bs32.SetTimer = (void *)set_timer35;
    bs32.WaitForEvent = (void *)wait_for_event35;
    bs32.SignalEvent = (void *)signal_event35;
    bs32.CloseEvent = (void *)close_event35;
    bs32.CheckEvent = (void *)check_event35;
    bs32.ExitBootServices = (void *)exit_boot_services35;
    bs32.GetNextMonotonicCount = (void *)get_next_monotonic35;
    bs32.SetWatchdogTimer = (void *)set_watchdog35;
    bs32.CreateEventEx = (void *)create_event_ex35;
    bs32.Hdr.CRC32 = 0;
    bs32.Hdr.CRC32 = crc32_bytes(&bs32, sizeof(bs32));

    g_system_table.RuntimeServices = &rt35;
    g_system_table.BootServices = &bs32;
    g_system_table.Hdr.CRC32 = 0;
    g_system_table.Hdr.CRC32 = crc32_bytes(&g_system_table, g_system_table.Hdr.HeaderSize);
}

int main(void)
{
    create_event35_fn create_event;
    signal_event35_fn signal_event;
    check_event35_fn check_event;
    wait_for_event35_fn wait_event;
    close_event35_fn close_event;
    exit_boot_services35_fn exit_bs;
    EFI_EVENT_35 event = 0;
    UINTN index = 99;
    UINTN map_size = 0, key_before = 0, desc_size = 0;
    u32 version = 0;
    void *pool = 0;
    static CHAR16 variable_name[] = {'S','e','c','u','r','e','B','o','o','t',0};
    static EFI_GUID global_variable_guid = {0x8be4df61U,0x93caU,0x11d2U,{0xaa,0x0d,0x00,0xe0,0x98,0x03,0x2b,0x8c}};
    UINTN variable_size = 1;
    u8 variable_data = 0;

    notify("PS4 Windows Loader: Stage 3.5 started");
    build_efi_shim();
    if (build32() != 0) {
        notify("Stage 3.5: EFI environment build FAILED");
        return 1;
    }
    install_stage34_services();
    install_stage35_services();
    notify("Stage 3.5: event + Runtime Services installed");

    if (g_system_table.RuntimeServices != &rt35 || g_system_table.BootServices != &bs32 ||
        rt35.Hdr.Signature != EFI_RUNTIME_SERVICES_SIGNATURE) {
        notify("Stage 3.5: SystemTable/RuntimeServices FAILED");
        return 1;
    }
    notify("Stage 3.5: RuntimeServices table published");

    create_event = (create_event35_fn)bs32.CreateEvent;
    signal_event = (signal_event35_fn)bs32.SignalEvent;
    check_event = (check_event35_fn)bs32.CheckEvent;
    wait_event = (wait_for_event35_fn)bs32.WaitForEvent;
    close_event = (close_event35_fn)bs32.CloseEvent;
    if (!create_event || !signal_event || !check_event || !wait_event || !close_event ||
        create_event(0, 4, 0, 0, &event) != EFI_SUCCESS || !event) {
        notify("Stage 3.5: CreateEvent FAILED");
        return 1;
    }
    if (check_event(event) != EFI_NOT_READY || signal_event(event) != EFI_SUCCESS ||
        check_event(event) != EFI_SUCCESS || wait_event(1, &event, &index) != EFI_SUCCESS || index != 0) {
        close_event(event);
        notify("Stage 3.5: event signal/wait semantics FAILED");
        return 1;
    }
    if (close_event(event) != EFI_SUCCESS) {
        notify("Stage 3.5: CloseEvent FAILED");
        return 1;
    }
    notify("Stage 3.5: Create/Signal/Wait/Check/CloseEvent OK");

    if (rt35.GetVariable(variable_name, &global_variable_guid, 0, &variable_size, &variable_data) != EFI_NOT_FOUND ||
        rt35.SetVariable(variable_name, &global_variable_guid, 0, 0, 0) != EFI_WRITE_PROTECTED) {
        notify("Stage 3.5: UEFI variable semantics FAILED");
        return 1;
    }
    notify("Stage 3.5: Runtime variable fallback semantics OK");

    if (bs32.GetMemoryMap(&map_size, 0, &key_before, &desc_size, &version) != EFI_BUFFER_TOO_SMALL || !key_before) {
        notify("Stage 3.5: initial MapKey FAILED");
        return 1;
    }
    if (bs32.AllocatePool(EFI_BOOT_SERVICES_DATA, 64, &pool) != EFI_SUCCESS || !pool) {
        notify("Stage 3.5: MapKey allocation test FAILED");
        return 1;
    }
    map_size = 0;
    {
        UINTN key_after = 0;
        if (bs32.GetMemoryMap(&map_size, 0, &key_after, &desc_size, &version) != EFI_BUFFER_TOO_SMALL ||
            key_after == key_before) {
            bs32.FreePool(pool);
            notify("Stage 3.5: MapKey did not change");
            return 1;
        }
        bs32.FreePool(pool);
        notify("Stage 3.5: GetMemoryMap MapKey tracking OK");

        exit_bs = (exit_boot_services35_fn)bs32.ExitBootServices;
        map_size = 0;
        if (!exit_bs || bs32.GetMemoryMap(&map_size, 0, &key_after, &desc_size, &version) != EFI_BUFFER_TOO_SMALL ||
            exit_bs((EFI_HANDLE)&image_handle32, key_after) != EFI_SUCCESS || !boot_services_exited35) {
            notify("Stage 3.5: ExitBootServices key validation FAILED");
            return 1;
        }
    }
    notify("Stage 3.5: ExitBootServices MapKey validation OK");
    notify("PS4 Windows Loader: Stage 3.5 pre-entry firmware self-test OK");

    munmap(arena32, 16U * 1024U * 1024U);
    return 0;
}
