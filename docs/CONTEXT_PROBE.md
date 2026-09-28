# Returning CPU context probe

## Purpose and status

`PS4-Windows-Loader-Context-Probe` tests the next unresolved boundary: can the
existing PS4 runtime call a small resident function at CPL0 and return to the
user process? It records actual CR0, CR3, CR4 and EFER values only after checking
CPL0. It does not call Boot Manager or establish a new EFI environment.

This is a new hardware test, not the repeated Stage 4.8 preflight. Host tests
cover rejection at CPL3 and mocked transport/error/reporting paths. The CPL0
path and return have **not yet been verified on the target PS4**.

## Requirements

- The already-installed runtime must support its `kexec(callback, argument)`
  facility (syscall 11 in the pinned runtime) and access to the resident
  payload code/data from that callback. This probe does not install the facility
  or apply firmware patches. Presence of a callable symbol in a binary does
  not prove the console has the corresponding runtime facility.
- USB storage available as `/mnt/usb0`, writable and supporting log flush.
- Successful `mlock` of callback code and snapshot storage; failure aborts
  before the callback. Residency does not establish physical ownership or
  independently validate the runtime implementation.

The implementation uses the callback route visible in the pinned runtime's
`linux/main-aio.c`, without its kernel allocation, patches, Linux kexec blob,
shutdown sequence or device changes. If the facility is absent or incompatible,
the console may reject the call or fault. The probe is not a recovery mechanism.

## One test

1. Download **PS4-Windows-Loader-Context-Probe** from the successful Actions run.
2. Extract the archive and launch `PS4WindowsLoader-Context-Probe.bin` using the
   existing payload sender. Do not rename it to the Stage 4.8 payload.
3. After it returns, collect `/mnt/usb0/PS4WL_CONTEXT.LOG`.

No Windows files are required for this test. The result records its source
commit in `BUILD:` and `COMMIT.txt`. The archive contains checksums and callback
disassembly for review. This result will determine whether the existing runtime
can supply a returning context for further platform inspection.

## Read the result

| Last result | Meaning |
| --- | --- |
| `snapshot mlock failed` / `callback mlock failed` | Callback not attempted; inspect errno |
| `log flush failed` | Callback not attempted; log destination did not flush |
| `calling existing runtime callback`, without `runtime call returned` | Return is not established; do not infer success from this truncated log |
| `privileged callback and return not confirmed` | Runtime failed, did not call the function, or execution/return was at the wrong privilege level |
| `privileged reads and user return observed; EFI handoff unverified` | CPL0 callback completed and returned to CPL3; this is not a Windows boot result |

A successful report still does not establish a physical RAM map, EFI memory
ownership, CPU/device handoff, working storage callbacks after host shutdown,
or the ability to execute Boot Manager in that context. Calling all of Boot
Manager inside the kernel callback is not the next automatic step: existing
EFI callbacks still depend on the user process and its system calls.

## Implementation and validation

- `payload/context_capture.S`: 105-byte returning callback; checks CS before
  `MOV CRn` / `RDMSR`, reads EFER at `0xC0000080`, preserves callee-saved
  registers and stack, and writes only the payload's snapshot.
- `payload/context_probe.c`: user-process preparation, memory residency,
  log flush, runtime invocation and result reporting. File I/O happens outside
  the callback. All locks are released on ordinary exit paths.
- `tests/test_context_capture.c`: executes the real callback at CPL3 and checks
  that privileged fields remain unchanged.
- `tests/test_context_probe.c`: mocks the transport to test preparation failures,
  missing callback, CPL3 rejection and synthetic successful reporting. Synthetic
  success does not exercise privileged instructions.

References: pinned runtime commit
`f70f43a60a973b3662daeb29c1f115d95408db93`, `lib/syscalls.py`,
`linux/main-aio.c`; AMD64 Architecture Programmer's Manual,
[Volume 2, System Programming](https://www.amd.com/content/dam/amd/en/documents/processor-tech-docs/programmer-references/24593.pdf).
