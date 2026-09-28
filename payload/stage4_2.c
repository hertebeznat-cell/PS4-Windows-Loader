#define main stage4_1_base_main
#include "stage4_1.c"
#undef main

/*
 * Stage 4.2
 *
 * The first real Microsoft HandleProtocol() call reached Stage 4.1 but the
 * machine stopped immediately after the entry notification.  Stage 4.2 keeps
 * the already hardware-verified Stage 4.1 firmware environment, but replaces
 * HandleProtocol with a much less invasive implementation:
 *
 *  - no PS4 notification calls from inside the Microsoft callback;
 *  - no speculative write to *Interface before a protocol match;
 *  - step-by-step tracing to a pre-opened USB file descriptor;
 *  - raw Handle / Protocol / Interface pointer values are recorded before any
 *    caller-owned pointer is dereferenced;
 *  - HandleProtocol uses EFI_UNSUPPORTED for a protocol that is not installed
 *    on the supplied handle, matching UEFI HandleProtocol semantics.
 *
 * The trace file is written to /mnt/usb0/PS4WL_STAGE42.LOG.
 */

static int trace_fd42 = -1;
static volatile u64 hp_calls42;
static volatile u64 hp_last_step42;

static size_t len42(const char *s)
{
    size_t n = 0;
    while (s && s[n]) ++n;
    return n;
}

static void log42(const char *s)
{
    if (trace_fd42 >= 0 && s) {
        size_t n = len42(s);
        if (n) (void)write(trace_fd42, s, n);
    }
}

static void log_hex42(const char *tag, u64 value)
{
    char b[96];
    static const char hex[] = "0123456789ABCDEF";
    size_t p = 0, i;

    while (tag && *tag && p + 1 < sizeof(b)) b[p++] = *tag++;
    if (p + 19 >= sizeof(b)) return;
    b[p++] = '0';
    b[p++] = 'x';
    for (i = 0; i < 16; ++i)
        b[p++] = hex[(value >> (60U - (u32)i * 4U)) & 0x0fU];
    b[p++] = '\n';
    b[p] = 0;
    log42(b);
}

static void hp_step42(u64 step, const char *msg)
{
    hp_last_step42 = step;
    log42(msg);
}

static int guid_equal42(const EFI_GUID *a, const EFI_GUID *b)
{
    const u8 *x = (const u8 *)a;
    const u8 *y = (const u8 *)b;
    UINTN i;

    for (i = 0; i < (UINTN)sizeof(EFI_GUID); ++i)
        if (x[i] != y[i]) return 0;
    return 1;
}

