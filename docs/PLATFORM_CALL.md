# Returning CPU and device ownership transaction

The development core now provides `pwl_native_platform_call`. This is an actual
CPL0 wrapper around the existing returning CR3/stack/FP transition, with required
platform callbacks. It is not connected to the console entry: no complete PS4
callback set or authoritative device inventory has been supplied.

The sequence is claim exclusive platform ownership, capture all enumerated PCI
functions, stop new driver work, drain all outstanding DMA, disable INTx before
MSI/MSI-X, clear Bus Master, freeze the CPU environment, capture/check live CPU
state, then perform the prepared returning call. Every PCI config write is
16-bit and read back; Command+Status is never written as a combined word.
Disabling Bus Master does not establish that DMA was drained. Only actual
target driver/controller hooks can establish that condition.

On return, the wrapper compares CR0/CR3/CR4/EFER, FP layout, IF/DF, descriptor
tables/selectors, FS/GS bases, GS thread/PCB, APIC identity/base, PAT and all
supported MTRRs. Arithmetic flags are not expected to match. This capture is
an observation and a returning comparison, not ownership, exception recovery,
or code that restores every MSR. The established original GS ABI and readable
architectural registers are preconditions. The complete NMI/MCE/TSS/stack,
AP parking and FP ownership requirements of the prepared call remain.

Cleanup restores device configuration while platform interrupts remain masked,
restoring MSI/MSI-X before Command. Only after successful configuration rollback
does it thaw CPUs/controllers, resume stopped drivers and release the exclusive
claim. Failed stop/write/freeze/claim attempts may have partial effects and are
included in rollback. Hooks must support retry and undo partial attempts.
Failure retains the remaining lease. Context mismatch or memory-service
retirement forbids preparation-context cleanup and releasing pinned owners.

`pwl_pci_mmio_bind` supplies a native 16-bit MMIO config adapter for explicitly
provided, already mapped ECAM function pages. It checks actual CPL0, live CR3
and PAT, supervisor RW/NX translations to exact physical pages and PAT UC=0.
Access repeats the live root/PAT check. It does not enumerate devices, discover
ECAM, map guessed physical addresses, certify that a supplied PA is a PCI
function, or contain machine checks. Authoritative provenance, pinned stable
mappings, and exclusive config/driver ownership are mandatory. UC-minus and
ordinary WB mappings are refused. Only Command and MSI/MSI-X control writes
are accepted; Status is not written.

Tests exercise synthetic config/capability transactions, every forward write
failure including partial effects, malformed inventories, hot-device identity
change, retained restore/retry, and the config-only phase before driver resume.
Synthetic page-table tests cover exact PA/permissions/cache type. Real CPL3
guards refuse unreadable arguments before privileged reads. These tests do not
execute privileged capture, MMIO access, AP parking, DMA drain or a native PS4
transition. There is no emulator or successful default hardware hook.

Remaining integration needs the complete actual inventory, target driver drain
and stop/resume hooks, controller/AP/FP freeze and thaw, stable physical table
access and complete transition dependencies. Permanent handoff additionally
needs service/runtime lifetime, runtime address conversion and reset support.
`ExitBootServices` still returns EFI_UNSUPPORTED; this returning transaction
does not authorize retiring boot services or permanent device ownership.
