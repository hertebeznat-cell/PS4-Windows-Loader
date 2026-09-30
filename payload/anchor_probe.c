/* Returning observations only. No reads through kernel-address candidates. */
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include "anchor_capture.h"
#ifndef PS4WL_BUILD_ID
#define PS4WL_BUILD_ID "local"
#endif
#ifndef PWL_ANCHOR_TEST
void *dlopen(const char *, int);
void *dlsym(void *, const char *);
typedef int (*notify_fn)(int, const char *);
typedef int (*sysctl_fn)(const char *, void *, size_t *, const void *, size_t);
static notify_fn notification;
static sysctl_fn metadata;
static void transport_init(void)
{
    void *m;
    notification = (notify_fn)0;
    metadata = (sysctl_fn)0;
    m = dlopen("/system/common/lib/libSceSysUtil.sprx", 0);
    if (!m) m = dlopen("libSceSysUtil.sprx", 0);
    if (m) notification = (notify_fn)dlsym(m, "sceSysUtilSendSystemNotificationWithText");
    m = dlopen("/system/common/lib/libkernel.sprx", 0);
    if (!m) m = dlopen("libkernel.sprx", 0);
    if (m) metadata = (sysctl_fn)dlsym(m, "sysctlbyname");
}
static void screen(const char *s) { if (notification) notification(222, s); }
static int screen_ready(void) { return notification != (notify_fn)0; }
static int query(const char *name, void *p, size_t *n)
{ return metadata ? metadata(name, p, n, (const void *)0, 0) : -1; }
#endif
extern int kexec(void (*)(void), void *);
extern int errno;
volatile struct pwl_anchor_snapshot pwl_anchor_result;
static int log_fd, log_bad;
static size_t length(const char *s) { size_t n = 0; while (s[n]) ++n; return n; }
static void report(const char *s)
{
    size_t n = length(s), offset = 0;
    screen(s);
    if (log_fd < 0 || log_bad) return;
    while (offset < n) {
        ssize_t rc = write(log_fd, s + offset, n - offset);
        if (rc <= 0) { log_bad = 1; return; }
        offset += (size_t)rc;
    }
    if (write(log_fd, "\n", 1) != 1) log_bad = 1;
}
static void number(const char *tag, unsigned long long v)
{
    const char hex[] = "0123456789ABCDEF";
    char s[112] = "PS4WL Anchor: ";
    size_t i = 14, j;
    while (*tag && i < sizeof(s) - 19) s[i++] = *tag++;
    s[i++] = '0'; s[i++] = 'x';
    for (j = 0; j < 16; ++j) s[i++] = hex[(v >> (60 - 4 * j)) & 15];
    s[i] = 0; report(s);
}
static void identity(const char *name)
{
    char data[512], s[128];
    size_t n = sizeof(data), offset = 0, i, k, part = 0;
    int rc;
    for (i = 0; i < sizeof(data); ++i) data[i] = 0;
    errno = 0;
    rc = query(name, data, &n);
    if (rc != 0 || n == 0 || n > sizeof(data)) {
        report("PS4WL Anchor: metadata unavailable (not a verified build ID)");
        number(name, (unsigned long long)(long long)rc);
        number("metadata errno=", (unsigned)errno);
        return;
    }
    while (offset < n && data[offset]) {
        i = 0; s[i++] = '['; s[i++] = (char)('1' + part++);
        s[i++] = ']'; s[i++] = ' ';
        for (k = 0; name[k] && i < 48; ++k) s[i++] = name[k];
        s[i++] = ':'; s[i++] = ' ';
        for (k = 0; k < 64 && offset < n && data[offset]; ++k) {
            unsigned char c = (unsigned char)data[offset++];
            s[i++] = c >= 32 && c <= 126 ? (char)c : ' ';
        }
        s[i] = 0; report(s);
    }
    if (!offset) report("PS4WL Anchor: empty metadata");
}
static unsigned ring(void)
{ unsigned short cs; __asm__ volatile("mov %%cs,%0" : "=r"(cs)); return cs & 3; }
int main(void)
{
    unsigned long begin, end;
    size_t bytes;
    int rc, saved, data_locked = 0, code_locked = 0, result = 1;
    log_fd = -1; log_bad = 0;
    transport_init();
    /* Screen evidence is mandatory; USB I/O is optional. */
    if (!screen_ready()) { write(1, "PS4WL Anchor: notifications unavailable; stopped\n", length("PS4WL Anchor: notifications unavailable; stopped\n")); return 1; }
    report("PS4WL Anchor: build " PS4WL_BUILD_ID);
    log_fd = open("/mnt/usb0/PS4WL_ANCHOR.LOG", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (log_fd < 0) log_fd = open("/mnt/usb1/PS4WL_ANCHOR.LOG", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (log_fd < 0) report("PS4WL Anchor: USB unavailable; use screen results");
    report("BUILD: " PS4WL_BUILD_ID);
    identity("kern.osrelease"); identity("kern.version");
    if (ring() != 3) { report("PS4WL Anchor: unexpected caller ring; stopped"); goto done; }
    pwl_anchor_result = (struct pwl_anchor_snapshot){0};
    pwl_anchor_result.state = PWL_ANCHOR_PENDING;
    __asm__ volatile("leaq pwl_anchor_capture(%%rip), %0" : "=r"(begin));
    __asm__ volatile("leaq pwl_anchor_capture_end(%%rip), %0" : "=r"(end));
    bytes = end > begin ? end - begin : 0;
    if (!bytes || bytes > 4096) { report("PS4WL Anchor: invalid callback extent"); goto done; }
    if (mlock((const void *)&pwl_anchor_result, sizeof(pwl_anchor_result)) != 0) {
        number("snapshot mlock errno=", (unsigned)errno); goto done;
    }
    data_locked = 1;
    if (mlock((const void *)begin, bytes) != 0) {
        number("callback mlock errno=", (unsigned)errno); goto done;
    }
    code_locked = 1;
    report("PS4WL Anchor: entering returning observation");
    if (log_fd >= 0 && !log_bad && fsync(log_fd) != 0) log_bad = 1;
    errno = 0;
    rc = kexec((void (*)(void))begin, (void *)0); saved = errno;
    number("callback rc=", (unsigned long long)(long long)rc);
    number("callback errno=", (unsigned)saved);
    number("state=", pwl_anchor_result.state);
    if (rc < 0 || ring() != 3 || pwl_anchor_result.state != PWL_ANCHOR_COMPLETE ||
        (pwl_anchor_result.cs & 3) != 0) {
        report("PS4WL Anchor: callback observation not confirmed"); goto done;
    }
    number("LSTAR=", pwl_anchor_result.lstar);
    number("RFLAGS=", pwl_anchor_result.flags);
    number("entry RSP modulo 16=", pwl_anchor_result.stack_mod16);
    number("CS=", pwl_anchor_result.cs);
    if (pwl_anchor_result.lstar < 0xffff800000000000ULL) {
        report("PS4WL Anchor: unexpected LSTAR; no kernel base derived"); goto done;
    }
    report("PS4WL Anchor: user return confirmed; memory binding unverified");
    result = 0;
done:
    if (code_locked && munlock((const void *)begin, bytes) != 0) number("code munlock errno=", (unsigned)errno);
    if (data_locked && munlock((const void *)&pwl_anchor_result, sizeof(pwl_anchor_result)) != 0) number("data munlock errno=", (unsigned)errno);
    if (log_fd >= 0) {
        if (fsync(log_fd) != 0) log_bad = 1;
        if (close(log_fd) != 0) log_bad = 1;
    }
    if (log_bad) screen("PS4WL Anchor: USB log failed; screen result remains valid");
    screen("PS4WL Anchor: finished; photograph notifications");
    return result;
}
