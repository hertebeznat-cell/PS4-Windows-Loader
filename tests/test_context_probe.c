/* Host-only transport mocks. Never invoke a PS4 syscall on the CI host. */
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

static int test_errno, fail_lock, lock_count, unlock_count, sync_failure;
static int callback_mode, callback_calls;
static int open_calls, fail_open_count, full_device;
static char last_notice[256];
static char notices[12][256];
static unsigned notice_count;
static void test_notify(const char *message);
static int test_open(const char *path, int flags, ...);
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
#define open test_open
#define PS4WL_CONTEXT_NOTIFY test_notify
#define PS4WL_CONTEXT_LOG_PATH "build/CONTEXT-TEST.LOG"
#define PS4WL_CONTEXT_LOG_PATH_ALT "build/CONTEXT-TEST-ALT.LOG"
#include "../payload/context_probe.c"
#undef open
#undef main
#undef kexec
#undef fsync
#undef munlock
#undef mlock
#undef errno

static void test_notify(const char *message)
{
    snprintf(last_notice, sizeof(last_notice), "%s", message);
    if (notice_count < 12)
        snprintf(notices[notice_count++], sizeof(notices[0]), "%s", message);
}
static int test_open(const char *path, int flags, ...)
{
    ++open_calls;
    if (open_calls <= fail_open_count) { test_errno = 2; return -1; }
    if (full_device) return open("/dev/full", O_WRONLY);
    return open(path, flags, 0666);
}

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
    open_calls = fail_open_count = 0;
    full_device = 0;
    last_notice[0] = 0;
    notice_count = 0;
    write_failed = 99; /* Reused raw payload memory must not suppress logging. */
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
    assert(context_probe_main() == 2);
    assert(callback_calls == 0 && unlock_count == 0);
    assert(strstr(last_notice, "flush failed") != NULL);
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
    assert(notice_count == 6);
    assert(strcmp(notices[1], "PS4WL Context: CR0=0x0000000000000000") == 0);
    assert(strcmp(notices[2], "PS4WL Context: CR3=0x0000000000001000") == 0);
    assert(strcmp(notices[3], "PS4WL Context: CR4=0x0000000000000000") == 0);
    assert(strcmp(notices[4], "PS4WL Context: EFER=0x0000000000000000") == 0);
    reset(); fail_open_count = 2;
    assert(context_probe_main() == 1);
    assert(callback_calls == 0 && lock_count == 0 && open_calls == 2);
    assert(strcmp(last_notice, "PS4WL Context: USB0 open errno=00000002; USB1=00000002 (hex)") == 0);
    reset(); full_device = 1;
    assert(context_probe_main() == 2);
    assert(callback_calls == 0 && lock_count == 0);
    assert(strstr(last_notice, "write/flush failed") != NULL);
    reset(); fail_open_count = 1; callback_mode = 2;
    assert(context_probe_main() == 0);
    assert(open_calls == 2 && callback_calls == 1);
    {
        char contents[4096];
        FILE *fp = fopen(PS4WL_CONTEXT_LOG_PATH_ALT, "rb");
        size_t n;
        assert(fp != NULL);
        n = fread(contents, 1, sizeof(contents) - 1, fp);
        contents[n] = 0;
        fclose(fp);
        assert(strstr(contents, "LOG: " PS4WL_CONTEXT_LOG_PATH_ALT) != NULL);
    }
    return 0;
}
