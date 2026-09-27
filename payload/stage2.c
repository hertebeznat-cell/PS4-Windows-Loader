#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>

void *dlopen(const char *path, int mode);
void *dlsym(void *handle, const char *name);

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef long long          s64;

typedef int (*notify_fn_t)(int, const char *);

#define IMAGE_FILE_MACHINE_AMD64 0x8664U
#define PE32_PLUS_MAGIC 0x20bU
#define IMAGE_SUBSYSTEM_EFI_APPLICATION 10U
#define IMAGE_REL_BASED_ABSOLUTE 0U
#define IMAGE_REL_BASED_DIR64 10U
#define IMAGE_DIRECTORY_ENTRY_BASERELOC 5U
#define MAX_EFI_FILE_SIZE (64ULL * 1024ULL * 1024ULL)
#define MAX_IMAGE_SIZE    (128ULL * 1024ULL * 1024ULL)
#define PWL_SEEK_SET 0
#define PWL_SEEK_END 2

static size_t str_len(const char *s)
{
    size_t n = 0;
    while (s && s[n])
        ++n;
    return n;
}

static void log_line(const char *s)
{
    size_t n = str_len(s);
    if (n)
        write(1, s, n);
    write(1, "\n", 1);
}

static void notify(const char *msg)
{
    static notify_fn_t fn = (notify_fn_t)0;

    if (!fn) {
        void *handle = dlopen("/system/common/lib/libSceSysUtil.sprx", 0);
        if (!handle)
            handle = dlopen("libSceSysUtil.sprx", 0);
        if (handle)
            fn = (notify_fn_t)dlsym(handle, "sceSysUtilSendSystemNotificationWithText");
    }

    if (fn)
        fn(222, msg);

    log_line(msg);
}

static void mem_zero(void *dst, size_t n)
{
    u8 *p = (u8 *)dst;
    while (n--)
        *p++ = 0;
}

static void mem_copy(void *dst, const void *src, size_t n)
{
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    while (n--)
        *d++ = *s++;
}

static u16 u16le(const u8 *p)
{
    return (u16)p[0] | ((u16)p[1] << 8);
}

static u32 u32le(const u8 *p)
{
    return (u32)p[0]
         | ((u32)p[1] << 8)
         | ((u32)p[2] << 16)
         | ((u32)p[3] << 24);
}

static u64 u64le(const u8 *p)
{
    return (u64)u32le(p) | ((u64)u32le(p + 4) << 32);
}

static void put_u64le(u8 *p, u64 v)
{
    p[0] = (u8)(v >> 0);
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
    p[4] = (u8)(v >> 32);
    p[5] = (u8)(v >> 40);
    p[6] = (u8)(v >> 48);
    p[7] = (u8)(v >> 56);
}

static int path_exists(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return 0;
    close(fd);
    return 1;
}

static void probe_mounts(void)
{
    if (path_exists("/mnt/usb0"))
        notify("PS4 Windows Loader: /mnt/usb0 is visible");
    if (path_exists("/mnt/usb1"))
        notify("PS4 Windows Loader: /mnt/usb1 is visible");
}

static int load_file(const char *path, u8 **out_data, size_t *out_size)
{
    int fd;
    off_t end;
    u8 *data;
    size_t total = 0;

    if (!out_data || !out_size)
        return -1;

    *out_data = (u8 *)0;
    *out_size = 0;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    end = lseek(fd, 0, PWL_SEEK_END);
    if (end <= 0 || (u64)end > MAX_EFI_FILE_SIZE) {
        close(fd);
        return -1;
    }

    if (lseek(fd, 0, PWL_SEEK_SET) < 0) {
        close(fd);
        return -1;
    }

    data = (u8 *)mmap((void *)0, (size_t)end,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);
    if (data == (u8 *)-1) {
        close(fd);
        return -1;
    }

    while (total < (size_t)end) {
        ssize_t got = read(fd, data + total, (size_t)end - total);
        if (got <= 0) {
            munmap(data, (size_t)end);
            close(fd);
            return -1;
        }
        total += (size_t)got;
    }

    close(fd);
    *out_data = data;
    *out_size = total;
    return 0;
}