static EFI_STATUS EFIAPI handle42(EFI_HANDLE h, EFI_GUID *g, void **out)
{
    u64 call_no = ++hp_calls42;

    hp_step42(0x4200, "HP42: enter\n");
    log_hex42("HP42: call=", call_no);
    log_hex42("HP42: handle=", (u64)(unsigned long)h);
    log_hex42("HP42: guid_ptr=", (u64)(unsigned long)g);
    log_hex42("HP42: out_ptr=", (u64)(unsigned long)out);

    if (!g || !out) {
        hp_step42(0x4201, "HP42: null argument -> EFI_INVALID_PARAMETER\n");
        return EFI_INVALID_PARAMETER;
    }

    /* Deliberately do not touch *out yet.  Stage 4.1 wrote through the caller
       pointer before even identifying the request, which was the first unsafe
       operation after its entry trace. */
    hp_step42(0x4202, "HP42: args non-null; before GUID read\n");
    log_hex42("HP42: guid.Data1=", (u64)g->Data1);
    hp_step42(0x4203, "HP42: GUID readable\n");

    if (h == (EFI_HANDLE)&image_handle32) {
        hp_step42(0x4210, "HP42: ImageHandle\n");
        if (guid_equal42(g, &loaded_guid32)) {
            hp_step42(0x4211, "HP42: LoadedImage GUID matched\n");

            if (loaded32.SystemTable != &g_system_table) {
                hp_step42(0x4212, "HP42: LoadedImage SystemTable BAD\n");
                return EFI_DEVICE_ERROR;
            }
            if (!loaded32.ImageBase || !loaded32.ImageSize) {
                hp_step42(0x4213, "HP42: LoadedImage image metadata BAD\n");
                return EFI_DEVICE_ERROR;
            }
            if (loaded32.DeviceHandle != (EFI_HANDLE)&device_handle32) {
                hp_step42(0x4214, "HP42: LoadedImage DeviceHandle BAD\n");
                return EFI_DEVICE_ERROR;
            }
            if (!loaded32.FilePath) {
                hp_step42(0x4215, "HP42: LoadedImage FilePath BAD\n");
                return EFI_DEVICE_ERROR;
            }

            hp_step42(0x4216, "HP42: before Interface write\n");
            *out = &loaded32;
            hp_step42(0x4217, "HP42: Interface write OK; EFI_SUCCESS\n");
            return EFI_SUCCESS;
        }

        hp_step42(0x4218, "HP42: protocol not installed on ImageHandle -> EFI_UNSUPPORTED\n");
        return EFI_UNSUPPORTED;
    }

    if (h == (EFI_HANDLE)&device_handle32) {
        hp_step42(0x4220, "HP42: DeviceHandle\n");

        if (guid_equal42(g, &device_path_guid32)) {
            hp_step42(0x4221, "HP42: DevicePath matched; before Interface write\n");
            *out = &end_path32;
            hp_step42(0x4222, "HP42: DevicePath Interface write OK; EFI_SUCCESS\n");
            return EFI_SUCCESS;
        }

        if (guid_equal42(g, &simple_fs_guid32)) {
            hp_step42(0x4223, "HP42: SimpleFS matched; before Interface write\n");
            *out = &fs32;
            hp_step42(0x4224, "HP42: SimpleFS Interface write OK; EFI_SUCCESS\n");
            return EFI_SUCCESS;
        }

        hp_step42(0x4225, "HP42: protocol not installed on DeviceHandle -> EFI_UNSUPPORTED\n");
        return EFI_UNSUPPORTED;
    }

    hp_step42(0x4230, "HP42: unknown handle -> EFI_UNSUPPORTED\n");
    return EFI_UNSUPPORTED;
}

static EFI_STATUS EFIAPI open_protocol42(EFI_HANDLE h, EFI_GUID *g, void **iface,
                                          EFI_HANDLE agent, EFI_HANDLE controller,
                                          u32 attr)
{
    EFI_STATUS rc;
    (void)agent;
    (void)controller;

    hp_step42(0x4240, "OP42: enter\n");

    if (!h || !g) return EFI_INVALID_PARAMETER;
    if (attr != EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL &&
        attr != EFI_OPEN_PROTOCOL_GET_PROTOCOL &&
        attr != EFI_OPEN_PROTOCOL_TEST_PROTOCOL)
        return EFI_UNSUPPORTED;
    if (attr != EFI_OPEN_PROTOCOL_TEST_PROTOCOL && !iface)
        return EFI_INVALID_PARAMETER;

    if (attr == EFI_OPEN_PROTOCOL_TEST_PROTOCOL) {
        void *tmp = 0;
        rc = handle42(h, g, &tmp);
        return rc;
    }

    return handle42(h, g, iface);
}

static void install_stage42_services(void)
{
    bs32.HandleProtocol = handle42;
    bs32.OpenProtocol = (void *)open_protocol42;
    refresh_crc40();
}

