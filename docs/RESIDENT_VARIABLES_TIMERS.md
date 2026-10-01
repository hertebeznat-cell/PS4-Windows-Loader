# Resident variable storage and polled timers

This change adds seven linked resident entry points without process imports:
GetVariable, GetNextVariableName, SetVariable, QueryVariableInfo, SetTimer,
Stall and SetWatchdogTimer. Offsets 0..40 remain unchanged; new offsets are
41..47 in that order. The image has 66 linked entries. Boot Services has 32
dedicated destinations (including the existing ExitBootServices refusal),
7 generic EFI_UNSUPPORTED destinations and the null Reserved field.

## Variables

The inline variable store contains 16 fixed slots. Each variable is identified
by its exact UTF-16 name and GUID; same-name variables with different GUIDs
are separate. Names include a terminator within 128 UTF-16 units, and name
bytes plus data cannot exceed 1024 bytes. Empty names cannot be written.
The supported stored attribute is EFI_VARIABLE_BOOTSERVICE_ACCESS. Non-volatile
storage, runtime access, authentication and hardware error records are unsupported.
No fixed BootOrder or other platform variable value is fabricated.

SetVariable creates/replaces data, deletes with zero attributes or zero data
without APPEND_WRITE, and appends with APPEND_WRITE. The append flag is not
stored or returned as a variable attribute. An empty append is a successful
no-op. Invalid requests and exhausted storage leave an existing value intact.
Inputs are snapshotted before clearing a slot, including aliased name/GUID/data.
Freed slots can be reused; replacement does not need a free slot.

GetVariable implements buffer-size negotiation, optional attribute output and
NOT_FOUND for absent variables. GetNextVariableName checks that its input cursor
terminator fits inside the supplied byte count; an unknown nonempty cursor is
invalid. Enumeration is deterministic by UTF-16 name and GUID bytes. A too-small
output updates only the required size. QueryVariableInfo reports the actual
fixed-slot storage quota: each live variable consumes one entire slot, including
its reserved name/data capacity and metadata. Deletion refunds that slot.

These are boot-only volatile variables. The four methods return EFI_UNSUPPORTED
after memory-manager retirement. The Runtime Services wire table is prepared
with correct AMD64 layout, linked variable addresses and CRC, but
SystemTable.RuntimeServices remains zero. Other status-returning Runtime methods
use the error adapter in this unpublished table. ResetSystem remains absent
because its non-returning VOID contract cannot use that adapter. Full publication
requires native reset, clock/runtime lifetime and virtual-address contracts.
This store is not persistent NVRAM and does not add OS-runtime support.

## Timers and delays

SetTimer and Stall are connected to their Boot Services slots. Creation accepts
plain, wait-notify and signal-notify timer events. Arming and nonzero Stall calls
require explicitly prepared clock.ready, frequency_hz and last_tsc state.
Default workspace preparation leaves the clock disabled. This change does not
capture or validate a PS4 clock calibration automatically. A trusted platform
preparer must establish invariant/stable TSC behavior, its frequency against
a monotonic reference, counter accessibility and CPU continuity before enabling
it. CPU core MHz must not be substituted for TSC frequency.

Resident code samples real RDTSC with CPUID serialization, avoiding assumptions
about LFENCE dispatch behavior on older AMD64 processors. Frequency must be
positive and at most 10 GHz. Time conversion rounds up to cycles and checks
arithmetic overflow and unrepresentable intervals. A regressing counter clears
clock.ready and returns an error instead of accepting an early expiry.

Relative timers expire once; periodic timers advance their existing deadline,
coalescing missed periods into one notification. Cancel disables future expiry
without discarding a signal or callback already queued. Zero relative delay
means the next 1 ms polling-grid tick; a zero periodic interval means every tick
on that grid. Group marking and TPL notification rules are preserved. Closing
an event removes both its timer and queued notification. Invalid reprogramming
preserves the existing timer settings.

Polling occurs in CheckEvent, WaitForEvent and event dispatch, including
RestoreTPL. No interrupt-driven timer source or hardware idle is installed.
Consequently callbacks do not preempt an application which makes no firmware
calls. Delivery can be late; this is a cooperative subset, not a complete
asynchronous UEFI timer implementation. Stall busy-waits on TSC without yielding
or calling a host sleep function. An unavailable clock returns EFI_UNSUPPORTED.
SetWatchdogTimer accepts disabling the absent watchdog with timeout zero;
positive timeouts remain unsupported until a native reset backend exists.

## Verification and remaining entry work

The actual copied RX image is invoked with Microsoft AMD64 ABI. Variable tests
cover sizes, GUID identity, append/delete, enumeration cursors, quotas, failed
replacements, aliased inputs and unsupported post-exit lifetime. Timer tests
calibrate the host CPU against CLOCK_MONOTONIC only in the harness, then call
real RDTSC in the copied image. They cover minimum delay, relative/periodic
expiry, cancellation, overflow, queued notifications at raised TPL, self-close,
group signaling, invalid/regressing clock refusal and retirement.

The host clock function is not part of the resident image. Tests use the CPU
directly, without emulators. The full ASan/UBSan suite and native development
build pass locally. No new PS4 result, complete Runtime Services publication,
hardware timer interrupt or PS4 Boot Manager entry is claimed. Separate
[resident application tests](RESIDENT_IMAGES.md) now execute native PE fixtures.

Clang lowers large variable-slot clearing to a memset call even in freestanding
mode. The resident link includes the repository's own freestanding byte-operation
implementation, so compiler-generated helpers resolve inside the copied image.
The no-import/no-relocation audit remains mandatory; no host C library is linked.

References:

- [UEFI 2.10 Errata A Boot Services](https://uefi.org/specs/UEFI/2.10_A/07_Services_Boot_Services.html)
- [UEFI 2.10 Runtime variable services](https://uefi.org/specs/UEFI/2.10/08_Services_Runtime_Services.html)
- [TianoCore UEFI method definitions](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Uefi/UefiSpec.h)
