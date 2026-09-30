#include "pwl_firmware.h"

#define CONVENTIONAL 7U

static uint64_t end_of(const pwl_fw_memory_entry_t *entry)
{
    return entry->descriptor.physical_start +
           entry->descriptor.number_of_pages * PWL_PAGE_SIZE;
}

static int active(const pwl_fw_memory_t *m)
{
    return m != NULL && m->count != 0 && m->key != 0 && !m->exited;
}

pwl_status_t pwl_fw_memory_init(pwl_fw_memory_t *m,
                                const pwl_phys_region_t *regions,
                                const uint64_t *cacheability, size_t count,
                                uint64_t image_handle)
{
    size_t i, needed;
    pwl_efi_memory_descriptor_t d;
    if (m == NULL || image_handle == 0 || count > PWL_FW_MAX_DESCRIPTORS ||
        pwl_memory_map_validate(regions, count) != PWL_OK || cacheability == NULL)
        return PWL_ERR_INVALID_ARGUMENT;
    /* Validate every attribute before writing anything into the destination. */
    for (i = 0; i < count; ++i)
        if (pwl_efi_descriptors_from_regions(regions + i, cacheability + i,
                                              1, &d, 1, &needed) != PWL_OK)
            return PWL_ERR_INVALID_ARGUMENT;
    for (i = 0; i < count; ++i) {
        (void)pwl_efi_descriptors_from_regions(regions + i, cacheability + i,
                    1, &m->entries[i].descriptor, 1, &needed);
        m->entries[i].allocated = 0;
    }
    m->count = count;
    m->key = 1;
    m->issued_key = 0;
    m->image_handle = image_handle;
    m->exited = 0;
    for (i=0;i<PWL_FW_MAX_POOLS;i++) {
        m->pools[i].address=0;
        m->pools[i].pages=0;
    }
    return PWL_OK;
}

uint64_t pwl_fw_allocate_pool(pwl_fw_memory_t *m,unsigned type,size_t bytes,uint64_t *out)
{
    if (!out || !m || !m->count || !m->key)
        return PWL_EFI_INVALID_PARAMETER;
    if (m->exited) return PWL_EFI_ACCESS_DENIED;
    /* Zero bytes still has a unique freeable allocation. No hidden header. */
    uint64_t pages=bytes/PWL_PAGE_SIZE+(bytes%PWL_PAGE_SIZE!=0);
    if (!pages) pages=1;
    if (pages>UINT64_MAX/PWL_PAGE_SIZE) return PWL_EFI_OUT_OF_RESOURCES;
    size_t slot;
    for (slot=0;slot<PWL_FW_MAX_POOLS;slot++)
        if (!m->pools[slot].address) break;
    if (slot==PWL_FW_MAX_POOLS) return PWL_EFI_OUT_OF_RESOURCES;
    uint64_t address=0;
    uint64_t status=pwl_fw_allocate_pages(m,PWL_ALLOCATE_ANY,type,pages,&address);
    if (status!=PWL_EFI_SUCCESS) return status;
    m->pools[slot].address=address;
    m->pools[slot].pages=pages;
    *out=address;
    return PWL_EFI_SUCCESS;
}

uint64_t pwl_fw_free_pool(pwl_fw_memory_t *m,uint64_t address)
{
    if (!m || !address || !m->count || !m->key) return PWL_EFI_INVALID_PARAMETER;
    if (m->exited) return PWL_EFI_ACCESS_DENIED;
    for (size_t i=0;i<PWL_FW_MAX_POOLS;i++) {
        if (m->pools[i].address!=address) continue;
        uint64_t pages=m->pools[i].pages;
        m->pools[i].address=0;
        uint64_t status=pwl_fw_free_pages(m,address,pages);
        if (status==PWL_EFI_SUCCESS) {
            m->pools[i].address=0;
            m->pools[i].pages=0;
        } else m->pools[i].address=address;
        return status;
    }
    return PWL_EFI_INVALID_PARAMETER;
}

static void coalesce(pwl_fw_memory_t *m)
{
    size_t i = 0, j;
    while (i + 1 < m->count) {
        pwl_fw_memory_entry_t *a = &m->entries[i], *b = &m->entries[i + 1];
        if (end_of(a) == b->descriptor.physical_start &&
            a->allocated == b->allocated &&
            a->descriptor.type == b->descriptor.type &&
            a->descriptor.attribute == b->descriptor.attribute) {
            a->descriptor.number_of_pages += b->descriptor.number_of_pages;
            for (j = i + 1; j + 1 < m->count; ++j)
                m->entries[j] = m->entries[j + 1];
            --m->count;
        } else ++i;
    }
}

/* Replace one span transactionally; insufficient descriptor space changes
 * neither the map nor its key. No heap or kernel calls are made here.
 */
static uint64_t retype(pwl_fw_memory_t *m, size_t index, uint64_t start,
                       uint64_t pages, unsigned type, unsigned allocated)
{
    pwl_fw_memory_entry_t old = m->entries[index], part;
    uint64_t end = start + pages * PWL_PAGE_SIZE, old_end = end_of(&old);
    size_t before = start != old.descriptor.physical_start;
    size_t after = end != old_end;
    size_t extra = before + after, i, at = index;
    if (extra > PWL_FW_MAX_DESCRIPTORS - m->count || m->key == UINT64_MAX)
        return PWL_EFI_OUT_OF_RESOURCES;
    for (i = m->count; i > index + 1; --i)
        m->entries[i - 1 + extra] = m->entries[i - 1];
    if (before) {
        part = old;
        part.descriptor.number_of_pages =
            (start - old.descriptor.physical_start) / PWL_PAGE_SIZE;
        m->entries[at++] = part;
    }
    part = old;
    part.descriptor.physical_start = start;
    part.descriptor.number_of_pages = pages;
    part.descriptor.type = type;
    part.allocated = allocated;
    m->entries[at++] = part;
    if (after) {
        part = old;
        part.descriptor.physical_start = end;
        part.descriptor.number_of_pages = (old_end - end) / PWL_PAGE_SIZE;
        m->entries[at] = part;
    }
    m->count += extra;
    coalesce(m);
    ++m->key;
    return PWL_EFI_SUCCESS;
}

