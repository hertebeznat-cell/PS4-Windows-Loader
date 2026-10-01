#ifndef PWL_GRAPHICS_H
#define PWL_GRAPHICS_H
#include "pwl_efi_tables.h"
typedef struct pwl_graphics_info {
    uint32_t version,width,height,pixel_format,masks[4],pitch;
} pwl_graphics_info_t;
typedef struct pwl_graphics_mode {
    uint32_t max_mode,mode;
    uint64_t info,info_bytes,framebuffer,framebuffer_bytes;
} pwl_graphics_mode_t;
typedef struct pwl_text_mode {
    int32_t max_mode,mode,attribute,column,row;
    unsigned char cursor_visible,padding[3];
} pwl_text_mode_t;
typedef struct pwl_resident_graphics {
    uint64_t gop[4],text[10];
    pwl_graphics_mode_t mode;
    pwl_graphics_info_t info;
    pwl_text_mode_t text_mode;
    unsigned enabled,cursor_drawn,pat_index;
} pwl_resident_graphics_t;
_Static_assert(sizeof(pwl_graphics_info_t)==36,"GOP information ABI");
_Static_assert(sizeof(pwl_graphics_mode_t)==40,"GOP mode ABI");
_Static_assert(sizeof(pwl_text_mode_t)==24,"Text mode ABI");
typedef struct pwl_graphics_spec {
    uint64_t framebuffer,bytes;
    uint32_t width,height,pitch,pixel_format,pat_index; /* pitch in pixels; RGB=0, BGR=1. */
} pwl_graphics_spec_t;
/* Geometry/extent checks only. Caller must establish a real linear writable
 * framebuffer, ownership, cacheability and identity mapping in the final root.
 * No GPU registers, mode negotiation, MMIO discovery or guessed addresses.
 * Does not touch framebuffer bytes or publish a console interface. */
pwl_status_t pwl_graphics_prepare(const pwl_graphics_spec_t *spec,
    const pwl_efi_table_spec_t *code,uint64_t destination_pa,
    pwl_resident_graphics_t *out);
#endif
