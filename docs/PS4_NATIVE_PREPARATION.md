# Checked native PS4 preparation entry

`PS4WindowsLoader-Native-Prepare` connects the real USB archive to the native
resident workspace using the checked installed-build 13.52 memory binding.
Its mode is PREPARATION_ONLY: it does not enter Microsoft code, change CR3,
stop CPUs or transfer device ownership. The older Stage 4.8 entry keeps its
existing behavior. This is a new console path, not a completed Windows loader.

Before the callback, the user entry reads a bounded PWL_BOOT.PAK from USB0 or
USB1, checks exact length/EOF/close and validates its hierarchy, then pins the
complete callback image including BSS, archive and protected-reader bounce
buffer. All ordinary filesystem operations occur outside the callback.
Volume label does not select the mount; no source files are written.

The installed-build reader uses the copyout ABI at live base+0x2BD6A0 already
exercised by the supplied kernel capture. This is its bootstrap assumption,
not a reader for arbitrary 13.52 builds. Before each read it checks CPL0,
IF/DF, live LSTAR/base, actual current thread/PCB and the observed thread
critical/lock fields. PCB_ONFAULT must be zero: nested recovery contexts are
refused. Copyout supplies recovery around the source/destination copy. Reads
are at most 256 bytes to pinned lower-canonical user bounce pages, copied to
caller output only on success. No new exception handler, kernel patch or
syscall is installed by this reader. Current GS thread/PCB metadata and the
onfault layout are trusted parts of this exact observed ABI; they are not
themselves read through an independent recovery mechanism.

`pwl_ps4_memory_bind_checked` captures live LSTAR/current thread/CR3, checks the
exact version, allocator/free/extractor/mutex/handler and copyout byte windows,
including the copy loop/fault return and original recovery target. It reads
map/root/counters twice and refuses changed inputs. Only then are memory
symbols converted to callable pointers. Before every arena allocation/release,
the production validator repeats these checks, verifies the same binding
thread/root and exact symbol/map relationships, and checks live context again.
No caller-provided ready bit or synthetic observation activates the binder.
The version-only API continues to refuse because it supplies no protected
reader/lifetime. Different kernel versions/builds must not use this profile.

The actual callback runs `pwl_native_boot_prepare` for
`\\EFI\\MICROSOFT\\BOOT\\BOOTMGFW.EFI`, copies/relocates the real application and
resident services into an owned contiguous arena, prepares inactive mappings
and audits the environment. It then frees using the original KVA and returns.
The request includes a 2 MiB pool, 64 KiB stack and 128 hardware table pages.
A 36 MiB archive requires roughly 42 MiB of contiguous owned RAM; allocation
may fail. The backend makes one allocation attempt and does not retry.

Notifications contain binding/preparation/audit/release status, physical arena
address, size and relocated entry. They also report CPU capture, clock status,
observed TSC frequency, APIC identity and (on capture success) PAT/MTRR/root.
The frontend observes console UTC/TSC before the callback; after checked binding
the callback captures its live CPU environment and installs a calibrated clock
only for matching CPU identity. See [native time](NATIVE_TIME.md). The prepared
Runtime Services table is still unpublished; no Microsoft call is enabled.
Stage 5 means successful preparation/audit
and returned cleanup, never a Boot Manager execution result. If ownership
cannot be released, the process keeps callback/data/bounce pages resident and
stops preparation without unpinning them. This avoids forgetting an owner or
freeing a guessed range. Kernel traps or a non-returning VM call are not made
recoverable by this returning preparation wrapper.

Host tests exercise the pure profile with valid fixtures, every immutable
window corrupted, protected-read errors, changing map/counter values and
actual production CPL3 refusal with unreadable arguments. They do not exercise
the live kernel reader, successful production binding or the new PS4 entry.
The previously supplied console allocation/workspace results justify the
same-build profile but are not hardware results for this new entry. The
freestanding core and console binary are linked with no unresolved imports;
CI rebuilds both and checks the explicit mode marker. No emulator is used.

No repeat of the completed probes is requested. The new entry still cannot
certify complete RAM/MMIO inventory, low reservations, exception/AP/DMA
ownership, actual video, runtime/device handoff or ExitBootServices. These
remain requirements before enabling the separate returning transition and
Microsoft entry.