struct pe_info {
    const u8 *coff;
    const u8 *optional;
    const u8 *sections;
    u16 section_count;
    u16 optional_size;
    u32 entry_rva;
    u64 image_base;
    u32 image_size;
    u32 headers_size;
    u32 reloc_rva;
    u32 reloc_size;
};

static int inspect_pe(const u8 *data, size_t size, struct pe_info *pe)
{
    u32 peoff;
    const u8 *coff;
    const u8 *optional;
    u16 optional_size;
    u16 section_count;
    size_t section_table_size;

    if (!data || !pe || size < 0x80U)
        return -1;

    if (data[0] != 'M' || data[1] != 'Z')
        return -1;

    peoff = u32le(data + 0x3c);
    if ((size_t)peoff > size || size - (size_t)peoff < 24U)
        return -1;

    coff = data + peoff;
    if (coff[0] != 'P' || coff[1] != 'E' || coff[2] != 0 || coff[3] != 0)
        return -1;

    if (u16le(coff + 4) != IMAGE_FILE_MACHINE_AMD64)
        return -1;

    section_count = u16le(coff + 6);
    optional_size = u16le(coff + 20);

    if (section_count == 0 || section_count > 96U)
        return -1;

    if (optional_size < 0xb0U)
        return -1;

    if (size - (size_t)(coff - data) < 24U + (size_t)optional_size)
        return -1;

    optional = coff + 24;
    if (u16le(optional) != PE32_PLUS_MAGIC)
        return -1;

    if (u16le(optional + 0x44) != IMAGE_SUBSYSTEM_EFI_APPLICATION)
        return -1;

    section_table_size = (size_t)section_count * 40U;
    if ((size_t)(optional + optional_size - data) > size ||
        size - (size_t)(optional + optional_size - data) < section_table_size)
        return -1;

    mem_zero(pe, sizeof(*pe));
    pe->coff = coff;
    pe->optional = optional;
    pe->sections = optional + optional_size;
    pe->section_count = section_count;
    pe->optional_size = optional_size;
    pe->entry_rva = u32le(optional + 0x10);
    pe->image_base = u64le(optional + 0x18);
    pe->image_size = u32le(optional + 0x38);
    pe->headers_size = u32le(optional + 0x3c);
    pe->reloc_rva = u32le(optional + 0x70 + IMAGE_DIRECTORY_ENTRY_BASERELOC * 8U);
    pe->reloc_size = u32le(optional + 0x74 + IMAGE_DIRECTORY_ENTRY_BASERELOC * 8U);

    if (pe->image_size == 0U || pe->image_size > MAX_IMAGE_SIZE)
        return -1;
    if (pe->headers_size == 0U || pe->headers_size > pe->image_size || pe->headers_size > size)
        return -1;
    if (pe->entry_rva >= pe->image_size)
        return -1;

    return 0;
}

static int map_sections(const u8 *file, size_t file_size,
                        const struct pe_info *pe, u8 *image)
{
    u16 i;

    mem_zero(image, pe->image_size);
    mem_copy(image, file, pe->headers_size);

    for (i = 0; i < pe->section_count; ++i) {
        const u8 *sh = pe->sections + (size_t)i * 40U;
        u32 virtual_size = u32le(sh + 8);
        u32 virtual_address = u32le(sh + 12);
        u32 raw_size = u32le(sh + 16);
        u32 raw_ptr = u32le(sh + 20);
        u32 copy_size = raw_size;

        if (virtual_address >= pe->image_size)
            return -1;

        if (virtual_size && copy_size > virtual_size)
            copy_size = virtual_size;

        if (copy_size == 0U)
            continue;

        if ((u64)raw_ptr + (u64)copy_size > (u64)file_size)
            return -1;
        if ((u64)virtual_address + (u64)copy_size > (u64)pe->image_size)
            return -1;

        mem_copy(image + virtual_address, file + raw_ptr, copy_size);
    }

    return 0;
}

