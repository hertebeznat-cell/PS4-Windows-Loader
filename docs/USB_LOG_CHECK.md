# USB log verification only

Build: `sh tools/build_workspace_probe.sh SDK_DIRECTORY COMMIT usb-log`.
Send `PS4WindowsLoader-USB-Log-Check.bin` once with the USB connected.
This build does not enter a kernel callback, allocate kernel memory or change
CPU tables. It checks only USB file visibility.

The previous console photos for build `3dfa5a0` showed journal operations
returning success at `/mnt/usb0/PS4WL_TRANSITION.LOG`, but the user could not
find the file on the removed USB. This is unresolved; successful open/write,
fsync and close alone do not establish which backing filesystem supplied that
path. The actual root experiment refused before allocation: stage=2 error=23.

This diagnostic compares the SDK stat device for `/mnt/usb0` and `/mnt/usb1`
against `/mnt`. It refuses writing if the USB directory is absent or reports
the same device as its parent. A distinct device is evidence of a filesystem
boundary, not independent proof of a physical removable device.

On a confirmed boundary it appends `USB_LOG_CHECK` to the short filename
`PWL.LOG`, synchronizes/closes it, attempts parent-directory synchronization,
reopens the file, seeks to the record and compares every byte. Unsupported
parent fsync is displayed separately. Notifications show both device IDs,
write status, readback status and directory synchronization status.

Retrieve `PWL.LOG` from the USB root. If absent, photograph all notifications;
this test does not require repeating the CPU transition experiment. A matching
readback proves visibility inside the console namespace, not persistence on the
removed physical device. Do not claim the latter without retrieving the file.
