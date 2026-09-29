# PS4 Windows Loader — Boot Flow

This document tracks the planned bare-metal boot path for the first target: **PS4 Slim CUH-2208B / Baikal**.

## 1. Entry

The project starts from a PS4 payload entry point after the console has already been exploited by an external jailbreak chain.

The loader must not assume PC BIOS/UEFI services exist.

Stage 4.8 showed that calling `bootmgfw.efi` inside a PS4 user process reaches
instructions requiring processor privileges (`mov rax, cr3` and `rdmsr`).
The active artifact therefore stops before EFI entry. See
[Execution boundary](EXECUTION_BOUNDARY.md) for trace evidence and the required
change in execution architecture.

### Resident preparation before displacing Orbis

`pwl_native_workspace_prepare` now acquires an arena through a supplied kernel
allocator binding, verifies physical translations, copies firmware and a disk
image into that arena, initializes resident memory/media state and builds
independent mappings with stack guards. It must run while Orbis kernel memory
services are still available. See [native backend](NATIVE_BACKEND.md) for the
implementation, source audit and tests.

The [13.52 binding gate](PS4_1352_BINDING.md) currently refuses production
allocation before any kernel call. Host fixtures exercise the transaction.
The transaction can now [map and relocate an optional EFI application](NATIVE_PE_LOADER.md)
and map its sections with separate code/data permissions. It does not enter it.
Verified target kernel bindings, the complete RAM/MMIO inventory, resident
firmware relocation and EFI table
installation, PAT/MTRR checks, a recoverable CPU transition and device ownership
remain required. The published payload does not call this preparation API.

## 2. Quiesce Orbis-owned hardware

Before handing devices to another operating system, the loader will need a deterministic hardware state. Candidate tasks include:

- stop active DMA where possible;
- mask/normalize interrupts;
- preserve or reconstruct the usable physical memory map;
- establish a stable framebuffer handoff;
- identify the boot USB/storage controller and target volume;
- record PCI configuration needed by the Windows platform layer.

Exact register operations are intentionally not filled in until validated against PS4-specific public source/research.

## 3. Firmware compatibility layer

Windows Boot Manager is an EFI application. The loader therefore needs to expose the subset of EFI/firmware behavior that the Microsoft boot chain actually consumes.

Initial targets:

- PE/COFF image loading;
- EFI-style memory descriptors;
- loaded-image metadata;
- device-path representation;
- filesystem/file access for the Windows EFI System Partition;
- console/framebuffer output where practical;
- configuration tables, including ACPI;
- clean `ExitBootServices`-style transition semantics.

This layer is not intended to become a general PC firmware implementation. The goal is the smallest correct environment required for Windows boot on PS4.

## 4. Windows boot chain

Expected high-level chain:

```text
PS4 payload entry
  -> PS4 Windows Loader
  -> EFI compatibility layer
  -> \\EFI\\Microsoft\\Boot\\bootmgfw.efi
  -> BCD selection
  -> winload.efi
  -> ntoskrnl.exe
```

## 5. First measurable milestones

### M0 — loader executes
A payload reaches the loader entry and emits a recognizable diagnostic marker.

### M1 — storage visible
The loader can enumerate the intended boot device and read a FAT EFI volume.

### M2 — PE/COFF parsed
The loader validates and maps a known x86-64 EFI executable.

### M3 — Microsoft Boot Manager entered (research milestone)
Control reached `bootmgfw.efi` inside a PS4 process. The same run cannot
complete privileged CPU operations; a platform-owned boot context is required
before advancing to M4.

### M4 — Windows loader entered
Control reaches `winload.efi`.

### M5 — NT kernel begins initialization
Early Windows kernel output/state confirms `ntoskrnl.exe` has started.

### M6 — Windows Setup visible
Framebuffer + input + storage are sufficient to use Windows Setup.

## 6. Driver strategy

Bring-up order is intentionally conservative:

1. basic framebuffer/display
2. storage
3. USB keyboard/mouse
4. Ethernet
5. audio/Bluetooth/HID
6. accelerated Liverpool graphics (WDDM)

A basic display path should exist before attempting a full WDDM acceleration stack.

## 7. Windows media

The repository does not ship Microsoft binaries. Future tooling may transform a locally supplied legitimate ISO by injecting project drivers and setup configuration.
