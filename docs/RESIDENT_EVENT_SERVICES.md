# Resident events and firmware discovery

Ten new Boot Services now have dedicated Microsoft AMD64 entry points in the
copied resident image. They replace the generic EFI_UNSUPPORTED adapter in
their table slots; the adapter remains for other missing methods.

| Boot table slot | Method | Implemented behavior |
| --- | --- | --- |
| 7 | CreateEvent | Plain, notification-on-wait and notification-on-signal events |
| 9 | WaitForEvent | Blocking cooperative polling in array order at TPL_APPLICATION |
| 10 | SignalEvent | Signal state, queued callbacks and group signaling |
| 11 | CloseEvent | Remove pending callbacks, invalidate handle and free registry slot |
| 12 | CheckEvent | Consume signal state; invoke wait notifications; EFI_NOT_READY otherwise |
| 43 | CreateEventEx | Non-timer events with optional GUID group |
| 36 | LocateHandleBuffer | Pool-owned snapshots for AllHandles and ByProtocol searches |
| 35 | ProtocolsPerHandle | Pool-owned pointer array and independent GUID copies |
| 34 | OpenProtocolInformation | Pool-owned snapshots of the existing query-mode open records |
| 21 | InstallConfigurationTable | Add, replace and remove GUID/address pairs; update System Table CRC |

Events use an inline 32-entry registry and two bounded FIFO queues at
TPL_CALLBACK (8) and TPL_NOTIFY (16). Pending notifications run when the current
TPL is below their priority, including on RestoreTPL. Higher-priority callbacks
can nest inside lower-priority callbacks. Repeated signals coalesce while a
notification is pending; signal-type state resets before callback invocation.
Wait-type state resets when CheckEvent consumes it. Event handles are opaque,
never dereferenced and never reused after closing; generation exhaustion is
reported rather than wrapping. A callback can close its own event safely.

Every group member is signaled before the first group callback runs. Members
are delivered by priority, with FIFO ordering at equal priority. Closing an
event removes it from its group and any pending queue. Configuration-table
changes publish the new array pointer, count and CRC before signaling the
GUID-matched group. The array has 32 entries; removal compacts it. Installing
a pointer does not construct or validate the underlying ACPI/SMBIOS table.

Protocol query buffers come from the resident page-backed AllocatePool manager
as EfiBootServicesData and must be released with FreePool. The allocation's
entire physical extent must already have a writable identity mapping. GUID
copies remain valid even if the protocol registry later changes. Open records
still cover only the supported query modes; this adds no driver binding,
controller ownership or protocol-install notification registration. Searches
by notification registration key still return EFI_UNSUPPORTED.

WaitForEvent repeatedly polls wait-notify callbacks and conditionally calibrated
TSC timers, using PAUSE between scans; an unsignaled event without a producer can
wait indefinitely. It does not return a fabricated timeout. There is no hardware
idle or asynchronous interrupt-driven event producer. Timer arming requires a
trusted clock calibration; see [variables and timers](RESIDENT_VARIABLES_TIMERS.md).
Automatic ExitBootServices and virtual address change event types remain unsupported.
Explicit GUID groups do not provide automatic platform lifecycle signaling.
Complete Runtime Services, the persistent
monotonic-counter seed, console protocols, child image execution and platform
handoff remain incomplete; ExitBootServices still refuses execution.

All service calls must be serialized on the firmware execution CPU. TPL queues
are software scheduling, not hardware interrupt masking or AP synchronization.
Notification functions and their contexts are caller-provided pointers and must
stay mapped in the native address space for their complete lifetime. No pointer
to a host process service is introduced by this module.

The image exports 66 linked entries, retaining offsets 0..30, including the
error adapter at 30. New events are 31..36 and discovery/configuration methods
are 37..40. Variable methods are 41..44 and timer/delay/watchdog-disable are 45..47.
There are 36 dedicated Boot table destinations (including the
existing ExitBootServices refusal), 7 generic error destinations and one null
Reserved field. The previous All30 mask and hardware evidence remain unchanged;
they do not validate these new entries on PS4.

Validation calls the actual copied RX image above 4 GiB with Microsoft AMD64 ABI.
Tests cover consumed signals, deferred/coalesced notifications, FIFO/priority
ordering, group marking, nested callbacks, self-close, cancellation and slot
reuse, cooperative wait completion, invalid/closed handles, registry exhaustion,
post-retirement refusal, allocated query snapshots, memory exhaustion with
unchanged outputs, aliased GUID removal and configuration CRC publication.
The full ASan/UBSan suite and closed native development build pass locally.
No emulator or Microsoft application execution is used. No new console launch
or complete Boot Manager entry is claimed.

Behavior references (original implementation in this repository):

- [UEFI 2.10 Errata A, Boot Services](https://uefi.org/specs/UEFI/2.10_A/07_Services_Boot_Services.html)
- [TianoCore event scheduling reference](https://github.com/tianocore/edk2/blob/master/MdeModulePkg/Core/Dxe/Event/Event.c)
- [TianoCore AMD64-independent UEFI type definitions](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Uefi/UefiSpec.h)
