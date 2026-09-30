# Existing Windows USB file write check

The user reported the root EFI checks passed but only the marker value was
visible in a text file. Exact final EFI fields were not supplied, so no new
hardware result is asserted from that report alone. Do not repeat the CPU/EFI
integration solely to investigate removable-drive logging.

Old retrieved Memory/Workspace/Resident logs used direct
`open(O_WRONLY|O_CREAT|O_APPEND)/write/fsync/close`. New report code added marker
selection, file replacement, directory fsync and readback. These differences
are observations, not a proven cause of the missing report.

Build with `sh tools/build_workspace_probe.sh SDK_DIRECTORY COMMIT usb-existing`.
This standalone check opens only `PWL_USB.TXT` after validating its exact marker
with optional LF/CRLF. It adds a newline and a build-specific result using
`O_WRONLY|O_APPEND` with no create, truncation or rename, then syncs/closes and
reopens/compares the appended record. It does not use stat or directory fsync.
It has no kernel callback, EFI calls or CPU transition. No other file is edited.

Copy the provided fresh `PWL_USB.TXT` into the root of the FAT32 drive WINDOWS
before the one invocation. Afterward the same Windows-created file should
contain both its original marker and `result=USB_EXISTING_FILE_WRITTEN`.
Return that file from Windows. Internal readback does not prove USB persistence
until the appended bytes are retrieved in Windows. After this check the marker
is no longer exact because the report was appended; restore the provided marker
before any later diagnostic that requires an exact marker. Do not repeat this
check simply because a log is missing.

Public-source review confirmed that projects use `/mnt/usbN` paths, but yielded
no confirmed FAT32 remedy for this specific disappearance. The capture and
retrieved files are stronger evidence for this project than generic advice.
No filesystem is reformatted, mounted, unmounted or changed by this check.
