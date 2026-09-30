#include "pwl_root_clone.h"
#include <assert.h>
#include <string.h>
static uint64_t source[512],destination[512];
int main(void)
{
 uint64_t address=123;
 assert(pwl_x64_root_direct_address(503,348,0x6c3c000,&address)==PWL_OK);
 assert(address==UINT64_C(0xfffffbd706c3c000));
 /* Regression: select an active process root, not the distinct kernel root. */
 assert(pwl_x64_root_direct_address(503,348,0xb28b000,&address)==PWL_OK);
 assert(address==UINT64_C(0xfffffbd70b28b000));
 assert(pwl_x64_root_direct_address(503,348,UINT64_C(0x27a300000),&address)==PWL_OK);
 assert(address==UINT64_C(0xfffffbd97a300000));
 assert(pwl_x64_root_direct_address(255,348,4096,&address)!=PWL_OK && !address);
 assert(pwl_x64_root_direct_address(512,348,4096,&address)!=PWL_OK && !address);
 assert(pwl_x64_root_direct_address(503,481,4096,&address)!=PWL_OK && !address);
 assert(pwl_x64_root_direct_address(503,348,0,&address)!=PWL_OK && !address);
 assert(pwl_x64_root_direct_address(503,348,4097,&address)!=PWL_OK && !address);
 assert(pwl_x64_root_direct_address(503,348,UINT64_C(32)<<30,&address)!=PWL_OK && !address);
 assert(pwl_x64_root_direct_address(511,480,(UINT64_C(32)<<30)-4096,&address)==PWL_OK);
 assert(address==UINT64_MAX-4095);
 assert(pwl_x64_root_direct_address(503,348,4096,NULL)!=PWL_OK);
 pwl_x64_cpu_state_t cpu={0x8005003b,0xb28b000,0x406f0,0xd01};
 for(size_t i=0;i<512;i++)source[i]=(uint64_t)i*4096|3;
 assert(pwl_x64_root_clone_prepare(&cpu,source,destination)==PWL_OK);
 assert(!memcmp(source,destination,sizeof(source)));
 destination[0]=0x1234;
 cpu.cr4|=UINT64_C(1)<<17;
 assert(pwl_x64_root_clone_prepare(&cpu,source,destination)!=PWL_OK);
 assert(destination[0]==0x1234);
 cpu.cr4&=~(UINT64_C(1)<<17);
 assert(pwl_x64_root_clone_prepare(&cpu,source,source)!=PWL_OK);
 assert(pwl_x64_root_clone_prepare(&cpu,source,source+1)!=PWL_OK);
 assert(pwl_x64_root_clone_prepare(NULL,source,destination)!=PWL_OK);
 assert(pwl_x64_root_clone_prepare(&cpu,NULL,destination)!=PWL_OK);
 assert(pwl_x64_root_clone_prepare(&cpu,source,(uint64_t *)((char *)destination+1))!=PWL_OK);
 cpu.cr3|=1;
 assert(pwl_x64_root_clone_prepare(&cpu,source,destination)!=PWL_OK);
 return 0;
}
