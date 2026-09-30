# Experimental returning resident-workspace preparation

One-page hardware allocation test passed on firmware 13.52 / GoldHEN v2.4b18.12:
commit ee371c259f1604046dd5f6f53b22a3033e9c7753, rc=0 stage=5 error=0,
critical=0 locks=0, CS=0x20 RFLAGS=0x246, PA=0x1BA68000. That trace does not
validate larger allocations, CPU transition or independent physical reclamation.

This diagnostic uses the same guarded callback and mlocked user image. It
prepares a larger owned arena using the existing native-workspace pipeline:
synthetic resident code, memory/media metadata, independent page tables,
64 KiB stack with unmapped guards, 512-byte synthetic media and 64 KiB heap.
The synthetic code/media are not EFI firmware or a Windows disk. Nothing is
executed from the arena. CR3/CPU/IRQ/AP/DMA state is not changed.

A separate diagnostic adapter accepts only the API established within the
same guarded callback. It is compiled only with PWL_WORKSPACE_DIAGNOSTIC;
production ps4_binding.c still refuses binding. This is not a user-supplied
verified flag or the host-fixture path. Freestanding memory helper symbols are
renamed to avoid collisions with SDK pointer globals.

The whole allocation is physically checked, resident copies are compared,
identity mappings and permissions are validated, and the original KVA is
released before return. A cleanup refusal retains the owner and is reported.
A successful void kmem_free return is not independent reclamation evidence.
No full RAM/MMIO map, EFI System Table, relocated firmware callbacks or CPU
handoff has been established. PAT/MTRR compatibility must precede activation.

## One console experiment

Risk of kernel panic or deadlock remains; larger contiguous allocation and
more pmap/VM calls have not been tested on this console. Use the same GoldHEN
session, connect USB0 and send only PS4WindowsLoader-Workspace-Probe.bin once.
Send PS4WL_WORKSPACE.LOG and photograph notifications. Do not repeat Memory-
Probe or the completed dumper. If the console stops responding, report the
last notification and do not retry or bypass guards.

Expected successful result: rc=0 stage=5 error=0; prep=0 tables_status=0
release=0 copies=1; nonzero arena bytes/root/table count. Stage 5 is the end of
this diagnostic, not Stage 5 of Windows boot. Error 13 means workspace/core
failure; the detailed statuses in the log identify the failing step.

Build: sh tools/build_workspace_probe.sh SDK_DIRECTORY PROJECT_COMMIT.
SDK commit b7326416c23ce14639e9180a24093bdc5eedb579. The binary must be PIE/DYN,
have no runtime relocations and include every allocated section with zero BSS.
