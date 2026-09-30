# Resident read-only EFI files

The resident image now exports Simple File System OpenVolume and the ten
revision-1 File Protocol methods, in addition to its previous 17 callbacks.
They operate on a validated immutable `PWLFILES` archive copied into the owned
media span before the CPU context changes. They do not use process filesystem
calls, live USB handles or a console-side filesystem callback after entry.

Implemented: absolute/relative UTF-16 paths, ASCII case-insensitive lookup,
`.`/`..` normalization bounded by the root, opening directories and files,
directory enumeration, partial reads, EOF, GetPosition/SetPosition, EFI_FILE_INFO,
Close and read-only Flush. Write/SetInfo return WRITE_PROTECTED. Delete closes
the handle and returns WARN_DELETE_FAILURE without removing the file.
Filesystem information GUIDs other than EFI_FILE_INFO remain unsupported.
The registry has 32 live file handles; every method validates its `This` value
against active owned slots before accessing it. Calls remain serialized.

## Preparing actual user-owned files

Run on a computer, with a directory containing the required boot files:

```sh
python3 tools/pack_boot_files.py BOOT_FILES_DIRECTORY build/boot-files.bin
```

The output must be outside the source directory. The tool preserves the
hierarchy, UTF-16 names and file bytes, includes the root/parent directories,
rejects symlinks and ASCII case duplicates, and produces deterministic data
padded to 512 bytes. At most 128 directory/file records and 255 UTF-16 path
code units are supported. Proprietary boot files are supplied by the user and
are not added to this repository.

Pass the resulting archive as `disk_image`/`disk_bytes` to resident native
preparation. A matching archive magic triggers full validation and publishes
SimpleFileSystem on its own handle with destination physical callback and
interface addresses. A malformed matching archive aborts and releases the
owner. Existing plain disk-image diagnostic inputs retain their previous
behavior without a filesystem protocol. The original memory-buffer LoadedImage
metadata still has no DeviceHandle/FilePath; origin binding must be provided
before an actual Boot Manager launch.

This is a cached file volume, not a FAT32 implementation or live USB driver.
All required files must fit the owned resident memory and the record limit.
It does not discover or preload files in the current console entry path.
A full Windows boot requires additional firmware/protocols and platform handoff;
this implementation is not a new Boot Manager launcher.

## Archive wire layout

Little-endian header: eight-byte `PWLFILES` magic, uint32 version 1, uint32
record count, uint64 total archive bytes. Each 536-byte record has a 512-byte
zero-padded UTF-16 absolute path, uint64 file offset, uint64 logical bytes,
uint32 directory flag (0/1), and uint32 reserved zero. File extents follow the
record table; they must lie within the archive without overlapping. Directory
records have zero logical bytes. Every non-root record must have a matching
parent directory. The root must be present. Empty files are supported.

Host checks exercise corrupt lengths/counts/extents, duplicate names, missing
parents, path traversal, truncation, partial reads and EOF; execute the copied
Microsoft-ABI file methods; and validate native filesystem publication,
physical callback addresses and cleanup on preparation failure. No emulator or
new console run is used.

EFI reference: [UEFI media access protocols](https://uefi.org/specs/UEFI/2.10/13_Protocols_Media_Access.html).
