# Owned-memory backend and resident firmware substrate

Status: implementation with host integration tests, **not hardware validated**.
Production memory calls are now blocked by an executable binding gate; firmware
13.52 remains unverified. See the [binding audit](PS4_1352_BINDING.md) for exact
missing evidence and public-source provenance. There is no new console payload. Stage 4.8 remains preflight.
Do not send `ps4wl-native-core.o` to a console: it is a relocatable development
object, without an entry point, EFI System Table, relocation/activation layer
or CPU transition. No Windows boot or new hardware milestone is claimed.

## Source audit at baseline f9b0b25

| Link in the boot path | Actual code | Missing connection |
| --- | --- | --- |
| PS4 entry | Runtime `lib/crt.asm` jumps to the process `main`; `stage4_8.c` includes the older stages | No native preparation or CPU transition is called by this entry |
| Physical ownership | `stage4_8.c:ps4wl_mmap48`, Stage 4.5 allocation callbacks | These are virtual process mappings, not owned physical EFI pages |
| Independent page tables | `loader/src/paging.c` | The earlier builder maps only image/stack/table pages; it is not used by the payload |
| CPU switch | `context_capture.S` reads registers and returns | No new CR3, stack, GDT/IDT/TSS, exception recovery or CPU/device handoff |
| EFI System Table / services | `stage3_2.c`, `stage3_5.c`, `stage4_1.c` and later overrides | Tables, callback code and state are process globals; memory/file/trace callbacks invoke Orbis syscalls |
| Microsoft entry | `stage4_5.c:main` | `PS4WL_STAGE48_PREFLIGHT_ONLY` stops before the call; previous CPL3 calls faulted on privileged instructions |
| BCD and child image | File Protocol uses `open/read/close`; `load_image40` and `start_image40` return `EFI_UNSUPPORTED` | No resident filesystem, full image services, or demonstrated Boot Manager BCD processing / `winload.efi` execution |
| ExitBootServices | `stage3_5.c:exit_boot_services35` checks handle/key and sets a flag | No platform ownership transfer, event/timer shutdown, runtime map or complete System Table update |
| ACPI / devices | Stage 4.1 starts with `config_count40=0` | No validated target ACPI installation, Windows device drivers, or recoverable AP/IRQ/DMA transition |

Neither `pwl_boot_windows()` nor the new preparation API is called by Stage 4.8.
The former still returns `PWL_ERR_UNSUPPORTED`. Removing the preflight macro
would restore the incorrect process-context call; it does not activate this work.

## What is now implemented

`loader/src/ps4_memory.c` uses the actual kernel allocator ABI described by the
pinned runtime: `kmem_alloc_contig`, `pmap_extract` and `kmem_free`. Its API describes
resolved kernel symbols, the dereferenced `kernel_map` and `kernel_pmap_store`,
but non-null pointers no longer authorize calls. `ps4_binding.c` refuses every
production profile until target verification exists. Only a host-test build can
exercise the allocator with synthetic callbacks. The allocator makes one
`M_NOWAIT | M_ZERO` call with 16 KiB alignment, asks for WB memory between
1 MiB and the low canonical identity limit, and checks
both ends of every 4 KiB hardware page. It preserves the original allocation KVA
and its size for release. Physical addresses are never synthesized from a KVA.
Validation failure releases the original allocation; no backend retry loop or
kernel patch is used. M_NOWAIT does not eliminate internal VM locks/retries.
The allocation remains owned until an explicit preparation-side release.

`loader/src/native_workspace.c` connects that allocator to the rest of the
preparation flow in one transaction:

1. Allocate and physically verify a contiguous resident arena.
2. Partition it into firmware code, inline firmware data, table pages, a stack
   with two unmapped guard pages, a preloaded disk image and an allocation pool.
3. Copy firmware/media bytes while Orbis is still available. Copying is not
   executable relocation; the firmware blob must be relocated/bound separately
   before it could ever be called. All EFI pointers must use the target physical
   identity addresses, not `prepare_address` or the preparation-side owner.
4. Initialize resident memory and media state, containing no kernel function
   pointers or process handles. The memory map covers this owned arena only;
   it is **not** the PS4 platform memory map. Tail alignment padding stays owned
   and unadvertised. Unknown RAM/MMIO is not made available for allocations.
5. Construct and independently walk four-level tables for **all six** spans.
   Code is RX, media is read-only NX, data/pool/tables/stack are RW NX. Guard
   pages have no leaf mappings. Reject unexpected mappings or permissions.
   This is a pre-activation check; hardware-set accessed/dirty bits are not
   accepted as fresh builder output.
6. On preparation failure, release the original KVA and clear the owner only
   when release succeeds. If the binding/owner is invalid, return the cleanup
   error and retain ownership. `kmem_free` returns void; no kernel free errno
   or recovery from a non-returning kernel fault is claimed.

The heap currently remains NX. Memory-type changes in the resident manager
do not change PTE permissions. Before loading executable child images, the
future EFI image layer must set/verify executable mappings and invalidate TLBs
in the controlled context. This is another reason the substrate is not an
EFI launcher. PAT index zero and effective MTRR cacheability must be checked
against the WB allocation before these tables can be activated.

`firmware_memory.c` implements allocation from that supplied physical inventory,
including Any/Max/Address requests, splitting and coalescing, preservation of
reservations/unknown gaps, freeing only manager-owned allocations, exact map
size queries, changing map keys, and a memory-service retirement state. Map
size queries do not issue a usable exit key. A stale key or wrong image handle
is rejected; all memory calls are rejected after retirement. Exhaustion of
descriptor storage leaves the map/key unchanged. Runtime/ACPI allocation types
are explicitly unsupported until their lifetime and mapping rules exist.
The manager is single-threaded. The future EFI/TPL layer must serialize calls
and prevent event/interrupt reentrancy; AP quiescence is not supplied here.

