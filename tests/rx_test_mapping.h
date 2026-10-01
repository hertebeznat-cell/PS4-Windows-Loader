#ifndef PWL_RX_TEST_MAPPING_H
#define PWL_RX_TEST_MAPPING_H
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
/* Clang's function sanitizer probes the eight bytes before an indirect-call
 * target for compiler metadata. Raw copied C/assembly has no such metadata.
 * Keep a zeroed, readable, non-executable prefix page before the page-aligned
 * image, so the probe is defined even for its first entry. This changes only
 * host allocation layout; ASan/UBSan, image offsets and ABI checks stay enabled. */
static inline unsigned char *rx_test_allocate(size_t bytes)
{
    assert(bytes && !(bytes&4095) && bytes<=SIZE_MAX-4096);
    unsigned char *allocation=mmap(NULL,bytes+4096,PROT_READ|PROT_WRITE,
        MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(allocation!=MAP_FAILED);
    assert(mprotect(allocation,4096,PROT_READ)==0);
    return allocation+4096;
}
static inline int rx_test_release(void *code,size_t bytes)
{ return munmap((unsigned char *)code-4096,bytes+4096); }
#endif
