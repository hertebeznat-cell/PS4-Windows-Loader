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
binary. CI builds the console payload with the pinned runtime. Hardware results
are not known until the user runs this new diagnostic.

Sources:
- [AMD64 Architecture Programmer's Manual, Volume 2](https://docs.amd.com/v/u/en-US/24593_3.44_APM_Vol2), SYSCALL/SYSRET and LSTAR.
- [AMD CPUID specification](https://www.amd.com/content/dam/amd/en/documents/archived-tech-docs/design-guides/25481.pdf), extended feature EDX bit 11.
- [Pinned runtime syscall wrapper](https://github.com/ps4-linux/ps4-linux-loader/blob/f70f43a60a973b3662daeb29c1f115d95408db93/lib/syscalls.py), syscall 11; this wrapper does not provide the installed handler implementation.
