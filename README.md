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
> device drivers have **not** been reached. The Stage 4.8 hardware trace stops
> at the same low-address allocation request.

## Where it stands

The loader runs as a PS4 payload and creates the EFI interfaces needed to call
Microsoft Boot Manager. It reads `bootmgfw.efi` from USB, maps and relocates the
PE32+ image, then enters its real EFI entry point with a synthetic system table.
It does not use Linux, QEMU, or a virtual machine for the Windows boot path.

| Milestone | Result |
| :-- | :-- |
| PS4 payload, USB file access, PE32+ mapping and relocations | Verified on hardware |
| EFI tables, Boot/Runtime Services, filesystem and BCD bridge | Self-tests passed on hardware |
| Enter Microsoft's `bootmgfw.efi` | Verified on hardware |
| Return LoadedImage and DevicePath to Boot Manager | Verified in hardware traces |
| Allocate a page at the requested `0x00102000` | **Blocked in Stage 4.7 and 4.8 hardware traces** |
| Stage 4.8 aligned 16 KiB backing window | Attempt skipped: `0x00100000` is already mapped in the payload process |
| Boot Manager loads `winload.efi` / Windows kernel | Not reached |

The latest supplied hardware log ends at `AllocatePages(AllocateAddress,
EfiLoaderData, 1, 0x00102000)`. Stage 4.8 found `0x00100000` occupied and
refused to replace its mapping. Its ordinary mapping attempt returned
`0x00124000` instead of `0x00102000`, so EFI returned `EFI_NOT_FOUND`. The
earlier Stage 4.7 trace checked `mincore(0x00102000)` but did not record its
error code: a failed check does **not** prove that address was free. The current
revision queries the native page size and logs aligned occupancy checks to
resolve that ambiguity. It requires a new hardware test.

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

These interfaces are partial implementations for bring-up. A successful
callback or pre-entry BCD self-test does not establish that Boot Manager can
complete its own BCD processing or start Windows.

## Build and collect the next trace

1. Download the **PS4-Windows-Loader-Latest** artifact from the latest
   [successful CI run](https://github.com/hertebeznat-cell/ps4-windows-loader/actions/workflows/ci.yml).
   Inspect `STAGE.txt` and `SHA256SUMS.txt`; CI builds `payload/stage4_8.c`.
2. Supply `EFI/Microsoft/Boot/bootmgfw.efi` and `EFI/Microsoft/Boot/BCD` on
   your own test USB volume. These files are not included in this repository.
3. Run `PS4WindowsLoader-latest.bin` with the PS4 payload method for your test
   console. Stage 4.8 writes `/mnt/usb0/PS4WL_STAGE48.LOG`.
4. Preserve the complete log, especially `MMAP48: native page size`, occupied
   backing-page lines, and the `AP45` status. These establish whether the
   requested 4 KiB EFI page shares a larger native page with an existing mapping.

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

## Next milestones

1. Test the updated Stage 4.8 and determine the actual VM page size and
   ownership of the 16 KiB window. Never force-map over an occupied page.
2. Model a truthful, stable physical memory map and required firmware tables;
   the current process mappings and synthetic EFI descriptors are insufficient
   for a Windows kernel handoff.
3. Implement the remaining Boot Manager services as observed; reach
   `winload.efi`, then kernel initialization.
4. Bring up PS4 platform description, storage, USB input, framebuffer and
   other device support before claiming a usable Windows installation.

For the longer architecture plan, see [Boot Flow](docs/BOOT_FLOW.md). Historical
payloads live in [`payload/`](payload/); the CI stage selector determines the
active source rather than the highest stage filename.

## Licensing and files

MIT license; see [LICENSE](LICENSE). The repository does not distribute
Microsoft Windows files, Sony firmware, product keys or proprietary drivers.
