#ifndef PWL_RESIDENT_TEST_IMAGE_H
#define PWL_RESIDENT_TEST_IMAGE_H
#include "resident_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include "rx_test_mapping.h"
#define EFI __attribute__((ms_abi))
typedef struct resident_test_image {
    unsigned char *code;
    size_t size;
    pwl_resident_data_t *data;
    pwl_resident_image_t image;
} resident_test_image_t;
static resident_test_image_t resident_test_open(void)
{
    resident_test_image_t t={0};t.image=resident_fixture();
    t.size=(t.image.size+4095)&~(size_t)4095;
    t.code=rx_test_allocate(t.size);
    t.data=calloc(1,sizeof(*t.data));
    assert(t.code!=MAP_FAILED && (uintptr_t)t.code>UINT32_MAX && t.data);
    memcpy(t.code,t.image.bytes,t.image.size);
    uint64_t address=(uintptr_t)t.data;
    memcpy(t.code+t.image.binding_offset,&address,sizeof(address));
    assert(mprotect(t.code,t.size,PROT_READ|PROT_EXEC)==0);t.data->tpl=4;
    pwl_efi_table_spec_t spec={0};spec.code_pa=(uintptr_t)t.code;spec.code_bytes=t.image.size;
    spec.data_pa=(uintptr_t)&t.data->efi;spec.data_bytes=sizeof(t.data->efi);
    for(size_t i=0;i<PWL_EFI_PREPARED_CALLBACKS;i++)spec.callback_offsets[i]=t.image.callbacks[i];
    assert(pwl_efi_tables_prepare(&spec,&t.data->efi)==PWL_OK);
    return t;
}
static void resident_test_close(resident_test_image_t *t)
{ assert(rx_test_release(t->code,t->size)==0);free(t->data); }
#define LOAD_FROM(address,type,name) type name; do { \
    uintptr_t target=(uintptr_t)(address); \
    _Static_assert(sizeof(name)==sizeof(target),"AMD64 callback pointer"); \
    memcpy(&name,&target,sizeof(name)); } while(0)
#endif
