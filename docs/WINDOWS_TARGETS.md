# Windows 11 and Windows Server 2025 targets

Status: preparation only. No Windows kernel or setup has run on the target PS4.
The CI artifact exits before calling Microsoft Boot Manager. The two observed
privileged instructions in `bootmgfw.efi` prove the current PS4 user-process
approach cannot complete the boot path. See [Execution boundary](EXECUTION_BOUNDARY.md).

## Requirement and evidence matrix

| Area | Windows 11 | Windows Server 2025 | Evidence for PS4 Slim CUH-2208B |
| :-- | :-- | :-- | :-- |
| Processor | Compatible 64-bit processor, 2+ cores, 1 GHz+ | x64, 1.4 GHz+, NX, CMPXCHG16b, LAHF/SAHF, PrefetchW, NPT/SLAT, SSE4.2, POPCNT | Sony lists an eight-core x86-64 Jaguar. Clock and CPUID feature evidence for this exact console have not been captured by this project; no compatibility claim. |
| RAM | 4 GB minimum | 2 GB minimum in the current Microsoft hardware guide; configuration matters | Sony lists 8 GB GDDR5 installed. The loader has not established a truthful usable physical RAM map. |
| Firmware | UEFI and Secure Boot capable | Secure Boot and TPM requirements depend on enabled features | Synthetic EFI callbacks have run in a PS4 process. No complete firmware or OS handoff. |
| TPM | TPM 2.0 required | TPM required for some features, such as BitLocker; 2.0 requirements apply when present | No TPM 2.0 interface has been demonstrated. |
| Display | DirectX 12 graphics with WDDM 2.0 driver in Microsoft's system requirements | Console path depends on installation/role | No PS4 Windows display driver is in this repository. A framebuffer handoff is not a WDDM driver. |
| Storage and input | Installation medium and working drivers required | Installation medium and working drivers required | USB reads during payload execution are verified. No Windows storage or input drivers for PS4 have been demonstrated. |

For Windows 11, the incompatible or unverified firmware, TPM, processor
eligibility, and graphics support are independent blockers. Server 2025 has
different feature-dependent TPM/Secure Boot conditions; it still cannot boot
through the existing user-process path. No installation image, Microsoft
binaries, firmware, license, or PS4 Windows drivers are distributed here.

## Build sequence and acceptance gates

1. **CPU and platform evidence:** capture CPUID leaves, a verified physical
   memory inventory, paging/interrupt state, device ownership, and a recoverable
   transition path. Do not confuse a user virtual address with a physical one.
2. **Firmware context:** construct page tables and a real physical memory map,
   provide ACPI/platform description and EFI services with accurate ownership
   and `ExitBootServices` semantics. Validate the transition independently of
   any Microsoft code before attempting Boot Manager.
3. **Boot chain:** use a legitimately supplied EFI boot image to reach
   `bootmgfw.efi`, then `winload.efi`, then kernel initialization, measuring each
   milestone separately. A firmware callback alone does not pass this gate.
4. **Usable OS:** provide storage, display, USB input, and other PS4-specific
   Windows drivers, then assess installation and normal operation separately
   for Windows 11 and Server 2025.

The portable memory-layout validator and EFI descriptor converter now reject
bad placement and only convert caller-supplied physical regions with explicit
cacheability. They do not discover RAM, build page tables, or authorize a CPU
transition. They remain separate from the PS4 payload until a platform backend
can supply verified data.

The Stage 4.8 preflight records raw CPUID leaves 0, 1, 7 (when supported),
`0x80000000`, `0x80000001` and `0x8000000A` (when supported) to its USB log.
This makes CPU evidence available after the next console check. No values for
the target PS4 are assumed in this matrix before a matching log is captured.

Sources: [Microsoft Windows 11 requirements](https://learn.microsoft.com/en-us/windows/whats-new/windows-11-requirements),
[Microsoft Windows Server hardware requirements](https://learn.microsoft.com/en-us/windows-server/get-started/hardware-requirements),
[Sony PS4 technical specifications](https://www.playstation.com/en-us/ps4/tech-specs/),
[UEFI Boot Services specification](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html).
