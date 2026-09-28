<div align="center">

# PS4 Windows Loader

**A native EFI boot path for Windows on PlayStation 4**

PS4 Slim CUH-2208B · Baikal · AMD Jaguar x86-64

![Build](https://github.com/hertebeznat-cell/ps4-windows-loader/actions/workflows/ci.yml/badge.svg)
![Latest source](https://img.shields.io/badge/latest_source-Stage_4.8-6e56cf)
![Hardware result](https://img.shields.io/badge/hardware_result-bootmgfw.efi_entered-287d55)
![License](https://img.shields.io/badge/license-MIT-blue)

</div>

> [!IMPORTANT]
> This is boot research, not a working Windows installation. `bootmgfw.efi` has
> executed on a PS4, but `winload.efi`, the Windows kernel, setup, graphics, and
> device drivers have **not** been reached. The latest Stage 4.8 trace records
> `SIGBUS` immediately after the low-address allocation succeeds. A later
> experimental build reached another fault, and a console shutdown was reported.
> The latest build is a limited preflight: it maps `bootmgfw.efi` and returns
> without calling Microsoft's entry point. The second fault's bytes are already
> present in the collected trace; no additional console run is needed.

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
| Allocate a page at the requested `0x00102000` | **Succeeded in the latest Stage 4.8 hardware trace** |
| Stage 4.8 aligned 16 KiB backing window | Mapped at `0x00100000`; native page size confirmed as 16 KiB |
| Execution after the successful allocation | `SIGBUS` (`si_code=3`) delivered; no subsequent EFI callback or return recorded |
| Boot Manager loads `winload.efi` / Windows kernel | Not reached |

Hardware runs vary: one Stage 4.8 run found `0x00100000` occupied and returned
`EFI_NOT_FOUND`, while the latest found the window free and mapped the full
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
- Conversion of verified physical regions into UEFI memory descriptors using
  cacheability supplied by the future platform backend.
- Portable and active Stage 4.8 PE32+ parsers check section-table and raw
  section bounds, executable entry placement, and image extents. CI exercises
  the active parser against malformed synthetic images before building the
  latest payload.

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
`MODE: PREFLIGHT_ONLY` marker. Use these to identify the exact build.
Follow the [Stage 4.8 preflight procedure](docs/PREFLIGHT_TEST.md) when checking
the latest artifact on the console.

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
