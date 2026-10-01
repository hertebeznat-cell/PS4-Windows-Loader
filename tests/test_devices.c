#include "pwl_devices.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct {
    uint16_t config[2][128],original[2][128];
    unsigned stops,drains,resumes,writes,fail_stop,fail_drain,fail_write,fail_restore;
    unsigned pending[2],write_order[64],order_count,ignore_write,stop_clears_master;
} f;
static pwl_status_t read16(void *c,uint64_t id,unsigned at,uint16_t *v)
{assert(c==&f && id<2 && !(at&1) && at<=254);*v=f.config[id][at/2];return PWL_OK;}
static pwl_status_t write16(void *c,uint64_t id,unsigned at,uint16_t v)
{
    assert(c==&f && id<2 && (at==4 || at==0x42 || at==0x52));
    assert(f.drains>=2 || f.fail_stop || f.fail_drain);
    f.writes++;if(f.order_count<64)f.write_order[f.order_count++]=at;
    if(f.ignore_write) {f.ignore_write=0;return PWL_OK;}
    f.config[id][at/2]=v; /* Failure may occur AFTER a real partial effect. */
    if(f.fail_write==f.writes || f.fail_restore)return PWL_ERR_IO;
    return PWL_OK;
}
static pwl_status_t stop(void *c,uint64_t id)
{assert(c==&f && id<2);f.stops++;f.pending[id]=1;
 if(f.stop_clears_master)f.config[id][2]&=~4U;
 return f.fail_stop==f.stops?PWL_ERR_IO:PWL_OK;}
static pwl_status_t drain(void *c,uint64_t id)
{assert(c==&f && id<2 && f.stops==2);f.drains++;return f.fail_drain==f.drains?PWL_ERR_IO:PWL_OK;}
static pwl_status_t resume(void *c,uint64_t id)
{assert(c==&f && id<2 && f.pending[id]);f.resumes++;f.pending[id]=0;return PWL_OK;}
static pwl_device_ops_t ops(void)
{return (pwl_device_ops_t){&f,read16,write16,stop,drain,resume};}
static void reset(void)
{
    memset(&f,0,sizeof(f));
    for(size_t i=0;i<2;i++) {
        f.config[i][0]=0x1022;f.config[i][1]=(uint16_t)(0x1000+i);
        f.config[i][2]=7;f.config[i][3]=0x10;f.config[i][0x34/2]=0x40;
        f.config[i][0x40/2]=0x5005;f.config[i][0x42/2]=0x81;
        f.config[i][0x50/2]=0x11;f.config[i][0x52/2]=0x8007;
    }
    memcpy(f.original,f.config,sizeof(f.config));
}
static void restored(void)
{assert(!memcmp(f.config,f.original,sizeof(f.config)) && !f.pending[0] && !f.pending[1]);}
int main(void)
{
    pwl_device_ops_t o=ops();uint64_t ids[]={0,1};pwl_device_lease_t l={0};reset();
    assert(pwl_devices_quiesce(&o,ids,2,&l)==PWL_OK && l.quiesced);
    assert(f.stops==2 && f.drains==2 && !f.resumes && f.write_order[0]==4);
    for(size_t i=0;i<2;i++)assert(f.config[i][2]==0x403 && f.config[i][0x42/2]==0x80 &&
        f.config[i][0x52/2]==0x4007 && f.config[i][3]==0x10);
    assert(pwl_devices_restore(&o,&l)==PWL_OK && l.count==2 && !f.resumes);
    assert(!memcmp(f.config,f.original,sizeof(f.config)) && f.pending[0] && f.pending[1]);
    assert(f.write_order[8]==0x42 && f.write_order[9]==0x52 && f.write_order[10]==4);
    unsigned writes=f.writes;
    assert(pwl_devices_restore(&o,&l)==PWL_OK && f.writes==writes && !f.resumes);
    assert(pwl_devices_release(&o,&l)==PWL_OK && !l.count && f.resumes==2);restored();
    reset();f.stop_clears_master=1;
    assert(pwl_devices_quiesce(&o,ids,2,&l)==PWL_OK);
    assert(!(f.config[0][2]&4) && !(f.config[1][2]&4));
    assert(pwl_devices_release(&o,&l)==PWL_OK);restored();
    for(unsigned i=1;i<=2;i++) {
        reset();f.fail_stop=i;assert(pwl_devices_quiesce(&o,ids,2,&l)==PWL_ERR_IO);
        assert(!l.count && !f.writes && f.resumes==i);restored();
        reset();f.fail_drain=i;assert(pwl_devices_quiesce(&o,ids,2,&l)==PWL_ERR_IO);
        assert(!l.count && !f.writes && f.resumes==2);restored();
    }
    for(unsigned i=1;i<=8;i++) {
        reset();f.fail_write=i;assert(pwl_devices_quiesce(&o,ids,2,&l)==PWL_ERR_IO);
        assert(!l.count && f.resumes==2);restored();
    }
    reset();f.ignore_write=1;
    assert(pwl_devices_quiesce(&o,ids,2,&l)==PWL_ERR_IO && !l.count);restored();
    reset();assert(pwl_devices_quiesce(&o,ids,2,&l)==PWL_OK);f.fail_restore=1;
    assert(pwl_devices_release(&o,&l)==PWL_ERR_IO && l.count==2 && !f.resumes);
    f.fail_restore=0;assert(pwl_devices_release(&o,&l)==PWL_OK);restored();
    reset();f.config[0][0x50/2]=0x4011; /* Capability cycle. */
    assert(pwl_devices_quiesce(&o,ids,2,&l)==PWL_ERR_BAD_IMAGE && !f.stops && !f.writes);
    reset();uint64_t duplicates[]={1,1};assert(pwl_devices_quiesce(&o,duplicates,2,&l)==PWL_ERR_INVALID_ARGUMENT);
    reset();assert(pwl_devices_quiesce(&o,ids,2,&l)==PWL_OK);f.config[0][0]=0xffff;
    assert(pwl_devices_release(&o,&l)!=PWL_OK && l.count==2 && !f.resumes);
    f.config[0][0]=0x1022;assert(pwl_devices_release(&o,&l)==PWL_OK);restored();
    puts("devices: stop/drain ordering, INTx/MSI/MSI-X/BME, partial-write rollback, retained restore and malformed inventory passed; synthetic config only");
}
