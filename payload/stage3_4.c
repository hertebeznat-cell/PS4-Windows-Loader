#include <time.h>
#define STAGE3_2_NO_MAIN 1
#include "stage3_2.c"

#define EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL 0x00000001U
#define EFI_OPEN_PROTOCOL_GET_PROTOCOL       0x00000002U
#define EFI_OPEN_PROTOCOL_TEST_PROTOCOL      0x00000004U
#define EFI_LOCATE_BY_PROTOCOL               2U

typedef short s16_34;

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
} EFI_TIME_34;

typedef struct {
    u64 Size;
    u64 FileSize;
    u64 PhysicalSize;
    EFI_TIME_34 CreateTime;
    EFI_TIME_34 LastAccessTime;
    EFI_TIME_34 ModificationTime;
    u64 Attribute;
    CHAR16 FileName[1];
} EFI_FILE_INFO_34;

typedef EFI_STATUS (EFIAPI *efi_file_get_info34_fn)(EFI_FILE_PROTOCOL_32 *, EFI_GUID *, UINTN *, void *);
typedef EFI_STATUS (EFIAPI *open_protocol34_fn)(EFI_HANDLE, EFI_GUID *, void **, EFI_HANDLE, EFI_HANDLE, u32);
typedef EFI_STATUS (EFIAPI *close_protocol34_fn)(EFI_HANDLE, EFI_GUID *, EFI_HANDLE, EFI_HANDLE);
typedef EFI_STATUS (EFIAPI *locate_handle_buffer34_fn)(u32, EFI_GUID *, void *, UINTN *, EFI_HANDLE **);
typedef EFI_STATUS (EFIAPI *protocols_per_handle34_fn)(EFI_HANDLE, EFI_GUID ***, UINTN *);
typedef EFI_STATUS (EFIAPI *calculate_crc32_34_fn)(void *, UINTN, u32 *);
typedef void (EFIAPI *copy_mem34_fn)(void *, void *, UINTN);
typedef void (EFIAPI *set_mem34_fn)(void *, UINTN, u8);
typedef EFI_STATUS (EFIAPI *stall34_fn)(UINTN);

static const EFI_GUID file_info_guid34 = {
    0x09576e92U, 0x6d3fU, 0x11d2U,
    {0x8e,0x39,0x00,0xa0,0xc9,0x69,0x72,0x3b}
};

static int guid_eq34(const EFI_GUID *a, const EFI_GUID *b)
{
    return a && b && mem_eq32(a, b, sizeof(EFI_GUID));
}

static const char *basename34(const char *path)
{
    const char *base = path;
    const char *p = path;
    while (p && *p) {
        if (*p == '/') base = p + 1;
        ++p;
    }
    return base;
}

