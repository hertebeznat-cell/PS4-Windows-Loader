# Stage 1 — PS4 payload + Windows EFI probe

Stage 1 is the first build that produces a real PS4 payload binary:

`PS4WindowsLoader-stage1.bin`

It is intentionally a **probe**, not yet the final Windows handoff loader.

## What the payload does

When executed by a PS4 payload sender, Stage 1:

1. starts in Orbis userland using the same freestanding PS4 payload runtime style as the public `ps4-linux-loader` project;
2. shows a PS4 system notification so execution is visible without a serial console;
3. checks the standard PS4 USB mount points (`/mnt/usb0` and `/mnt/usb1`);
4. searches for:
   - `EFI/Microsoft/Boot/bootmgfw.efi`
   - `EFI/Boot/bootx64.efi`
5. reads the PE header;
6. validates that the candidate is:
   - PE/COFF;
   - AMD64 (`0x8664`);
   - PE32+ (`0x20b`);
   - EFI Application subsystem;
7. reports success/failure through a PS4 notification and stdout.

## What it does NOT do yet

Stage 1 does **not** jump into Microsoft code yet. It does not construct EFI Boot Services, ACPI tables, a Windows memory map, or a device path. Those are the next milestones.

That separation is deliberate: before attempting a firmware handoff, we first prove that our own PS4 payload executes and can see/validate the Windows EFI loader on the target console.

## Preparing a test USB

Use a FAT32/exFAT USB device that Orbis mounts as `/mnt/usb0` or `/mnt/usb1` and place a legitimate x64 Windows EFI boot tree on it. The repository does not contain or redistribute Microsoft binaries.

A normal Windows installation USB commonly contains one or both of these paths:

```text
EFI/Microsoft/Boot/bootmgfw.efi
EFI/Boot/bootx64.efi
```

## Expected first-test result

If the payload starts correctly, the console should display:

```text
PS4 Windows Loader: Stage 1 started
```

If a valid x64 EFI image is found, it should then display:

```text
PS4 Windows Loader: valid Windows/EFI boot image found on USB
```

Otherwise:

```text
PS4 Windows Loader: no valid bootmgfw.efi/bootx64.efi found on USB
```

## Build provenance

CI pins the PS4 freestanding runtime dependency to a specific public `ps4-linux-loader` commit and stores that commit SHA next to the Stage 1 artifact. This keeps test binaries reproducible and prevents an upstream change from silently altering payload behavior.
