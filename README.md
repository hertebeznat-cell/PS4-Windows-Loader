<div align="center">

# 🪟 PS4 Windows Loader

### Bare-metal Windows boot research for PlayStation 4

**Target:** PS4 Slim CUH-2208B · Baikal · x86-64  
**Goal:** Windows 11 / Windows Server 2025 directly on PS4 hardware — **no Linux host, no QEMU, no virtualization**.

![Stage](https://img.shields.io/badge/stage-3.1%20Boot%20Services-blueviolet)
![Hardware](https://img.shields.io/badge/real%20hardware-verified-success)
![Target](https://img.shields.io/badge/target-PS4%20Baikal-blue)
![Architecture](https://img.shields.io/badge/arch-x86__64-lightgrey)
![CI](https://img.shields.io/github/actions/workflow/status/hertebeznat-cell/PS4-Windows-Loader/ci.yml?label=latest%20payload)
![License](https://img.shields.io/badge/license-MIT-green)

</div>

---

## 🚀 What is PS4 Windows Loader?

**PS4 Windows Loader** is an experimental open-source project exploring a real **bare-metal Windows boot path on PlayStation 4 hardware**.

The intended boot chain is:

```text
PS4 jailbreak / payload entry
          │
          ▼
PS4 Windows Loader
          │
          ├── PS4 hardware handoff
          ├── memory/platform discovery
          ├── ACPI tables
          ├── framebuffer
          ├── USB/storage bootstrap
          └── UEFI-compatible environment
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
| Display target | Existing HDMI framebuffer first |
| Boot media | External USB first |
| Windows targets | Windows 11 / Windows Server 2025 |

Development is intentionally focused on one known machine before expanding to other PS4 revisions.

> **Safety rule for development:** early experiments use external USB media. Do not repartition or overwrite the PS4 internal system drive while the boot path is still experimental.

---

## ✅ Verified real-hardware progress

The following milestones have been tested successfully on a real **PS4 Slim CUH-2208B / Baikal / firmware 13.52**.

### Stage 1 — PS4 payload execution ✅

Confirmed:

- freestanding `.bin` payload executes on PS4;
- PS4 system notifications work;
- Orbis USB mount points can be accessed from the payload;
- Windows EFI file paths can be probed directly.

### Stage 2.5 — Windows EFI image loader ✅

The loader successfully:

- opens `/mnt/usb0/EFI/Microsoft/Boot/bootmgfw.efi`;
- reads the complete file into memory;
- validates DOS + PE/COFF headers;
- validates **AMD64 / PE32+ / EFI Application** format;
- allocates an in-memory executable image;
- maps PE sections to their virtual addresses;
- applies `IMAGE_REL_BASED_DIR64` base relocations;
- resolves the EFI image entry point.

Real-hardware success sequence:

```text
PS4 Windows Loader: Stage 2.5 started
Stage 2.5: direct usb0 open OK
Stage 2.5: stream read to EOF OK
Stage 2.5: PE32+ EFI validation OK
Stage 2.5: sections mapped OK
PS4 Windows Loader: Stage 2.5 PE map + relocations OK
```

### Stage 3 — EFI System Table + x64 ABI ✅

Stage 3 has now also been verified on real PS4 hardware.

Confirmed:

- minimal `EFI_SYSTEM_TABLE` construction;
- correct EFI System Table signature;
- EFI console `OutputString` callback path;
- Microsoft x64 / UEFI calling convention (`ms_abi`);
- successful re-read and validation of `bootmgfw.efi`;
- executable PE image mapping and relocations;
- calculation of the real Microsoft Boot Manager entry point.

Real-hardware success sequence:

```text
PS4 Windows Loader: Stage 3 started
Stage 3: EFI SystemTable constructed
Stage 3: SystemTable signature OK
Stage 3 EFI OutputString OK
Stage 3: Microsoft x64 EFI ABI test OK
Stage 3: bootmgfw.efi read OK
Stage 3: PE32+ EFI validation OK
Stage 3: bootmgfw executable image ready
```

`bootmgfw.efi` is now prepared as executable code in PS4 memory, but its entry point is intentionally **not called yet**. The next work is implementing the UEFI Boot Services and protocols it expects.

### Stage 3.1 — Boot Services 🚧

Current development adds real callable UEFI-style services and protocol discovery. The first Stage 3.1 hardware test covers:

- `AllocatePages` / `FreePages`;
- `AllocatePool` / `FreePool`;
- `GetMemoryMap` buffer semantics;
- `HandleProtocol`;
- `LocateProtocol`;
- Loaded Image protocol;
- Device Path protocol;
- publication of Simple File System protocol.

The following step will bridge `EFI_FILE_PROTOCOL` to the PS4 `/mnt/usb0` filesystem so Microsoft Boot Manager can request BCD and other boot files.

---

## 🧭 Current boot status

```text
PS4 payload entry                         ✅
        │
        ▼
Read bootmgfw.efi from USB                ✅
        │
        ▼
Validate PE32+ / AMD64 / EFI              ✅
        │
        ▼
Map PE sections                           ✅
        │
        ▼
Apply x64 DIR64 relocations               ✅
        │
        ▼
Build minimal EFI System Table            ✅
        │
        ▼
Validate Microsoft x64 EFI ABI            ✅
        │
        ▼
Implement required EFI Boot Services      🚧
        │
        ▼
Implement EFI filesystem bridge           ⏳
        │
        ▼
Enter bootmgfw.efi                         ⏳
        │
        ▼
Windows Boot Manager                      ⏳
        │
        ▼
winload.efi                               ⏳
        │
        ▼
ntoskrnl.exe                              ⏳
```

---

## 🗺️ Roadmap

### Stage 1 — PS4 payload + Windows EFI discovery ✅
- [x] real PS4 payload build
- [x] PS4 notification/log path
- [x] USB mount probing
- [x] locate `bootmgfw.efi`
- [x] AMD64 PE32+ EFI validation

### Stage 2 — PE loader ✅
- [x] stream-read EFI file from USB
- [x] PE section mapper
- [x] x64 relocation engine
- [x] executable image allocation
- [x] entry-point calculation
- [x] verified on real PS4 hardware

### Stage 3 — EFI compatibility layer 🚧
- [x] initial EFI type definitions
- [x] minimal System Table prototype
- [x] console/output shim prototype
- [x] Microsoft x64 ABI test path
- [x] executable `bootmgfw.efi` mapping
- [x] entry-point preparation
- [x] Stage 3 verified on real PS4 hardware
- [ ] memory map service — Stage 3.1 test pending
- [ ] `AllocatePages` / `FreePages` — Stage 3.1 test pending
- [ ] `AllocatePool` / `FreePool` — Stage 3.1 test pending
- [ ] `HandleProtocol` — Stage 3.1 test pending
- [ ] `LocateProtocol` — Stage 3.1 test pending
- [ ] `OpenProtocol`
- [ ] Loaded Image protocol — Stage 3.1 test pending
- [ ] Device Path protocol — Stage 3.1 test pending
- [ ] Simple File System protocol — Stage 3.1 publication test pending
- [ ] EFI File Protocol filesystem bridge
- [ ] Block I/O protocol
- [ ] ACPI configuration table
- [ ] GOP-compatible framebuffer description

### Stage 4 — Windows Boot Manager
- [ ] transfer control to `bootmgfw.efi`
- [ ] trace first missing EFI service/protocol
- [ ] satisfy Boot Manager protocol dependencies
- [ ] BCD access
- [ ] reach `winload.efi`

### Stage 5 — Windows kernel
- [ ] Windows-compatible memory handoff
- [ ] ACPI platform description
- [ ] interrupt controller setup
- [ ] timers / TSC / HPET bring-up
- [ ] PCI enumeration
- [ ] reach `ntoskrnl.exe`

### Stage 6 — installable Windows
- [ ] USB storage path
- [ ] boot-critical storage driver
- [ ] USB keyboard/mouse
- [ ] basic framebuffer display driver
- [ ] Windows Setup
- [ ] install to a dedicated external drive

### Stage 7 — usable system
- [ ] Ethernet / Wi-Fi
- [ ] audio
- [ ] Bluetooth
- [ ] DualShock HID
- [ ] power/reset management

### Stage 8 — Liverpool acceleration
- [ ] WDDM kernel-mode display driver
- [ ] Windows user-mode graphics path
- [ ] hardware-accelerated desktop
- [ ] DirectX bring-up

Liverpool/WDDM is expected to be one of the largest parts of the project.

---

## 📁 Repository layout

```text
PS4-Windows-Loader/
├── payload/
│   ├── stage1.c
│   ├── stage2*.c
│   ├── stage3.c
│   └── stage3_1.c         # current development payload
├── loader/
│   ├── include/           # shared loader interfaces
│   └── src/               # portable PE/boot core
├── drivers/               # future Windows drivers
├── docs/
│   └── BOOT_FLOW.md
├── setup/                 # future ISO/driver injection tools
└── .github/workflows/
    └── ci.yml
```

---

## 🔨 Builds

GitHub Actions builds the current development payload on every push.

To avoid confusion during hardware testing, the workflow publishes **one current artifact only**:

```text
PS4-Windows-Loader-Latest
```

It contains:

```text
PS4WindowsLoader-latest.bin
PS4WindowsLoader-latest.elf
SHA256SUMS.txt
PS4_RUNTIME_COMMIT.txt
STAGE.txt
```

`STAGE.txt` identifies which source stage produced the current `latest` payload.

The PS4 payload runtime dependency is pinned to a known public `ps4-linux-loader` commit so builds remain reproducible.

> The current payload is still experimental firmware/boot research code. A successful build does **not** mean Windows is bootable yet.

---

## 💿 Preparing the current USB test file

Current Stage 2/3 development expects:

```text
/mnt/usb0/EFI/Microsoft/Boot/bootmgfw.efi
```

On a FAT32 USB drive this corresponds to:

```text
EFI/
└── Microsoft/
    └── Boot/
        └── bootmgfw.efi
```

The Microsoft file must come from the tester's own legitimate Windows installation or installation media. It is not included in this repository.

---

## 💿 Windows media policy

This repository will **never redistribute a modified Windows ISO**.

A future builder may accept the user's own legitimate Windows 11 / Windows Server 2025 media and locally inject this project's open-source drivers, ACPI/platform data, and setup configuration.

Microsoft files stay outside this repository.

---

## 🧠 Engineering approach

The project deliberately separates several hard problems:

1. **PS4 payload execution and hardware handoff**
2. **PE/COFF loading**
3. **UEFI compatibility**
4. **Microsoft boot chain**
5. **Windows platform drivers**
6. **Liverpool graphics / WDDM**

Every stage has a concrete real-hardware success condition. This makes failures attributable to a specific layer instead of treating "Windows did not boot" as one giant unknown.

Detailed architecture: [`docs/BOOT_FLOW.md`](docs/BOOT_FLOW.md).

---

## ⚠️ Experimental status

This project is early-stage low-level boot research.

Expect:

- crashes;
- hangs;
- kernel panics;
- incomplete hardware support;
- frequent binary/interface changes between stages.

Use dedicated external test media and keep backups of anything important.

---

## 🤝 Contributions

Useful areas include:

- Baikal PCI/device mapping
- PS4 interrupt/timer research
- ACPI generation
- UEFI Boot Services implementation
- PE/COFF loading
- USB/storage bring-up
- Windows Driver Kit development
- Liverpool display/GPU research
- reproducible logs from real PS4 hardware

Please keep proprietary Sony/Microsoft code and leaked material out of the project.

---

## 📜 License

MIT — see [`LICENSE`](LICENSE).

---

<div align="center">

### PS4 Windows Loader

**From a PS4 payload to Windows Boot Manager — one verified milestone at a time.**

</div>
