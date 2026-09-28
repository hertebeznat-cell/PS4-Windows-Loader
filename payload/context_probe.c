/* A returning CPU-context observation through the existing PS4 runtime.
 * It requires the runtime's callback facility to be installed already.
 * This program neither installs that facility nor starts an EFI image.
 */
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "context_capture.h"

#ifndef PS4WL_BUILD_ID
#define PS4WL_BUILD_ID "local-unversioned"
#endif
#ifndef PS4WL_CONTEXT_LOG_PATH
#define PS4WL_CONTEXT_LOG_PATH "/mnt/usb0/PS4WL_CONTEXT.LOG"
#endif

extern int kexec(void (*callback)(void), void *argument);
extern int errno;
volatile struct pwl_context_snapshot pwl_context_result;
static int log_fd = -1, write_failed;

static void log_line(const char *s)
{
    size_t length = 0, offset = 0;
    while (s[length]) ++length;
    while (offset < length && !write_failed) {
        ssize_t n = write(log_fd, s + offset, length - offset);
        if (n <= 0) write_failed = 1;
        else offset += (size_t)n;
    }
}

static void log_hex(const char *tag, unsigned long long value)
{
    const char digits[] = "0123456789ABCDEF";
    char line[96];
    size_t i = 0;
    unsigned j;
    while (*tag && i < sizeof(line) - 20) line[i++] = *tag++;
    line[i++] = '0'; line[i++] = 'x';
    for (j = 0; j < 16; ++j) line[i++] = digits[(value >> (60 - 4 * j)) & 15];
    line[i++] = '\n'; line[i] = 0;
    log_line(line);
}

static unsigned current_ring(void)
{
    unsigned short cs;
    __asm__ volatile("mov %%cs,%0" : "=r"(cs));
    return cs & 3;
}

int main(void)
{
    int rc, saved_errno, result = 1, data_locked = 0, code_locked = 0;
    size_t code_size = (size_t)((unsigned long)pwl_context_capture_end -
                              (unsigned long)pwl_context_capture);
    log_fd = open(PS4WL_CONTEXT_LOG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (log_fd < 0) return 1;
    log_line("PS4 Windows Loader returning context probe\n");
    log_line("BUILD: " PS4WL_BUILD_ID "\n");
    log_line("MODE: CONTEXT_READ_ONLY; no EFI entry\n");
    log_hex("CONTEXT: caller CPL=", current_ring());
    if (current_ring() != 3) {
        log_line("RESULT: unexpected caller context; callback not attempted\n");
        goto done;
    }
    pwl_context_result.state = PWL_CONTEXT_PENDING;
    pwl_context_result.cs = pwl_context_result.cr0 = pwl_context_result.cr3 = 0;
    pwl_context_result.cr4 = pwl_context_result.efer = pwl_context_result.rflags = 0;
    if (!code_size || code_size > 4096) {
        log_line("RESULT: invalid callback extent\n");
        goto done;
    }
    /* Abort before kernel entry unless code and destination are resident. */
    if (mlock((const void *)&pwl_context_result, sizeof(pwl_context_result)) != 0) {
        saved_errno = errno;
        log_line("RESULT: snapshot mlock failed; callback not attempted\n");
        log_hex("CONTEXT: errno=", (unsigned)saved_errno);
        goto done;
    }
    data_locked = 1;
    if (mlock((const void *)(unsigned long)pwl_context_capture, code_size) != 0) {
        saved_errno = errno;
        log_line("RESULT: callback mlock failed; callback not attempted\n");
        log_hex("CONTEXT: errno=", (unsigned)saved_errno);
        goto done;
    }
    code_locked = 1;
    log_line("CONTEXT: calling existing runtime callback\n");
    /* Flush the marker before entering; no logging occurs in the callback. */
    if (write_failed) goto done;
    if (fsync(log_fd) != 0) {
        saved_errno = errno;
        log_line("RESULT: log flush failed; callback not attempted\n");
        log_hex("CONTEXT: errno=", (unsigned)saved_errno);
        goto done;
    }
    errno = 0;
    rc = kexec(pwl_context_capture, (void *)0);
    saved_errno = errno;
    log_line("CONTEXT: runtime call returned\n");
    log_hex("CONTEXT: return code=", (unsigned long long)(long long)rc);
    log_hex("CONTEXT: errno=", (unsigned)saved_errno);
    log_hex("CONTEXT: returned CPL=", current_ring());
    log_hex("CONTEXT: state=", pwl_context_result.state);
    log_hex("CONTEXT: CS=", pwl_context_result.cs);
    if (rc < 0 || pwl_context_result.state != PWL_CONTEXT_COMPLETE ||
        (pwl_context_result.cs & 3) != 0 || current_ring() != 3) {
        log_line("RESULT: privileged callback and return not confirmed\n");
        goto done;
    }
    log_hex("CONTEXT: CR0=", pwl_context_result.cr0);
    log_hex("CONTEXT: CR3=", pwl_context_result.cr3);
    log_hex("CONTEXT: CR4=", pwl_context_result.cr4);
    log_hex("CONTEXT: EFER=", pwl_context_result.efer);
    log_hex("CONTEXT: RFLAGS=", pwl_context_result.rflags);
    log_line("RESULT: privileged reads and user return observed; EFI handoff unverified\n");
    result = 0;
done:
    if (code_locked) munlock((const void *)(unsigned long)pwl_context_capture, code_size);
    if (data_locked) munlock((const void *)&pwl_context_result, sizeof(pwl_context_result));
    if (close(log_fd) != 0 || write_failed) result = 2;
    return result;
}
