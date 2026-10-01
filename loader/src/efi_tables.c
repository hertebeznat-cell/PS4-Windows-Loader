#include "pwl_efi_tables.h"
#include <stddef.h>

_Static_assert(sizeof(pwl_efi_header_t) == 24, "EFI header ABI");
_Static_assert(sizeof(pwl_efi_system_table_t) == 120, "AMD64 System Table ABI");
_Static_assert(sizeof(pwl_efi_runtime_table_t)==136,"AMD64 Runtime Services ABI");
_Static_assert(sizeof(pwl_efi_boot_table_t) == 376, "AMD64 Boot Services ABI");
_Static_assert(offsetof(pwl_efi_system_table_t, boot_services) == 96, "Boot Services offset");
_Static_assert(offsetof(pwl_efi_boot_table_t, functions) == 24, "Boot Services slots");

static const unsigned slots[PWL_EFI_BOOT_CALLBACKS] =
    {0,1,2,3,4,26,40,41,42,5,6,13,14,15,16,19,37,32,33,7,10,11,12,43,9,36,35,34,21,8,28,29,22,23,24,25};
static const unsigned callback_indices[PWL_EFI_BOOT_CALLBACKS] =
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,28,29,31,32,33,34,35,36,37,38,39,40,45,46,47,48,49,50,51};

uint32_t pwl_efi_crc32(const void *bytes, size_t size)
{
    const unsigned char *p = bytes;
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < size; ++i) {
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320) & (0U - (crc & 1U)));
    }
    return ~crc;
}

static int span(uint64_t base, uint64_t size)
{
    /* Four-level lower canonical identity space; zero and wrap forbidden. */
    return base && size && base < (UINT64_C(1) << 47) &&
           size <= (UINT64_C(1) << 47) - base;
}

static int valid_spec(const pwl_efi_table_spec_t *s)
{
    if (!s || !span(s->code_pa, s->code_bytes) ||
        !span(s->data_pa, s->data_bytes) || (s->data_pa & 7) ||
        s->data_bytes < sizeof(pwl_efi_prepared_tables_t) ||
        (s->code_pa < s->data_pa + s->data_bytes &&
         s->data_pa < s->code_pa + s->code_bytes)) return 0;
    for (size_t i = 0; i < PWL_EFI_PREPARED_CALLBACKS; ++i)
        if (s->callback_offsets[i] >= s->code_bytes) return 0;
    return 1;
}

pwl_status_t pwl_efi_tables_prepare(const pwl_efi_table_spec_t *s,
                                    pwl_efi_prepared_tables_t *out)
{
    if (!out || !valid_spec(s)) return PWL_ERR_INVALID_ARGUMENT;
    /* Snapshot inputs before touching output, including possible aliasing. */
    pwl_efi_table_spec_t spec = *s;
    *out = (pwl_efi_prepared_tables_t){0};
    out->system.header = (pwl_efi_header_t){UINT64_C(0x5453595320494249),
                                          0x00020000, sizeof(out->system), 0, 0};
    out->boot.header = (pwl_efi_header_t){UINT64_C(0x56524553544f4f42),
                                        0x00020000, sizeof(out->boot), 0, 0};
    out->runtime.header=(pwl_efi_header_t){UINT64_C(0x56524553544e5552),
        0x00020000,sizeof(out->runtime),0,0};
    /* An unpublished preparation table. ResetSystem has a VOID/non-returning
     * contract and cannot use the status-returning unsupported adapter. Keep
     * it absent and do not publish SystemTable.RuntimeServices yet. */
    for (size_t i=0;i<PWL_EFI_RUNTIME_SLOTS;i++)
        if (i!=10) out->runtime.functions[i]=spec.code_pa+spec.callback_offsets[30];
    const unsigned variable_slots[]={6,7,8,13};
    for (size_t i=0;i<4;i++)
        out->runtime.functions[variable_slots[i]]=spec.code_pa+spec.callback_offsets[41+i];
    out->runtime.functions[0]=spec.code_pa+spec.callback_offsets[66];
    out->runtime.header.crc32=pwl_efi_crc32(&out->runtime,sizeof(out->runtime));
    out->system.boot_services = spec.data_pa + offsetof(pwl_efi_prepared_tables_t, boot);
    /* Preserve Reserved at slot 17 as NULL. Unsupported services have a real
     * ABI adapter so an attempted call returns an error instead of branching
     * through address zero. This does not implement those services. */
    for (size_t i = 0; i < PWL_EFI_BOOT_SLOTS; ++i)
        if (i != 17) out->boot.functions[i] = spec.code_pa + spec.callback_offsets[30];
    for (size_t i = 0; i < PWL_EFI_BOOT_CALLBACKS; ++i)
        out->boot.functions[slots[i]] = spec.code_pa + spec.callback_offsets[callback_indices[i]];
    out->boot.header.crc32 = pwl_efi_crc32(&out->boot, sizeof(out->boot));
    out->system.header.crc32 = pwl_efi_crc32(&out->system, sizeof(out->system));
    return PWL_OK;
}

pwl_status_t pwl_efi_tables_validate(const pwl_efi_table_spec_t *s,
                                    const pwl_efi_prepared_tables_t *tables)
{
    pwl_efi_prepared_tables_t expected;
    if (!tables || pwl_efi_tables_prepare(s, &expected) != PWL_OK)
        return PWL_ERR_INVALID_ARGUMENT;
    /* Exact reconstruction checks unused slots too, even if CRC is recomputed.
     * Header sizes from untrusted input are never used as read lengths.
     */
    const unsigned char *a = (const unsigned char *)&expected;
    const unsigned char *b = (const unsigned char *)tables;
    for (size_t i = 0; i < sizeof(expected); ++i)
        if (a[i] != b[i]) return PWL_ERR_INVALID_ARGUMENT;
    return PWL_OK;
}
