# Returning resident EFI calls under an identical root

Build: `sh tools/build_workspace_probe.sh SDK_DIRECTORY COMMIT root-efi`.
This combines the already passed active-root resolution with nine copied
resident EFI callbacks on the temporary kernel allocation's 16 KiB stack.
The live root copy retains all original mappings. No independent EFI root,
Microsoft code, device initialization or ExitBootServices transition occurs.

Before the kernel callback, the resident image is validated, copied into a
process mapping, bound to owned static data, changed from RW to RX, and locked
in memory. The complete raw image, including fixture/data/selftest, is also
locked. Under the clone, SMEP and SMAP must be disabled (otherwise refusal),
ordinary interrupts are masked, and no SDK, allocator, log or host service is
called. The assembly checks current CR3 and all 512 root entries again before
switching. Its SysV wrapper calls the copied Microsoft x64 ABI functions.
The callback checks a local stack address is inside the temporary stack.
Original root, stack, flags and callee-saved registers are restored before
release and user-context reporting. No fault recovery is provided.

The nine functions use synthetic memory descriptors and never access the
addresses returned by AllocatePages. ExitBootServices must return UNSUPPORTED.
This is a returning integration check, not a full firmware or Windows boot.

## FAT32 result file

Keep `PWL_USB.TXT` containing `DAVID_USB_E_20260930` on the FAT32 drive whose
Windows label is `WINDOWS`. Label alone is not a mount identity: the check
confirms a separate filesystem and exact marker before writing. It uses the
8.3 filenames `PWL_EFI.TMP` and `PWL_EFI.TXT`, syncs/closes the temporary header,
renames it, syncs the directory, then reopens/compares it. Subsequent checkpoints
and the final report are synced, closed, directory-synced and read back.
Only these diagnostic result files are replaced; the marker is unchanged.
The label is recorded as expected, not claimed independently verified. No
filesystem is reformatted, mounted or unmounted by this diagnostic.

Send `PS4WindowsLoader-Root-EFI.bin` once. The result is `PWL_EFI.TXT` in the
USB root (for example `E:\PWL_EFI.TXT`). USB logging remains optional for the
CPU check; if marker/flush verification fails, only notifications are available.
Successful internal readback still does not prove post-removal persistence
until the file is retrieved in Windows.

Expected: `rc=0 stage=5 error=0`, `status=0 switched=1 restored=1 released=1`,
`passed_mask=1ff last_call=9 code_unlock=0 code_release=0`. All nine checks run
in one invocation. This new integration has not yet run on PS4. A crash remains
possible; do not repeat a hung run. The prior root-clone and process-only
resident/stack checks do not need repeating.
