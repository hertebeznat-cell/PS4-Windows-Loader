# Native EFI entry adapter

`pwl_native_efi_entry_prepare` connects an audited resident workspace and its
independent transition plan to an EFI entry context. It retains all resident
mappings with exact physical addresses, supervisor permissions and PAT index
zero. The entry is now the resident `pwl_resident_boot_entry` wrapper, which calls
StartImage for the audited preloaded application. ImageHandle and SystemTable
come from the resident firmware metadata. The shared lifecycle handles both
normal return and the application's Exit call; the preloaded PE span stays
reserved for the outer arena owner. Output is unchanged when
validation fails. An absent application or a mismatched plan is refused.

`pwl_native_image_mapping_bind` validates identity RW/NX mappings for the
contiguous final table pool and records that inactive root in resident state.
The entry capture pipeline performs this binding after preparing the context.
The permission adapter still refuses actual execution unless the live CPU
matches that root, including CPL0, NXE, usable SSE state, no PCID/PGE/LA57 and
clear IF/DF. The preparation result is not an authorization or a hardware
readiness certificate. See [resident applications](RESIDENT_IMAGES.md).

An optional [fixed framebuffer console](RESIDENT_GRAPHICS.md) is published only
after all framebuffer pages are checked in the final root. Subsequent entry
preparation rechecks those identity addresses, RW/NX permissions and PAT index.

`pwl_x64_efi_entry_call` is a position-independent SysV callback suitable for the
existing controlled address-call primitive. It passes ImageHandle in RCX and
SystemTable in RDX, reserves 32 bytes of shadow space, aligns the call stack,
preserves RBX and stores all 64 bits of EFI_STATUS plus a returned marker in its
resident context. Text up to its end symbol has no external references and can
be copied to owned executable RAM. It does not change CPU state itself.

Host tests copy the adapter into RX memory and call an actual Microsoft AMD64
ABI fixture, verifying high address arguments and an EFI error with bit 63 set.
Workspace integration tests also verify generated arguments for a relocated EFI
application and reject mismatched roots without modifying the output context.
No emulator is used. This does not invoke Microsoft code on the console.

The context must be copied into exclusively owned resident memory before using
the callback. The callback adapter and address-call thunk must retain executable
mappings under both roots. The report, context and BOTH stack extents must retain
writable mappings under both roots: interrupts/exceptions can occur between CR3
and RSP updates. Stack pointers alone do not certify their extents. NMI/MCE
handler, descriptor, TSS, GS and other exception dependencies must be complete
and stable. Ordinary IRQ masking does not provide that guarantee.

This adapter is included in the native development core, but the console entry
remains preparation-only. Live dependency capture, a verified production memory
backend, CPU/device ownership, firmware memory-map publication and recovery are
not provided by this adapter. No new console binary or Boot Manager launch is
claimed by this change.
