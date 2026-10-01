#include "pwl_devices.h"
#define COMMAND 4U
#define DIRTY_COMMAND 1U
#define DIRTY_MSI 2U
#define DIRTY_MSIX 4U
static int valid(const pwl_device_ops_t *o)
{ return o && o->read16 && o->write16 && o->stop && o->drain && o->resume; }
static pwl_status_t capture(const pwl_device_ops_t *o,uint64_t id,pwl_device_saved_t *d)
{
    *d=(pwl_device_saved_t){0};d->id=id;uint16_t status,pointer;
    pwl_status_t s;
#define READ(at,to) do {s=o->read16(o->context,id,(at),&(to));if(s!=PWL_OK)return s;} while(0)
    READ(0,d->vendor);READ(2,d->device);READ(0x0e,d->header);READ(COMMAND,d->command);
    if(d->vendor==0xffff || (d->header&0x7f)>1)return PWL_ERR_UNSUPPORTED;
    READ(6,status);
    if(status&0x10) {
        READ(0x34,pointer);unsigned at=pointer&255;uint64_t seen=0;
        for(unsigned n=0;at;n++) {
            if(n>=48 || at<0x40 || at>0xfc || (at&3))return PWL_ERR_BAD_IMAGE;
            uint64_t bit=UINT64_C(1)<<((at-0x40)/4);
            if(seen&bit)return PWL_ERR_BAD_IMAGE;
            seen|=bit;uint16_t cap;READ(at,cap);
            if((cap&255)==5) {
                if(d->msi)return PWL_ERR_BAD_IMAGE;
                d->msi=(uint16_t)at;READ(at+2,d->msi_control);
            } else if((cap&255)==0x11) {
                if(d->msix)return PWL_ERR_BAD_IMAGE;
                d->msix=(uint16_t)at;READ(at+2,d->msix_control);
            }
            at=cap>>8;
        }
    }
    uint16_t vendor,device;
    READ(0,vendor);READ(2,device);
    if(vendor!=d->vendor || device!=d->device)return PWL_ERR_BAD_IMAGE;
#undef READ
    return PWL_OK;
}
static pwl_status_t write_checked(const pwl_device_ops_t *o,uint64_t id,unsigned at,uint16_t value)
{
    pwl_status_t s=o->write16(o->context,id,at,value);if(s!=PWL_OK)return s;
    uint16_t actual;s=o->read16(o->context,id,at,&actual);
    return s!=PWL_OK?s:actual==value?PWL_OK:PWL_ERR_IO;
}
static int same_function(const pwl_device_saved_t *a,const pwl_device_saved_t *b)
{return a->vendor==b->vendor && a->device==b->device && a->header==b->header &&
 a->msi==b->msi && a->msix==b->msix;}
