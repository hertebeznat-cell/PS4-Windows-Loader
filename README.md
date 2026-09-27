<div align="center">

# 🪟 PS4 Windows Loader

### Bare-metal Windows boot research for PlayStation 4

**Target:** PS4 Slim CUH-2208B · Baikal · x86-64  
**Goal:** Windows 11 / Windows Server 2025 directly on PS4 hardware — **no Linux host, no QEMU, no virtualization**.

![Stage](https://img.shields.io/badge/stage-3.4%20pre--bootmgr%20services-blueviolet)
![Hardware](https://img.shields.io/badge/real%20hardware-verified-success)
![Target](https://img.shields.io/badge/target-PS4%20Baikal-blue)
![Architecture](https://img.shields.io/badge/arch-x86__64-lightgrey)
![CI](https://img.shields.io/github/actions/workflow/status/hertebeznat-cell/PS4-Windows-Loader/ci.yml?label=latest%20payload)
![License](https://img.shields.io/badge/license-MIT-green)

</div>

---

## 🚀 What is PS4 Windows Loader?

**PS4 Windows Loader** is an experimental open-source project exploring a real **bare-metal Windows boot path on PlayStation 4 hardware**.

```text
PS4 jailbreak / payload entry
          │
          ▼
PS4 Windows Loader
          │
          ├── PS4 hardware handoff
          ├── PE/COFF loader
          ├── UEFI-compatible environment
          ├── USB filesystem bridge
          ├── memory/platform description
          └── later: ACPI / framebuffer / drivers
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
| Boot media | External USB first |
| Display target | Existing HDMI framebuffer first |
| Windows targets | Windows 11 / Windows Server 2025 |

Development is intentionally focused on one known machine before expanding to other PS4 revisions.

> **Safety rule:** early experiments use external USB media. Do not repartition or overwrite the PS4 internal system drive while the boot path is still experimental.

---

## ✅ Verified real-hardware progress

The milestones below have been tested successfully on a real **PS4 Slim CUH-2208B / Baikal / firmware 13.52**.

### Stage 1 — PS4 payload execution ✅

Confirmed:

- freestanding `.bin` payload executes on PS4;
- PS4 system notifications work;
- Orbis USB mount points are accessible;
- Windows EFI paths can be probed directly.

### Stage 2.5 — Windows EFI image loader ✅

Confirmed:

- opens `/mnt/usb0/EFI/Microsoft/Boot/bootmgfw.efi`;
- streams the complete file into memory;
- validates AMD64 / PE32+ / EFI Application headers;
- maps PE sections;
- applies `IMAGE_REL_BASED_DIR64` relocations;
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

### Stage 3 — EFI System Table + Microsoft x64 ABI ✅

Confirmed:

- minimal `EFI_SYSTEM_TABLE` construction;
- EFI console `OutputString` callback path;
- Microsoft x64 / UEFI calling convention (`ms_abi`);
- executable mapping of `bootmgfw.efi`;
- Microsoft Boot Manager entry-point preparation.

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

### Stage 3.1 — first EFI Boot Services ✅

Confirmed:

- `AllocatePool` / `FreePool`;
- `AllocatePages` / `FreePages`;
- `GetMemoryMap` query semantics;
- `HandleProtocol`;
- `LocateProtocol`;
- Loaded Image protocol publication;
- Device Path protocol publication;
- Simple File System protocol publication.

Real-hardware success sequence:

```text
PS4 Windows Loader: Stage 3.1 started
Stage 3.1: Boot Services table constructed
Stage 3.1: AllocatePool/FreePool OK
Stage 3.1: AllocatePages/FreePages OK
Stage 3.1: GetMemoryMap semantics OK
Stage 3.1: LoadedImage protocol OK
Stage 3.1: DevicePath protocol OK
Stage 3.1: SimpleFileSystem protocol published
PS4 Windows Loader: Stage 3.1 EFI Boot Services self-test OK
```

### Stage 3.2 — EFI File Protocol bridge ✅

Confirmed on real hardware:

- `SimpleFileSystem.OpenVolume`;
- `EFI_FILE_PROTOCOL.Open`;
- read-only EFI-style file handles;
- `Read`;
- `GetPosition` / `SetPosition`;
- `Close`;
- opening `\\EFI\\Microsoft\\Boot\\bootmgfw.efi` through the EFI filesystem layer;
- reading its `MZ` signature through that layer.

### Stage 3.3 — BCD through EFI File Protocol ✅

Stage 3.3 is verified on real PS4 hardware.

The loader opens the real Windows Boot Configuration Data file through the project's UEFI-style filesystem bridge:

```text
\\EFI\\Microsoft\\Boot\\BCD
```

Confirmed:

- Simple File System discovery;
- volume open;
- BCD open through `EFI_FILE_PROTOCOL`;
- BCD read through the EFI bridge;
- validation of the registry-hive `regf` header;
- seek / rewind semantics;
- clean handle close.

Real-hardware success sequence:

```text
PS4 Windows Loader: Stage 3.3 started
Stage 3.3: EFI filesystem bridge constructed
Stage 3.3: LocateProtocol(SimpleFS) OK
Stage 3.3: OpenVolume OK
Stage 3.3: EFI File Open BCD OK
Stage 3.3: BCD registry hive header regf OK
Stage 3.3: BCD seek/rewind OK
Stage 3.3: BCD Close OK
PS4 Windows Loader: Stage 3.3 BCD EFI read self-test OK
```

This proves that the compatibility layer can reach both the Microsoft EFI executable and its real BCD data through UEFI-style file APIs. **Microsoft Boot Manager itself has not been entered yet.**

### Stage 3.4 — pre-Boot-Manager services 🚧

Current development fills in more of the UEFI surface expected before the first controlled call into `bootmgfw.efi`.

The Stage 3.4 hardware self-test covers:

- `EFI_FILE_PROTOCOL.GetInfo` with `EFI_FILE_INFO` sizing semantics;
- BCD file size and filename metadata without disturbing the active file position;
- `OpenProtocol` / `CloseProtocol` plumbing;
- `LocateHandleBuffer` by protocol;
- `ProtocolsPerHandle`;
- `CalculateCrc32`;
- `CopyMem` / `SetMem`;
- `Stall`;
- continued access to the real BCD file through the EFI filesystem bridge.

Stage 3.4 is built in CI and is **awaiting real-hardware confirmation**.

---

## 🧭 Current boot status

```text
PS4 payload entry                         ✅
        │
        ▼
Read bootmgfw.efi from USB                ✅
        │
        ▼
Validate / map / relocate PE32+ image     ✅
        │
        ▼
Build EFI System Table + x64 ABI          ✅
        │
        ▼
Core EFI Boot Services                    ✅
        │
        ▼
LoadedImage / DevicePath / SimpleFS       ✅
        │
        ▼
EFI File Protocol bridge                  ✅
        │
        ▼
Read real Windows BCD via EFI             ✅
        │
        ▼
File metadata + protocol services         🚧
        │
        ▼
Runtime/event/memory-map hardening        ⏳
        │
        ▼
Enter bootmgfw.efi                         ⏳
        │
        ▼
Windows Boot Manager consumes BCD         ⏳
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
- [x] EFI System Table
- [x] console/output shim
- [x] Microsoft x64 ABI path
- [x] executable `bootmgfw.efi` mapping
- [x] initial memory map service
- [x] `AllocatePages` / `FreePages`
- [x] `AllocatePool` / `FreePool`
- [x] `HandleProtocol`
- [x] `LocateProtocol`
- [x] Loaded Image protocol
- [x] Device Path protocol
- [x] Simple File System protocol
- [x] EFI File Protocol bridge
- [x] BCD open/read/seek through EFI File Protocol
- [ ] `GetInfo` / file metadata — Stage 3.4 test pending
- [ ] `OpenProtocol` / `CloseProtocol` — Stage 3.4 test pending
- [ ] `LocateHandleBuffer` — Stage 3.4 test pending
- [ ] `ProtocolsPerHandle` — Stage 3.4 test pending
- [ ] CRC / CopyMem / SetMem / Stall — Stage 3.4 test pending
- [ ] directory enumeration
- [ ] minimal event/timer support
- [ ] Runtime Services stubs/semantics
- [ ] more accurate memory map + MapKey tracking
- [ ] Block I/O protocol if required
- [ ] ACPI configuration table
- [ ] GOP-compatible framebuffer description

### Stage 4 — Windows Boot Manager
- [ ] first controlled transfer to `bootmgfw.efi`
- [ ] trace EFI calls made by Microsoft Boot Manager
- [ ] implement missing services/protocols as encountered
- [ ] Boot Manager successfully reads BCD
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
│   ├── stage3_1.c
│   ├── stage3_2.c
│   ├── stage3_3.c         # latest hardware-verified stage
│   └── stage3_4.c         # current development payload
├── loader/
│   ├── include/
│   └── src/
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

To avoid confusion during hardware testing, each new workflow run publishes **one current artifact only**:

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

`STAGE.txt` identifies which source stage produced the current payload.

The PS4 payload runtime dependency is pinned to a known public `ps4-linux-loader` commit so builds remain reproducible.

> A successful payload build does **not** mean Windows is bootable yet.

---

## 💿 Current USB test layout

Current development uses Windows boot files supplied by the tester from their own legitimate Windows installation/media:

```text
EFI/
└── Microsoft/
    └── Boot/
        ├── bootmgfw.efi
        ├── BCD
        ├── bootmgr.efi
        ├── memtest.efi
        ├── Fonts/
        ├── Resources/
        └── ...
```

On PS4 this is currently reached under:

```text
/mnt/usb0/EFI/Microsoft/Boot/
```

Microsoft files are **not** included in this repository.

---

## 💿 Windows media policy

This repository will **never redistribute a modified Windows ISO**.

A future builder may accept the user's own legitimate Windows 11 / Windows Server 2025 media and locally inject this project's open-source drivers, ACPI/platform data, and setup configuration.

Microsoft files stay outside this repository.

---

## 🧠 Engineering approach

The project deliberately separates the difficult layers:

1. **PS4 payload execution and hardware handoff**
2. **PE/COFF loading**
3. **UEFI compatibility**
4. **Microsoft boot chain**
5. **Windows platform drivers**
6. **Liverpool graphics / WDDM**

Every stage has a concrete real-hardware success condition so failures can be attributed to a specific layer.

Detailed architecture: [`docs/BOOT_FLOW.md`](docs/BOOT_FLOW.md).

---

## ⚠️ Experimental status

This project is early-stage low-level boot research. Expect crashes, hangs, incomplete hardware support and frequent binary/interface changes between stages. Use dedicated external test media and keep backups of anything important.

---

## 🤝 Contributions

Useful areas include Baikal PCI/device mapping, PS4 interrupt/timer research, ACPI generation, UEFI services, USB/storage bring-up, Windows Driver Kit development, Liverpool display/GPU research and reproducible hardware logs.

Please keep proprietary Sony/Microsoft code and leaked material out of the project.

---

## 📜 License

MIT — see [`LICENSE`](LICENSE).

---

<div align="center">

### PS4 Windows Loader

**From a PS4 payload to Windows Boot Manager — one verified milestone at a time.**

</div>
