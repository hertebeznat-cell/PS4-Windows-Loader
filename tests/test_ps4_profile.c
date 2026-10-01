#include "pwl_ps4_profile.h"
#include "pwl_ps4_reader.h"
#include "../loader/src/ps4_profile_signatures.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BASE UINT64_C(0xffffffff8433c000)
#define THREAD UINT64_C(0xffffffffca000000)
static struct { unsigned reads,fail_at,critical,locks,change_map;uint64_t corrupt; } fixture;
static pwl_status_t read_fixture(void *context,uint64_t address,void *out,size_t n)
{
    (void)context;fixture.reads++;
    if(fixture.fail_at==fixture.reads)return PWL_ERR_IO;
    static const unsigned char version[]="r228995/release_branches/release_13.520 Jun 11 2026 05:25:24";
    const struct { uint64_t offset;const void *bytes;size_t count; } windows[]={
        {0x1520080,version,sizeof(version)},
        {0x24d4f0,pwl_sig_alloc,sizeof(pwl_sig_alloc)},
        {0x466460,pwl_sig_free,sizeof(pwl_sig_free)},
        {0x573d0,pwl_sig_extract,sizeof(pwl_sig_extract)},
        {0x378a80,pwl_sig_mutex,sizeof(pwl_sig_mutex)},
        {0x4d6d0,pwl_sig_handler,sizeof(pwl_sig_handler)},
        {0x2bd6a0,pwl_sig_copyout,sizeof(pwl_sig_copyout)},
        {0x2bd721,pwl_sig_copyloop,sizeof(pwl_sig_copyloop)},
        {0x2bd750,pwl_sig_copyfault,sizeof(pwl_sig_copyfault)}};
    for(size_t i=0;i<sizeof(windows)/sizeof(windows[0]);i++)if(address==BASE+windows[i].offset) {
        assert(n==windows[i].count);memcpy(out,windows[i].bytes,n);
        if(fixture.corrupt==address)((unsigned char *)out)[n-1]^=1;
        return PWL_OK;
    }
    if(address==THREAD+0x128) { assert(n==4);uint32_t x=fixture.critical;memcpy(out,&x,n);return PWL_OK; }
    if(address==THREAD+0xfc) { assert(n==2);uint16_t x=(uint16_t)fixture.locks;memcpy(out,&x,n);return PWL_OK; }
    uint64_t value=0;
    if(address==BASE+0x111f990)value=BASE+0x2bd750;
    else if(address==BASE+0x22d1d50) {
        value=UINT64_C(0xffffffffcc000000);
        if(fixture.change_map && fixture.reads>14)value+=8;
    } else if(address==BASE+0x1b2c3c0)value=UINT64_C(0xffffda570c7b4000);
    else assert(0);
    assert(n==8);if(fixture.corrupt==address)value^=1;memcpy(out,&value,n);return PWL_OK;
}
int main(void)
{
    pwl_ps4_observation_t observed={BASE+0x1c0,THREAD,0xcf0a000,0x246,0x20};
    pwl_ps4_profile_t profile;
    assert(pwl_ps4_profile_inspect(&observed,read_fixture,NULL,&profile)==PWL_OK);
    assert(profile.base==BASE && profile.map==UINT64_C(0xffffffffcc000000) &&
        profile.pmap==BASE+0x1b2c3a0 && profile.alloc_contig==BASE+0x24d4f0 &&
        profile.free==BASE+0x466460 && profile.extract==BASE+0x573d0);
    unsigned successful_reads=fixture.reads;
    const uint64_t corrupt[]={0x1520080,0x24d4f0,0x466460,0x573d0,0x378a80,0x4d6d0,
        0x2bd6a0,0x2bd721,0x2bd750,0x111f990,0x22d1d50,0x1b2c3c0};
    pwl_ps4_profile_t sentinel=profile;
    for(size_t i=0;i<sizeof(corrupt)/sizeof(corrupt[0]);i++) {
        memset(&fixture,0,sizeof(fixture));fixture.corrupt=BASE+corrupt[i];
        assert(pwl_ps4_profile_inspect(&observed,read_fixture,NULL,&profile)==PWL_ERR_BAD_IMAGE);
        assert(!memcmp(&profile,&sentinel,sizeof(profile)));
    }
    for(unsigned i=1;i<=successful_reads;i++) {
        memset(&fixture,0,sizeof(fixture));fixture.fail_at=i;
        assert(pwl_ps4_profile_inspect(&observed,read_fixture,NULL,&profile)==PWL_ERR_IO);
        assert(!memcmp(&profile,&sentinel,sizeof(profile)));
    }
    memset(&fixture,0,sizeof(fixture));fixture.change_map=1;
    assert(pwl_ps4_profile_inspect(&observed,read_fixture,NULL,&profile)==PWL_ERR_BAD_IMAGE);
    fixture.change_map=0;fixture.critical=1;
    assert(pwl_ps4_profile_inspect(&observed,read_fixture,NULL,&profile)==PWL_ERR_ACCESS_DENIED);
    fixture.critical=0;fixture.locks=1;
    assert(pwl_ps4_profile_inspect(&observed,read_fixture,NULL,&profile)==PWL_ERR_ACCESS_DENIED);
    memset(&fixture,0,sizeof(fixture));observed.cs=0x23;
    assert(pwl_ps4_profile_inspect(&observed,read_fixture,NULL,&profile)==PWL_ERR_ACCESS_DENIED && !fixture.reads);
    observed.cs=0x20;observed.flags=0x46;
    assert(pwl_ps4_profile_inspect(&observed,read_fixture,NULL,&profile)==PWL_ERR_ACCESS_DENIED && !fixture.reads);
    observed.flags=0x646;
    assert(pwl_ps4_profile_inspect(&observed,read_fixture,NULL,&profile)==PWL_ERR_ACCESS_DENIED && !fixture.reads);
    /* These are the actual production wrappers, with unreadable arguments.
     * Synthetic observations never let the host invoke candidate functions. */
    assert(pwl_ps4_protected_read((void *)1,UINT64_MAX,(void *)1,256)==PWL_ERR_UNSUPPORTED);
    pwl_ps4_memory_api_t api;
    assert(pwl_ps4_memory_bind_checked(1352,(void *)1,&api)==PWL_ERR_UNSUPPORTED);
    assert(api.firmware==1352 && !api.binding && !api.alloc_contig);
    puts("PS4 profile: exact build, all symbols/copyout, live-state changes and read failures; actual CPL3 wrappers refuse before reads");
    return 0;
}