static void open_trace42(void)
{
    trace_fd42 = open("/mnt/usb0/PS4WL_STAGE42.LOG",
                      O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (trace_fd42 >= 0) {
        log42("PS4 Windows Loader Stage 4.2 trace\n");
        log42("HandleProtocol callback-safe diagnostic enabled\n");
    }
}

int main(void)
{
    u8 *file = 0, *image = 0;
    size_t file_size = 0;
    struct pe_info pe;
    UINTN pages;
    EFI_PHYSICAL_ADDRESS imgaddr = 0;
    efi_entry40_fn entry;
    EFI_STATUS rc;

    notify("PS4 Windows Loader: Stage 4.2 started");

    build_efi_shim();
    if (build32() != 0) {
        notify("Stage 4.2: EFI environment build FAILED");
        return 1;
    }

    install_stage35_services();
    install_stage40_services();
    open_trace42();
    install_stage42_services();

    notify("Stage 4.2: safe HandleProtocol + USB trace installed");

    if (load_bootmgfw(&file, &file_size) != 0 ||
        inspect_pe(file, file_size, &pe) != 0) {
        if (file) munmap(file, (size_t)READ_CAP);
        log42("Stage 4.2: bootmgfw read/PE validation FAILED\n");
        notify("Stage 4.2: bootmgfw read/PE validation FAILED");
        if (trace_fd42 >= 0) close(trace_fd42);
        return 1;
    }

    pages = (pe.image_size + 4095U) / 4096U;
    if (alloc_pages40(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_CODE,
                      pages, &imgaddr) != EFI_SUCCESS) {
        munmap(file, (size_t)READ_CAP);
        log42("Stage 4.2: bootmgfw AllocatePages FAILED\n");
        notify("Stage 4.2: bootmgfw AllocatePages FAILED");
        if (trace_fd42 >= 0) close(trace_fd42);
        return 1;
    }

    image = (u8 *)(unsigned long)imgaddr;
    if (map_sections40(file, file_size, &pe, image) != 0 ||
        apply_relocs(image, &pe) != 0) {
        free_pages40(imgaddr, pages);
        munmap(file, (size_t)READ_CAP);
        log42("Stage 4.2: bootmgfw map/relocations FAILED\n");
        notify("Stage 4.2: bootmgfw map/relocations FAILED");
        if (trace_fd42 >= 0) close(trace_fd42);
        return 1;
    }

    build_loaded_path40();
    loaded32.SystemTable = &g_system_table;
    loaded32.DeviceHandle = (EFI_HANDLE)&device_handle32;
    loaded32.FilePath = (EFI_DEVICE_PATH_PROTOCOL_32 *)loaded_path40.bytes;
    loaded32.ImageBase = image;
    loaded32.ImageSize = pe.image_size;
    loaded32.ImageCodeType = EFI_LOADER_CODE;
    loaded32.ImageDataType = EFI_LOADER_DATA;
    entry = (efi_entry40_fn)(image + pe.entry_rva);
    refresh_crc40();

    log_hex42("ENTRY42: ImageHandle=", (u64)(unsigned long)&image_handle32);
    log_hex42("ENTRY42: SystemTable=", (u64)(unsigned long)&g_system_table);
    log_hex42("ENTRY42: BootServices=", (u64)(unsigned long)&bs32);
    log_hex42("ENTRY42: HandleProtocol=", (u64)(unsigned long)bs32.HandleProtocol);
    log_hex42("ENTRY42: bootmgfw entry=", (u64)(unsigned long)entry);
    log42("ENTRY42: entering Microsoft bootmgfw.efi\n");

    notify("Stage 4.2: ENTERING Microsoft bootmgfw.efi NOW");
    rc = entry((EFI_HANDLE)&image_handle32, &g_system_table);

    log42("ENTRY42: Microsoft bootmgfw.efi returned\n");
    log_hex42("ENTRY42: status=", rc);
    notify("Stage 4.2: Microsoft bootmgfw.efi RETURNED");
    notify_status40(rc);

    if (trace_fd42 >= 0) close(trace_fd42);
    free_pages40(imgaddr, pages);
    munmap(file, (size_t)READ_CAP);
    munmap(arena32, (size_t)(arena_pages40 * EFI_PAGE_SIZE));
    return 0;
}
