<div align="center">

# 🪟 PS4 Windows Loader

### Bare-metal Windows boot research for PlayStation 4

**Target:** PS4 Slim CUH-2208B · Baikal · x86-64  
**Goal:** Windows 11 / Windows Server 2025 directly on PS4 hardware — **no Linux host, no QEMU, no virtualization**.

![Stage](https://img.shields.io/badge/stage-3.6%20final%20pre--entry%20preflight-blueviolet)
![Hardware](https://img.shields.io/badge/real%20hardware-verified-success)
![Target](https://img.shields.io/badge/target-PS4%20Baikal-blue)
![Architecture](https://img.shields.io/badge/arch-x86__64-lightgrey)
![CI](https://img.shields.io/github/actions/workflow/status/hertebeznat-cell/PS4-Windows-Loader/ci.yml?label=latest%20payload)
![License](https://img.shields.io/badge/license-MIT-green)

</div>

---

## 🚀 Project

**PS4 Windows Loader** is an experimental open-source project building a native Windows boot path for PlayStation 4 hardware.

```text
PS4 jailbreak / payload
        ↓
PS4 Windows Loader
        ↓
UEFI-compatible firmware layer
        ↓
Microsoft bootmgfw.efi
        ↓
winload.efi
        ↓
Windows kernel
        ↓
PS4-specific Windows drivers
```

This is **not** Windows inside Linux and **not** a virtual machine.

The repository does not redistribute Microsoft Windows files, Sony firmware dumps, product keys, leaked material, or proprietary drivers.

---

## 🎮 Initial hardware target

| Component | Target |
|---|---|
| Console | **PS4 Slim CUH-2208B** |
| Southbridge | **Baikal** |
| CPU | AMD Jaguar x86-64 |
| GPU | AMD Liverpool |
| Test firmware | **13.52** |
| Boot media | External USB first |
| Windows target | Windows 11 / Windows Server 2025 |

Early tests intentionally use an external USB device. The PS4 internal system drive should remain untouched while the boot path is experimental.

---

# ✅ Real-hardware milestones

All checked milestones below were physically tested on **PS4 Slim CUH-2208B / Baikal / firmware 13.52**.

## Stage 1 — payload execution ✅

Confirmed:

- freestanding PS4 payload execution;
- PS4 notifications;
- access to Orbis USB mounts;
- Windows EFI file discovery.

## Stage 2.5 — PE32+ EFI loader ✅

Confirmed:

- open `/mnt/usb0/EFI/Microsoft/Boot/bootmgfw.efi`;
- stream-read the complete file;
- AMD64 / PE32+ / EFI Application validation;
- PE section mapping;
- x64 `IMAGE_REL_BASED_DIR64` relocations;
- EFI entry-point calculation.

```text
PS4 Windows Loader: Stage 2.5 started
Stage 2.5: direct usb0 open OK
Stage 2.5: stream read to EOF OK
Stage 2.5: PE32+ EFI validation OK
Stage 2.5: sections mapped OK
PS4 Windows Loader: Stage 2.5 PE map + relocations OK
```

## Stage 3 — EFI System Table + Microsoft x64 ABI ✅

Confirmed:

- minimal `EFI_SYSTEM_TABLE`;
- EFI console `OutputString` callback;
- Microsoft x64 / UEFI calling convention (`ms_abi`);
- executable `bootmgfw.efi` image preparation.

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

## Stage 3.1 — core EFI Boot Services ✅

Confirmed:

- `AllocatePool` / `FreePool`;
- `AllocatePages` / `FreePages`;
- `GetMemoryMap` query semantics;
- `HandleProtocol` / `LocateProtocol`;
- Loaded Image protocol;
- Device Path protocol;
- Simple File System publication.

## Stage 3.2 — EFI File Protocol bridge ✅

Confirmed:

- `SimpleFileSystem.OpenVolume`;
- `EFI_FILE_PROTOCOL.Open`;
- `Read`;
- `GetPosition` / `SetPosition`;
- `Close`;
- opening `\\EFI\\Microsoft\\Boot\\bootmgfw.efi` through the UEFI-style filesystem bridge;
- reading the `MZ` signature through that bridge.

## Stage 3.3 — real Windows BCD through EFI ✅

Confirmed:

- open `\\EFI\\Microsoft\\Boot\\BCD`;
- read through `EFI_FILE_PROTOCOL`;
- validate the BCD registry-hive `regf` header;
- seek / rewind;
- clean close.

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

## Stage 3.4 — pre-Boot-Manager services ✅

Confirmed:

- `EFI_FILE_PROTOCOL.GetInfo`;
- `EFI_FILE_INFO` file size / filename metadata;
- `OpenProtocol`;
- `LocateHandleBuffer`;
- `ProtocolsPerHandle`;
- `CalculateCrc32`;
- `CopyMem` / `SetMem`;
- `Stall`.

```text
PS4 Windows Loader: Stage 3.4 started
Stage 3.4: metadata + protocol services installed
Stage 3.4: BCD opened through EFI File Protocol
Stage 3.4: EFI_FILE_INFO size/name metadata OK
Stage 3.4: OpenProtocol(SimpleFS) OK
Stage 3.4: LocateHandleBuffer(SimpleFS) OK
Stage 3.4: ProtocolsPerHandle(device) OK
Stage 3.4: CRC/CopyMem/SetMem/Stall OK
PS4 Windows Loader: Stage 3.4 pre-bootmgr services self-test OK
```

## Stage 3.5 — event/runtime/MapKey firmware layer ✅

Stage 3.5 is **hardware-verified**.

Confirmed:

- `RaiseTPL` / `RestoreTPL`;
- `CreateEvent` / `SignalEvent` / `WaitForEvent` / `CheckEvent` / `CloseEvent`;
- `SetTimer` and `CreateEventEx` slots;
- `GetNextMonotonicCount`;
- `SetWatchdogTimer`;
- ordered `EFI_RUNTIME_SERVICES` table publication;
- UEFI variable fallback semantics;
- System Table / Boot Services / Runtime Services CRC refresh;
- `GetMemoryMap` MapKey changes after allocation;
- `ExitBootServices` MapKey validation.

```text
PS4 Windows Loader: Stage 3.5 started
Stage 3.5: event + Runtime Services installed
Stage 3.5: RuntimeServices table published
Stage 3.5: Create/Signal/Wait/Check/CloseEvent OK
Stage 3.5: Runtime variable fallback semantics OK
Stage 3.5: GetMemoryMap MapKey tracking OK
Stage 3.5: ExitBootServices MapKey validation OK
PS4 Windows Loader: Stage 3.5 pre-entry firmware self-test OK
```

## Stage 3.6 — final pre-entry Boot Manager preflight 🚧

Current development stage. Stage 3.6 intentionally still **does not execute Microsoft `bootmgfw.efi`**.

The final dry-run adds and tests:

- tracked EFI page/pool allocations in `GetMemoryMap`;
- a larger conventional-memory arena for firmware testing;
- `LocateHandle`;
- `LocateDevicePath`;
- tracked `LocateHandleBuffer` / `ProtocolsPerHandle` allocations;
- `OpenProtocolInformation` fallback semantics;
- `RegisterProtocolNotify` plumbing;
- `InstallConfigurationTable` add/remove semantics;
- non-null typed fallbacks for additional Boot Services such as protocol installation, image loading and controller services;
- corrected PE section-copy semantics without clamping raw data to `VirtualSize`;
- loading `bootmgfw.efi` into an EFI LoaderCode allocation represented in the memory map;
- complete `EFI_LOADED_IMAGE_PROTOCOL` image base/size metadata;
- a real Media FilePath device-path node for `\\EFI\\Microsoft\\Boot\\bootmgfw.efi`;
- final calculation of the Microsoft EFI entry address.

If Stage 3.6 passes on hardware, the next milestone is the **first controlled call into Microsoft Boot Manager code**.

---

## 🧭 Current boot status

```text
PS4 payload execution                     ✅
        ↓
Read bootmgfw.efi from USB                ✅
        ↓
Validate / map / relocate PE32+           ✅
        ↓
EFI System Table + Microsoft x64 ABI      ✅
        ↓
Core Boot Services                        ✅
        ↓
LoadedImage / DevicePath / SimpleFS       ✅
        ↓
EFI File Protocol                         ✅
        ↓
Read Windows BCD through EFI              ✅
        ↓
Metadata / protocol helpers               ✅
        ↓
Runtime / events / MapKey hardening       ✅
        ↓
Final Boot Manager entry preflight        🚧
        ↓
First controlled bootmgfw.efi entry       ⏳
        ↓
Windows Boot Manager                      ⏳
        ↓
winload.efi                               ⏳
        ↓
ntoskrnl.exe                              ⏳
```

---

## 🗺️ Roadmap

### EFI compatibility layer
- [x] System Table
- [x] Microsoft x64 ABI
- [x] PE32+ mapper and relocations
- [x] memory allocation services
- [x] initial memory map + MapKey
- [x] protocol lookup
- [x] Loaded Image / Device Path / SimpleFS
- [x] EFI File Protocol
- [x] BCD access
- [x] File `GetInfo`
- [x] `OpenProtocol`
- [x] `LocateHandleBuffer`
- [x] `ProtocolsPerHandle`
- [x] CRC / memory helpers / Stall
- [x] events / timers baseline
- [x] Runtime Services baseline
- [x] `ExitBootServices` MapKey validation
- [ ] Stage 3.6 hardware verification
- [ ] directory enumeration as required
- [ ] fuller timer semantics as required
- [ ] real platform/physical memory description for Windows handoff
- [ ] additional protocols requested by Boot Manager

### Windows Boot Manager
- [ ] first controlled call to `bootmgfw.efi`
- [ ] trace EFI service/protocol calls
- [ ] implement missing dependencies as encountered
- [ ] Boot Manager consumes BCD
- [ ] reach `winload.efi`

### Windows kernel
- [ ] ACPI platform description
- [ ] APIC / interrupts
- [ ] timers
- [ ] PCI enumeration
- [ ] reach `ntoskrnl.exe`

### Drivers / usable system
- [ ] USB/storage
- [ ] keyboard/mouse
- [ ] framebuffer display
- [ ] network
- [ ] audio
- [ ] Bluetooth / DualShock
- [ ] power management
- [ ] Liverpool WDDM / DirectX

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
│   ├── stage3_3.c
│   ├── stage3_4.c         # hardware verified
│   ├── stage3_5.c         # hardware verified
│   └── stage3_6.c         # current development payload
├── loader/
│   ├── include/
│   └── src/
├── drivers/
├── docs/
│   └── BOOT_FLOW.md
├── setup/
└── .github/workflows/
    └── ci.yml
```

---

## 🔨 CI builds

Every push builds the current payload. Each run exposes one test artifact:

```text
PS4-Windows-Loader-Latest
```

Contents:

```text
PS4WindowsLoader-latest.bin
PS4WindowsLoader-latest.elf
SHA256SUMS.txt
PS4_RUNTIME_COMMIT.txt
STAGE.txt
```

`STAGE.txt` identifies the current source stage.

---

## 💿 USB test layout

The tester supplies Windows boot files from their own legitimate Windows installation/media:

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

On the current PS4 test system this is mounted at:

```text
/mnt/usb0/EFI/Microsoft/Boot/
```

Microsoft files are not included in this repository.

---

## ⚠️ Status

This is early-stage firmware and boot research. Crashes and hangs are expected. Use dedicated external media and keep the PS4 system drive unchanged.

---

## 📜 License

MIT — see [`LICENSE`](LICENSE).

<div align="center">

**From a PS4 payload to Windows Boot Manager — one verified milestone at a time.**

</div>
