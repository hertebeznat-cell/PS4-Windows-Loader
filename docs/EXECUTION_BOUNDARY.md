# Execution boundary: Stage 4.8

## What the console traces establish

Earlier experimental payloads ran inside a PS4 user process. They mapped Microsoft's
`bootmgfw.efi` image at `0x40000000`, relocated it, and called its EFI entry
with synthetic Boot and Runtime Services. This successfully produced firmware
callbacks, including `HandleProtocol` and an `AllocatePages` request for
`0x102000`. A successful callback shows that those calls ran; it does not
establish a viable Windows firmware environment.

| Trace | Image offset | Instruction | Observation |
| :-- | --: | :-- | :-- |
| Stage 4.8 (3) | `0x58666` | `0F 20 D8` (`mov rax, cr3`) | `SIGBUS`, code 3 after `AllocatePages` succeeded |
| Stage 4.8 (4) | `0x28544A` | bytes captured in Stage 4.8 (6) | Reached only after an experimental zero substitution for the first instruction |
| Stage 4.8 (6) | `0x28544A` | `0F 32` (`rdmsr`) | `SIGBUS`, code 3; handler logged the image bytes and signal context |

The earlier zero substitution for `CR3` was removed. A console shutdown and
subsequent system storage check were reported after an experimental run. The
trace ends in the fault handler; it does not identify the cause of the console
shutdown. The current CI artifact deliberately exits before calling
`bootmgfw.efi`. No additional console run is needed to identify these two
instructions.

## Consequence for the design

Reading `CR3` and executing `RDMSR` require processor privileges that the
current user process does not have. Matching individual instruction byte
patterns and returning fabricated values does not provide a coherent address
space, CPU state, or firmware handoff. This path cannot be treated as an
incremental route to `winload.efi` until execution moves out of the PS4 user
process into a controlled boot context.

The next implementation needs a separately validated transition that owns the
CPU context and physical memory before invoking the EFI image. It also needs
an accurate RAM map, page tables, platform configuration tables, storage and
console services, and a defined `ExitBootServices` handoff. Device ownership
and outstanding DMA must be understood before the host OS is displaced. These
are design requirements, not features implemented by Stage 4.8.

## Portable implementation now available

`loader/src/handoff.c` validates a supplied physical memory map and the
placement of an image, stack and root page-table page. It rejects overlapping,
unaligned or overflowing regions, unknown memory, incorrect ownership kinds,
and entry offsets outside the image. Adjacent regions of the same kind are
accepted; gaps are never implicitly treated as RAM.

`tests/test_handoff.c` uses synthetic memory regions to exercise valid and
invalid layouts. CI compiles the module freestanding and runs the tests with
AddressSanitizer and UndefinedBehaviorSanitizer. These tests do not need a PS4.

This module is not wired into the PS4 payload yet: the payload has no source
of verified physical-region data. `PWL_OK` means the supplied placement data
is internally consistent. It does not certify page-table contents, virtual
addresses, CPU mode, device state, or permission to transfer control.

The portable descriptor converter also accepts explicit cacheability for each
verified region. It reports the required descriptor count before writing,
rejects unsupported or missing attributes, and preserves unknown address gaps.
It is a checked conversion of evidence supplied by a future backend, not a
source of physical address information.

`loader/src/paging.c` validates supplied four-level page-table snapshots for
identity mapped image, stack and table pages. CI compiles it freestanding and
tests the walk with sanitizer instrumentation. The snapshots cannot establish
the active CR3, page ownership or safe execution. The detailed prerequisites
are in [the EFI handoff contract](EFI_HANDOFF_CONTRACT.md).

## Existing transition code: adaptation required

The pinned external runtime includes `linux/ps4-kexec-common/linux_boot.c`
and `linux_thunk.S`. That path builds Linux boot parameters, changes page
tables and segments, and transfers to Linux startup. The current CI links
only the runtime library; it does not build or execute that Linux transition.
Its argument layout and entry sequence cannot be substituted for an EFI call.
Adapting it requires a Windows/EFI-specific firmware environment that no longer
depends on PS4 user-process syscalls after the transition.

## Returning callback implementation

The [context probe](CONTEXT_PROBE.md) uses the pinned runtime callback facility
to read CPU state and return. Build `dedeffd` showed a successful return
notification on the target PS4, following the callback's CPL0 and user-return
checks. Build `abe468c` then showed CR0 `0x8005003B`, CR3 `0x0B28B000`,
CR4 `0x406F0` and EFER `0xD01` on-screen. This callback does not
replace the EFI handoff or remove the firmware layer's process dependencies.

## Remaining implementation

The [native preparation transaction](NATIVE_BACKEND.md) now joins a kernel
allocation adapter, physical page checks, resident memory/media state and
page-table construction. It is compiled/tested separately, with no Stage 4.8
entry connection. The source audit and exact remaining links are documented
there; its memory retirement function is not a platform ExitBootServices.
The [13.52 memory binding gate](PS4_1352_BINDING.md) now prevents allocation
and release through unverified symbols; there is no new console test.

1. Separate the portable PE/COFF and EFI-table construction code from calls
   that depend on the live PS4 process. Keep the current preflight as the
   default artifact while the execution architecture is reviewed.
2. Specify a handoff contract for a future boot context: entry registers,
   page tables, physical memory ownership, interrupt state, and a way to
   recover if the handoff fails. Validate each component independently.
3. Construct and unit-check EFI memory descriptors from actual physical RAM
   regions rather than treating `mmap` addresses as physical addresses.
4. Resume Boot Manager entry only after the new context and its recovery path
   are verified on the target hardware.

References: [Intel architecture manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html), [UEFI Boot Services](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html).
