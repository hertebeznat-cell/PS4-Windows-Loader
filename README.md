<div align="center">

# 🪟 PS4 Windows Loader

### Bare-metal Windows boot research for PlayStation 4

**Target:** PS4 Slim CUH-2208B · Baikal · x86-64  
**Goal:** Windows 11 / Windows Server 2025 directly on PS4 hardware — **no Linux host, no QEMU, no virtualization**.

![Stage](https://img.shields.io/badge/stage-1%20payload-blueviolet)
![Target](https://img.shields.io/badge/target-PS4%20Baikal-blue)
![Architecture](https://img.shields.io/badge/arch-x86__64-lightgrey)
![CI](https://img.shields.io/github/actions/workflow/status/hertebeznat-cell/PS4-Windows-Loader/ci.yml?label=payload%20build)
![License](https://img.shields.io/badge/license-MIT-green)

</div>

---

## 🚀 What is PS4 Windows Loader?

**PS4 Windows Loader** is an experimental open-source project exploring a real **bare-metal Windows boot path on PlayStation 4 hardware**.

The long-term target is:

```text
PS4 jailbreak / payload entry
          │
          ▼
PS4 Windows Loader
          │
          ├── PS4 hardware handoff
          ├── memory map
          ├── ACPI tables
          ├── framebuffer
          ├── storage / USB bootstrap
          └── minimal UEFI-compatible environment
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

This is **not** Windows running inside Linux and it is **not** a virtual machine.

The repository does not contain Windows ISOs, product keys, Microsoft binaries, Sony firmware dumps, leaked material, or proprietary drivers.

---

## 🎮 First hardware target

| Component | Initial target |
|---|---|
| Console | **PS4 Slim CUH-2208B** |
| Southbridge | **Baikal** |
| CPU | AMD Jaguar x86-64 |
| GPU | AMD Liverpool |
| Test firmware | **13.52** |
| Display target | HDMI framebuffer first |
| Windows targets | Windows 11 / Windows Server 2025 |

Development is intentionally focused on one known machine before expanding to Aeolia, Belize, Belize2, PS4 Pro, and other revisions.

---

## ✅ Current progress

### Stage 0 — foundation ✅

- project architecture
- portable loader core
- PE/COFF parser
- CI compile validation
- boot-flow documentation

### Stage 1 — real PS4 payload 🚧

CI now builds a real freestanding PS4 payload:

```text
PS4WindowsLoader-stage1.bin
```

The Stage 1 payload:

- executes as a PS4 payload;
- emits visible PS4 system notifications;
- scans `/mnt/usb0` and `/mnt/usb1`;
- looks for `EFI/Microsoft/Boot/bootmgfw.efi` and `EFI/Boot/bootx64.efi`;
- validates PE/COFF, AMD64, PE32+, and EFI Application headers;
- reports whether a valid Windows EFI loader was found.

**Stage 1 does not transfer control to Windows yet.** Its purpose is to prove the PS4 payload path and Windows EFI discovery before we introduce firmware handoff code.

See [`docs/STAGE1.md`](docs/STAGE1.md).

---

## 🗺️ Roadmap

### Stage 1 — Windows EFI discovery 🚧
- [x] real PS4 payload build
- [x] USB mount probing
- [x] x64 PE32+ EFI validation
- [ ] map complete PE sections
- [ ] relocation engine
- [ ] EFI image context

### Stage 2 — EFI compatibility layer
- [ ] EFI System Table
- [ ] minimal Boot Services
- [ ] memory descriptors
- [ ] Loaded Image protocol
- [ ] device paths
- [ ] filesystem bridge
- [ ] ACPI configuration table
- [ ] framebuffer/GOP-compatible description

### Stage 3 — Windows Boot Manager
- [ ] load `bootmgfw.efi`
- [ ] satisfy required EFI protocols
- [ ] enter Microsoft Boot Manager
- [ ] load BCD
- [ ] reach `winload.efi`

### Stage 4 — Windows kernel
- [ ] Windows-compatible memory handoff
- [ ] interrupt/timer bring-up
- [ ] ACPI platform description
- [ ] reach `ntoskrnl.exe`

### Stage 5 — installable Windows
- [ ] storage driver
- [ ] USB keyboard/mouse
- [ ] basic display driver
- [ ] Windows Setup disk selection
- [ ] installation to external/internal target

### Stage 6 — usable system
- [ ] Ethernet / Wi-Fi
- [ ] audio
- [ ] Bluetooth
- [ ] DualShock HID
- [ ] power management

### Stage 7 — Liverpool acceleration
- [ ] WDDM kernel-mode display driver
- [ ] Windows user-mode graphics path
- [ ] hardware-accelerated desktop
- [ ] DirectX bring-up

Liverpool/WDDM is expected to be the largest driver sub-project.

---

## 📁 Repository layout

```text
PS4-Windows-Loader/
├── payload/
│   └── stage1.c           # current PS4-executable probe payload
├── loader/
│   ├── include/           # loader interfaces
│   └── src/               # portable PE/boot core
├── drivers/               # future Windows drivers
├── docs/
│   ├── BOOT_FLOW.md
│   └── STAGE1.md
├── setup/                 # future ISO/driver injection tools
└── .github/workflows/
    └── ci.yml             # reproducible payload builds
```

---

## 🔨 Builds

Every push is built by GitHub Actions.

The Stage 1 artifact contains:

```text
PS4WindowsLoader-stage1.bin
PS4WindowsLoader-stage1.elf
SHA256SUMS.txt
PS4_RUNTIME_COMMIT.txt
```

The PS4 payload runtime dependency is pinned to a known public `ps4-linux-loader` commit so builds are reproducible.

> **Important:** the current `.bin` is a discovery/probe payload. It is not yet a Windows boot payload and should not be described as one.

---

## 💿 Windows media policy

This repository will **never redistribute a modified Windows ISO**.

A future builder may accept the user's own legitimate Windows 11 / Windows Server 2025 ISO and locally inject this project's open-source drivers, ACPI/platform data, and setup configuration.

Microsoft files stay outside this repository.

---

## 🧠 Engineering approach

The project separates four difficult problems instead of mixing them together:

1. **PS4 payload + hardware control**
2. **UEFI/firmware compatibility**
3. **Microsoft boot chain**
4. **Windows drivers for PS4 hardware**

Each stage gets a measurable success condition, so failures can be debugged on real hardware instead of guessing whether the problem is the payload, EFI layer, Windows loader, or a device driver.

Detailed architecture: [`docs/BOOT_FLOW.md`](docs/BOOT_FLOW.md).

---

## 🤝 Contributions

Useful areas include:

- Baikal PCI/device mapping
- PS4 interrupt/timer research
- ACPI generation
- PE/COFF loading and relocations
- EFI protocol implementation
- USB/storage bring-up
- Windows Driver Kit development
- Liverpool display/GPU research
- reproducible boot logs from real PS4 hardware

Please keep proprietary Sony/Microsoft code and leaked material out of the project.

---

## 📜 License

MIT — see [`LICENSE`](LICENSE).

---

<div align="center">

### PS4 Windows Loader

**From a PS4 payload to Windows Boot Manager — one verified milestone at a time.**

</div>
