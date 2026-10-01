#define _GNU_SOURCE
#include "resident_test_image.h"
typedef uint64_t (EFI *get_fn)(const uint16_t *,const pwl_efi_guid_t *,uint32_t *,size_t *,void *);
typedef uint64_t (EFI *set_fn)(const uint16_t *,const pwl_efi_guid_t *,uint32_t,size_t,const void *);
typedef uint64_t (EFI *next_fn)(size_t *,uint16_t *,pwl_efi_guid_t *);
typedef uint64_t (EFI *query_fn)(uint32_t,uint64_t *,uint64_t *,uint64_t *);
int main(void)
{
    resident_test_image_t t=resident_test_open();
    LOAD_FROM(t.data->efi.runtime.functions[6],get_fn,get);
    LOAD_FROM(t.data->efi.runtime.functions[8],set_fn,set);
    LOAD_FROM(t.data->efi.runtime.functions[7],next_fn,next);
    LOAD_FROM(t.data->efi.runtime.functions[13],query_fn,query);
    assert(!t.data->efi.system.runtime_services && !t.data->efi.runtime.functions[10]);
    const uint16_t a[]={'A',0},b[]={'B',0},empty[]={0};
    pwl_efi_guid_t one={{1}},two={{2}},cursor={{9}};
    size_t size=0;uint32_t flags=99;char out[32]={0};
    assert(get(a,&one,&flags,&size,NULL)==PWL_EFI_NOT_FOUND && flags==99 && !size);
    uint64_t maximum=0,remaining=0,variable_max=0;
    assert(query(2,&maximum,&remaining,&variable_max)==0);
    assert(maximum==sizeof(t.data->variables) && remaining==maximum && variable_max==1024);
    assert(set(a,&one,2,3,"ABC")==0);
    assert(get(a,&one,&flags,&size,NULL)==PWL_EFI_BUFFER_TOO_SMALL && size==3 && flags==99);
    assert(get(a,&one,&flags,&size,NULL)==PWL_EFI_INVALID_PARAMETER && flags==99);
    assert(get(a,&one,&flags,&size,out)==0 && size==3 && flags==2 && !memcmp(out,"ABC",3));
    assert(set(a,&two,2,1,"X")==0 && set(b,&one,2,1,"B")==0);
    assert(set(a,&one,66,3,"DEF")==0);
    size=sizeof(out);assert(get(a,&one,&flags,&size,out)==0 && size==6 && flags==2 && !memcmp(out,"ABCDEF",6));
    assert(set(a,&one,66,0,NULL)==0);
    uint16_t unseen[]={'Z',0};assert(set(unseen,&one,66,0,NULL)==0);
    size=sizeof(out);assert(get(unseen,&one,NULL,&size,out)==PWL_EFI_NOT_FOUND);
    uint16_t name[128]={0};size=2;
    assert(next(&size,name,&cursor)==PWL_EFI_BUFFER_TOO_SMALL && size==4 && !name[0] && cursor.bytes[0]==9);
    size=sizeof(name);assert(next(&size,name,&cursor)==0 && name[0]=='A' && cursor.bytes[0]==1);
    size=sizeof(name);assert(next(&size,name,&cursor)==0 && name[0]=='A' && cursor.bytes[0]==2);
    size=sizeof(name);assert(next(&size,name,&cursor)==0 && name[0]=='B' && cursor.bytes[0]==1);
    size=sizeof(name);assert(next(&size,name,&cursor)==PWL_EFI_NOT_FOUND && name[0]=='B');
    name[0]='Z';size=sizeof(name);assert(next(&size,name,&cursor)==PWL_EFI_INVALID_PARAMETER);
    uint16_t short_name[]={1,0};size=2;
    assert(next(&size,short_name,&cursor)==PWL_EFI_INVALID_PARAMETER);
    name[0]=0;size=1;assert(next(&size,name,&cursor)==PWL_EFI_INVALID_PARAMETER);
    /* Failed replacements preserve bytes/attributes and storage accounting. */
    pwl_resident_variable_t before=t.data->variables[0];
    assert(set(a,&one,3,3,"NEW")==PWL_EFI_UNSUPPORTED);
    assert(set(a,&one,6,3,"NEW")==PWL_EFI_UNSUPPORTED);
    assert(set(a,&one,4,3,"NEW")==PWL_EFI_INVALID_PARAMETER);
    assert(set(a,&one,0x100,3,"NEW")==PWL_EFI_INVALID_PARAMETER);
    assert(set(a,&one,2,3,NULL)==PWL_EFI_INVALID_PARAMETER);
    assert(set(a,&one,2,SIZE_MAX,"x")==PWL_EFI_INVALID_PARAMETER);
    assert(!memcmp(&before,&t.data->variables[0],sizeof(before)));
    uint16_t long_name[128];for(unsigned i=0;i<128;i++)long_name[i]='L';
    assert(set(long_name,&one,2,1,"x")==PWL_EFI_INVALID_PARAMETER);
    assert(set(empty,&one,2,1,"x")==PWL_EFI_INVALID_PARAMETER);
    assert(get(NULL,&one,&flags,&size,out)==PWL_EFI_INVALID_PARAMETER);
    assert(get(a,NULL,&flags,&size,out)==PWL_EFI_INVALID_PARAMETER);
    assert(next(NULL,name,&cursor)==PWL_EFI_INVALID_PARAMETER);
    assert(query(0,&maximum,&remaining,&variable_max)==PWL_EFI_INVALID_PARAMETER);
    assert(query(3,&maximum,&remaining,&variable_max)==PWL_EFI_UNSUPPORTED);
    assert(query(2,NULL,&remaining,&variable_max)==PWL_EFI_INVALID_PARAMETER);
    /* SetVariable snapshots aliased name/GUID/data before clearing its slot. */
    pwl_resident_variable_t *v=&t.data->variables[0];
    assert(set(v->name,&v->guid,2,2,v->data+2)==0);
    size=sizeof(out);assert(get(a,&one,NULL,&size,out)==0 && size==2 && !memcmp(out,"CD",2));
    unsigned char payload[1020];memset(payload,0xa5,sizeof(payload));
    assert(set(a,&one,2,sizeof(payload),payload)==0);
    assert(set(a,&one,66,1,"x")==PWL_EFI_INVALID_PARAMETER);
    assert(set(a,&one,2,1021,payload)==PWL_EFI_INVALID_PARAMETER);
    assert(query(2,&maximum,&remaining,&variable_max)==0 && remaining==maximum-3*sizeof(*v));
    assert(set(a,&one,0,0,NULL)==0 && set(a,&one,0,0,NULL)==PWL_EFI_NOT_FOUND);
    assert(set(a,&two,2,0,NULL)==0 && set(b,&one,2,0,NULL)==0);
    assert(query(2,&maximum,&remaining,&variable_max)==0 && remaining==maximum);
    for(unsigned i=0;i<PWL_RESIDENT_VARIABLES;i++) {
        uint16_t n[]={(uint16_t)('A'+i),0};assert(set(n,&one,2,1,"x")==0);
    }
    assert(set(unseen,&one,2,1,"y")==PWL_EFI_OUT_OF_RESOURCES);
    assert(set(a,&one,2,3,"NEW")==0); /* Replacement needs no new slot. */
    assert(query(2,&maximum,&remaining,&variable_max)==0 && !remaining);
    assert(set(a,&one,0,0,NULL)==0 && set(unseen,&one,2,1,"y")==0);
    t.data->memory.exited=1;
    assert(get(unseen,&one,NULL,&size,out)==PWL_EFI_UNSUPPORTED);
    assert(set(unseen,&one,2,1,"z")==PWL_EFI_UNSUPPORTED);
    assert(next(&size,name,&cursor)==PWL_EFI_UNSUPPORTED);
    assert(query(2,&maximum,&remaining,&variable_max)==PWL_EFI_UNSUPPORTED);
    resident_test_close(&t);
    puts("resident variables: copied RX calls, append/enumeration, quotas, aliasing and unsupported lifetime passed");
}
