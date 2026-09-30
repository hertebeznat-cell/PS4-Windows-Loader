#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include "workspace_report.h"
static unsigned char *owner;
static unsigned long long owned;
static unsigned allocations,frees;
static int fail_allocation,break_translation;
static unsigned long long allocate(void *map,unsigned long long n,int flags,
 unsigned long long low,unsigned long long high,unsigned long alignment,unsigned long boundary,char attr) {
 assert(map==(void*)1 && !owner && flags==0x101 && low==0x100000 && high==(1ULL<<47));
 assert(alignment==16384 && boundary==0 && attr==6 && !(n%16384));allocations++;
 if(fail_allocation)return 0;
 owner=aligned_alloc(16384,(size_t)n);assert(owner);owned=n;
 for(size_t i=0;i<(size_t)n;i++)owner[i]=0;
 return (unsigned long long)(uintptr_t)owner;
}
static void release(void *map,unsigned long long kva,unsigned long long n) {
 assert(map==(void*)1 && kva==(unsigned long long)(uintptr_t)owner && n==owned);
 frees++;free(owner);owner=NULL;owned=0;
}
static unsigned long long extract(void *pmap,unsigned long long kva) {
 assert(pmap==(void*)2 && owner);
 unsigned long long off=kva-(unsigned long long)(uintptr_t)owner;assert(off<owned);
 if(break_translation && off>=4096)return 0;
 return 0x3ff8000+off; /* exercise page-table boundary crossing */
}
int main(void) {
 pwl_workspace_report_t r={0};
 assert(pwl_workspace_experiment((void*)1,(void*)2,allocate,release,extract,&r)==0);
 assert(r.stage==5 && !r.prepare_status && !r.table_status && !r.release_status && r.copy_ok==1);
 assert(r.bytes>=16384 && r.root>=r.pa && r.root<r.pa+r.bytes && r.tables>=4 && r.regions==8);
 assert(allocations==1 && frees==1 && !owner);
 fail_allocation=1;r=(pwl_workspace_report_t){0};
 assert(pwl_workspace_experiment((void*)1,(void*)2,allocate,release,extract,&r)!=0);
 assert(allocations==2 && frees==1 && !owner);fail_allocation=0;
 break_translation=1;r=(pwl_workspace_report_t){0};
 assert(pwl_workspace_experiment((void*)1,(void*)2,allocate,release,extract,&r)!=0);
 assert(allocations==3 && frees==2 && !owner);break_translation=0;
 r=(pwl_workspace_report_t){0};
 assert(pwl_workspace_experiment((void*)1,(void*)2,allocate,release,extract,&r)==0);
 assert(allocations==4 && frees==3 && r.stage==5 && !owner);
 assert(pwl_workspace_experiment(NULL,(void*)2,allocate,release,extract,&r)!=0);
 assert(allocations==4);
}
