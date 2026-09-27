#define main stage4_0_base_main
#include "stage4_0.c"
#undef main

/*
 * Stage 4.1
 *
 * Stage 4.0 proved that the real Microsoft bootmgfw.efi entry point executes
 * on the target PS4 and reaches BootServices->HandleProtocol().  The last
 * hardware trace was the HandleProtocol entry marker, so this stage replaces
 * that callback with a deliberately small, direct implementation and emits
 * markers around every decision needed for the first request.
 *
 * The goal is to distinguish:
 *   1. bad ImageHandle / GUID input,
 *   2. a failure inside our protocol lookup,
 *   3. a failure after EFI_LOADED_IMAGE_PROTOCOL is returned to Microsoft.
 */

static u32 hp_calls41;

static void notify_guid41(const EFI_GUID *g)
{
    char s[] = "Stage 4.1 HP: GUID Data1=0x00000000";
    static const char hex[] = "0123456789ABCDEF";
    int i;
    u32 v;

    if (!g) {
        notify("Stage 4.1 HP: protocol GUID is NULL");
        return;
    }

    v = g->Data1;
    for (i = 0; i < 8; ++i)
        s[27 + i] = hex[(v >> (28 - i * 4)) & 0x0fU];
    notify(s);
}

static EFI_STATUS EFIAPI handle41(EFI_HANDLE h, EFI_GUID *g, void **out)
{
    ++hp_calls41;
    notify("Stage 4.1 HP: entered");

    if (!g || !out) {
        notify("Stage 4.1 HP: invalid parameter -> EFI_INVALID_PARAMETER");
        return EFI_INVALID_PARAMETER;
    }

    *out = 0;

    if (h == (EFI_HANDLE)&image_handle32)
        notify("Stage 4.1 HP: handle = bootmgfw ImageHandle");
    else if (h == (EFI_HANDLE)&device_handle32)
        notify("Stage 4.1 HP: handle = USB DeviceHandle");
    else
        notify("Stage 4.1 HP: handle = UNKNOWN");

    if (guid_eq35(g, &loaded_guid32)) {
        notify("Stage 4.1 HP: protocol = LoadedImage");
        if (h != (EFI_HANDLE)&image_handle32) {
            notify("Stage 4.1 HP: LoadedImage wrong handle -> EFI_NOT_FOUND");
            return EFI_NOT_FOUND;
        }

        if (loaded32.SystemTable != &g_system_table) {
            notify("Stage 4.1 HP: LoadedImage SystemTable BAD");
            return EFI_DEVICE_ERROR;
        }
        notify("Stage 4.1 HP: LoadedImage SystemTable OK");

        if (!loaded32.ImageBase || !loaded32.ImageSize) {
            notify("Stage 4.1 HP: LoadedImage image metadata BAD");
            return EFI_DEVICE_ERROR;
        }
        notify("Stage 4.1 HP: LoadedImage ImageBase/ImageSize OK");

        if (loaded32.DeviceHandle != (EFI_HANDLE)&device_handle32) {
            notify("Stage 4.1 HP: LoadedImage DeviceHandle BAD");
            return EFI_DEVICE_ERROR;
        }
        notify("Stage 4.1 HP: LoadedImage DeviceHandle OK");

        if (!loaded32.FilePath) {
            notify("Stage 4.1 HP: LoadedImage FilePath BAD");
            return EFI_DEVICE_ERROR;
        }
        notify("Stage 4.1 HP: LoadedImage FilePath OK");

        *out = &loaded32;
        notify("Stage 4.1 HP: returning LoadedImage + EFI_SUCCESS");
        return EFI_SUCCESS;
    }

    if (guid_eq35(g, &device_path_guid32)) {
        notify("Stage 4.1 HP: protocol = DevicePath");
        if (h != (EFI_HANDLE)&device_handle32) {
            notify("Stage 4.1 HP: DevicePath wrong handle -> EFI_NOT_FOUND");
            return EFI_NOT_FOUND;
        }
        *out = &end_path32;
        notify("Stage 4.1 HP: returning DevicePath + EFI_SUCCESS");
        return EFI_SUCCESS;
    }

    if (guid_eq35(g, &simple_fs_guid32)) {
        notify("Stage 4.1 HP: protocol = SimpleFS");
        if (h != (EFI_HANDLE)&device_handle32) {
            notify("Stage 4.1 HP: SimpleFS wrong handle -> EFI_NOT_FOUND");
            return EFI_NOT_FOUND;
        }
        *out = &fs32;
        notify("Stage 4.1 HP: returning SimpleFS + EFI_SUCCESS");
        return EFI_SUCCESS;
    }

    notify("Stage 4.1 HP: protocol = UNKNOWN");
    notify_guid41(g);
    notify("Stage 4.1 HP: returning EFI_NOT_FOUND");
    return EFI_NOT_FOUND;
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

    notify("PS4 Windows Loader: Stage 4.1 started");

    build_efi_shim();
    if (build32() != 0) {
        notify("Stage 4.1: EFI environment build FAILED");
        return 1;
    }

    install_stage35_services();
    install_stage40_services();

    /* Replace only the callback implicated by the Stage 4.0 hardware trace. */
    hp_calls41 = 0;
    bs32.HandleProtocol = handle41;
    refresh_crc40();
    notify("Stage 4.1: diagnostic HandleProtocol installed");

    if (load_bootmgfw(&file, &file_size) != 0 || inspect_pe(file, file_size, &pe) != 0) {
        if (file) munmap(file, (size_t)READ_CAP);
        notify("Stage 4.1: bootmgfw read/PE validation FAILED");
        return 1;
    }

    pages = (pe.image_size + 4095U) / 4096U;
    if (alloc_pages40(EFI_ALLOCATE_ANY_PAGES, EFI_LOADER_CODE, pages, &imgaddr) != EFI_SUCCESS) {
        munmap(file, (size_t)READ_CAP);
        notify("Stage 4.1: bootmgfw AllocatePages FAILED");
        return 1;
    }

    image = (u8 *)(unsigned long)imgaddr;
    if (map_sections40(file, file_size, &pe, image) != 0 || apply_relocs(image, &pe) != 0) {
        free_pages40(imgaddr, pages);
        munmap(file, (size_t)READ_CAP);
        notify("Stage 4.1: bootmgfw map/relocations FAILED");
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

    notify("Stage 4.1: bootmgfw mapped + LoadedImage ready");
    notify("Stage 4.1: ENTERING Microsoft bootmgfw.efi NOW");

    rc = entry((EFI_HANDLE)&image_handle32, &g_system_table);

    notify("Stage 4.1: Microsoft bootmgfw.efi RETURNED");
    notify_status40(rc);

    free_pages40(imgaddr, pages);
    munmap(file, (size_t)READ_CAP);
    munmap(arena32, (size_t)(arena_pages40 * EFI_PAGE_SIZE));
    return 0;
}
