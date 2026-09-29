#include "pwl_pe_loader.h"

#define EXEC UINT32_C(0x20000000)
#define WRITE UINT32_C(0x80000000)
#define IDENTITY_LIMIT (UINT64_C(1) << 47)

static uint16_t u16(const unsigned char *p)
{ return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8); }
static uint32_t u32(const unsigned char *p)
{ return (uint32_t)u16(p) | ((uint32_t)u16(p + 2) << 16); }
static uint64_t u64(const unsigned char *p)
{ return (uint64_t)u32(p) | ((uint64_t)u32(p + 4) << 32); }
static void put64(unsigned char *p, uint64_t n)
{ unsigned i; for (i = 0; i < 8; ++i) p[i] = (unsigned char)(n >> (8 * i)); }
static void zero(void *p, size_t n)
{ unsigned char *d = p; size_t i; for (i = 0; i < n; ++i) d[i] = 0; }
static void copy(void *p, const void *q, size_t n)
{ unsigned char *d = p; const unsigned char *s = q; size_t i; for (i = 0; i < n; ++i) d[i] = s[i]; }
static uint64_t page_up(uint64_t n)
{ return (n + PWL_PAGE_SIZE - 1) & ~(PWL_PAGE_SIZE - 1); }
static int power2(uint32_t n) { return n && !(n & (n - 1)); }

typedef struct layout {
    pwl_pe_image_t image;
    size_t section_offset;
    uint32_t headers, reloc_rva, reloc_size;
    uint16_t sections, characteristics;
} layout_t;

static pwl_status_t layout(const unsigned char *file, size_t bytes, layout_t *l)
{
    const unsigned char *coff, *opt;
    uint32_t sa, fa, dirs;
    uint64_t end;
    size_t i, j;
    int entry = 0;
    pwl_status_t status = pwl_pe_inspect(file, bytes, &l->image);
    if (status != PWL_OK) return status;
    coff = file + u32(file + 0x3c); opt = coff + 24;
    l->sections = u16(coff + 6); l->characteristics = u16(coff + 22);
    l->section_offset = (size_t)(opt - file) + u16(coff + 20);
    l->headers = u32(opt + 60);
    l->reloc_rva = l->reloc_size = 0;
    sa = u32(opt + 32); fa = u32(opt + 36); dirs = u32(opt + 108);
    if (u16(opt + 68) != 10 || l->sections > PWL_PE_MAX_SECTIONS)
        return PWL_ERR_UNSUPPORTED;
    if (!power2(sa) || sa < PWL_PAGE_SIZE || !power2(fa) || fa < 512 ||
        fa > 65536 || sa < fa || l->headers % fa ||
        l->image.image_size % sa || l->image.preferred_base % 65536 ||
        l->image.preferred_base > UINT64_MAX - l->image.image_size ||
        dirs > (u16(coff + 20) - 112U) / 8U)
        return PWL_ERR_BAD_IMAGE;
    /* Static EFI images only; do not pretend unbound imports/TLS are ready. */
    for (i = 0; i < dirs; ++i) {
        uint32_t rva = u32(opt + 112 + i * 8), size = u32(opt + 116 + i * 8);
        if ((i == 1 || i == 9 || i == 13) && (rva || size))
            return PWL_ERR_UNSUPPORTED;
        if (i == 5) { l->reloc_rva = rva; l->reloc_size = size; }
    }
    if ((!l->reloc_rva != !l->reloc_size) ||
        ((l->characteristics & 1) && l->reloc_size) ||
        l->reloc_rva > l->image.image_size ||
        l->reloc_size > l->image.image_size - l->reloc_rva)
        return PWL_ERR_BAD_IMAGE;
    end = page_up(l->headers);
    for (i = 0; i < l->sections; ++i) {
        const unsigned char *s = file + l->section_offset + i * 40;
        uint32_t vs = u32(s + 8), va = u32(s + 12), raw = u32(s + 16);
        uint32_t offset = u32(s + 20), flags = u32(s + 36);
        uint64_t extent = vs > raw ? vs : raw;
        if ((flags & (EXEC | WRITE)) == (EXEC | WRITE)) return PWL_ERR_UNSUPPORTED;
        if (va % sa || va < end || extent == 0 ||
            (raw && (offset < l->headers || offset % fa || raw % fa)))
            return PWL_ERR_BAD_IMAGE;
        end = va + page_up(extent);
        if (end > l->image.image_size) return PWL_ERR_BAD_IMAGE;
        /* Reject overlapping raw ranges as well as overlapping virtual pages. */
        for (j = 0; raw && j < i; ++j) {
            const unsigned char *t = file + l->section_offset + j * 40;
            uint64_t a = u32(t + 20), n = u32(t + 16);
            if (n && offset < a + n && a < (uint64_t)offset + raw)
                return PWL_ERR_BAD_IMAGE;
        }
        if ((flags & EXEC) && l->image.entry_rva >= va &&
            l->image.entry_rva - va < raw) entry = 1;
    }
    return entry ? PWL_OK : PWL_ERR_BAD_IMAGE;
}

pwl_status_t pwl_pe_efi_size(const void *file, size_t bytes, uint64_t *size)
{
    layout_t l;
    pwl_status_t status;
    if (size == NULL) return PWL_ERR_INVALID_ARGUMENT;
    *size = 0;
    status = layout(file, bytes, &l);
    if (status == PWL_OK) *size = l.image.image_size;
    return status;
}

/* Require targets and relocation metadata to belong to actual image sections,
 * not a hole/header. Relocation metadata must be completely file backed.
 */
