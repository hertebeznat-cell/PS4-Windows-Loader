# Experimental one-page memory diagnostic — 13.52

This is a new hardware experiment, not a Windows launcher or approval of the
production memory backend. It can still panic, deadlock or restart the console.
Return checks and byte guards do not provide kernel exception containment.
No successful hardware allocation/free trace is available before this test.

The exact installed dump version and short byte signatures of allocator, free,
extractor, mutex entry and GoldHEN dispatch must match before any VM call. Those
guards identify observed code, not every part of its transitive dependencies.
The callback requires CPL0, IF set, DF clear and its thread argument equal to
GS:0. It requires zero at thread+0x128 (checked by the observed mutex routine)
and zero in the 16-bit counter at thread+0xFC (incremented by that routine).
These are conservative observed guards; they do not establish every thread,
scheduler or lock invariant. No struct layout is declared universally correct.

The installed entry bytes FF 26 decode as `jmp qword ptr [rsi]`: the first
syscall argument supplies the callback address. It is a direct tail dispatch,
not an unknown extra GoldHEN trampoline. The remaining original-function bytes
after this patch are not executed by this dispatch. Successful dump callbacks
corroborate argument transport in this installed environment.

Before entering the callback the user payload must successfully mlock its
whole code/data/BSS image. It unlocks that range after return. Kernel-stack
residency and all scheduler constraints remain platform assumptions.

Exactly one 16 KiB contiguous allocation is requested, flags 0x101 (NOWAIT|ZERO),
physical lower bound 1 MiB, upper bound 2^47, 16 KiB alignment, no boundary,
write-back memattr 6. NOWAIT does not make VM locks nonblocking. The original
KVA is retained. All four 4 KiB subpage starts/ends must translate consecutively.
If translation passes, zeroing and first/last-byte writes are checked. A valid
owner is passed to void kmem_free even when a subsequent check fails. An
unexpected noncanonical, unaligned or overflowing owner is retained rather
than freeing a guessed range. This case would require a reboot, not a retry.

No credentials/control registers, IRQ/AP/DMA state or Microsoft code are changed.
The production `pwl_ps4_memory_bind()` remains unsupported.

## One test

Use the same firmware 13.52 / GoldHEN v2.4b18.12 environment as the successful
dump. Send only `PS4WindowsLoader-Memory-Probe.bin` from this archive, once.
With USB0 connected, a returning test appends its result to `PS4WL_MEMORY.LOG`;
otherwise photograph every PS4WL Memory notification. This return-only log
cannot record progress after a kernel panic or hang. If the system
stops responding, do not send the payload again; report the last notification.

Expected successful sequence includes `returned rc=0 stage=5 error=0`, then
critical/lock/flags and KVA/PA values. Stage 5 means void kmem_free returned;
it is not independent proof that the physical page was reclaimed.

| Error | Meaning |
| --- | --- |
| 1 | CPU privilege/IF/DF rejected |
| 2 | Current-thread identity rejected |
| 3 | LSTAR-derived base rejected |
| 4 | Same-build version/code guard mismatch |
| 5 | Thread critical/lock counters nonzero; no allocation |
| 6 | Map/root pointer shape rejected |
| 7 | Allocation returned zero |
| 8 | Unexpected KVA retained; no guessed free |
| 9 | Physical base/range rejected |
| 10 | Physical continuity failed |
| 11 | Allocated bytes were not zero |
| 12 | First/last-byte readback failed |

Stages: 1 entered/guards; 2 allocator call; 3 translation; 4 free call;
5 free returned. Early refusal is a useful result, not a reason to bypass guards.

## Build

SDK pinned to `Scene-Collective/ps4-payload-sdk@b7326416c23ce14639e9180a24093bdc5eedb579`.
Run `sh tools/build_memory_probe.sh SDK_DIRECTORY PROJECT_COMMIT`.
Compilation uses no red zone or generated SIMD in the callback. Raw packaging
must cover every allocated section with zero-filled BSS and no relocations.
An RWX linker segment is expected for the sender's raw payload format; this
is not a Windows executable or a hardened process image.
