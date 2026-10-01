# USB archive to owned Boot Manager image

`pwl_boot_source_prepare()` joins preparation I/O and native boot-image
preparation in one transaction. It takes an explicitly selected archive path,
maximum archive size, EFI file path, memory binding and resident service image.
The console adapter is `pwl_console_boot_source_io` from
`payload/boot_source_io.h`. It reads a regular file using the pinned SDK.

The caller may select `/mnt/usb0/PWL_BOOT.PAK` for the mounted USB. This path
does not verify the FAT32 label `WINDOWS`: mount indices and Windows drive
letters are different identities. The adapter neither scans other devices
nor creates files. FAT32 is accessed by the existing preparation OS; resident
EFI sees the immutable archive, not a process descriptor or USB driver.

Pack a directory containing your own EFI tree and associated boot resources:

```sh
python3 tools/pack_boot_files.py boot-files PWL_BOOT.PAK
```

Place that archive on the USB. Preserve paths inside the selected directory;
for a normal Microsoft EFI tree the selected application path is
`\EFI\Microsoft\Boot\bootmgfw.efi`. Include the needed BCD, fonts and related
resources; archive presence does not demonstrate that Boot Manager can use them.
The current archive capacity is 256 files/directories. No Microsoft files are
supplied or downloaded by this project.

The transaction rejects oversized/non-block-aligned files before allocating
a staging buffer, accepts partial reads, detects early EOF and trailing growth,
and closes the source before validating/copying the archive. A read failure
remains the primary error if close also fails; `close_status` preserves that
secondary failure. Staging storage is released after every path that allocated
it. Preparation failures use the native workspace's existing rollback; retained
ownership on failed release remains visible to the caller.

The report distinguishes OPEN, READ, CLOSE, VALIDATE, PREPARE and READY.
READY means resident files, relocated application, metadata and independent
table mappings have been prepared and audited. It does not mean application
entry has been called. All I/O callbacks are preparation-side only.

Build the core and separate SDK adapter:

```sh
sh tools/build_boot_source.sh dumper-sdk
```

This produces development objects without a console entry. Stage 4.8 is not
changed to execute Microsoft code. The production memory-binding gate still
refuses the unverified profile. Native context/exception handling, platform
memory inventory, device ownership and remaining EFI/image services remain
required for a full Boot Manager launch. Host integration exercises this entire
source-to-owned-image transaction above 4 GiB and validates the environment
after the staging buffer is freed; it does not emulate a console.