static int section_contains(const unsigned char *file, const layout_t *l,
                            uint64_t rva, uint64_t size, int raw_only)
{
    size_t i;
    for (i = 0; i < l->sections; ++i) {
        const unsigned char *s = file + l->section_offset + i * 40;
        uint64_t va = u32(s + 12), n = u32(s + 16), vs = u32(s + 8);
        if (!raw_only && vs > n) n = vs;
        if (rva >= va && rva - va <= n && size <= n - (rva - va)) return 1;
    }
    return 0;
}

static pwl_status_t relocate(const unsigned char *file, const layout_t *l,
                              unsigned char *dst, uint64_t target, int apply)
{
    uint64_t pos = l->reloc_rva, limit = pos + l->reloc_size, previous_end = 0;
    uint64_t delta = target - l->image.preferred_base;
    if (!l->reloc_size || (l->characteristics & 1))
        return delta ? PWL_ERR_UNSUPPORTED : PWL_OK;
    if (!section_contains(file, l, pos, l->reloc_size, 1)) return PWL_ERR_BAD_IMAGE;
    while (pos < limit) {
        uint32_t page, block;
        uint64_t i;
        if (limit - pos < 8) return PWL_ERR_BAD_IMAGE;
        page = u32(dst + pos); block = u32(dst + pos + 4);
        if (page % PWL_PAGE_SIZE || page >= l->image.image_size ||
            block < 8 || block % 4 || block > limit - pos) return PWL_ERR_BAD_IMAGE;
        for (i = pos + 8; i < pos + block; i += 2) {
            uint16_t fix = u16(dst + i);
            uint64_t rva = (uint64_t)page + (fix & 4095);
            if ((fix >> 12) == 0) continue;
            if ((fix >> 12) != 10) return PWL_ERR_UNSUPPORTED;
            if (!section_contains(file, l, rva, 8, 0) || rva < previous_end ||
                (rva < limit && (uint64_t)l->reloc_rva < rva + 8))
                return PWL_ERR_BAD_IMAGE;
            /* Strictly ordered, non-overlapping fixups avoid double relocation.
             * Fail closed for unusual unordered images rather than quadratic
             * comparison or allocating a large relocation bitmap in kernel. */
            previous_end = rva + 8;
            if (apply) put64(dst + rva, u64(dst + rva) + delta);
        }
        pos += block;
    }
    return PWL_OK;
}

static int overlap(uintptr_t a, size_t n, uintptr_t b, size_t m)
{
    if (n > UINTPTR_MAX - a || m > UINTPTR_MAX - b) return 1;
    return a < b + m && b < a + n;
}

static void range(pwl_pe_loaded_t *out, uint64_t rva, uint64_t size,
                   unsigned writable, unsigned executable)
{
    if (!size) return;
    out->ranges[out->range_count++] = (pwl_x64_identity_range_t){
        out->physical_address + rva, size, writable, executable};
}

pwl_status_t pwl_pe_load_efi(const void *file, size_t bytes,
                            void *destination, size_t capacity,
                            uint64_t target_pa, pwl_pe_loaded_t *out)
{
    const unsigned char *src = file;
    unsigned char *dst = destination;
    layout_t l;
    pwl_status_t status;
    uint64_t end = 0;
    size_t i;
    if (!file || !destination || !out ||
        overlap((uintptr_t)file, bytes, (uintptr_t)destination, capacity) ||
        overlap((uintptr_t)out, sizeof(*out), (uintptr_t)file, bytes) ||
        overlap((uintptr_t)out, sizeof(*out), (uintptr_t)destination, capacity))
        return PWL_ERR_INVALID_ARGUMENT;
    *out = (pwl_pe_loaded_t){0};
    status = layout(src, bytes, &l);
    if (status != PWL_OK) return status;
    if (target_pa < 0x100000 || target_pa % PWL_PAGE_SIZE ||
        target_pa >= IDENTITY_LIMIT || l.image.image_size > IDENTITY_LIMIT - target_pa)
        return PWL_ERR_INVALID_ARGUMENT;
    if (capacity < l.image.image_size) return PWL_ERR_BUFFER_TOO_SMALL;
    zero(dst, l.image.image_size);
    copy(dst, src, l.headers);
    for (i = 0; i < l.sections; ++i) {
        const unsigned char *s = src + l.section_offset + i * 40;
        if (u32(s + 16))
            copy(dst + u32(s + 12), src + u32(s + 20), u32(s + 16));
    }
    /* Validate the entire stream before changing the first relocation target. */
    status = relocate(src, &l, dst, target_pa, 0);
    if (status != PWL_OK) { zero(dst, l.image.image_size); return status; }
    status = relocate(src, &l, dst, target_pa, 1);
    if (status != PWL_OK) { zero(dst, l.image.image_size); return status; }
    out->physical_address = target_pa;
    out->image_size = l.image.image_size;
    out->entry_address = target_pa + l.image.entry_rva;
    for (i = 0; i < l.sections; ++i) {
        const unsigned char *s = src + l.section_offset + i * 40;
        uint64_t va = u32(s + 12), vs = u32(s + 8), raw = u32(s + 16);
        uint64_t size = page_up(vs > raw ? vs : raw);
        uint32_t flags = u32(s + 36);
        range(out, end, va - end, 0, 0); /* Headers and gaps: read-only NX. */
        range(out, va, size, !!(flags & WRITE), !!(flags & EXEC));
        end = va + size;
    }
    range(out, end, l.image.image_size - end, 0, 0);
    return PWL_OK;
}
