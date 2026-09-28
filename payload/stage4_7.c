/* Stage 4.7: exact-address allocation for the observed Windows request.
 *
 * Stage 4.6 confirmed that the platform treats a low-address mapping request
 * as a hint: 0x00102000 was returned as 0x00104000. For the single observed
 * one-page request, first check whether the requested page is already mapped.
 * If it is free, request an exact mapping; if it is occupied, keep the safer
 * non-replacing behaviour and log the condition.
 */

#define mmap ps4wl_mmap47
#include "stage4_5.c"
#undef mmap

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);

static int mmap47_busy;

void *ps4wl_mmap47(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    u64 requested = (u64)(unsigned long)addr;
    int target = addr && requested == 0x0000000000102000ULL &&
                 len == (size_t)EFI_PAGE_SIZE && (flags & MAP_ANONYMOUS);
    int effective_flags = flags;
    int occupied = 0;
    char vec = 0;
    void *p;

    if (target) {
        occupied = (mincore(addr, len, &vec) == 0);
        if (!occupied)
            effective_flags |= MAP_FIXED;
    }

    p = mmap(addr, len, prot, effective_flags, fd, off);

    if (!mmap47_busy && target) {
        mmap47_busy = 1;
        log45("MMAP47: exact low-address request\n");
        log_hex45("MMAP47: requested=", requested);
        log_hex45("MMAP47: length=", (u64)len);
        log_hex45("MMAP47: occupied=", (u64)occupied);
        log_hex45("MMAP47: input_flags=", (u64)(u32)flags);
        log_hex45("MMAP47: effective_flags=", (u64)(u32)effective_flags);
        log_hex45("MMAP47: returned=", (u64)(unsigned long)p);
        mmap47_busy = 0;
    }

    return p;
}
