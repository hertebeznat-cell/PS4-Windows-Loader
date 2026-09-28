# Execution boundary: Stage 4.8

## What the console traces establish

The current payload runs inside a PS4 user process. It maps Microsoft's
`bootmgfw.efi` image at `0x40000000`, relocates it, and calls its EFI entry
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

## Work that can proceed without another console run

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
