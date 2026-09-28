# Returning CPU context probe

## Purpose and status

`PS4-Windows-Loader-Context-Probe` tests the next unresolved boundary: can the
existing PS4 runtime call a small resident function at CPL0 and return to the
user process? It records actual CR0, CR3, CR4 and EFER values only after checking
CPL0. It does not call Boot Manager or establish a new EFI environment.

Build `dedeffdd0515187056e155c55b997433e0c3b6db` produced the on-screen
notification `returned; PS4WL_CONTEXT.LOG ready` on the target PS4. In this
source version that notification follows a completed callback, CPL0 CS check,
return to CPL3, successful file close and no detected write error. The
photographs confirm the final notification; the USB log with the recorded
register values has not yet been reviewed. The tester found no log on the USB
drive despite that notification. A successful close does not prove that the
PS4 mount path is exposed to a PC. New builds also show the four register
values as on-screen notifications and no longer claim the file is ready.

Host tests cover rejection at CPL3 and mocked transport/error/reporting paths.
An initial hardware run of build `84d35cb` stopped before the callback: the
raw payload was relocated, while one C function pointer retained its linked
address. The callback address and extent are now resolved relative to RIP at
runtime, with a linked-ELF check in CI.

## Requirements

- The already-installed runtime must support its `kexec(callback, argument)`
  facility (syscall 11 in the pinned runtime) and access to the resident
  payload code/data from that callback. This probe does not install the facility
  or apply firmware patches. Presence of a callable symbol in a binary does
  not prove the console has the corresponding runtime facility.
- USB storage available as `/mnt/usb0` or `/mnt/usb1`, writable and supporting
  log flush. The probe tries USB0 first, then USB1 if opening the file fails.
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
3. Watch the on-screen notification: it identifies USB0 or USB1, reports
   open errors with their errno values, or reports a write/flush failure.
4. After it returns, photograph the `CR0`, `CR3`, `CR4`, and `EFER` notices
   (including leading zeroes) in the PS4 notifications list. A log may also
   be available at `/mnt/usb0/PS4WL_CONTEXT.LOG` or `/mnt/usb1/PS4WL_CONTEXT.LOG`
   through a PS4 file manager/FTP, but the notification does not assert that
   a PC can read it from a removable drive.

No Windows files are required for this test. The result records its source
commit in `BUILD:` and `COMMIT.txt`. The archive contains checksums and callback
disassembly for review. The next inspection needs the actual register values
from the on-screen notices or log before using them as input to a platform memory inventory.

## Log initialization and raw binary packaging

A hardware report for build `12870df` said no log was found; the exact cause
was not established. Inspection found that its `.bin` omitted the ELF `.bss`
bytes and its write-error flag depended on initially zero memory. The updated
packaging serializes zeroed BSS into the raw image and checks every shipped
code/data section against the ELF. The probe also resets its logging state
on every invocation, including runs in reused payload storage.

The header is written and flushed before memory locking or runtime entry.
A failed log open or write/flush is reported via the same system notification
mechanism as the existing Stage 1 payload, with stdout as an additional channel.
The callback is not attempted if file logging cannot be established.

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
- `payload/context_probe.c`: user-process preparation, relocation-aware callback
  address resolution, memory residency,
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

The reporting tests also cover stale write-error state, failure of both USB
paths, successful USB1 fallback, and a full log device. The packaging check
rejects raw images that omit or alter BSS initialization bytes.
