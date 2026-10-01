# User EFI archive, 2026-10-01

The supplied EFI.zip contains 148 files and 48 directories. Including the root,
the resident archive requires 197 records. The former 128-record limit prevented
packing the complete supplied tree. The packer and resident reader now accept at
most 256 records; boundary tests cover 256 acceptance and 257 refusal. The file
format is still version 1. Older readers supporting 128 records reject this
larger archive, so the updated reader is required.

The complete tree was packed without omitting any files. All 148 payloads were
compared byte-for-byte to their extracted originals. PWL_BOOT.PAK is 36,036,608
bytes, includes BCD/fonts/resources/locales and passes the resident C validator.
The Microsoft file bytes are user inputs and are not committed to this repository.

| File | Finding |
| --- | --- |
| EFI/Microsoft/Boot/bootmgfw.efi | AMD64, EFI application subsystem 10, 3,087,400 file bytes, mapped size 3,383,296, entry RVA 0x422e0 |
| EFI/Boot/bootx64.efi | Byte-identical to bootmgfw.efi |
| EFI/Microsoft/Boot/bootmgr.efi | Subsystem 16; correctly refused by this EFI-application loader |
| EFI/Microsoft/Boot/memtest.efi | Subsystem 16; not a supported EFI application input |
| EFI/Microsoft/Boot/SecureBootRecovery.efi | Subsystem 10; native PE preparation succeeds |
| EFI/Microsoft/Boot/BCD | regf header, 20,480 bytes, sequence numbers 3/3; base-block checksum matches. This is not full BCD semantic validation |

bootmgfw/bootx64 SHA256:
4bb32c300df0815d8b6c54a0d3793ae1d78fcdc07f8e2b9f831289b2eb109092.

The actual bootmgfw.efi passes the current native PE size query and copy/relocation
at target 0x27a300000, with prepared entry 0x27a3422e0 and seven permission ranges.
It has no import, TLS or delay-import directory. The test ran again using the
image selected directly from PWL_BOOT.PAK (record 14), with the same result.
No image code was executed; these results do not prove EFI/runtime/platform
compatibility or successful Boot Manager startup on PS4.

The provided tree is EFI content, not a full Windows installation. Its BCD has
references to winload.efi, but winload.efi is not included in this tree. The data
archive has no console entry, hardware memory binding or CPU transition. It is
input for the future native entry, not a replacement for a .bin launcher.
