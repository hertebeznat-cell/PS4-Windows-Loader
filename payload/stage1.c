#include <sys/types.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>

void *dlopen(const char *path, int mode);
void *dlsym(void *handle, const char *name);

typedef int (*notify_fn_t)(int, const char *);

#define IMAGE_FILE_MACHINE_AMD64 0x8664U
#define PE32_PLUS_MAGIC 0x20bU
#define IMAGE_SUBSYSTEM_EFI_APPLICATION 10U
#define HEADER_BUF_SIZE 4096U

static unsigned char g_header[HEADER_BUF_SIZE];

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

static uint16_t u16le(const unsigned char *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t u32le(const unsigned char *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static int read_header(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    ssize_t got = read(fd, g_header, sizeof(g_header));
    close(fd);

    if (got <= 0)
        return -1;

    return (int)got;
}

static int valid_x64_efi_image(const unsigned char *data, size_t size)
{
    uint32_t peoff;
    const unsigned char *coff;
    const unsigned char *optional;
    uint16_t opt_size;

    if (!data || size < 0x80U)
        return 0;

    if (data[0] != 'M' || data[1] != 'Z')
        return 0;

    peoff = u32le(data + 0x3c);
    if ((size_t)peoff > size || size - (size_t)peoff < 24U)
        return 0;

    coff = data + peoff;
    if (coff[0] != 'P' || coff[1] != 'E' || coff[2] != 0 || coff[3] != 0)
        return 0;

    if (u16le(coff + 4) != IMAGE_FILE_MACHINE_AMD64)
        return 0;

    opt_size = u16le(coff + 20);
    if (opt_size < 0x46U || size - (size_t)(coff - data) < 24U + (size_t)opt_size)
        return 0;

    optional = coff + 24;
    if (u16le(optional) != PE32_PLUS_MAGIC)
        return 0;

    if (u16le(optional + 0x44) != IMAGE_SUBSYSTEM_EFI_APPLICATION)
        return 0;

    return 1;
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

    notify("PS4 Windows Loader: Stage 1 started");
    log_line("Scanning PS4 USB mounts for an x64 Windows EFI loader...");

    for (i = 0; i < (unsigned int)(sizeof(g_candidates) / sizeof(g_candidates[0])); ++i) {
        int got = read_header(g_candidates[i]);
        if (got < 0)
            continue;

        if (valid_x64_efi_image(g_header, (size_t)got)) {
            log_line("Valid x64 EFI application found:");
            log_line(g_candidates[i]);
            notify("PS4 Windows Loader: valid Windows/EFI boot image found on USB");
            return 0;
        }

        log_line("Candidate exists but is not a valid x64 EFI application:");
        log_line(g_candidates[i]);
    }

    notify("PS4 Windows Loader: no valid bootmgfw.efi/bootx64.efi found on USB");
    return 1;
}
