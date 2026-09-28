/* Stage 4.6: diagnostic wrapper around Stage 4.5.
 *
 * The Stage 4.5 hardware log shows that Microsoft requests one page at the
 * exact address 0x00102000 and the current allocator returns EFI_NOT_FOUND.
 * This revision does not force that mapping. It records what address the
 * underlying OS returns when a low-address mmap hint is requested, so the
 * next allocator change can be based on observed behaviour rather than a
 * guess.
 */

#define mmap ps4wl_mmap46
#include "stage4_5.c"
#undef mmap

/* The system header declaration was macro-renamed above, so restore the real
 * mmap prototype for the diagnostic wrapper implementation below. */
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);

static int mmap46_busy;

void *ps4wl_mmap46(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
    void *p = mmap(addr, len, prot, flags, fd, off);

    if (!mmap46_busy && addr && (u64)(unsigned long)addr < 0x01000000ULL) {
        mmap46_busy = 1;
        log45("MMAP46: low-address mapping request\n");
        log_hex45("MMAP46: requested=", (u64)(unsigned long)addr);
        log_hex45("MMAP46: length=", (u64)len);
        log_hex45("MMAP46: returned=", (u64)(unsigned long)p);
        log_hex45("MMAP46: prot=", (u64)(u32)prot);
        log_hex45("MMAP46: flags=", (u64)(u32)flags);
        mmap46_busy = 0;
    }

    return p;
}
