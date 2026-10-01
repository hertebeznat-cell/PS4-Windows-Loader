# Combined returning EFI service check

This experimental file calls all 30 implemented resident callbacks during the
existing identical-root round trip. It adds pools with an actual scratch-page
write, protocol installation/replacement/discovery/open/close/removal, and all
read-only file methods using a built-in three-byte file. It needs no boot archive
or Microsoft files on USB. File writes are expected to return write-protected.

Send PS4WindowsLoader-EFI-All30.bin once. Results appear in notifications even
when USB logging is unavailable. Photograph these final notifications:

- returned rc=0 stage=5 error=0
- status=0 switched=1 restored=1 released=1
- EFI passed=3fffffff last=30 code_release=0

The callback mask contains all thirty successes. A smaller mask or another last
value identifies the failed check. USB reports retain their existing names;
the source build and passed mask distinguish this run. No Microsoft code is
called, no independent EFI root is activated, and platform memory/device
ownership is not validated by success. This is not a Windows launcher or a
promise of the final prerequisite. Console faults can still occur; if it hangs,
do not repeat the run.

## Confirmed hardware result, 2026-10-01

User photographs of build b47eca5 confirm rc=0 stage=5 error=0,
status=0 switched=1 restored=1 released=1 and passed_mask=3fffffff
last_call=30 code_release=0. All thirty callback checks passed in the returning
identical-root context. Root PA changed ac5a000 -> 527c8000 -> ac5a000;
stack returned to ffffff8081c4f840 after using ffffff8081c5c000.
The current direct-map indices were 439/412, not the earlier boot's 436/348.
USB report persistence separately returned stage=-5 errno=0 and is not confirmed.
Do not repeat this completed diagnostic. Native EFI entry and Windows remain
unexecuted. Evidence: user photos 1000009859, 1000009865, 1000009866,
1000009867 and 1000009868.
