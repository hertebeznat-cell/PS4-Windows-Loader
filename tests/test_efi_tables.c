#include "pwl_efi_tables.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    pwl_efi_table_spec_t s = {UINT64_C(0x27a300000),4096,
                              UINT64_C(0x27a301000),4096,
                              {0,16,32,48,64,80,96,112,128,144,160,176,192,208,224,240,256}};
    pwl_efi_prepared_tables_t t, good;
    assert(pwl_efi_crc32("123456789",9) == UINT32_C(0xcbf43926));
    assert(pwl_efi_crc32(NULL,0) == 0);
    assert(pwl_efi_tables_prepare(&s,&t) == PWL_OK);
    good=t;
    assert(t.system.boot_services == s.data_pa+120);
    assert(t.boot.functions[26] == s.code_pa+80);
    assert(t.boot.functions[40] == s.code_pa+96);
    assert(t.boot.functions[5] == s.code_pa+144);
    assert(t.boot.functions[6] == s.code_pa+160);
    assert(t.boot.functions[13] == s.code_pa+176);
    assert(t.boot.functions[16] == s.code_pa+224);
    assert(t.boot.functions[19] == s.code_pa+240);
    assert(t.boot.functions[37] == s.code_pa+256);
    assert(pwl_efi_tables_validate(&s,&t) == PWL_OK);
    /* Every byte, including padding, absent services and CRC, is checked. */
    for (size_t i=0;i<sizeof(t);++i) {
        ((unsigned char *)&t)[i]^=1;
        assert(pwl_efi_tables_validate(&s,&t) != PWL_OK);
        t=good;
    }
    t.boot.functions[2]=(uint32_t)t.boot.functions[2];
    t.boot.header.crc32=0;
    t.boot.header.crc32=pwl_efi_crc32(&t.boot,sizeof(t.boot));
    assert(pwl_efi_tables_validate(&s,&t) != PWL_OK);
    t=good; t.boot.functions[17]=s.code_pa;
    t.boot.header.crc32=0;
    t.boot.header.crc32=pwl_efi_crc32(&t.boot,sizeof(t.boot));
    assert(pwl_efi_tables_validate(&s,&t) != PWL_OK);
    for (size_t i=0;i<PWL_EFI_PREPARED_CALLBACKS;++i) {
        uint64_t saved=s.callback_offsets[i];
        s.callback_offsets[i]=s.code_bytes;
        t=good;
        assert(pwl_efi_tables_prepare(&s,&t) != PWL_OK);
        assert(memcmp(&t,&good,sizeof(t)) == 0);
        s.callback_offsets[i]=saved;
    }
    pwl_efi_table_spec_t bad=s;
    bad.code_pa=UINT64_MAX-16;
    assert(pwl_efi_tables_prepare(&bad,&t) != PWL_OK);
    bad=s;bad.data_pa=s.code_pa;
    assert(pwl_efi_tables_prepare(&bad,&t) != PWL_OK);
    bad=s;bad.data_bytes=sizeof(t)-1;
    assert(pwl_efi_tables_prepare(&bad,&t) != PWL_OK);
    bad=s;bad.data_pa++;
    assert(pwl_efi_tables_prepare(&bad,&t) != PWL_OK);
    bad=s;bad.code_pa=(UINT64_C(1)<<47)-4095;
    assert(pwl_efi_tables_prepare(&bad,&t) != PWL_OK);
    assert(pwl_efi_tables_prepare(NULL,&t) != PWL_OK);
    assert(pwl_efi_tables_prepare(&s,NULL) != PWL_OK);
    assert(pwl_efi_tables_validate(&s,NULL) != PWL_OK);
    puts("EFI table preparation: AMD64 layout, high addresses, CRC and corruption checks passed");
}
