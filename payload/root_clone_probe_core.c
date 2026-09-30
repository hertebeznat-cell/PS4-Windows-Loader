#include "pwl_root_clone.h"
#include "root_clone_report.h"
#include "root_clone_source.h"
/* Same-target returning diagnostic; never enables the production binding. */
static int canonical(uint64_t p) {return p>=UINT64_C(0xffff800000000000);}
int pwl_root_clone_experiment(void *map,void *pmap,uint32_t dm_pml4,uint32_t dm_pdpt,
 pwl_workspace_alloc_fn allocate,
 pwl_workspace_free_fn release,pwl_workspace_extract_fn extract,
 volatile pwl_root_clone_report_t *r)
{
 if (!r || !map || !pmap || !allocate || !release || !extract) return -1;
 r->stage=2;r->transition_status=-3;
 pwl_x64_cpu_state_t cpu;
 uint32_t lo,hi;
 __asm__ volatile("mov %%cr0,%0":"=r"(cpu.cr0));
 __asm__ volatile("mov %%cr3,%0":"=r"(cpu.cr3));
 __asm__ volatile("mov %%cr4,%0":"=r"(cpu.cr4));
 __asm__ volatile("rdmsr":"=a"(lo),"=d"(hi):"c"(0xc0000080));
 cpu.efer=((uint64_t)hi<<32)|lo;
 r->cr0=cpu.cr0;r->cr3=cpu.cr3;r->cr4=cpu.cr4;r->efer=cpu.efer;
 if (pwl_x64_cpu_state_validate(&cpu,cpu.cr3)!=PWL_OK) {r->error=1;return -1;}
 /* The kernel pmap's root need not be the current process root. Confirm
  * its direct-map alias first, then translate the live root's alias before
  * dereferencing it. No subtraction-derived base or alternate field scan. */
 uint64_t kernel_source=*(volatile uint64_t *)((unsigned char *)pmap+0x20);
 r->kernel_source=kernel_source;
 r->direct_pml4=dm_pml4;r->direct_pdpt=dm_pdpt;
 if (!canonical(kernel_source) || kernel_source%4096 || kernel_source>UINT64_MAX-4096) {
  r->error=2;return -1;
 }
 int source_error=pwl_root_clone_source(pmap,extract,cpu.cr3,dm_pml4,dm_pdpt,kernel_source,r);
 if (source_error) {r->error=(unsigned)source_error;return -1;}
 uint64_t source=r->source;
 uint64_t kva=allocate(map,32768,0x101,0x100000,UINT64_C(1)<<47,16384,0,6);
 r->kva=kva;
 if (!kva) {r->error=4;return -1;}
 if (!canonical(kva) || kva%16384 || kva>UINT64_MAX-32768) {r->error=5;return -1;}
 uint64_t pa=extract(pmap,kva);r->pa=pa;
 int status=-1;
 if (pa<0x100000 || pa%16384 || pa>=(UINT64_C(1)<<47)-32768 || (cpu.cr3>=pa && cpu.cr3-pa<32768)) {
  r->error=6;goto cleanup;
 }
 for (uint64_t offset=0;offset<32768;offset+=4096)
  if (extract(pmap,kva+offset)!=pa+offset || extract(pmap,kva+offset+4095)!=pa+offset+4095) {
   r->error=7;goto cleanup;
  }
 if (pwl_x64_root_clone_prepare(&cpu,(const volatile uint64_t *)(uintptr_t)source,
     (uint64_t *)(uintptr_t)kva)!=PWL_OK) {r->error=8;goto cleanup;}
#ifdef PWL_ROOT_EFI_PROBE
 if (cpu.cr4 & ((UINT64_C(1)<<20)|(UINT64_C(1)<<21))) {
  r->error=12;goto cleanup;
 }
#endif
 r->stage=3;
 pwl_address_call_report_t report={0};
#ifdef PWL_ROOT_EFI_PROBE
 extern int pwl_root_efi_callback(void *);
 pwl_root_efi_call_t call={cpu.cr3,pwl_root_efi_callback,(void *)r};
 r->transition_status=pwl_x64_root_efi_call(pa,kva+32768,&report,
     (const volatile uint64_t *)(uintptr_t)source,(const uint64_t *)(uintptr_t)kva,&call);
#else
 r->transition_status=pwl_x64_root_clone_call(pa,kva+32768,&report,
     (const volatile uint64_t *)(uintptr_t)source,(const uint64_t *)(uintptr_t)kva,cpu.cr3);
#endif
 r->root_before=report.root_before;r->root_entered=report.root_entered;r->root_after=report.root_after;
 r->stack_before=report.stack_before;r->stack_entered=report.stack_entered;r->stack_after=report.stack_after;
 r->switched=report.root_entered==pa;
 r->restored=report.root_before==cpu.cr3 && report.root_after==cpu.cr3 &&
     report.stack_before && report.stack_before==report.stack_after;
 if (r->transition_status || !r->switched || !r->restored || report.stack_entered!=kva+32768) {
  r->error=9;goto cleanup;
 }
 status=0;
cleanup:
 {
  uint64_t current_root;
  __asm__ volatile("mov %%cr3,%0":"=r"(current_root));
  if (current_root!=cpu.cr3) {r->error=10;return -1;}
 }
 r->stage=4;release(map,kva,32768);r->released=1;r->stage=5;
 return status;
}
