#include "pwl.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *p, uint32_t value)
{
    put16(p, (uint16_t)value);
    put16(p + 2, (uint16_t)(value >> 16));
}

int main(void)
{
    uint8_t file[0x200] = {0};
    pwl_pe_image_t image;
    const size_t pe = 0x80, optional = pe + 24;

    file[0] = 'M'; file[1] = 'Z';
    put32(file + 0x3c, (uint32_t)pe);
    file[pe] = 'P'; file[pe + 1] = 'E';
    put16(file + pe + 4, 0x8664);
    put16(file + pe + 20, 0x70);
    put16(file + optional, 0x20b);
    put32(file + optional + 0x10, 0x1000);
    put32(file + optional + 0x38, 0x2000);
    put32(file + optional + 0x3c, 0x200);
    assert(pwl_pe_inspect(file, sizeof(file), &image) == PWL_OK);
    assert(image.entry_rva == 0x1000 && image.image_size == 0x2000);

    assert(pwl_pe_inspect(file, optional + 0x6f, &image) == PWL_ERR_BAD_IMAGE);
    put32(file + optional + 0x10, 0x2000);
    assert(pwl_pe_inspect(file, sizeof(file), &image) == PWL_ERR_BAD_IMAGE);
    assert(image.file_data == NULL);
    put32(file + optional + 0x10, 0x1000);
    put32(file + optional + 0x3c, 0x3000);
    assert(pwl_pe_inspect(file, sizeof(file), &image) == PWL_ERR_BAD_IMAGE);
    put32(file + optional + 0x3c, 0x200);
    put16(file + pe + 20, 0xffff);
    assert(pwl_pe_inspect(file, sizeof(file), &image) == PWL_ERR_BAD_IMAGE);
    put16(file + pe + 20, 0x70);
    assert(pwl_pe_inspect(file, sizeof(file), &image) == PWL_OK);
    puts("portable PE header checks passed");
    return 0;
}
