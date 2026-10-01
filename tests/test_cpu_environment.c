#include "pwl_platform_call.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    pwl_cpu_environment_t a={0},b={0};a.cpu.cr0=b.cpu.cr0=0x8005003b;
    a.flags=0x246;b.flags=0x202;assert(pwl_cpu_environment_equal(&a,&b)==PWL_OK);
    b.gs_base=1;assert(pwl_cpu_environment_equal(&a,&b)==PWL_ERR_BAD_IMAGE);b.gs_base=0;
    b.idt.limit=1;assert(pwl_cpu_environment_equal(&a,&b)==PWL_ERR_BAD_IMAGE);b.idt.limit=0;
    for(size_t i=0;i<32;i++) {b.mtrr_variable[i]=1;
        assert(pwl_cpu_environment_equal(&a,&b)==PWL_ERR_BAD_IMAGE);b.mtrr_variable[i]=0;}
    b.flags^=0x200;assert(pwl_cpu_environment_equal(&a,&b)==PWL_ERR_BAD_IMAGE);
    assert(pwl_x64_cpu_environment_read((void *)1)==PWL_ERR_UNSUPPORTED);
    assert(pwl_native_call_checked((void *)1,(void *)1,(void *)2)==PWL_ERR_UNSUPPORTED);
    assert(pwl_native_platform_call((void *)1,(void *)1,1,(void *)1,(void *)1,(void *)1)==PWL_ERR_UNSUPPORTED);
    assert(pwl_native_platform_cleanup((void *)1,(void *)1)==PWL_ERR_UNSUPPORTED);
    puts("CPU environment: descriptor/GS/MTRR differences and actual CPL3 refusal before unreadable arguments; no privileged capture or transition tested");
}
