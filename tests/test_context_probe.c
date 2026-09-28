/* Host-only transport mocks. Never invoke a PS4 syscall on the CI host. */
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int test_errno, fail_lock, lock_count, unlock_count, sync_failure;
static int callback_mode, callback_calls;
static int test_mlock(const void *p, size_t n);
static int test_munlock(const void *p, size_t n);
static int test_fsync(int fd);
static int test_kexec(void (*callback)(void), void *argument);

#define errno test_errno
#define mlock test_mlock
#define munlock test_munlock
#define fsync test_fsync
#define kexec test_kexec
#define main context_probe_main
#define PS4WL_CONTEXT_LOG_PATH "build/CONTEXT-TEST.LOG"
#include "../payload/context_probe.c"
#undef main
#undef kexec
#undef fsync
#undef munlock
#undef mlock
#undef errno

static int test_mlock(const void *p, size_t n)
{
    assert(p != NULL && n != 0);
    ++lock_count;
    return fail_lock == lock_count ? -1 : 0;
}
static int test_munlock(const void *p, size_t n)
{
    assert(p != NULL && n != 0);
    ++unlock_count;
    return 0;
}
static int test_fsync(int fd) { assert(fd >= 0); return sync_failure ? -1 : 0; }
static int test_kexec(void (*callback)(void), void *argument)
{
    assert(argument == NULL);
    ++callback_calls;
    if (callback_mode == 0) { test_errno = 78; return -1; }
    if (callback_mode == 1) { callback(); return 0; }
    if (callback_mode == 2) {
        /* Synthetic success tests reporting only, never CPU execution. */
        pwl_context_result.cs = 0x20;
        pwl_context_result.cr3 = 0x1000;
        pwl_context_result.state = PWL_CONTEXT_COMPLETE;
    }
    return 0;
}
static void reset(void)
{
    fail_lock = lock_count = unlock_count = sync_failure = 0;
    callback_mode = callback_calls = 0;
    write_failed = 0;
}
static int log_contains(const char *needle)
{
    char data[4096];
    FILE *fp = fopen(PS4WL_CONTEXT_LOG_PATH, "rb");
    size_t n;
    assert(fp != NULL);
    n = fread(data, 1, sizeof(data) - 1, fp);
    data[n] = 0;
    fclose(fp);
    return strstr(data, needle) != NULL;
}
int main(void)
{
    reset(); fail_lock = 1;
    assert(context_probe_main() == 1);
    assert(callback_calls == 0 && unlock_count == 0);
    assert(log_contains("snapshot mlock failed"));
    reset(); fail_lock = 2;
    assert(context_probe_main() == 1);
    assert(callback_calls == 0 && unlock_count == 1);
    reset(); sync_failure = 1;
    assert(context_probe_main() == 1);
    assert(callback_calls == 0 && unlock_count == 2);
    reset();
    assert(context_probe_main() == 1);
    assert(callback_calls == 1 && unlock_count == 2);
    assert(log_contains("return not confirmed"));
    reset(); callback_mode = 1;
    assert(context_probe_main() == 1);
    assert(pwl_context_result.state == PWL_CONTEXT_WRONG_RING);
    assert(!log_contains("reads and user return observed"));
    reset(); callback_mode = 3; /* Runtime returns without invoking callback. */
    assert(context_probe_main() == 1);
    assert(!log_contains("reads and user return observed"));
    reset(); callback_mode = 2;
    assert(context_probe_main() == 0);
    assert(log_contains("reads and user return observed; EFI handoff unverified"));
    return 0;
}
