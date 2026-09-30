# Returning resident callback test in the process context

Resident preparation build `64a2f64211849a68a307a23da8d3778ea3d7a019`
completed on the PS4. The supplied log records `rc=0 stage=5 error=0`,
224 KiB owned memory at PA `0x516C0000`, four table pages, eight regions,
successful copies/table validation/release and `efi_status=0 code_called=0`.
That run establishes preparation and return, not executable permission of the
kernel allocation or a CPU transition. It does not need repeating.

This new diagnostic tests real calls from a separate copied resident image in
the existing **process** address context. It allocates RW process memory,
copies and binds the linked code to independent process data, requests RX
permissions, checks the image and context binding, then invokes nine entries
with Microsoft x64 ABI. It does not execute from the kernel workspace, use its
physical pointers, activate CR3 or enter Microsoft code. The descriptor map is
explicitly synthetic; allocated page addresses are never dereferenced.

Before each call it syncs a checkpoint to `PS4WL_CALLS.LOG` on USB0 or USB1.
The test stops before execution if logging or the RX mapping fails. It checks
TPL state, page allocation/free and map-key change, GetMemoryMap sizing and
descriptors, CRC32, overlapping CopyMem in both directions and SetMem.
ExitBootServices must return `EFI_UNSUPPORTED` without retiring the manager.
The code mapping is released after a normal return.

## One test

1. Use the existing dedicated USB and send `PS4WindowsLoader-Resident-Calls.bin`
   once through the same method.
2. Return `PS4WL_CALLS.LOG` or photograph screen notifications if writing fails.
3. Expected final line: `result=0 passed_mask=1ff last_call=9`,
   `exit_status=8000000000000003 release=0 mode=PROCESS_CALLBACK_TEST cpu_switch=0`.
4. An RX permission refusal is a stopped test, not a reason to change page
   permissions through undocumented platform calls. Preserve that log.
5. If the console hangs, do not repeat the run.

Hardware execution of this new variant is unverified; a crash remains possible.
Passing establishes process-context callback behavior only. Privileged resident
execution, executable kernel mappings, platform handoff and Windows boot still
require separate work.

Build: `sh tools/build_resident_calls.sh SDK_DIRECTORY COMMIT`.
