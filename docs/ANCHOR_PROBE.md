# Returning syscall-anchor observation

This is a new, separately named console diagnostic. It observes LSTAR and
callback-entry flags/alignment, and queries kernel version strings from the
user process. It does not confirm the five allocator symbols or start Windows.
Do not repeat Context-Probe or send Native-Core for this test.

## Run and collect

1. Download the successful CI artifact `PS4-Windows-Loader-Anchor-Probe`.
2. Use the same payload sender/environment that successfully ran Context-Probe.
   Send only `PS4WindowsLoader-Anchor-Probe.bin` once. Keep the notification
   display visible and preferably record a video so all metadata chunks survive.
3. Collect the build notification; every `kern.osrelease` / `kern.version`
   chunk or the metadata-unavailable message; callback return code, errno and
   state; LSTAR; RFLAGS; entry RSP modulo 16; CS; and the final result.
4. If accessible, also collect `/mnt/usb0/PS4WL_ANCHOR.LOG` or the USB1 equivalent.
   Missing/failed USB logging does not prevent screen results or the callback.

The source prints the full commit ID. `COMMIT.txt`, the pinned runtime commit,
callback disassembly and binary hashes are included in the artifact.
Notifications must resolve before any callback attempt. Code and output data
must pass `mlock`; failures stop and appear on screen. There is one callback
attempt and no allocator, address dereference, CPU-control write or device change.
No exception recovery is installed; the test still depends on the existing
callback facility and resident executable/output pages, as the prior probe did.

## What the observations mean

The AMD architectural LSTAR MSR (`0xC0000082`) holds the 64-bit SYSCALL target.
The callback first checks CPL0, then the CPUID SYSCALL feature, before RDMSR.
It captures entry RFLAGS before changing arithmetic flags and records only
RSP modulo 16, not stack contents. CPUID's callee-saved RBX is preserved.
The assembly returns zero and stores a completion marker. The user process
requires successful transport, CPL0 observation and a return to CPL3.

The probe deliberately does not subtract the candidate `0x1C0`, dereference
LSTAR or resolve/call any kernel symbol. A high canonical LSTAR is an observation,
not proof of the image base or an unmodified entry handler. Kernel metadata may
be unavailable, generic or altered; it is supporting evidence, not a unique
same-build fingerprint. RFLAGS.IF and stack alignment do not establish thread,
lock, callback argument or VM allocation contracts.

Review the returned address against independently verified same-build entry
code and obtain the installed handler's source/build identity before proposing
base arithmetic or reads. The absence of that evidence keeps the production
memory binding refused. See [binding audit](PS4_1352_BINDING.md).

## Build and checks

```
make -C /path/to/pinned-runtime/lib
sh tools/build_anchor_probe.sh /path/to/pinned-runtime full-project-commit-id
```

Host mocks cover missing notifications, missing USB, failed USB writes, both
mlock failures, unsupported/missing callback, actual CPL3 refusal, synthetic
screen reporting, and cleanup. Host tests never execute RDMSR. Link validation
checks RIP-relative callback addresses and inclusion of zero BSS in the raw
binary. CI builds the console payload with the pinned runtime. The first photographed hardware result is recorded below.

Sources:
- [AMD64 Architecture Programmer's Manual, Volume 2](https://docs.amd.com/v/u/en-US/24593_3.44_APM_Vol2), SYSCALL/SYSRET and LSTAR.
- [AMD CPUID specification](https://www.amd.com/content/dam/amd/en/documents/archived-tech-docs/design-guides/25481.pdf), extended feature EDX bit 11.
- [Pinned runtime syscall wrapper](https://github.com/ps4-linux/ps4-linux-loader/blob/f70f43a60a973b3662daeb29c1f115d95408db93/lib/syscalls.py), syscall 11; this wrapper does not provide the installed handler implementation.

## Photographed hardware result — 2026-09-30

Target reported by the user: PS4 Slim CUH-2208B, firmware 13.52.
Evidence: screen notifications supplied by the user, not a recovered USB log.
Payload build: `22716362725afd9b08978d51d87aa65c4ef0742e`.
[Successful build](https://github.com/hertebeznat-cell/PS4-Windows-Loader/actions/runs/36669956023).

| Observation | Photographed value |
| --- | --- |
| kern.osrelease | `0.0-prototype` |
| kern.version | `r228995/release_branches/release_13.520 Jun 11 2026 05:25:24` |
| Callback return code | `0` |
| Callback errno | `0` |
| Completion state | `2` |
| LSTAR | `0xFFFFFFFF8433C1C0` |
| Entry RFLAGS | `0x0000000000000246` |
| Entry RSP modulo 16 | `0x8` |
| CS | `0x20` |
| Final result | `user return confirmed; memory binding unverified` |
| Completion notification | `finished; photograph notifications` |

The callback completed at CPL0 and the process confirmed return to CPL3.
Entry IF was set and the observed stack alignment is consistent with a SysV
function entry. These observations do not prove VM lock state, allocator safety,
or the implementation of the installed syscall handler. No memory function
was called. No Windows image was entered. USB log persistence is not established
by these photographs. This diagnostic does not need repeating.

Subtracting the upstream candidate `0x1C0` gives the arithmetic candidate
`0xFFFFFFFF8433C000`. It remains **unverified**: alignment and agreement with a
candidate offset are not proof that this is the installed kernel image base.
Do not dereference it or enable the production binding on this evidence alone.

### Evidence needed before a returning allocation test

1. Identify the installed callback provider and exact version/build (including
   the GoldHEN version and payload source used to install the handler). Match
   its actual implementation to the observed environment; another HEN's source
   alone does not establish this handler's contract.
2. Obtain independently reviewable same-build evidence for the syscall anchor,
   three memory functions, `kernel_map` variable and `kernel_pmap_store` object.
   Firmware labels and copied offset tables alone are insufficient.
3. Establish a fault-contained address-validation method and a VM-capable
   callback/thread contract before reading candidates or calling the allocator.

Once these requirements are satisfied, the next hardware milestone is a small
owned allocation, per-page PA validation, original-KVA release and successful
return. It is not a Windows launch. The production gate remains refused until
its required evidence is supplied and reviewed.
