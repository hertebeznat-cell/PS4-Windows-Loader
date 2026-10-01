#include "pwl_ps4_profile.h"
#include "ps4_profile_signatures.h"
static int canonical(uint64_t address)
{ return address>=UINT64_C(0xffff800000000000); }
static int equal(const unsigned char *a,const unsigned char *b,size_t n)
{ for(size_t i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1; }
static pwl_status_t signature(pwl_ps4_kernel_read_fn read,void *context,
    uint64_t address,const unsigned char *expected,size_t bytes)
{
    unsigned char buffer[64];
    if(bytes>sizeof(buffer))return PWL_ERR_INVALID_ARGUMENT;
    pwl_status_t status=read(context,address,buffer,bytes);
    if(status!=PWL_OK)return status;
    return equal(buffer,expected,bytes)?PWL_OK:PWL_ERR_BAD_IMAGE;
}
static pwl_status_t state(const pwl_ps4_observation_t *o,pwl_ps4_kernel_read_fn read,
    void *context,uint64_t base,uint64_t *map,uint64_t *root)
{
    uint32_t critical;uint16_t locks;pwl_status_t status;
    status=read(context,o->thread+0x128,&critical,sizeof(critical));
    if(status!=PWL_OK)return status;
    status=read(context,o->thread+0xfc,&locks,sizeof(locks));
    if(status!=PWL_OK)return status;
    if(critical || locks)return PWL_ERR_ACCESS_DENIED;
    status=read(context,base+0x22d1d50,map,sizeof(*map));
    if(status!=PWL_OK)return status;
    status=read(context,base+0x1b2c3c0,root,sizeof(*root));
    if(status!=PWL_OK)return status;
    if(!canonical(*map) || *map%8 || !canonical(*root) || *root%4096)
        return PWL_ERR_BAD_IMAGE;
    return PWL_OK;
}
pwl_status_t pwl_ps4_profile_inspect(const pwl_ps4_observation_t *o,
    pwl_ps4_kernel_read_fn read,void *context,pwl_ps4_profile_t *out)
{
    if(!o || !read || !out)return PWL_ERR_INVALID_ARGUMENT;
    if((o->cs&3) || !(o->flags&0x200) || (o->flags&0x400) ||
       !canonical(o->thread) || o->thread>UINT64_MAX-0x12c || o->thread%8 ||
       !o->root || o->root%4096 || o->root>=(UINT64_C(1)<<52) ||
       o->lstar<UINT64_C(0xffff8000000001c0))return PWL_ERR_ACCESS_DENIED;
    uint64_t base=o->lstar-0x1c0;
    if(base%16384 || base>UINT64_MAX-0x22d1d58)return PWL_ERR_BAD_IMAGE;
    static const unsigned char version[]="r228995/release_branches/release_13.520 Jun 11 2026 05:25:24";
    pwl_status_t status=signature(read,context,base+0x1520080,version,sizeof(version));
    if(status!=PWL_OK)return status;
#define CHECK_WINDOW(offset, bytes) do { \
    status=signature(read,context,base+(offset),(bytes),sizeof(bytes)); \
    if(status!=PWL_OK)return status; \
} while(0)
    CHECK_WINDOW(0x24d4f0,pwl_sig_alloc);
    CHECK_WINDOW(0x466460,pwl_sig_free);
    CHECK_WINDOW(0x573d0,pwl_sig_extract);
    CHECK_WINDOW(0x378a80,pwl_sig_mutex);
    CHECK_WINDOW(0x4d6d0,pwl_sig_handler);
    CHECK_WINDOW(0x2bd6a0,pwl_sig_copyout);
    CHECK_WINDOW(0x2bd721,pwl_sig_copyloop);
    CHECK_WINDOW(0x2bd750,pwl_sig_copyfault);
#undef CHECK_WINDOW
    uint64_t fault;
    status=read(context,base+0x111f990,&fault,sizeof(fault));
    if(status!=PWL_OK)return status;
    if(fault!=base+0x2bd750)return PWL_ERR_BAD_IMAGE;
    uint64_t map,root,again_map,again_root;
    status=state(o,read,context,base,&map,&root);if(status!=PWL_OK)return status;
    status=state(o,read,context,base,&again_map,&again_root);if(status!=PWL_OK)return status;
    if(map!=again_map || root!=again_root)return PWL_ERR_BAD_IMAGE;
    *out=(pwl_ps4_profile_t){base,map,base+0x1b2c3a0,base+0x24d4f0,base+0x466460,base+0x573d0};
    return PWL_OK;
}
