# Resident EFI application lifecycle

LoadImage, StartImage, Exit and UnloadImage are now linked into the closed
resident image at indices 48..51 and Boot slots 22..25. Indices 52/53 are private
page-permission and preloaded-entry adapters. The complete image has 66 linked
entries, including the fixed-framebuffer console, with no imports, ELF
relocations or writable globals. It remains a preparation image, not a
standalone PS4 boot binary.

## Loading and ownership

LoadImage accepts a memory buffer or a single bounded absolute FilePath node
over the validated resident archive. It copies the path, copies/relocates the
PE and installs LoadedImage and LoadedImageDevicePath on a fresh monotonic
handle. A null device path installs a null LoadedImageDevicePath interface.
The parent must have LoadedImage. Only AMD64 EFI applications are accepted;
drivers, dynamic imports, TLS, W+X sections and unsupported device-path forms
are refused. Buffer BootPolicy is ignored. File BootPolicy uses the supplied
explicit path on the sole archive volume; firmware device selection is absent.

There are eight simultaneously loaded children. Each owns an external-state
pool record and a page allocation from the already owned heap. The entire PE
allocation is EfiLoaderCode, reflected by both LoadedImage memory-type fields;
data sections still have RW NX permissions. This preserves a contiguous freeable
allocation rather than advertising fictitious separately typed data descriptors.
It is a restricted application loader, not image authentication or a driver
binding implementation. No Microsoft instructions are modified.

Invalid source/layout or a rejected transactional permission change publishes
no handle, frees the acquired pages and record, and preserves the caller's
output. Resource exhaustion and cleanup errors are returned. If safe release
fails after publication, the record is quarantined and pages stay owned; later
StartImage refuses it. No success is reported for an incomplete cleanup.

## Actual entry and Exit

StartImage calls the relocated entry with the Microsoft AMD64 ABI. Nested starts
track the current application; recursive start and unloading a running image
are refused. Normal return and Exit both restore the previous current image and
TPL_APPLICATION, close query-mode protocol opens, remove the application's
handle protocols, remove executable permission, clear/free PE pages and release
the record. The service reports the full 64-bit application status or a cleanup
error. Drivers are rejected, so no driver retention/unload callback is simulated.

Exit accepts only the currently running image, or discards a loaded image that
has not started. A successful running Exit never returns to its caller. The
private jump context preserves RBX/RBP/R12..R15, RSP/RIP, XMM6..15, MXCSR and x87
control state. Error exit data is copied to a separate pool allocation after a
bounded UTF-16 terminator check; optional binary bytes are retained. At most
64 KiB of exit data is accepted. If both StartImage output pointers are supplied,
the caller owns that buffer and must FreePool it. Otherwise it is freed.

The preloaded Boot Manager is registered as the initial application. Its entry
context now targets the same resident StartImage wrapper. Its PE pages belong
to the preparation arena and remain reserved when it returns; only the outer
owner can release them. This connects the lifecycle to the native preparation
pipeline. It does not connect that pipeline to the console's Stage 4.8 entry.

## Permission adapter and CPU contract

The default adapter requires CPL0, the exact bound private CR3, paging and WP,
NXE, CR0.EM/TS clear, CR4.OSFXSR set, PCID/PGE/LA57 clear and IF/DF clear. CPL3 is
refused before any privileged read. It does not clear TS, enable SSE, change
CR3, toggle PGE, mask interrupts, stop APs or claim ownership of the old OS's
floating-point state. The photographed PS4 values CR0=0x8005003B and
CR4=0x406F0 have TS and PGE set: those original settings are deliberately
insufficient for this new entry. Handling them belongs to a controlled platform
transition, including saving/restoring any borrowed CPU state.

The pure page helper checks the complete PE extent, owned heap/preloaded span,
contiguous table pool, all four-level paths, exact physical identity, writable
ancestors, NX inheritance, supervisor access, absence of global/large leaves,
and no W+X ranges before editing any page. It preserves cache/A/D bits. The
privileged adapter invalidates every edited local page with INVLPG. It never
writes control registers or MSRs. The build auditor allows these privileged
reads and INVLPG only in this isolated adapter, still forbidding process entry,
CLI/STI/HLT, WRMSR and control-register writes.

Tables and image spans must already be identity-accessible. The final graph
must be private, exclusive, stable and free of incompatible physical aliases;
other CPUs must be stopped and device/DMA ownership established by the platform.
An ordinary IRQ mask does not contain NMI/MCE or establish those conditions.
These calls do not recover from faults or enable an unverified production
memory profile. No new console execution is claimed.

## Verification

Host tests execute copied RX resident code and actual relocated PE fixtures
above 4 GiB, with native Microsoft ABI calls: normal error return, an Exit that
skips its following instructions, copied exit data, nonvolatile XMM restoration,
nested starts, current-image checks, preloaded-owner preservation, archive/path
copying, eight-image exhaustion, failed-load rollback and release quarantine.
The host test substitutes mprotect for the privileged permission backend; that
function is not linked into the distributed resident image. Separate pure table
tests check complete validation before mutation, NX ancestors, wrong PA,
global/user leaves, permission restoration and exact entry permissions.
The real adapter's CPL3 refusal is exercised. CPL0 edits, NMI/MCE, AP/DMA handoff
and Microsoft code are not hardware validated by these tests. No emulator runs.

Primary contracts: [UEFI image services](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html#image-services),
[LoadedImage](https://uefi.org/specs/UEFI/2.10/09_Protocols_EFI_Loaded_Image.html),
[TianoCore image lifecycle](https://github.com/tianocore/edk2/blob/master/MdeModulePkg/Core/Dxe/Image/Image.c).
