#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
static int mock_errno, lock_fail, locks, unlocks, calls, mode, no_screen, bad_log;
static char notices[40][160];
static size_t notice_count;
static void transport_init(void) {}
static int screen_ready(void) { return !no_screen; }
static void screen(const char *s) { assert(notice_count < 40); snprintf(notices[notice_count++],160,"%s",s); }
static int query(const char *s, void *p, size_t *n)
{ (void)s; assert(*n >= 12); memcpy(p,"target-test",12); *n=12; return 0; }
static int test_open(const char *s, int f, ...)
{ (void)s; (void)f; return bad_log ? open("/dev/full", O_WRONLY) : -1; }
static int test_lock(const void *p, size_t n)
{ assert(p && n); return ++locks == lock_fail ? -1 : 0; }
static int test_unlock(const void *p, size_t n)
{ assert(p && n); ++unlocks; return 0; }
static int test_callback(void (*f)(void), void *a);
#define PWL_ANCHOR_TEST 1
#define errno mock_errno
#define mlock test_lock
#define munlock test_unlock
#define open test_open
#define kexec test_callback
#define main anchor_main
#include "../payload/anchor_probe.c"
#undef main
#undef kexec
#undef open
#undef munlock
#undef mlock
#undef errno
static int test_callback(void (*f)(void), void *a)
{
    assert(!a); ++calls;
    if(mode==0) { mock_errno=78; return -1; }
    if(mode==1) { f(); return 0; }
    if(mode==2) {
        pwl_anchor_result.cs=0x20;
        pwl_anchor_result.state=PWL_ANCHOR_COMPLETE;
        pwl_anchor_result.lstar=0xffffffff800001c0ULL;
        pwl_anchor_result.flags=0x202;
        pwl_anchor_result.stack_mod16=8;
    }
    return 0;
}
static int contains(const char *s)
{ size_t i; for(i=0;i<notice_count;++i) if(strstr(notices[i],s)) return 1; return 0; }
static void reset(void)
{ mock_errno=lock_fail=locks=unlocks=calls=mode=no_screen=bad_log=0; notice_count=0; log_bad=77; }
int main(void)
{
    reset(); no_screen=1; assert(anchor_main()==1 && calls==0 && locks==0);
    reset(); lock_fail=1; assert(anchor_main()==1 && calls==0 && unlocks==0);
    reset(); lock_fail=2; assert(anchor_main()==1 && calls==0 && unlocks==1);
    reset(); assert(anchor_main()==1 && calls==1 && unlocks==2);
    reset(); mode=1; assert(anchor_main()==1 && calls==1);
    assert(pwl_anchor_result.state==PWL_ANCHOR_WRONG_RING);
    assert(!contains("LSTAR="));
    reset(); mode=3; assert(anchor_main()==1 && calls==1);
    reset(); mode=2; assert(anchor_main()==0 && calls==1 && unlocks==2);
    assert(contains("USB unavailable") && contains("LSTAR=0xFFFFFFFF800001C0"));
    assert(contains("memory binding unverified") && contains("target-test"));
    reset(); mode=2; bad_log=1; assert(anchor_main()==0 && calls==1 && unlocks==2);
    assert(contains("USB log failed") && contains("LSTAR=0xFFFFFFFF800001C0"));
    puts("anchor probe: screen output, optional USB, locking and failed callbacks checked");
    return 0;
}
