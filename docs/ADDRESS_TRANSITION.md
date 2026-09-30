# Returning address-context entry: development only

`loader/src/address_call.S` provides a SysV CPL0 entry that saves callee-preserved
registers, RFLAGS, RSP and CR3, masks ordinary interrupts, selects a supplied
root and aligned stack, calls a resident function, and restores CR3 and RSP
before restoring flags and returning its integer result. The report records
old, entered and restored root/stack addresses. It does not change descriptors,
segment registers, EFER, CR0 or CR4. The report layout is in
`pwl_address_call.h`.

This entry is compiled into the development object only. No console build
includes or calls it. Assembly/link inspection does not establish execution.
It has not executed on the PS4. No emulator is used for this work.

## Required integration before a PS4 execution test

The currently prepared identity tables cover only the owned workspace. The
transition entry is not present at its current virtual address under that root.
Activating that root from the current code would therefore lose execution.

A platform integration must first provide and verify:

- A resident transition entry at the same executable virtual address in both
  address contexts, plus the same writable report address in both contexts.
- Verified translation of those pages under the current root and exact mappings
  under the new root, with coherent RAM cache attributes.
- A current CPU snapshot, PCID disabled, four-level paging and NX enabled.
- A resident target callback, context and stack; guarded stack boundaries;
  preserved original stack and root for return.
- Mappings and state for NMI, machine-check and other exception delivery,
  including descriptor tables, handlers and their stack dependencies. CLI alone
  does not supply this. The entry has no exception recovery handler.
- An execution context whose CPU cannot migrate during the transition and whose
  page ownership remains stable, plus a checkpoint saved before entry.

The returning transition is a bounded callback experiment, not an OS handoff.
It must not call the SDK, USB logging, allocators or other host services while
using the alternate root. It must restore the original root before invoking
those services or releasing the owner.

Compatible platform checks should run in one console transaction, stop on the
first failed gate and write one log. Completed console tests need not be run
again individually. No transition binary is offered until the above platform
requirements are implemented and checked. Windows Boot Manager is not entered.