uint64_t pwl_fw_allocate_pages(pwl_fw_memory_t *m, unsigned allocation_type,
                               unsigned type, uint64_t pages, uint64_t *address)
{
    size_t i, best = SIZE_MAX;
    uint64_t bytes, candidate = 0;
    if (!active(m)) return PWL_EFI_ACCESS_DENIED;
    if (address == NULL || pages == 0 || pages > UINT64_MAX / PWL_PAGE_SIZE ||
        allocation_type > PWL_ALLOCATE_ADDRESS)
        return PWL_EFI_INVALID_PARAMETER;
    /* Runtime/ACPI allocations need additional lifetime and mapping support. */
    if (type < 1 || type > 4) return PWL_EFI_UNSUPPORTED;
    bytes = pages * PWL_PAGE_SIZE;
    if (allocation_type == PWL_ALLOCATE_ADDRESS &&
        (*address % PWL_PAGE_SIZE || bytes > UINT64_MAX - *address))
        return PWL_EFI_INVALID_PARAMETER;
    for (i = 0; i < m->count; ++i) {
        const pwl_fw_memory_entry_t *e = &m->entries[i];
        uint64_t lo = e->descriptor.physical_start, hi = end_of(e), start;
        if (e->descriptor.type != CONVENTIONAL || hi - lo < bytes) continue;
        /* Do not return NULL as an EFI pointer, even if page zero is RAM. */
        if (lo == 0) lo = PWL_PAGE_SIZE;
        if (lo > hi || hi - lo < bytes) continue;
        if (allocation_type == PWL_ALLOCATE_ADDRESS) {
            start = *address;
            if (start < lo || start > hi - bytes) continue;
        } else if (allocation_type == PWL_ALLOCATE_MAX) {
            if (*address != UINT64_MAX && hi > *address + 1) hi = *address + 1;
            hi &= ~(PWL_PAGE_SIZE - 1);
            if (hi < lo || hi - lo < bytes) continue;
            start = hi - bytes;
        } else start = lo;
        if (best == SIZE_MAX || start > candidate) { best = i; candidate = start; }
        if (allocation_type != PWL_ALLOCATE_MAX) break;
    }
    if (best == SIZE_MAX) return allocation_type == PWL_ALLOCATE_ANY ?
                                 PWL_EFI_OUT_OF_RESOURCES : PWL_EFI_NOT_FOUND;
    {
        uint64_t result = retype(m, best, candidate, pages, type, 1);
        if (result == PWL_EFI_SUCCESS) *address = candidate;
        return result;
    }
}

uint64_t pwl_fw_free_pages(pwl_fw_memory_t *m, uint64_t address, uint64_t pages)
{
    size_t i;
    uint64_t bytes;
    if (!active(m)) return PWL_EFI_ACCESS_DENIED;
    if (!pages || address % PWL_PAGE_SIZE ||
        pages > (UINT64_MAX - address) / PWL_PAGE_SIZE)
        return PWL_EFI_INVALID_PARAMETER;
    bytes = pages * PWL_PAGE_SIZE;
    for (i=0;i<PWL_FW_MAX_POOLS;i++) {
        uint64_t pool=m->pools[i].address;
        if (pool && address<pool+m->pools[i].pages*PWL_PAGE_SIZE &&
            pool<address+bytes) return PWL_EFI_INVALID_PARAMETER;
    }
    for (i = 0; i < m->count; ++i) {
        pwl_fw_memory_entry_t *e = &m->entries[i];
        if (e->allocated && address >= e->descriptor.physical_start &&
            address < end_of(e) && bytes <= end_of(e) - address)
            return retype(m, i, address, pages, CONVENTIONAL, 0);
    }
    return PWL_EFI_NOT_FOUND;
}

uint64_t pwl_fw_get_memory_map(pwl_fw_memory_t *m, size_t *size,
                              pwl_efi_memory_descriptor_t *map, uint64_t *key,
                              size_t *descriptor_size, uint32_t *version)
{
    size_t i, needed;
    if (!active(m)) return PWL_EFI_ACCESS_DENIED;
    if (!size || !key || !descriptor_size || !version)
        return PWL_EFI_INVALID_PARAMETER;
    needed = m->count * sizeof(*map);
    *descriptor_size = sizeof(*map);
    *version = 1;
    if (*size < needed) { *size = needed; return PWL_EFI_BUFFER_TOO_SMALL; }
    if (!map) return PWL_EFI_INVALID_PARAMETER;
    for (i = 0; i < m->count; ++i) map[i] = m->entries[i].descriptor;
    *size = needed;
    *key = m->key;
    m->issued_key = m->key;
    return PWL_EFI_SUCCESS;
}

uint64_t pwl_fw_memory_exit(pwl_fw_memory_t *m, uint64_t image_handle, uint64_t key)
{
    if (!active(m)) return PWL_EFI_ACCESS_DENIED;
    if (image_handle != m->image_handle || key != m->key || key != m->issued_key)
        return PWL_EFI_INVALID_PARAMETER;
    m->exited = 1;
    return PWL_EFI_SUCCESS;
}