pwl_status_t pwl_devices_restore(const pwl_device_ops_t *o,pwl_device_lease_t *l)
{
    if(!valid(o) || !l || l->count>PWL_DEVICE_MAX)return PWL_ERR_INVALID_ARGUMENT;
    l->quiesced=0;pwl_status_t first=PWL_OK;
    /* Restore config for every stopped function before resuming ANY driver.
     * The caller still owns the interrupt controllers and serialization. */
    for(size_t n=l->count;n;n--) {
        pwl_device_saved_t *d=&l->devices[n-1];if(!d->dirty && !d->stopped)continue;
        pwl_device_saved_t current;pwl_status_t s=capture(o,d->id,&current);
        if(s==PWL_OK && !same_function(&current,d))s=PWL_ERR_BAD_IMAGE;
        if(s!=PWL_OK) {if(first==PWL_OK)first=s;continue;}
        const unsigned bits[]={DIRTY_MSI,DIRTY_MSIX,DIRTY_COMMAND};
        for(size_t i=0;i<3;i++)if(d->dirty&bits[i]) {
            unsigned at=i==2?COMMAND:i==0?d->msi+2U:d->msix+2U;
            uint16_t value=i==2?d->command:i==0?d->msi_control:d->msix_control;
            s=write_checked(o,d->id,at,value);
            if(s==PWL_OK)d->dirty&=~bits[i];else if(first==PWL_OK)first=s;
        }
    }
    l->restore_status=first;
    return first;
}
pwl_status_t pwl_devices_release(const pwl_device_ops_t *o,pwl_device_lease_t *l)
{
    pwl_status_t first=pwl_devices_restore(o,l);
    if(first!=PWL_OK)return first;
    for(size_t n=l->count;n;n--) {
        pwl_device_saved_t *d=&l->devices[n-1];if(!d->stopped)continue;
        pwl_status_t s=o->resume(o->context,d->id);
        if(s==PWL_OK)d->stopped=0;else if(first==PWL_OK)first=s;
    }
    l->restore_status=first;
    if(first==PWL_OK)*l=(pwl_device_lease_t){0};
    return first;
}
pwl_status_t pwl_devices_quiesce(const pwl_device_ops_t *o,const uint64_t *ids,size_t count,
    pwl_device_lease_t *l)
{
    if(!valid(o) || !ids || !l || !count || count>PWL_DEVICE_MAX || l->count || l->quiesced)
        return PWL_ERR_INVALID_ARGUMENT;
    uint64_t inventory[PWL_DEVICE_MAX];
    for(size_t i=0;i<count;i++) {
        inventory[i]=ids[i];
        for(size_t j=0;j<i;j++)if(inventory[i]==inventory[j])return PWL_ERR_INVALID_ARGUMENT;
    }
    pwl_status_t s=PWL_OK;
    for(size_t i=0;i<count;i++) {
        s=capture(o,inventory[i],&l->devices[i]);
        if(s!=PWL_OK) {*l=(pwl_device_lease_t){0};return s;}
    }
    l->count=count;
    for(size_t i=0;i<count;i++) {
        l->devices[i].stopped=1;s=o->stop(o->context,inventory[i]);if(s!=PWL_OK)goto failure;
    }
    for(size_t i=0;i<count;i++) {s=o->drain(o->context,inventory[i]);if(s!=PWL_OK)goto failure;}
    pwl_device_saved_t drained[PWL_DEVICE_MAX];
    for(size_t i=0;i<count;i++) {
        s=capture(o,inventory[i],&drained[i]);if(s!=PWL_OK)goto failure;
        if(!same_function(&drained[i],&l->devices[i])) {s=PWL_ERR_BAD_IMAGE;goto failure;}
    }
    for(size_t i=0;i<count;i++) {
        pwl_device_saved_t *d=&l->devices[i];
        /* Disable legacy delivery before disabling MSI modes, which may
         * otherwise fall back to INTx while CPUs/controllers are still live. */
        d->dirty|=DIRTY_COMMAND;
        s=write_checked(o,d->id,COMMAND,(uint16_t)(drained[i].command|0x400U));
        if(s!=PWL_OK)goto failure;
        if(d->msi) {
            d->dirty|=DIRTY_MSI;s=write_checked(o,d->id,d->msi+2U,(uint16_t)(drained[i].msi_control&~1U));
            if(s!=PWL_OK)goto failure;
        }
        if(d->msix) {
            d->dirty|=DIRTY_MSIX;s=write_checked(o,d->id,d->msix+2U,(uint16_t)((drained[i].msix_control|0x4000U)&~0x8000U));
            if(s!=PWL_OK)goto failure;
        }
        s=write_checked(o,d->id,COMMAND,(uint16_t)((drained[i].command|0x400U)&~4U));
        if(s!=PWL_OK)goto failure;
    }
    l->quiesced=1;return PWL_OK;
failure:
    l->operation_status=s;
    {pwl_status_t restore=pwl_devices_release(o,l);
     if(restore!=PWL_OK)return restore;}
    return s;
}
