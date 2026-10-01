#ifndef PWL_PS4_READER_H
#define PWL_PS4_READER_H
#include "pwl_ps4_memory.h"
typedef struct pwl_ps4_reader {
    uint64_t kernel_base;
    void *bounce;
    size_t capacity;
} pwl_ps4_reader_t;
/* Uses the exact-build copyout path already exercised by the supplied kernel
 * capture. Bootstrap assumes that installed copyout ABI; it is not a generic
 * reader for unknown kernels. Caller pins bounce user pages and all callback
 * code/state for the full binding lifetime. No syscalls or allocation here.
 * Reads at most 256 bytes per call; no reader argument is examined at CPL3.
 * Current PCB's onfault slot must be idle; copyout supplies its own recovery.
 * Failure changes no caller output, although bounce contents may be partial.
 */
pwl_status_t pwl_ps4_protected_read(void *reader,uint64_t address,void *out,size_t bytes);
#endif
