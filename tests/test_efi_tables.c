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
    s.callback_offsets[28]=272;s.callback_offsets[29]=288;s.callback_offsets[30]=304;
    for(size_t i=31;i<PWL_EFI_PREPARED_CALLBACKS;i++)s.callback_offsets[i]=i*16;
    assert(pwl_efi_crc32("123456789",9) == UINT32_C(0xcbf43926));
    assert(pwl_efi_crc32(NULL,0) == 0);
    assert(pwl_efi_tables_prepare(&s,&t) == PWL_OK);
    good=t;
    for(size_t i=0;i<PWL_EFI_BOOT_SLOTS;i++)assert(i==17 ? t.boot.functions[i]==0 : t.boot.functions[i]!=0);
    for(size_t i=0;i<4;i++)assert(t.boot.functions[22+i]==s.code_pa+(48+i)*16);
    assert(t.boot.functions[43]==s.code_pa+35*16);
    const unsigned event_slots[]={7,10,11,12,43,9,36,35,34,21};
    for(size_t i=0;i<10;i++)assert(t.boot.functions[event_slots[i]]==s.code_pa+(31+i)*16);
    assert(!t.system.runtime_services); /* Platform runtime is deliberately unpublished. */
    assert(t.runtime.header.signature==UINT64_C(0x56524553544e5552));
    assert(t.runtime.header.header_size==136);
    const unsigned variable_slots[]={6,7,8,13};
    for(size_t i=0;i<PWL_EFI_RUNTIME_SLOTS;i++)assert(i==10 ? !t.runtime.functions[i] : t.runtime.functions[i]);
    for(size_t i=0;i<4;i++)assert(t.runtime.functions[variable_slots[i]]==s.code_pa+(41+i)*16);
    assert(t.boot.functions[8]==s.code_pa+45*16);
    assert(t.boot.functions[28]==s.code_pa+46*16);
    assert(t.boot.functions[29]==s.code_pa+47*16);
    pwl_efi_runtime_table_t runtime=t.runtime;runtime.header.crc32=0;
    assert(pwl_efi_crc32(&runtime,sizeof(runtime))==t.runtime.header.crc32);
    assert(t.system.boot_services == s.data_pa+120);
    assert(t.boot.functions[26] == s.code_pa+80);
    assert(t.boot.functions[40] == s.code_pa+96);
    assert(t.boot.functions[5] == s.code_pa+144);
    assert(t.boot.functions[6] == s.code_pa+160);
    assert(t.boot.functions[13] == s.code_pa+176);
    assert(t.boot.functions[16] == s.code_pa+224);
    assert(t.boot.functions[19] == s.code_pa+240);
    assert(t.boot.functions[37] == s.code_pa+256);
    assert(t.boot.functions[32] == s.code_pa+272);
    assert(t.boot.functions[33] == s.code_pa+288);
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
