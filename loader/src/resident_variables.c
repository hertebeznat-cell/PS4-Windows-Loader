#include "pwl_resident.h"
#define EFI __attribute__((ms_abi))
#define BOOT_ACCESS 2U
#define APPEND_WRITE 64U
extern const uint64_t pwl_resident_binding __attribute__((visibility("hidden")));
static pwl_resident_data_t *state(void)
{ return (pwl_resident_data_t *)(uintptr_t)pwl_resident_binding; }

static size_t name_units(const uint16_t *name,size_t capacity)
{
    if (!name) return 0;
    if (capacity>PWL_VARIABLE_NAME_UNITS) capacity=PWL_VARIABLE_NAME_UNITS;
    for (size_t i=0;i<capacity;i++) if (!name[i]) return i+1;
    return 0;
}
static int guid_equal(const pwl_efi_guid_t *a,const pwl_efi_guid_t *b)
{
    for (size_t i=0;i<16;i++) if (a->bytes[i]!=b->bytes[i]) return 0;
    return 1;
}
static pwl_resident_variable_t *find(pwl_resident_data_t *d,const uint16_t *name,
    size_t units,const pwl_efi_guid_t *guid)
{
    for (size_t i=0;i<PWL_RESIDENT_VARIABLES;i++) {
        pwl_resident_variable_t *v=&d->variables[i];
        if (v->name_units!=units || !guid_equal(&v->guid,guid)) continue;
        size_t j;
        for (j=0;j<units;j++) if (v->name[j]!=name[j]) break;
        if (j==units) return v;
    }
    return NULL;
}
static uint64_t attribute_status(uint32_t attributes)
{
    if (attributes&~UINT32_C(0x7f) || ((attributes&4) && !(attributes&2)))
        return PWL_EFI_INVALID_PARAMETER;
    /* No persistent storage, runtime lifetime, authentication or error records
     * are advertised. Returning success here would lose caller state on reset. */
    if (attributes&UINT32_C(0x3d)) return PWL_EFI_UNSUPPORTED;
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_get_variable(const uint16_t *name,const pwl_efi_guid_t *guid,
    uint32_t *attributes,size_t *size,void *data)
{
    pwl_resident_data_t *d=state();
    size_t units=name_units(name,PWL_VARIABLE_NAME_UNITS);
    if (!d || !units || !guid || !size) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_UNSUPPORTED;
    pwl_resident_variable_t *v=find(d,name,units,guid);
    if (!v) return PWL_EFI_NOT_FOUND;
    if (*size<v->data_size) { *size=v->data_size;return PWL_EFI_BUFFER_TOO_SMALL; }
    if (!data) return PWL_EFI_INVALID_PARAMETER;
    /* Snapshot before output writes, including possible overlapping buffers. */
    unsigned char copy[PWL_VARIABLE_MAX_BYTES];
    size_t bytes=v->data_size;uint32_t flags=v->attributes;
    for (size_t i=0;i<bytes;i++) copy[i]=v->data[i];
    for (size_t i=0;i<bytes;i++) ((unsigned char *)data)[i]=copy[i];
    if (attributes) *attributes=flags;
    *size=bytes;
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_set_variable(const uint16_t *name,const pwl_efi_guid_t *guid,
    uint32_t attributes,size_t size,const void *data)
{
    pwl_resident_data_t *d=state();
    size_t units=name_units(name,PWL_VARIABLE_NAME_UNITS);
    if (!d || units<2 || !guid) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_UNSUPPORTED;
    uint64_t status=attribute_status(attributes);
    if (status) return status;
    pwl_resident_variable_t *v=find(d,name,units,guid);
    int append=(attributes&APPEND_WRITE)!=0;
    if (!attributes || (!size && !append)) {
        if (!v) return PWL_EFI_NOT_FOUND;
        *v=(pwl_resident_variable_t){0};
        return PWL_EFI_SUCCESS;
    }
    if ((attributes&~APPEND_WRITE)!=BOOT_ACCESS || (size && !data))
        return PWL_EFI_INVALID_PARAMETER;
    if (append && v && v->attributes!=(attributes&~APPEND_WRITE))
        return PWL_EFI_INVALID_PARAMETER;
    if (append && !size) return PWL_EFI_SUCCESS;
    size_t prefix=append && v ? v->data_size : 0;
    size_t name_bytes=units*sizeof(*name);
    if (size>PWL_VARIABLE_MAX_BYTES-name_bytes ||
        prefix>PWL_VARIABLE_MAX_BYTES-name_bytes-size) return PWL_EFI_INVALID_PARAMETER;
    if (!v) {
        for (size_t i=0;i<PWL_RESIDENT_VARIABLES;i++)
            if (!d->variables[i].name_units) { v=&d->variables[i];break; }
        if (!v) return PWL_EFI_OUT_OF_RESOURCES;
    }
    uint16_t name_copy[PWL_VARIABLE_NAME_UNITS];pwl_efi_guid_t guid_copy=*guid;
    unsigned char data_copy[PWL_VARIABLE_MAX_BYTES];
    for (size_t i=0;i<units;i++) name_copy[i]=name[i];
    for (size_t i=0;i<prefix;i++) data_copy[i]=v->data[i];
    for (size_t i=0;i<size;i++) data_copy[prefix+i]=((const unsigned char *)data)[i];
    *v=(pwl_resident_variable_t){0};
    v->guid=guid_copy;v->name_units=units;v->data_size=prefix+size;
    v->attributes=attributes&~APPEND_WRITE;
    for (size_t i=0;i<units;i++) v->name[i]=name_copy[i];
    for (size_t i=0;i<prefix+size;i++) v->data[i]=data_copy[i];
    return PWL_EFI_SUCCESS;
}
static int before(const pwl_resident_variable_t *a,const pwl_resident_variable_t *b)
{
    for (size_t i=0;;i++) {
        if (a->name[i]!=b->name[i]) return a->name[i]<b->name[i];
        if (!a->name[i]) break;
    }
    for (size_t i=0;i<16;i++)
        if (a->guid.bytes[i]!=b->guid.bytes[i]) return a->guid.bytes[i]<b->guid.bytes[i];
    return 0;
}
uint64_t EFI pwl_resident_get_next_variable_name(size_t *size,uint16_t *name,pwl_efi_guid_t *guid)
{
    pwl_resident_data_t *d=state();
    if (!d || !size || !name || !guid) return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_UNSUPPORTED;
    size_t units=name_units(name,*size/sizeof(*name));
    if (!units) return PWL_EFI_INVALID_PARAMETER;
    pwl_resident_variable_t *cursor=NULL,*next=NULL;
    if (units>1) {
        cursor=find(d,name,units,guid);
        if (!cursor) return PWL_EFI_INVALID_PARAMETER;
    }
    for (size_t i=0;i<PWL_RESIDENT_VARIABLES;i++) {
        pwl_resident_variable_t *v=&d->variables[i];
        if (!v->name_units || (cursor && !before(cursor,v))) continue;
        if (!next || before(v,next)) next=v;
    }
    if (!next) return PWL_EFI_NOT_FOUND;
    size_t bytes=next->name_units*sizeof(*name);
    if (*size<bytes) { *size=bytes;return PWL_EFI_BUFFER_TOO_SMALL; }
    uint16_t copy[PWL_VARIABLE_NAME_UNITS];pwl_efi_guid_t value=next->guid;
    units=next->name_units;
    for (size_t i=0;i<units;i++) copy[i]=next->name[i];
    for (size_t i=0;i<units;i++) name[i]=copy[i];
    *guid=value;*size=bytes;
    return PWL_EFI_SUCCESS;
}
uint64_t EFI pwl_resident_query_variable_info(uint32_t attributes,uint64_t *maximum,
    uint64_t *remaining,uint64_t *variable_maximum)
{
    pwl_resident_data_t *d=state();
    if (!d || !maximum || !remaining || !variable_maximum || !attributes)
        return PWL_EFI_INVALID_PARAMETER;
    if (d->memory.exited) return PWL_EFI_UNSUPPORTED;
    uint64_t status=attribute_status(attributes);
    if (status) return status;
    if (attributes!=BOOT_ACCESS) return PWL_EFI_UNSUPPORTED;
    size_t free_slots=0;
    for (size_t i=0;i<PWL_RESIDENT_VARIABLES;i++) if (!d->variables[i].name_units) free_slots++;
    *maximum=sizeof(d->variables);
    *remaining=free_slots*sizeof(d->variables[0]);
    *variable_maximum=PWL_VARIABLE_MAX_BYTES;
    return PWL_EFI_SUCCESS;
}
