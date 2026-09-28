# Windows 11 and Windows Server 2025 targets

Status: preparation only. No Windows kernel or setup has run on the target PS4.
The CI artifact exits before calling Microsoft Boot Manager. The two observed
privileged instructions in `bootmgfw.efi` prove the current PS4 user-process
approach cannot complete the boot path. See [Execution boundary](EXECUTION_BOUNDARY.md).

## Requirement and evidence matrix

| Area | Windows 11 | Windows Server 2025 | Evidence for PS4 Slim CUH-2208B |
| :-- | :-- | :-- | :-- |
| Processor | Compatible 64-bit processor, 2+ cores, 1 GHz+ | x64, 1.4 GHz+, NX, CMPXCHG16b, LAHF/SAHF, PrefetchW, NPT/SLAT, SSE4.2, POPCNT | Sony lists an eight-core x86-64 Jaguar. Matching hardware logs report all seven listed instruction/translation flags as present. Clock and Windows 11 processor eligibility remain unverified. |
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

1. **CPU and platform evidence:** CPUID instruction flags have been captured;
   establish clock evidence, a verified physical memory inventory,
   paging/interrupt state, device ownership, and a recoverable transition path.
   Do not confuse a user virtual address with a physical one.
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

## Hardware evidence: 28 September 2026

Two console logs from build `fb00bcd85b639d4574c95d71a57cfa30fc012a22`
contain identical CPUID records for leaves 0, 1, 7, `0x80000000`,
`0x80000001`, and `0x8000000A`. The standalone CPU report ends with
`CPU probe finished`; SHA-256 of the received `PS4WL_CPU.LOG` is
`65994895903c40247318d6b1e6e5feb22f00a64567c5aae1bd17b17457ddfc45`.
The preflight log's SHA-256 is
`0339f0053f674908936b444df42a14e8b7ed19e41ef0cf1955d9a9dbb4f25cc7`.
Both report NX/DEP, CMPXCHG16B, LAHF/SAHF, PREFETCHW, NPT, SSE4.2 and
POPCNT. These are **reported CPU capabilities**, not a passed Windows Server
compatibility or installation test. Frequency, firmware, physical RAM map,
device support and OS handoff are separate open items.

In the preflight, the EFI arena mapped at `0x40000000`, the image allocation
returned success for `0x33A` EFI pages and `bootmgfw.efi` was mapped with
`ImageSize=0x33A000`. The last log line is an image word from the expected
pre-entry inspection. This log contains no Microsoft entry call, processor
fault, or explicit cleanup/return marker; normal process return must be
observed separately. The earlier experimental instruction failures remain the
boundary for the PS4 user-process design.

Sources: [Microsoft Windows 11 requirements](https://learn.microsoft.com/en-us/windows/whats-new/windows-11-requirements),
[Microsoft Windows Server hardware requirements](https://learn.microsoft.com/en-us/windows-server/get-started/hardware-requirements),
[Sony PS4 technical specifications](https://www.playstation.com/en-us/ps4/tech-specs/),
[UEFI Boot Services specification](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html).