static EFI_STATUS scan_file_size34(FILE_HANDLE_32 *h, u64 *out_size)
{
    int fd;
    u8 scratch[4096];
    u64 total = 0;
    ssize_t got;

    if (!h || !out_size || h->is_root) return EFI_INVALID_PARAMETER;
    fd = open(h->path, O_RDONLY);
    if (fd < 0) return EFI_NOT_FOUND;

    for (;;) {
        got = read(fd, scratch, sizeof(scratch));
        if (got < 0) {
            close(fd);
            return EFI_DEVICE_ERROR;
        }
        if (got == 0) break;
        total += (u64)got;
    }
    close(fd);
    *out_size = total;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI file_get_info34(
    EFI_FILE_PROTOCOL_32 *proto,
    EFI_GUID *info_type,
    UINTN *buffer_size,
    void *buffer)
{
    FILE_HANDLE_32 *h = (FILE_HANDLE_32 *)proto;
    EFI_FILE_INFO_34 *info;
    const char *name;
    size_t name_len = 0;
    UINTN required;
    u64 file_size = 0;
    size_t i;
    EFI_STATUS rc;

    if (!h || !info_type || !buffer_size) return EFI_INVALID_PARAMETER;
    if (!guid_eq34(info_type, &file_info_guid34)) return EFI_UNSUPPORTED;
    if (h->is_root) return EFI_UNSUPPORTED;

    name = basename34(h->path);
    while (name[name_len]) ++name_len;
    required = (UINTN)offsetof(EFI_FILE_INFO_34, FileName) +
               (UINTN)((name_len + 1U) * sizeof(CHAR16));

    if (!buffer || *buffer_size < required) {
        *buffer_size = required;
        return EFI_BUFFER_TOO_SMALL;
    }

    rc = scan_file_size34(h, &file_size);
    if (rc != EFI_SUCCESS) return rc;

    mem_zero(buffer, (size_t)required);
    info = (EFI_FILE_INFO_34 *)buffer;
    info->Size = required;
    info->FileSize = file_size;
    info->PhysicalSize = file_size;
    info->Attribute = 0;
    for (i = 0; i < name_len; ++i) info->FileName[i] = (CHAR16)(u8)name[i];
    info->FileName[name_len] = 0;
    *buffer_size = required;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI file_open34(
    EFI_FILE_PROTOCOL_32 *proto,
    EFI_FILE_PROTOCOL_32 **new_handle,
    CHAR16 *name,
    u64 mode,
    u64 attrs)
{
    EFI_STATUS rc = file_open32(proto, new_handle, name, mode, attrs);
    if (rc == EFI_SUCCESS && new_handle && *new_handle) {
        (*new_handle)->Open = file_open34;
        (*new_handle)->GetInfo = (void *)file_get_info34;
    }
    return rc;
}

static EFI_STATUS EFIAPI open_protocol34(
    EFI_HANDLE handle,
    EFI_GUID *protocol,
    void **interface,
    EFI_HANDLE agent,
    EFI_HANDLE controller,
    u32 attributes)
{
    void *found = 0;
    EFI_STATUS rc;
    (void)agent;
    (void)controller;

    if (!handle || !protocol) return EFI_INVALID_PARAMETER;
    if (attributes != EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL &&
        attributes != EFI_OPEN_PROTOCOL_GET_PROTOCOL &&
        attributes != EFI_OPEN_PROTOCOL_TEST_PROTOCOL) return EFI_UNSUPPORTED;
    if (attributes != EFI_OPEN_PROTOCOL_TEST_PROTOCOL && !interface) return EFI_INVALID_PARAMETER;

    rc = protocol32(handle, protocol, &found);
    if (rc != EFI_SUCCESS) return rc;
    if (attributes != EFI_OPEN_PROTOCOL_TEST_PROTOCOL) *interface = found;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI close_protocol34(
    EFI_HANDLE handle,
    EFI_GUID *protocol,
    EFI_HANDLE agent,
    EFI_HANDLE controller)
{
    void *found = 0;
    (void)agent;
    (void)controller;
    if (!handle || !protocol) return EFI_INVALID_PARAMETER;
    return protocol32(handle, protocol, &found);
}

static EFI_STATUS EFIAPI locate_handle_buffer34(
    u32 search_type,
    EFI_GUID *protocol,
    void *search_key,
    UINTN *count,
    EFI_HANDLE **buffer)
{
    EFI_HANDLE match;
    EFI_HANDLE *arr = 0;
    EFI_STATUS rc;
    (void)search_key;

    if (!count || !buffer) return EFI_INVALID_PARAMETER;
    *count = 0;
    *buffer = 0;
    if (search_type != EFI_LOCATE_BY_PROTOCOL || !protocol) return EFI_UNSUPPORTED;

    if (guid_eq34(protocol, &loaded_guid32)) {
        match = (EFI_HANDLE)&image_handle32;
    } else if (guid_eq34(protocol, &device_path_guid32) ||
               guid_eq34(protocol, &simple_fs_guid32)) {
        match = (EFI_HANDLE)&device_handle32;
    } else {
        return EFI_NOT_FOUND;
    }

    rc = alloc_pool32(EFI_BOOT_SERVICES_DATA, sizeof(EFI_HANDLE), (void **)&arr);
    if (rc != EFI_SUCCESS) return rc;
    arr[0] = match;
    *buffer = arr;
    *count = 1;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI protocols_per_handle34(
    EFI_HANDLE handle,
    EFI_GUID ***protocol_buffer,
    UINTN *protocol_count)
{
    EFI_GUID **arr = 0;
    UINTN count;
    EFI_STATUS rc;

    if (!handle || !protocol_buffer || !protocol_count) return EFI_INVALID_PARAMETER;
    *protocol_buffer = 0;
    *protocol_count = 0;

    if (handle == (EFI_HANDLE)&image_handle32) count = 1;
    else if (handle == (EFI_HANDLE)&device_handle32) count = 2;
    else return EFI_NOT_FOUND;

    rc = alloc_pool32(EFI_BOOT_SERVICES_DATA, count * sizeof(EFI_GUID *), (void **)&arr);
    if (rc != EFI_SUCCESS) return rc;

    if (handle == (EFI_HANDLE)&image_handle32) {
        arr[0] = (EFI_GUID *)&loaded_guid32;
    } else {
        arr[0] = (EFI_GUID *)&device_path_guid32;
        arr[1] = (EFI_GUID *)&simple_fs_guid32;
    }

    *protocol_buffer = arr;
    *protocol_count = count;
    return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI calculate_crc32_34(void *data, UINTN size, u32 *out_crc)
{
    if ((!data && size) || !out_crc) return EFI_INVALID_PARAMETER;
    *out_crc = crc32_bytes(data, (size_t)size);
    return EFI_SUCCESS;
}

static void EFIAPI copy_mem34(void *dst, void *src, UINTN size)
{
    if (size) mem_copy(dst, src, (size_t)size);
}

static void EFIAPI set_mem34(void *dst, UINTN size, u8 value)
{
    u8 *p = (u8 *)dst;
    UINTN i;
    for (i = 0; i < size; ++i) p[i] = value;
}

static EFI_STATUS EFIAPI stall34(UINTN microseconds)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(microseconds / 1000000ULL);
    ts.tv_nsec = (long)((microseconds % 1000000ULL) * 1000ULL);
    return nanosleep(&ts, 0) == 0 ? EFI_SUCCESS : EFI_DEVICE_ERROR;
}

static void install_stage34_services(void)
{
    root32.proto.Open = file_open34;
    bs32.OpenProtocol = (void *)open_protocol34;
    bs32.CloseProtocol = (void *)close_protocol34;
    bs32.LocateHandleBuffer = (void *)locate_handle_buffer34;
    bs32.ProtocolsPerHandle = (void *)protocols_per_handle34;
    bs32.CalculateCrc32 = (void *)calculate_crc32_34;
    bs32.CopyMem = (void *)copy_mem34;
    bs32.SetMem = (void *)set_mem34;
    bs32.Stall = (void *)stall34;
    bs32.Hdr.CRC32 = 0;
    bs32.Hdr.CRC32 = crc32_bytes(&bs32, sizeof(bs32));
}

int main(void)
{
    void *iface = 0;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_32 *fs;
    EFI_FILE_PROTOCOL_32 *root = 0, *bcd = 0;
    efi_file_get_info34_fn get_info;
    open_protocol34_fn open_protocol;
    locate_handle_buffer34_fn locate_handles;
    protocols_per_handle34_fn protocols_per_handle;
    calculate_crc32_34_fn calc_crc;
    stall34_fn stall_fn;
    UINTN info_size = 0, handle_count = 0, protocol_count = 0;
    EFI_HANDLE *handles = 0;
    EFI_GUID **protocols = 0;
    u8 info_buffer[256];
    EFI_FILE_INFO_34 *info;
    EFI_STATUS rc;
    u8 sample[4] = {1,2,3,4};
    u8 copy_src[4] = {9,8,7,6};
    u8 copy_dst[4] = {0,0,0,0};
    u32 crc = 0;
    static CHAR16 bcd_path[] = {
        '\\','E','F','I','\\','M','i','c','r','o','s','o','f','t',
        '\\','B','o','o','t','\\','B','C','D',0
    };

    notify("PS4 Windows Loader: Stage 3.4 started");
    build_efi_shim();
    if (build32() != 0) {
        notify("Stage 3.4: EFI environment build FAILED");
        return 1;
    }
    install_stage34_services();
    notify("Stage 3.4: metadata + protocol services installed");

    if (bs32.LocateProtocol((EFI_GUID *)&simple_fs_guid32, 0, &iface) != EFI_SUCCESS || iface != &fs32) {
        notify("Stage 3.4: LocateProtocol(SimpleFS) FAILED");
        return 1;
    }
    fs = (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_32 *)iface;
    if (fs->OpenVolume(fs, &root) != EFI_SUCCESS || !root) {
        notify("Stage 3.4: OpenVolume FAILED");
        return 1;
    }
    if (root->Open(root, &bcd, bcd_path, EFI_FILE_MODE_READ, 0) != EFI_SUCCESS || !bcd) {
        notify("Stage 3.4: Open BCD FAILED");
        return 1;
    }
    notify("Stage 3.4: BCD opened through EFI File Protocol");

    get_info = (efi_file_get_info34_fn)bcd->GetInfo;
    if (!get_info) {
        bcd->Close(bcd);
        notify("Stage 3.4: GetInfo pointer missing");
        return 1;
    }
    rc = get_info(bcd, (EFI_GUID *)&file_info_guid34, &info_size, 0);
    if (rc != EFI_BUFFER_TOO_SMALL || info_size == 0 || info_size > sizeof(info_buffer)) {
        bcd->Close(bcd);
        notify("Stage 3.4: GetInfo sizing FAILED");
        return 1;
    }
    info_size = sizeof(info_buffer);
    rc = get_info(bcd, (EFI_GUID *)&file_info_guid34, &info_size, info_buffer);
    if (rc != EFI_SUCCESS) {
        bcd->Close(bcd);
        notify("Stage 3.4: GetInfo FAILED");
        return 1;
    }
    info = (EFI_FILE_INFO_34 *)info_buffer;
    if (!info->FileSize || info->FileName[0] != 'B' || info->FileName[1] != 'C' || info->FileName[2] != 'D' || info->FileName[3] != 0) {
        bcd->Close(bcd);
        notify("Stage 3.4: EFI_FILE_INFO validation FAILED");
        return 1;
    }
    notify("Stage 3.4: EFI_FILE_INFO size/name metadata OK");

    open_protocol = (open_protocol34_fn)bs32.OpenProtocol;
    iface = 0;
    if (!open_protocol || open_protocol((EFI_HANDLE)&device_handle32, (EFI_GUID *)&simple_fs_guid32,
        &iface, (EFI_HANDLE)&image_handle32, 0, EFI_OPEN_PROTOCOL_GET_PROTOCOL) != EFI_SUCCESS || iface != &fs32) {
        bcd->Close(bcd);
        notify("Stage 3.4: OpenProtocol FAILED");
        return 1;
    }
    notify("Stage 3.4: OpenProtocol(SimpleFS) OK");

    locate_handles = (locate_handle_buffer34_fn)bs32.LocateHandleBuffer;
    if (!locate_handles || locate_handles(EFI_LOCATE_BY_PROTOCOL, (EFI_GUID *)&simple_fs_guid32, 0,
        &handle_count, &handles) != EFI_SUCCESS || handle_count != 1 || !handles ||
        handles[0] != (EFI_HANDLE)&device_handle32) {
        bcd->Close(bcd);
        notify("Stage 3.4: LocateHandleBuffer FAILED");
        return 1;
    }
    bs32.FreePool(handles);
    notify("Stage 3.4: LocateHandleBuffer(SimpleFS) OK");

    protocols_per_handle = (protocols_per_handle34_fn)bs32.ProtocolsPerHandle;
    if (!protocols_per_handle || protocols_per_handle((EFI_HANDLE)&device_handle32, &protocols,
        &protocol_count) != EFI_SUCCESS || protocol_count != 2 || !protocols) {
        bcd->Close(bcd);
        notify("Stage 3.4: ProtocolsPerHandle FAILED");
        return 1;
    }
    bs32.FreePool(protocols);
    notify("Stage 3.4: ProtocolsPerHandle(device) OK");

    calc_crc = (calculate_crc32_34_fn)bs32.CalculateCrc32;
    if (!calc_crc || calc_crc(sample, sizeof(sample), &crc) != EFI_SUCCESS || crc != crc32_bytes(sample, sizeof(sample))) {
        bcd->Close(bcd);
        notify("Stage 3.4: CalculateCrc32 FAILED");
        return 1;
    }
    ((copy_mem34_fn)bs32.CopyMem)(copy_dst, copy_src, sizeof(copy_src));
    if (!mem_eq32(copy_dst, copy_src, sizeof(copy_src))) {
        bcd->Close(bcd);
        notify("Stage 3.4: CopyMem FAILED");
        return 1;
    }
    ((set_mem34_fn)bs32.SetMem)(copy_dst, sizeof(copy_dst), 0x5a);
    if (copy_dst[0] != 0x5a || copy_dst[1] != 0x5a || copy_dst[2] != 0x5a || copy_dst[3] != 0x5a) {
        bcd->Close(bcd);
        notify("Stage 3.4: SetMem FAILED");
        return 1;
    }
    stall_fn = (stall34_fn)bs32.Stall;
    if (!stall_fn || stall_fn(1000) != EFI_SUCCESS) {
        bcd->Close(bcd);
        notify("Stage 3.4: Stall FAILED");
        return 1;
    }
    notify("Stage 3.4: CRC/CopyMem/SetMem/Stall OK");

    if (bcd->Close(bcd) != EFI_SUCCESS) {
        notify("Stage 3.4: BCD Close FAILED");
        return 1;
    }
    notify("PS4 Windows Loader: Stage 3.4 pre-bootmgr services self-test OK");
    munmap(arena32, 16U * 1024U * 1024U);
    return 0;
}
