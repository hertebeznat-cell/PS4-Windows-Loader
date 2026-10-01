# What still prevents the complete PS4 launch

The native preparation path now routes the initial application through a real
resident image lifecycle, supplies child image services, updates section
permissions in a private root, and can publish a validated fixed framebuffer
console. Host tests run copied RX code and relocated PE fixtures directly on
the processor. This is substantial executable code, not a PS4 launch result.

| Requirement | Current evidence / missing work |
| --- | --- |
| Image copy/relocation, children, normal return and Exit | Implemented and native host-tested; actual PS4 execution of this block not performed |
| Private root and local permission changes | Constructed/audited in preparation; CPL3 refused; controlled CPL0 operation requires established CPU ownership |
| Main PS4 entry to native preparation/transition | Still absent; Stage 4.8 remains preflight and production memory profiles are refused |
| Actual RAM/MMIO map and low physical allocations | Owned arena only; no complete platform inventory or reservation of PA 0x00102000, requested in the earlier trace |
| CPU/exception/FP ownership | Context photographs are point-in-time observations; complete NMI/MCE/GDT/IDT/TSS/GS/stack dependencies and recoverable state transition absent; original TS/PGE must be handled correctly |
| Other CPUs, IRQ/DMA/PAT/MTRR and device handoff | No complete target-specific ownership implementation or validation |
| Screen output | GOP/text implemented for an explicit verified linear buffer; PS4 address, format, linearity, cacheability and mapping not supplied, so default output stays disabled |
| Input and timing | Keyboard absent; PS4 TSC calibration absent; cooperative timer backend stays disabled without validated clock input |
| Runtime Services | Wire table prepared but not published; complete time/reset/runtime lifetime and virtual-address services absent |
| ACPI/SMBIOS/platform configuration | Registry exists, but verified target tables and hardware inventory are not synthesized |
| ExitBootServices | Still explicitly unsupported; retiring an allocation manager is not hardware ownership transfer |
| Windows loader/kernel files | The supplied EFI-only archive does not include a complete Windows installation |

Do not remove the old process preflight guard, fabricate addresses/readiness,
enable a guessed clock, or label the development object a boot binary. The
existing successful console probes do not establish these missing hardware
facts. Repeating them will not supply a full inventory. No new console test is
requested by this implementation, and no claim of a final or complete Boot
Manager launch is made.

The next necessary work is the actual target platform backend and its evidence:
source of the complete memory/device inventory, owned low-memory reservations,
linear-video capture, clock/input, firmware platform tables, and a controlled
transition with exception/FP/AP/DMA handling. Additional generic EFI callbacks
alone cannot substitute for that backend.
