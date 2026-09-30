#include "root_clone_source.h"
#include <assert.h>
static const uint64_t base=UINT64_C(0xfffffbd700000000);
static const uint64_t kernel=UINT64_C(0x6c3c000),active=UINT64_C(0xb28b000);
static unsigned calls,active_calls;
static int broken;
static unsigned long long extract(void *pmap,unsigned long long va)
{
 assert(pmap==(void *)1);calls++;
 uint64_t pa=va-base;
 assert(pa==kernel || pa==kernel+4095 || pa==active || pa==active+4095);
 if(pa==active || pa==active+4095) active_calls++;
 if ((broken==1 && pa==kernel) || (broken==2 && pa==kernel+4095) ||
     (broken==3 && pa==active) || (broken==4 && pa==active+4095)) return 0;
 return pa;
}
int main(void)
{
 pwl_root_clone_report_t r={0};
 assert(pwl_root_clone_source((void *)1,extract,active,503,348,base+kernel,&r)==0);
 assert(calls==4 && active_calls==2 && r.source==base+active && r.source_pa==active);
 assert(r.kernel_source_pa==kernel && r.direct_base==base);
 calls=active_calls=0;
 assert(pwl_root_clone_source((void *)1,extract,active,502,348,base+kernel,&r)==11);
 assert(calls==1 && !active_calls && !r.source);
 for(broken=1;broken<=4;broken++) {
  calls=active_calls=0;
  assert(pwl_root_clone_source((void *)1,extract,active,503,348,base+kernel,&r)==(broken<3?11:3));
  if(broken<3) assert(!active_calls && !r.source);
 }
 broken=0;calls=active_calls=0;
 assert(pwl_root_clone_source((void *)1,extract,active|1,503,348,base+kernel,&r)==11);
 assert(!active_calls && !r.source);
 assert(pwl_root_clone_source(NULL,extract,active,503,348,base+kernel,&r)==11);
 assert(pwl_root_clone_source((void *)1,NULL,active,503,348,base+kernel,&r)==11);
 return 0;
}