`pwl_fw_memory_exit()` implements **only the memory portion** of a future
ExitBootServices wrapper. It does not claim to stop devices, signal EFI events,
update the System Table/CRC, install runtime services or transfer the platform.
It does not call Orbis to free backing memory. The arena must stay reserved
throughout the target lifetime; preparation cleanup is illegal after transition.

`firmware_media.c` reads a preloaded read-only block image with bounds, media-ID,
alignment and overflow checks, without filesystem or USB syscalls. It is not
yet an installed EFI BlockIo protocol, FAT filesystem, BCD implementation or
Windows post-ExitBootServices storage driver. A boot medium larger than the
reserved arena needs a native device path/driver or another justified design;
the process USB file descriptor cannot remain the backing store.

## Pinned runtime findings and unresolved target binding

Audited runtime commit:
[`f70f43a60a973b3662daeb29c1f115d95408db93`](https://github.com/ps4-linux/ps4-linux-loader/tree/f70f43a60a973b3662daeb29c1f115d95408db93).

- `linux/ps4-kexec-common/kernel.h` provides the allocator/extractor ABI and
  16 KiB VM page size; `kernel.c:kernel_alloc_contig` retries a waiting allocation
  and converts the KVA into a direct-map pointer. Its free helper accepts an
  address and forwards it to `kmem_free`. This implementation keeps the original
  allocator address explicitly instead of passing a converted alias to free.
- `linux/magic.h` has a `PS4_13_52` block, annotated **"from 12.02"**, including
  `kmem_alloc_contig=0x24D4F0`, `kmem_free=0x466460`,
  `pmap_extract=0x573D0`, `kernel_pmap_store=0x1B2C3A0`,
  `kernel_map=0x22D1D50`. These are upstream candidate offsets, **not verified
  symbols on this CUH-2208B**. The context probe did not call these functions or
  validate their addresses. No candidate offsets are silently executed here.
- The runtime build uses `NO_SYMTAB`, `KASLR`, and `DO_NOT_REMAP_RWX`; it does
  not dynamically prove those symbols. `main-aio.c:kernel_main` additionally
  patches allocation code and copies its kexec blob into kernel memory. Those
  writes are not part of this backend.
- `linux_boot.c:prepare_boot_params` labels its E820 entries as firmware 1.01
  values. `hook_icc_query_nowait` enters an SMP rendezvous that never returns.
  The boot path alters IOMMU/MSI/GPU/VRAM state; the thunk changes CR3/GDT and
  copies Linux into a fixed physical destination. It is not an EFI trampoline
  or a recoverable context-switch primitive.
- `acpi.c` rewrites tables for the Linux path at assumed physical addresses.
  Copying those assumptions would not establish valid Windows ACPI on Baikal.

The immediate blocker to running this backend on the PS4 is the absence of a
verified kernel-symbol binding and preparation-context contract on firmware
13.52. Beyond it, the independent blockers are a trustworthy RAM/MMIO inventory,
complete resident EFI tables/protocols and PE relocation, a recoverable native
CPU/exception context, AP/IRQ/device/DMA ownership, validated ACPI and Windows
storage/device support. The photographed CR3 cannot establish any of these.
One more short callback would not resolve them all.

## Verification and artifacts

```sh
sh tools/test_native_core.sh       # ASan + UBSan on an ordinary Linux host
sh tools/test_native_core.sh undefined  # UBSan if sandbox blocks LeakSanitizer
sh tools/build_native_core.sh
```

The integration test uses an explicit hosted-only fixture build. A separate
production-build test supplies fatal callbacks and proves that 13.52, older,
unknown and host IDs all refuse before calling them. CI rejects the fixture
macro in freestanding builds and checks exact C ABI compatibility with the
pinned runtime header. The integration test substitutes allocator/extractor callbacks.
It exercises the complete preparation pipeline with deliberately different KVA
and PA values and a span crossing a 2 MiB boundary. It checks an unmapped 4 KiB
subpage inside a 16 KiB allocation, original-KVA rollback, resident copies,
low/unaligned/out-of-range PA, incorrect last-byte translations, absent free
callbacks, refused release with retained ownership, idempotent cleanup,
stack guards, unexpected permissions/mappings, descriptor exhaustion, map-key
retry/retirement, media bounds and failure unwind. After preparation, memory
and media operations must leave all kernel callback counters unchanged.

CI also builds a closed freestanding relocatable core and checks for unresolved
symbols and process-entry instructions. `PS4-Windows-Loader-Native-Core` contains
that object, disassembly, checksums, commit and this document. A successful host
test or build does not test the kernel ABI, actual RAM ownership, active CR3,
cacheability, hardware recovery or Windows. No console test is requested.

Local baseline verification rebuilt the Stage 4.8 ELF/bin against the pinned
runtime and passed the existing host tests. The new memory/media and integrated
workspace tests passed with ASan+UBSan (LeakSanitizer disabled locally because
the execution sandbox restricts `/proc`; CI retains its default sanitizers).
The freestanding object built with GCC, with no undefined symbols. These local
results are separate from GitHub Actions and from hardware validation.

Relevant specifications:
[UEFI Boot Services](https://uefi.org/specs/UEFI/2.10_A/07_Services_Boot_Services.html),
[FreeBSD 9 contiguous allocation implementation](https://github.com/freebsd/freebsd-src/blob/stable/9/sys/vm/vm_contig.c).
