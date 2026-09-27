<div align="center">

# 🪟 PS4 Windows Loader

### Bare-metal Windows boot research for PlayStation 4

**Target:** PS4 Slim CUH-2208B · Baikal · x86-64  
**Goal:** Windows 11 / Windows Server 2025 directly on PS4 hardware — no Linux host, no QEMU, no virtualization.

![Status](https://img.shields.io/badge/status-early%20research-orange)
![Target](https://img.shields.io/badge/target-PS4%20Baikal-blue)
![Architecture](https://img.shields.io/badge/arch-x86__64-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

</div>

---

## What is this?

**PS4 Windows Loader** is an experimental open-source project exploring whether modern Windows can be booted **directly on PlayStation 4 hardware**.

The project is not a Windows distribution and does **not** include Microsoft binaries, ISO images, product keys, firmware dumps, Sony proprietary code, or copyrighted drivers.

The intended design is:

```text
PS4 jailbreak / payload entry
          │
          ▼
PS4 Windows Loader
          │
          ├─ hardware handoff / reset
          ├─ memory map
          ├─ ACPI compatibility tables
          ├─ framebuffer handoff
          ├─ storage / USB bootstrap
          └─ UEFI-like runtime services needed for boot
          │
          ▼
Microsoft bootmgfw.efi
          │
          ▼
winload.efi
          │
          ▼
Windows kernel
          │
          ▼
PS4-specific Windows drivers
```

## 🎯 First hardware target

Initial development is intentionally narrow:

| Component | Initial target |
|---|---|
| Console | PS4 Slim CUH-2208B |
| Southbridge/platform | Baikal |
| CPU | AMD Jaguar x86-64 |
| GPU | AMD Liverpool |
| Firmware used for testing | 13.52 |
| Display | HDMI framebuffer first |
| OS targets | Windows 11 / Windows Server 2025 |

Other PS4 revisions can be added only after the first platform reaches reliable boot milestones.

## 🚦 Development stages

### Stage 0 — project foundation ✅
- repository structure
- boot architecture documentation
- payload entry skeleton
- PE/COFF loader interfaces
- CI compile checks

### Stage 1 — Windows Boot Manager
- initialize a known PS4 hardware state
- locate a FAT/EFI volume on USB storage
- parse PE/COFF images
- load `\\EFI\\Microsoft\\Boot\\bootmgfw.efi`
- provide enough firmware-style services to reach Microsoft Boot Manager

**Success criterion:** Windows Boot Manager or the first Windows boot screen appears on the PS4 display.

### Stage 2 — Windows kernel handoff
- memory map compatible with Windows loader expectations
- ACPI tables
- interrupt/timer setup
- framebuffer description
- boot-device path
- reach `winload.efi` and Windows kernel initialization

### Stage 3 — installable Windows
- storage driver
- USB input
- basic display driver
- Windows Setup reaches disk selection and installation

### Stage 4 — usable desktop/server
- Ethernet / Wi-Fi
- audio
- Bluetooth
- DualShock HID
- power management

### Stage 5 — accelerated Liverpool graphics
- WDDM kernel-mode display driver
- user-mode graphics path
- hardware acceleration

This is expected to be the largest driver sub-project.

## 📁 Repository layout

```text
PS4-Windows-Loader/
├── loader/
│   ├── include/          # loader interfaces
│   └── src/              # Stage 0/1 loader code
├── drivers/              # future Windows drivers
├── docs/
│   └── BOOT_FLOW.md      # architecture and milestone notes
├── setup/                # future ISO/driver injection tooling
└── .github/workflows/    # CI
```

## 🧩 Windows image policy

This repository will **never ship a modified Windows ISO**.

A future setup builder may accept a user's own legitimate Microsoft Windows 11 / Windows Server 2025 ISO and inject this project's open-source drivers and configuration into a locally generated test image.

## ⚠️ Current status

This is **early research**, not a working Windows boot solution yet.

At the moment the repository contains the Stage 0 loader architecture and compileable interfaces. Do not expect the current output to boot Windows on a PS4 yet.

## 🛠️ Building the Stage 0 code

The CI currently verifies that the portable loader core compiles cleanly. A real PS4 payload build will be added after the hardware entry/handoff layer is selected and validated.

Locally with Clang:

```bash
clang -std=c11 -Wall -Wextra -Werror \
  -Iloader/include \
  -c loader/src/main.c -o ps4wl-main.o
```

## 🧠 Design principle

The project deliberately separates:

1. **PS4 hardware bring-up**
2. **firmware/UEFI compatibility layer**
3. **Microsoft PE/COFF boot chain**
4. **Windows platform drivers**

That makes failures measurable instead of trying to jump straight from a PS4 payload into a full Windows desktop.

## 🤝 Contributing

Useful contributions include:

- PS4 Baikal PCI/device maps
- ACPI research
- PE/COFF loader work
- EFI protocol implementation
- storage and USB bring-up
- Windows Driver Kit work
- Liverpool display/GPU documentation
- reproducible boot logs

Please keep proprietary Sony/Microsoft binaries and leaked material out of the repository.

## 📜 License

MIT. See `LICENSE`.

---

<div align="center">

**PS4 Windows Loader**  
*From payload to Windows Boot Manager — one milestone at a time.*

</div>
