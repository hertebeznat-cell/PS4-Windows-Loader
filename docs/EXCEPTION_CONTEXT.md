# Returning exception-context observation

This experimental file reads the active GDT, IDT, task selector and the first
104 bytes of the selected AMD64 TSS. It decodes privilege stacks and seven IST
stack pointers, plus NMI (2), double fault (8), GP (13), PF (14) and machine check
(18) gates. It records CR3, PAT and GS/KERNEL_GS bases and verifies the descriptor
registers/task selector/root did not change between the two observations.

Send PS4WindowsLoader-Exception-Context.bin once. USB is unnecessary; photograph
all PS4WL Context notifications, especially:

- rc=0 stage=4 error=0 unlock=0
- GDT, IDT, TSS, PAT, GS/KGS
- RSP0, IST1 through IST7
- vector=2/8/13/14/18 with entry, CS and IST

Zero unused RSP/IST fields are allowed. A gate naming a zero IST pointer is
rejected. Unexpected descriptor types, reserved bits, short table extents,
noncanonical addresses and a changed register snapshot stop the check.
Malformed fields are decoded on copied bytes; descriptor-derived physical
addresses are never dereferenced by the decoder. The observer copies only live
CPU-installed high virtual GDT/IDT/TSS addresses after bounded extent checks.
It reads PAT only when CPUID advertises PAT. Code has no GDT/IDT/TSS installation,
CR3 writes, IRQ masking, device writes, EFI calls or Windows entry.

The first complete observation is stage=4. It is a point-in-time sample, not a
proof that the context cannot migrate or that table memory cannot change.
Identical register endpoints do not prove identical descriptor bytes. Full
handler/data dependencies, physical translations and mapped stack extents still
need verification before independent-root execution. A single handler entry
page cannot stand in for its complete code/data dependencies.

This gathers different missing facts; the passed All30 test is not repeated.
Do not modify the USB's Windows files. On a hang, do not repeat the run.

## Hardware observation, 2026-10-01

All 23 supplied photographs of build
594a017c7b67783f69b65ac3cf6b31da8f4b300e were inspected. The observation returned
rc=0 stage=4 error=0 unlock=0, with CR3 ac5a000 at both endpoints. GDT, IDT,
selected TSS, stack fields and all five requested gates were reported.
The test passed; repeating this observation is unnecessary.

This confirms successful observation and decoding on the console. It does not
confirm independent-root execution, complete handler dependencies or Windows
Boot Manager entry. Live addresses must be captured again by the implementation
when preparing a transition; the photographed addresses must not be hardcoded.
