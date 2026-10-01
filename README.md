<div align="center">

# PS4 Windows Loader

**A native EFI boot path for Windows on PlayStation 4**

PS4 Slim CUH-2208B · Baikal · AMD Jaguar x86-64

![Build](https://github.com/hertebeznat-cell/ps4-windows-loader/actions/workflows/ci.yml/badge.svg)
![Latest source](https://img.shields.io/badge/latest_source-Stage_4.8-6e56cf)
![Hardware result](https://img.shields.io/badge/latest_hardware-one_page_memory_passed-287d55)
![License](https://img.shields.io/badge/license-MIT-blue)

</div>

> [!IMPORTANT]
> This is boot research, not a working Windows installation. `bootmgfw.efi` has
> executed on a PS4, but `winload.efi`, the Windows kernel, setup, graphics, and
> device drivers have **not** been reached. Earlier Stage 4.8 entry runs
> recorded `SIGBUS` after a low-address allocation and after an experimental
> instruction substitution; a console shutdown was reported after the latter.
> The current build is a limited preflight: it maps `bootmgfw.efi` and returns
> without calling Microsoft's entry point. The second fault's bytes are already
> present in the collected trace; no additional console run is needed.

## Latest hardware progress · 30 September 2026

| Check | Result |
| :-- | :-- |
| Installed 13.52 kernel capture | ELF/base/version checked; kept private |
| One 16 KiB allocation | Passed on PS4: physical continuity, zeroing and readback |
| Original KVA release | `kmem_free` returned; independent reclamation not proven |
| Resident workspace and EFI tables | Passed on PS4: 224 KiB, copies, tables and release |
| Resident callback execution | Nine checks passed on PS4 in process context; separate stack pending |
| CPU handoff and Windows kernel | Not reached |

The completed [Memory-Probe](docs/MEMORY_PROBE.md) returned
`rc=0 stage=5 error=0`. Its internal stage 5 is not Windows boot Stage 5.
The completed [resident preparation](docs/RESIDENT_PROBE.md) additionally checked
a larger owned arena, resident code/data copies, EFI tables and independent
page tables, then freed it and returned. The next
[resident process call test](docs/RESIDENT_CALLS.md) checks nine actual callback
entries from copied RX code with synthetic descriptors. Neither activates CR3
or enters Microsoft code. Production memory binding still refuses calls.
Do not repeat the completed preparation tests.

## Where it stands

The loader runs as a PS4 payload and creates the EFI interfaces needed to call
Microsoft Boot Manager. Earlier hardware traces entered its real EFI entry
point. The current build reads `bootmgfw.efi` from USB, maps and relocates the
PE32+ image, logs the bytes at the second fault, and exits before entry.
It does not use Linux, QEMU, or a virtual machine for the Windows boot path.

| Milestone | Result |
| :-- | :-- |
| PS4 payload, USB file access, PE32+ mapping and relocations | Verified on hardware |
| EFI tables, Boot/Runtime Services, filesystem and BCD bridge | Self-tests passed on hardware |
| Enter Microsoft's `bootmgfw.efi` | Verified on hardware |
| Return LoadedImage and DevicePath to Boot Manager | Verified in hardware traces |
| Allocate a page at the requested `0x00102000` | Succeeded in an earlier Stage 4.8 entry trace |
| Stage 4.8 aligned 16 KiB backing window | Earlier entry trace mapped it at `0x00100000`; native page size reported as 16 KiB |
| Current preflight and CPU probe | Both logged identical CPU flags; preflight mapped `bootmgfw.efi` and stopped before entry |
| Returning CPU context probe | CPL0 callback returned to CPL3; CR0/CR3/CR4/EFER were captured from on-screen notifications and are recorded in `docs/CONTEXT_PROBE.md` |
| Returning syscall anchor probe | Build `2271636` returned to CPL3; LSTAR `0xFFFFFFFF8433C1C0` and reported kernel build `r228995/release_13.520` were photographed; memory binding remains unverified |
| Execution after the earlier successful allocation | `SIGBUS` (`si_code=3`) delivered; no subsequent EFI callback or return recorded |
| Boot Manager loads `winload.efi` / Windows kernel | Not reached |

Earlier entry runs varied: one Stage 4.8 run found `0x00100000` occupied and returned
`EFI_NOT_FOUND`, while another found the window free and mapped the full
16 KiB at `0x00100000`. Its `hw.pagesize` query returned `0x4000` (16 KiB).
`AllocatePages(AllocateAddress, EfiLoaderData, 1, 0x00102000)` then returned
`EFI_SUCCESS` and the requested address. The following fault handler recorded
`SIGBUS`, code `3`, and `siginfo.si_addr=0x40058666`, within the mapped
`bootmgfw.efi` image at offset `0x58666`. The next trace confirmed the bytes
`0F 20 D8` there, an x86-64 instruction that reads `CR3` and cannot run in
the PS4 payload's user process. An experimental build replaced the instruction
with a zero result and reached a second `SIGBUS` at offset `0x28544A`, where
the bytes `0F 32` encode `RDMSR`. A console
shutdown and system storage check were then reported; their cause has not been
established. The instruction replacement has been removed from `main`.
The previous handler's reported
instruction and stack pointers are unreliable because its context layout does
not match the signal frame seen on this PS4. The diagnostic logs raw context
words and image bytes near a fault address. The current preflight does not
execute Boot Manager.

## Boot path

```text
PS4 payload → EFI compatibility layer → bootmgfw.efi
                                      → BCD → winload.efi → ntoskrnl.exe
```

The initial test machine is a **PS4 Slim CUH-2208B**, Baikal southbridge,
firmware **13.52**. The project targets Windows 11 and Windows Server 2025,
but neither is currently bootable here. The USB path on that machine is
`/mnt/usb0/EFI/Microsoft/Boot/`.

### Implemented so far

- PS4 payload execution, USB reading, PE32+ validation, section mapping and
  `IMAGE_REL_BASED_DIR64` relocation.
- Microsoft x64 EFI calling convention, EFI System Table, memory allocation
  callbacks, memory descriptors and MapKey handling.
- Loaded Image, Device Path, Simple File System, File Protocol and BCD file
  access; baseline events and Runtime Services.
- Real entry into `bootmgfw.efi` with callback traces to a USB log.
- Portable physical-memory layout checks with automated tests for overlap,
  alignment, address overflow and image/stack/page-table placement. This module
  is not connected to the PS4 payload until verified physical data is available.
- Portable x86-64 page-table snapshot checks for supervisor identity mappings,
  executable image pages and writable NX stack/table pages; CI tests invalid
  mappings. This checks supplied snapshots, not the console's active CR3.
- Conversion of verified physical regions into UEFI memory descriptors using
  cacheability supplied by the future platform backend.
- Portable and active Stage 4.8 PE32+ parsers check section-table and raw
  section bounds, executable entry placement, and image extents. CI exercises
  the active parser against malformed synthetic images before building the
  latest payload.
- The active relocation mapper validates every relocation block before changing
  image bytes and rejects malformed block sizes and self-modifying directories.

These interfaces are partial implementations for bring-up. A successful
callback or pre-entry BCD self-test does not establish that Boot Manager can
complete its own BCD processing or start Windows.

## Build status

The [CI workflow](https://github.com/hertebeznat-cell/ps4-windows-loader/actions/workflows/ci.yml)
builds Stage 4.8 as **PS4-Windows-Loader-Latest**. Its current binary is a
preflight and does not run Boot Manager. Existing traces already contain both
faulting instructions; no further hardware run is needed for them. Do not reuse
earlier artifacts containing the experimental `CR3` substitution. Microsoft
boot files are not included in this repository.

New CI artifacts include `COMMIT.txt`; traces include `BUILD:` and an explicit
`MODE: PREFLIGHT_ONLY` marker. The preflight also records raw CPUID registers
for checking processor requirements against evidence from the console. Use the
build markers to identify the exact artifact.
Follow the [Stage 4.8 preflight procedure](docs/PREFLIGHT_TEST.md) when checking
the latest artifact on the console.
The separate [CPU-only probe](docs/CPU_PROBE.md) records processor flags with
far less setup and does not read or map a Windows image.

A separate [returning CPU context probe](docs/CONTEXT_PROBE.md) is now built as
**PS4-Windows-Loader-Context-Probe**. It tests the existing runtime callback,
records real control registers at CPL0 and checks return to CPL3. Its success
notification was observed on the PS4 with build `dedeffd`; build `abe468c`
subsequently displayed CR0, CR3, CR4 and EFER on-screen. The measured values
are in the probe documentation. It is not a Windows launcher.

The [native EFI handoff contract](docs/EFI_HANDOFF_CONTRACT.md) documents what
a privileged backend must establish before any further Boot Manager entry.

The [native EFI PE mapper](docs/NATIVE_PE_LOADER.md) now copies and relocates
an optional EFI application into the native workspace and builds per-section
RX/RW-NX mappings. Synthetic integration tests verify physical relocations and
rollback. No Microsoft image execution or new console payload is claimed.

The [owned-memory backend and resident firmware substrate](docs/NATIVE_BACKEND.md)
now connects a kernel allocation adapter, per-page physical verification,
resident firmware/media copies, an autonomous memory manager, and independent
page tables in one preparation transaction. Host integration tests cover the
pipeline and rollback. An [enforced binding gate and source audit](docs/PS4_1352_BINDING.md) now reject
unverified kernel calls, including the upstream 13.52 candidates. Exact ABI
types and release ownership are checked in CI. Verified firmware 13.52 binding,
a complete platform map,
EFI protocol installation and CPU/device handoff remain unresolved. The new
`PS4-Windows-Loader-Native-Core` artifact is a relocatable development object,
**not a console payload**. No new console test is requested.

Use dedicated external test media. This experimental payload may hang or crash
the console. The internal system drive is outside the test plan.

### Local build

The CI workflow in [`.github/workflows/ci.yml`](.github/workflows/ci.yml) is
the reproducible build recipe. It pins the external
[`ps4-linux-loader`](https://github.com/ps4-linux/ps4-linux-loader) runtime
commit, builds its freestanding library, compiles the latest payload, and
publishes `.elf` and `.bin` artifacts. It also compiles the portable core in
`loader/src/main.c`. Building is a compile check; a PS4 run is required to
verify firmware behavior.

The portable placement tests run on a desktop without invoking PS4 code:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iloader/include \
  loader/src/handoff.c tests/test_handoff.c -o /tmp/test-handoff
/tmp/test-handoff
```

## Next milestones

The [resident EFI preparation experiment](docs/RESIDENT_PROBE.md) now includes
a linked position-independent code image, nine Microsoft x64 ABI callback
entries and physical workspace binding. Host tests execute copied RX code above
4 GiB and check tables, mappings and cleanup. ExitBootServices returns
EFI_UNSUPPORTED and TPL is only serialized state tracking. The new returning
console experiment prepares and checks the image but never calls it; hardware
preparation passed on the PS4 in build `64a2f64`. The separate
[returning process callback test](docs/RESIDENT_CALLS.md) now checks all nine
entries from a copied RX image using synthetic memory descriptors. It does not
execute in the kernel workspace or switch CPU context. Windows is not launched.
That process test passed on the PS4 in build `3f52f8c`; the
[separate-stack test](docs/RESIDENT_STACK.md) adds guard pages and checks return
to the original stack while keeping the same process address context. It passed
on the PS4 in build `91ce9bc`: `restored=1 result=0 passed_mask=1ff`.

Resident preparation now audits the owned spans, exact code/data/table mappings,
both unmapped stack guards, code bytes and state binding, and EFI table pointers
as one environment check. A failed audit unwinds preparation. This is preparation
only; a recoverable CPU transition and complete Boot Manager services remain
necessary before entering `bootmgfw.efi`. The environment audit passed on PS4
in build `0a5199ac`; it does not need repeating. The
[returning address-context entry](docs/ADDRESS_TRANSITION.md) is now compiled
into the development object, with platform mapping and exception dependencies
still required before it can be included in a console test. It has not executed
on hardware. No emulator is used.

1. Replace the user-process execution path with a platform-owned boot context
   capable of executing privileged CPU instructions; review the shutdown before
   resuming entry tests. See [Execution boundary](docs/EXECUTION_BOUNDARY.md).
2. Model a truthful physical memory map and required firmware tables. The
   current process mappings and synthetic EFI descriptors are insufficient for
   a Windows kernel handoff. Never force-map over an occupied page.
3. Implement the remaining Boot Manager services, then reach
   `winload.efi`, then kernel initialization.
4. Bring up PS4 platform description, storage, USB input, framebuffer and
   other device support before claiming a usable Windows installation.

For the longer architecture plan, see [Boot Flow](docs/BOOT_FLOW.md). Historical
payloads live in [`payload/`](payload/); the CI stage selector determines the
active source rather than the highest stage filename.

The separate [Windows 11 and Server 2025 readiness matrix](docs/WINDOWS_TARGETS.md)
tracks Microsoft requirements against evidence available for the target PS4.

## Licensing and files

MIT license; see [LICENSE](LICENSE). The repository does not distribute
Microsoft Windows files, Sony firmware, product keys or proprietary drivers.


The separate [Anchor-Probe](docs/ANCHOR_PROBE.md) diagnostic collects the live
syscall entry address, callback flags/alignment and available kernel version
strings through screen notifications, with optional USB logging. It provides
photographed observations for the 13.52 binding investigation. The test completed
and does not need repeating. Allocator confirmation and Windows boot remain
unresolved; see its hardware-results section for the exact evidence still needed.


The [returning root-clone diagnostic](docs/ROOT_CLONE.md#successful-photographed-console-result--2026-09-30)
passed on PS4: `rc=0 stage=5 error=0`, `status=0 switched=1 restored=1 released=1`.
Photographs confirm active root `0x0CF0A000`, clone root `0x558D8000`, restoration
of the original root and stack, and return from release. It uses the live direct
map (indices 436/348), so the kernel root may differ from the active process
root. No emulator was used; USB logging is optional. Do not repeat this check.
The resident EFI callback execution under identical mappings also passed;
its complete report was retrieved in Windows. Independent EFI mappings and
Windows Boot Manager entry in this native context remain untested.


The [USB-only readback check](docs/USB_LOG_CHECK.md) investigates the reported
absence of `PS4WL_TRANSITION.LOG` despite successful write notifications. It
checks filesystem device boundaries and reopens/compares a short `PWL.LOG`
record. It does not execute a CPU transition or allocate kernel memory.
The physical USB persistence issue remains unresolved until the file is retrieved.


The [returning root EFI integration](docs/ROOT_EFI.md) passed on PS4 in build
`31140d3`: all nine callbacks, root/stack restoration and cleanup succeeded. It executes all nine copied resident EFI callbacks under an
identical copy of the active root with the temporary stack and restores the
original context. Its memory descriptors remain synthetic; Microsoft code is
not entered. FAT32 reporting uses `PWL_EFI.TXT`, selected by exact USB marker
identity, with file/directory synchronization and readback. Windows label
`WINDOWS` is recorded as the user-supplied label, not independently verified.
Internal file checks do not establish post-removal persistence.


Native preparation now supports the resident services and a relocated EFI
application together in one arena. The combined audit checks the application
entry and all section mappings, including RX code and RW/NX data. This is a
preparation implementation, not a new console launcher. No repetition of the
completed returning checks is requested.


The [resident read-only file volume](docs/RESIDENT_FILES.md) now provides EFI
OpenVolume and revision-1 file methods from an owned preloaded archive. Native
preparation publishes the filesystem protocol with physical addresses; copied
code checks cover opening, reading, directory enumeration and file information.
This adds 11 filesystem callbacks (30 total resident entries including the
new OpenProtocol/CloseProtocol query services), not live USB
access or a Boot Manager launch. The unified archive-to-image preparation now binds DeviceHandle/FilePath to
the chosen resident file. Console preload and platform handoff remain incomplete.

USB archive preloading is connected to owned boot-image preparation through `pwl_boot_source_prepare()`; see [boot source transaction](docs/BOOT_SOURCE.md). This remains preparation code, without a new console entry or Boot Manager execution.

The 2026-10-01 PS4 All30 photographs confirm all thirty returning callback checks; see [hardware result](docs/EFI_ALL30.md). Independent-root mapping continuity checks are implemented separately in [transition mappings](docs/TRANSITION_MAP.md); no independent-root activation is claimed.

AMD64 exception-context decoding and a returning GDT/IDT/TSS/PAT observer are available in [exception context](docs/EXCEPTION_CONTEXT.md). They collect missing descriptor and stack facts without changing CPU tables or calling Windows.
