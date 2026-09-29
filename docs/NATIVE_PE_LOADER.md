# Native EFI image preparation

Status: implemented and host-tested; **no Windows entry or console validation**.
This closes the missing PE mapping/relocation link inside native workspace
preparation. It does not resolve the [13.52 kernel binding](PS4_1352_BINDING.md).
There is no new console payload. Native-Core remains a relocatable development
object, not a file to send to the PS4.

## Implemented path

`pwl_native_request_t` now accepts optional `boot_image` / `boot_image_bytes`:
a caller-owned, already-read EFI application file. Both must be set together.
Without them the original resident-workspace preparation remains available.

With an image, `pwl_native_workspace_prepare`:

1. Validates the AMD64 PE32+ EFI application layout and obtains SizeOfImage
   before requesting kernel memory.
2. Includes the image in the single owned, 16 KiB-rounded arena allocation.
3. Copies headers/sections and zeros BSS/padding into the image's preparation
   KVA. Physical ownership is still supplied by the memory backend.
4. Validates the complete relocation stream, then applies DIR64 fixups using
   **image physical address minus preferred ImageBase**. The preparation KVA
   never becomes the relocation base or the future entry address.
5. Adds the image to the reserved loader-code memory inventory and adds its
   individual section ranges to page-table construction. Executable sections
   are RX, writable data is RW/NX, headers/gaps/read-only data are RO/NX.
6. Independently verifies those page mappings together with all resident
   workspace mappings and stack guards. On any error, the preparation
   transaction releases its original KVA; a refused free retains the owner.

`workspace.boot_image.entry_address` identifies the future identity-mapped EFI
entry. It is metadata, not permission to call it. No CR3 or CPU state changes
occur. The workspace and image-description structs are preparation-side
owners; only the copied image and resident spans survive a future transition.
The initial image is one non-freeable loader-code descriptor, with PTEs providing
per-section access permissions; this is not a LoadImage allocation lifecycle.

## Parser and relocation contract

`loader/src/pe_loader.c` reuses `pwl_pe_inspect` and adds strict native layout
checks: section/file alignment, optional-directory bounds, ordered nonoverlapping
virtual sections, disjoint raw ranges, no section/header overlap, and an entry
inside file-backed executable bytes. The initial policy supports at most 96
sections, matching the documented Windows image section limit.

Only AMD64 EFI applications are supported. Dynamic imports, delay imports,
TLS and W+X sections are rejected. Sub-page SectionAlignment is rejected.
Relocations support ABSOLUTE padding and DIR64; other types are unsupported.
Fixups must be ordered and nonoverlapping. The implementation deliberately
rejects unordered streams rather than silently applying a repeated fixup or
allocating a large tracking bitmap. This is a restricted loader policy, not
a claim that every rejected image violates the PE specification.

The relocation directory must be wholly file-backed inside one section.
Targets must fit inside actual image sections and cannot overlap relocation
metadata. This keeps validation stable during the second, applying pass.
Images without relocations can load only at their preferred address; the
workspace never forces an allocation at an occupied address to accommodate one.
Inconsistent RELOCS_STRIPPED plus a nonempty relocation directory is rejected.

`pwl_pe_load_efi` performs no allocation, syscall or image entry. Caller-owned
source, destination and output must be disjoint and remain stable during the
call. Layout/capacity failure leaves the destination unchanged. Relocation
failure clears copied image bytes and publishes no entry or permission plan.
Invalid aliased arguments are refused before touching any of those buffers.
The output only becomes usable after a successful return.

## Validation and limits

`tools/test_native_core.sh` includes synthetic fixtures testing downward/upward
physical relocation, source immutability, BSS and ignored zero-raw offsets,
page permissions, every truncated-file prefix, invalid alignments/subsystems,
raw/virtual overlaps, imports/TLS/W+X, relocation size/type/target errors,
self-modifying or duplicate fixups, missing relocations and destination bounds.

Workspace tests use different KVA/PA addresses, verify the resulting RX and
RW/NX PTEs, check entry and relocated pointer values, and check rollback after
a malformed relocation discovered after arena allocation. Existing no-image
workspace tests remain enabled. CI runs ASan/UBSan and builds the closed
freestanding object; tests never execute the synthetic image instructions.

No Microsoft binary is bundled or tested by these fixtures. Compatibility
with the user's particular Windows 11/Server 2025 bootmgfw.efi remains untested.
This module is the PE preparation part of a future LoadImage implementation,
not an installed EFI protocol. It does not verify Authenticode/Secure Boot,
construct LoadedImage/SystemTable, load child images from FAT/BCD, relocate
the resident firmware's own code/pointers, or implement StartImage/Exit.
The heap remains NX. Platform activation, AP/IRQ/DMA handling, recovery,
ACPI, the complete physical map and Windows device support remain separate
requirements before any Boot Manager call.

Sources:
[Microsoft PE/COFF format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format),
[UEFI image services](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html),
[UEFI Loaded Image protocol](https://uefi.org/specs/UEFI/2.10/09_Protocols_EFI_Loaded_Image.html).