static int apply_relocations(u8 *image, const struct pe_info *pe)
{
    u64 mapped_base = (u64)(unsigned long)image;
    s64 delta = (s64)(mapped_base - pe->image_base);
    u32 pos = 0;

    if (delta == 0)
        return 0;

    if (pe->reloc_rva == 0U || pe->reloc_size == 0U)
        return -1;

    if ((u64)pe->reloc_rva + (u64)pe->reloc_size > (u64)pe->image_size)
        return -1;

    while (pos < pe->reloc_size) {
        u8 *block;
        u32 page_rva;
        u32 block_size;
        u32 entry_count;
        u32 j;

        if (pe->reloc_size - pos < 8U)
            return -1;

        block = image + pe->reloc_rva + pos;
        page_rva = u32le(block);
        block_size = u32le(block + 4);

        if (block_size < 8U || block_size > pe->reloc_size - pos)
            return -1;

        entry_count = (block_size - 8U) / 2U;
        for (j = 0; j < entry_count; ++j) {
            u16 entry = u16le(block + 8U + j * 2U);
            u16 type = (u16)(entry >> 12);
            u16 off = (u16)(entry & 0x0fffU);
            u64 target_rva = (u64)page_rva + (u64)off;

            if (type == IMAGE_REL_BASED_ABSOLUTE)
                continue;

            if (type != IMAGE_REL_BASED_DIR64)
                return -1;

            if (target_rva + 8ULL > (u64)pe->image_size)
                return -1;

            put_u64le(image + (size_t)target_rva,
                      (u64)((s64)u64le(image + (size_t)target_rva) + delta));
        }

        pos += block_size;
    }

    return 0;
}

static const char *const g_candidates[] = {
    "/mnt/usb0/EFI/Microsoft/Boot/bootmgfw.efi",
    "/mnt/usb1/EFI/Microsoft/Boot/bootmgfw.efi",
    "/mnt/usb0/efi/microsoft/boot/bootmgfw.efi",
    "/mnt/usb1/efi/microsoft/boot/bootmgfw.efi",
    "/mnt/usb0/EFI/Boot/bootx64.efi",
    "/mnt/usb1/EFI/Boot/bootx64.efi"
};

int main(void)
{
    unsigned int i;

    notify("PS4 Windows Loader: Stage 2 started");
    probe_mounts();

    for (i = 0; i < (unsigned int)(sizeof(g_candidates) / sizeof(g_candidates[0])); ++i) {
        u8 *file = (u8 *)0;
        size_t file_size = 0;
        struct pe_info pe;
        u8 *image;

        if (load_file(g_candidates[i], &file, &file_size) != 0)
            continue;

        notify("PS4 Windows Loader: EFI candidate loaded from USB");

        if (inspect_pe(file, file_size, &pe) != 0) {
            munmap(file, file_size);
            notify("PS4 Windows Loader: EFI candidate failed PE32+ validation");
            continue;
        }

        image = (u8 *)mmap((void *)0, pe.image_size,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS,
                           -1, 0);
        if (image == (u8 *)-1) {
            munmap(file, file_size);
            notify("PS4 Windows Loader: image allocation failed");
            return 1;
        }

        if (map_sections(file, file_size, &pe, image) != 0) {
            munmap(image, pe.image_size);
            munmap(file, file_size);
            notify("PS4 Windows Loader: PE section mapping failed");
            return 1;
        }

        if (apply_relocations(image, &pe) != 0) {
            munmap(image, pe.image_size);
            munmap(file, file_size);
            notify("PS4 Windows Loader: PE relocation failed");
            return 1;
        }

        notify("PS4 Windows Loader: Stage 2 PE map + relocations OK");
        log_line(g_candidates[i]);

        /*
         * Intentionally do not execute the EFI entry point yet.
         * Stage 3 will provide EFI System Table / Boot Services shims first.
         */
        munmap(image, pe.image_size);
        munmap(file, file_size);
        return 0;
    }

    notify("PS4 Windows Loader: no Windows EFI loader found on visible USB mounts");
    return 1;
}
