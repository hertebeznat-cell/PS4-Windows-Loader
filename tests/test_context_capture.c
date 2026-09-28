#include "../payload/context_capture.h"
#include <assert.h>

volatile struct pwl_context_snapshot pwl_context_result;

int main(void)
{
    /* Real host execution: CPL3 must branch before any MOV CRn / RDMSR. */
    pwl_context_result.state = PWL_CONTEXT_PENDING;
    pwl_context_result.cr0 = 0x1234;
    pwl_context_result.cr3 = 0x5678;
    pwl_context_result.cr4 = 0x9abc;
    pwl_context_result.efer = 0xdef0;
    pwl_context_result.rflags = 0xfeed;
    pwl_context_capture();
    assert(pwl_context_result.state == PWL_CONTEXT_WRONG_RING);
    assert((pwl_context_result.cs & 3) == 3);
    assert(pwl_context_result.cr0 == 0x1234);
    assert(pwl_context_result.cr3 == 0x5678);
    assert(pwl_context_result.cr4 == 0x9abc);
    assert(pwl_context_result.efer == 0xdef0);
    assert(pwl_context_result.rflags == 0xfeed);
    return 0;
}
