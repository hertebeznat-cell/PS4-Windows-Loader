#include "pwl_pe_loader.h"
#include "pe_fixture.h"
#include <assert.h>
#include <stdio.h>

static unsigned char file[PE_FIXTURE_BYTES], original[PE_FIXTURE_BYTES], dst[0x5000];
static pwl_pe_loaded_t loaded;

static void empty_destination(void)
{
    size_t i;
    for (i = 0; i < 0x4000; ++i) assert(dst[i] == 0);
    assert(loaded.image_size == 0 && loaded.range_count == 0 && loaded.entry_address == 0);
}

int main(void)
{
    uint64_t size;
    size_t i;
    const struct { size_t offset; unsigned width; uint32_t value; pwl_status_t status; } bad[] = {
        {PE_OPT + 68, 2, 3, PWL_ERR_UNSUPPORTED}, /* Windows subsystem, not EFI. */
        {PE_OPT + 108, 4, 17, PWL_ERR_BAD_IMAGE},
        {PE_OPT + 120, 4, 0x2000, PWL_ERR_UNSUPPORTED}, /* Imports. */
        {PE_OPT + 112 + 9*8, 4, 0x2000, PWL_ERR_UNSUPPORTED}, /* TLS. */
        {PE_OPT + 32, 4, 0x800, PWL_ERR_BAD_IMAGE},
        {PE_OPT + 36, 4, 0, PWL_ERR_BAD_IMAGE},
        {PE_SEC + 36, 4, 0xe0000020, PWL_ERR_UNSUPPORTED}, /* W+X. */
        {PE_SEC + 40 + 12, 4, 0x1000, PWL_ERR_BAD_IMAGE}, /* VA overlap. */
        {PE_SEC + 40 + 20, 4, 0x200, PWL_ERR_BAD_IMAGE}, /* Raw overlap. */
        {PE_OPT + 16, 4, 0x1800, PWL_ERR_BAD_IMAGE}, /* Entry not file backed. */
        {PE_SEC + 20, 4, 0xffffff00, PWL_ERR_BAD_IMAGE},
        {PE_OPT + 56, 4, 0x3800, PWL_ERR_BAD_IMAGE}
    };
    pe_fixture(file);
    assert(pwl_pe_efi_size(file, sizeof(file), &size) == PWL_OK && size == 0x4000);
    for (i = 0; i < sizeof(file); ++i)
        assert(pwl_pe_efi_size(file, i, &size) != PWL_OK);
    memcpy(original, file, sizeof(file));
    memset(dst, 0x5a, sizeof(dst));
    assert(pwl_pe_load_efi(file, sizeof(file), dst, sizeof(dst), 0x4000000, &loaded) == PWL_OK);
    assert(memcmp(original, file, sizeof(file)) == 0);
    assert(pe_get64(dst + 0x2000) == 0x4001010);
    assert(dst[0x1000] == 0xc3 && dst[0x2200] == 0 && dst[0x4000] == 0x5a);
    assert(loaded.entry_address == 0x4001000 && loaded.range_count == 4);
    assert(!loaded.ranges[0].writable && !loaded.ranges[0].executable);
    assert(!loaded.ranges[1].writable && loaded.ranges[1].executable);
    assert(loaded.ranges[2].writable && !loaded.ranges[2].executable);
    assert(!loaded.ranges[3].writable && !loaded.ranges[3].executable);
    assert(pwl_pe_load_efi(file, sizeof(file), dst, sizeof(dst), UINT64_C(0x200000000), &loaded) == PWL_OK);
    assert(pe_get64(dst + 0x2000) == UINT64_C(0x200001010));
    for (i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) {
        pe_fixture(file);
        if (bad[i].width == 2) pe16(file + bad[i].offset, (uint16_t)bad[i].value);
        else pe32(file + bad[i].offset, bad[i].value);
        memset(dst, 0x5a, sizeof(dst));
        assert(pwl_pe_load_efi(file, sizeof(file), dst, sizeof(dst), 0x4000000, &loaded) == bad[i].status);
        assert(dst[0] == 0x5a && loaded.entry_address == 0);
    }
    pe_fixture(file);
    assert(pwl_pe_load_efi(file, sizeof(file), dst, 0x3fff, 0x4000000, &loaded) == PWL_ERR_BUFFER_TOO_SMALL);
    assert(pwl_pe_load_efi(file, sizeof(file), dst, sizeof(dst), UINT64_MAX - 4095, &loaded) == PWL_ERR_INVALID_ARGUMENT);
    assert(pwl_pe_load_efi(file, sizeof(file), file, sizeof(file), 0x4000000, &loaded) == PWL_ERR_INVALID_ARGUMENT);
    assert(pwl_pe_efi_size(NULL, 0, &size) == PWL_ERR_INVALID_ARGUMENT && size == 0);
    /* Fixups must not edit their own metadata, a hole/header, or overlap. */
    pe32(file + 0x600, 0x3000);
    assert(pwl_pe_load_efi(file, sizeof(file), dst, sizeof(dst), 0x4000000, &loaded) == PWL_ERR_BAD_IMAGE);
    empty_destination();
    pe_fixture(file); pe32(file + 0x604, 0xfffffff0);
    assert(pwl_pe_load_efi(file, sizeof(file), dst, sizeof(dst), 0x4000000, &loaded) == PWL_ERR_BAD_IMAGE);
    empty_destination();
    pe_fixture(file); pe16(file + 0x60a, 0xa000);
    assert(pwl_pe_load_efi(file, sizeof(file), dst, sizeof(dst), 0x4000000, &loaded) == PWL_ERR_BAD_IMAGE);
    empty_destination();
    pe_fixture(file); pe16(file + 0x60a, 0x3008); /* Bad second fixup after valid first. */
    assert(pwl_pe_load_efi(file, sizeof(file), dst, sizeof(dst), 0x4000000, &loaded) == PWL_ERR_UNSUPPORTED);
    empty_destination();
    pe_fixture(file); pe32(file + 0x600, 0);
    assert(pwl_pe_load_efi(file, sizeof(file), dst, sizeof(dst), 0x4000000, &loaded) == PWL_ERR_BAD_IMAGE);
    empty_destination();
    pe_fixture(file); pe32(file + PE_OPT + 112 + 5*8, 0); pe32(file + PE_OPT + 116 + 5*8, 0);
    assert(pwl_pe_load_efi(file, sizeof(file), dst, sizeof(dst), PE_BASE, &loaded) == PWL_OK);
    assert(pe_get64(dst + 0x2000) == PE_BASE + 0x1010);
    assert(pwl_pe_load_efi(file, sizeof(file), dst, sizeof(dst), 0x4000000, &loaded) == PWL_ERR_UNSUPPORTED);
    empty_destination();
    /* Zero-raw BSS may have an ignored file offset; never form that pointer. */
    pe_fixture(file); pe32(file + PE_SEC + 40 + 16, 0);
    pe32(file + PE_SEC + 40 + 20, UINT32_MAX);
    pe32(file + PE_OPT + 112 + 5*8, 0); pe32(file + PE_OPT + 116 + 5*8, 0);
    assert(pwl_pe_load_efi(file, sizeof(file), dst, sizeof(dst), PE_BASE, &loaded) == PWL_OK);
    assert(pe_get64(dst + 0x2000) == 0);
    puts("native EFI PE: physical relocations, section permissions, malformed input and rollback passed");
    return 0;
}
