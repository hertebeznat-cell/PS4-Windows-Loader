#ifndef PS4_WINDOWS_LOADER_PWL_H
#define PS4_WINDOWS_LOADER_PWL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PWL_SIGNATURE 0x5057344cU /* 'PW4L' */
#define PWL_VERSION_MAJOR 0U
#define PWL_VERSION_MINOR 1U

typedef enum pwl_status {
    PWL_OK = 0,
    PWL_ERR_INVALID_ARGUMENT = -1,
    PWL_ERR_UNSUPPORTED = -2,
    PWL_ERR_NOT_FOUND = -3,
    PWL_ERR_BAD_IMAGE = -4,
    PWL_ERR_IO = -5
} pwl_status_t;

typedef struct pwl_framebuffer {
    uint64_t physical_address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t format;
} pwl_framebuffer_t;

typedef struct pwl_platform_info {
    uint32_t signature;
    uint16_t version_major;
    uint16_t version_minor;
    uint64_t usable_memory_bytes;
    pwl_framebuffer_t framebuffer;
} pwl_platform_info_t;

typedef struct pwl_pe_image {
    const void *file_data;
    size_t file_size;
    uint64_t preferred_base;
    uint64_t entry_rva;
    uint32_t image_size;
} pwl_pe_image_t;

pwl_status_t pwl_platform_probe(pwl_platform_info_t *info);
pwl_status_t pwl_pe_inspect(const void *data, size_t size, pwl_pe_image_t *image);
pwl_status_t pwl_boot_windows(void);

#ifdef __cplusplus
}
#endif

#endif
