# Returning resident calls on a separate process stack

Process callback build `3f52f8ca4d8cd1972cf9de8fe988509232b7eee5` passed
on the PS4: `result=0 passed_mask=1ff last_call=9 release=0`. Copied resident
code executed at process address `0x200CE8000`, above 4 GiB. The expected
ExitBootServices stub returned `8000000000000003`. No CPU address-context change
occurred. That completed callback test does not need repeating.

This new variant additionally allocates a 1 MiB RW process stack between two
16 KiB PROT_NONE guards. A small SysV assembly entry saves the old stack pointer
and preserved registers, aligns the new top to 16 bytes, invokes the callback,
then restores the original stack and returns the callback result. The callback
checks that a local stack object lies inside the new stack and runs all nine
resident checks through Microsoft x64 ABI entries. It performs no SDK or USB
calls on the new stack. Durable checkpoints before and after the whole sequence
are written on the original stack.

Host tests verify aligned entry, repeated return, preserved results, expected
faults on both guard pages, and the actual copied resident services on the
separate guarded stack. GCC and Clang run these tests. Stack-switch tests are
isolated from ASan because they do not install ASan's fiber annotations; the
ordinary resident and workspace tests retain their sanitizers.

## Single console test

1. Use the existing dedicated USB and send `PS4WindowsLoader-Resident-Stack.bin`
   once through the same method.
2. Retrieve `PS4WL_STACK.LOG`, or photograph notifications if logging fails.
3. Expected: `checkpoint=AFTER_STACK restored=1 stack_release=0`, then
   `result=0 passed_mask=1ff last_call=9 release=0 mode=PROCESS_STACK_TEST cpu_switch=0`.
4. If it hangs, do not repeat the run. Preserve the last log checkpoint.

Build `91ce9bc` passed on the PS4: `restored=1 stack_release=0 result=0
passed_mask=1ff last_call=9 release=0`. The observed local address was inside
the new stack and the original stack pointer matched after return. Do not repeat
this completed test. The memory descriptor
map remains synthetic; returned allocation addresses are never dereferenced.
It is a process stack test, not execution in the kernel workspace. No CR3,
interrupt, CPU control-register or platform device changes are made. Passing
does not establish privileged resident execution or a Windows handoff.

Build: `sh tools/build_resident_calls.sh SDK_DIRECTORY COMMIT stack`.


The following environment audit also passed on PS4 in build `0a5199ac`:
`rc=0 stage=5 error=0 prep=0 tables_status=0 release=0 copies=1 efi_status=0`.
Its allocation started at PA `0x100000`, with root `0x105000`. This is the new
entry appended after the earlier `64a2f64` result. Neither test activated tables.
