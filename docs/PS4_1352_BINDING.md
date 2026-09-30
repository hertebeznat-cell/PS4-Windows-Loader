# Firmware 13.52 memory binding audit

Status: **UNVERIFIED / REFUSED**, not a hardware-ready allocator.
Audit baseline: project `59b0f8a9d20f6b4707e88658fb6fe14fd68d41d3`.
There is no allocator-ready console payload. The [Anchor-Probe hardware result](ANCHOR_PROBE.md#photographed-hardware-result--2026-09-30) records LSTAR and a successful return,
without confirming memory symbols. Do not run Native-Core, repeat the completed
probes, or disable Stage 4.8 preflight to test this work.

## Sources and confidence

The pinned runtime is
[`ps4-linux-loader@f70f43a`](https://github.com/ps4-linux/ps4-linux-loader/tree/f70f43a60a973b3662daeb29c1f115d95408db93).
Its [`kernel.h`](https://github.com/ps4-linux/ps4-linux-loader/blob/f70f43a60a973b3662daeb29c1f115d95408db93/linux/ps4-kexec-common/kernel.h)
defines the ABI, not the identity of functions in the installed Sony kernel.

| Symbol / anchor | Upstream 13.52 candidate offset | Interpretation |
| --- | ---: | --- |
| Xfast syscall / IA32_LSTAR | `0x1C0` | Subtract from live LSTAR to propose kernel base; not physical CR3 |
| printf | `0x2E0510` | Runtime derives base again from supplied early_printf |
| kmem_alloc_contig | `0x24D4F0` | Function, returns original allocation KVA |
| kmem_free | `0x466460` | Function, accepts original KVA and size, returns void |
| pmap_extract | `0x573D0` | Function, translates KVA to PA in supplied pmap |
| kernel_map | `0x22D1D50` | Address of a pointer variable: dereference once |
| kernel_pmap_store | `0x1B2C3A0` | Address of the pmap object: do not dereference as a pointer variable |

[`magic.h`](https://github.com/ps4-linux/ps4-linux-loader/blob/f70f43a60a973b3662daeb29c1f115d95408db93/linux/magic.h)
labels the block `PS4_13_52 //ArabPixel from 12.02`.
The introducing commit
[`9acef9f`](https://github.com/ps4-linux/ps4-linux-loader/commit/9acef9fbf79097a2bb39d6c9c17228198bc445cc)
adds constants, not same-build function bytes/signatures or a returning allocation
trace. This does not establish that every constant is wrong; it does not supply
the evidence needed to call them on this target.

Additional public sources checked:

- [SLOPOS 13.52 table at 70ed72a](https://github.com/alferdoss/SLOPOS-offsets/blob/70ed72abb7392ea435f90617f597670c428c478e/ps4/1352.h)
  agrees on the five memory symbols, but its
  [README](https://github.com/alferdoss/SLOPOS-offsets/blob/70ed72abb7392ea435f90617f597670c428c478e/README.md)
  explicitly identifies ps4-linux-loader as their source. It is a copy, not
  independent verification.
- [Scene-Collective 13.52 source](https://github.com/Scene-Collective/ps4-hen/blob/2beb4cfcef1d416a32d6fb7b35f01189e9eb62e2/kpayload/source/offsets/1352.c)
  corroborates some surrounding offsets (LSTAR anchor, printf), but does not
  provide the three allocator/extractor functions and two map symbols as a
  verified set. Agreement on printf cannot validate a different function.

No full binding confirmation was found in these sources. This is a bounded
source audit, not a claim that no such evidence exists anywhere.

## ABI, address spaces and lifetime

The pinned ABI uses `unsigned long long` for `vm_offset_t`, `vm_size_t` and
`vm_paddr_t`, 64-bit `unsigned long` for alignment/boundary, `int` flags,
and `char` for `vm_memattr_t`. The backend now matches these C function types
exactly; `uint64_t` can be `unsigned long` on a host even though both are 64-bit.
`tests/test_ps4_runtime_abi.c` checks function-pointer compatibility directly
against the pinned header, including the one dereference of `kernel_map`.
These are SysV kernel calls, not Microsoft EFI `ms_abi` calls.

Pinned `freebsd-headers/sys/malloc.h` defines `M_NOWAIT=1`, `M_ZERO=0x100`;
`machine/vm.h` and `specialreg.h` define WB/default as `6`. The native request
uses `0x101`, 16 KiB size/alignment, boundary zero, and physical bounds
`[0x100000, 1<<47)`. Excluding the first MiB is a policy for future tests,
not a discovered PS4 RAM map. Every 4 KiB subpage's start and last byte must
translate consecutively; zero, low, unaligned, discontinuous and overflowing
PA results are rejected and the original allocation is freed.

Upstream `kernel_alloc_contig` loops with `M_WAITOK|M_ZERO`, then replaces the
returned KVA with `PA_TO_DM(pmap_extract(...))`. Its free helper forwards the
supplied address to `kmem_free`. The native backend does not use either helper:
it retains the original KVA, map and rounded allocation size until cleanup.
The PA and any future identity address are never substituted for that KVA.
Both the pinned declarations and
[FreeBSD 9 kmem_free](https://github.com/freebsd/freebsd-src/blob/stable/9/sys/vm/vm_kern.c)
have a **void** return. The API cannot report a fictitious kernel free errno.

M_NOWAIT means one call from this backend, not a bounded, lock-free operation.
[FreeBSD 9 kmem_alloc_contig](https://github.com/freebsd/freebsd-src/blob/stable/9/sys/vm/vm_contig.c)
takes VM locks, can retry internally and wires the mapping. This explains why
an allocator-capable thread context must be established separately; it is not
proof of the exact 13.52 implementation. The allocation must remain wired/owned
through preparation and target use. Release is legal only in the original
Orbis preparation context, never after switching away or at ExitBootServices.

## Base discovery and callback are separate contracts

`main-aio.c` proposes a KASLR base from `RDMSR(IA32_LSTAR) - 0x1C0`, then writes
kernel allocation/pstate patches and calls `kmem_alloc` before entering kexec.
`kernel.c` subtracts the candidate printf offset from early_printf, checks
16 KiB base alignment, reads direct-map indices and resolves base+offset under
NO_SYMTAB. An aligned address is not symbol identity verification. Calling
`kernel_init` to validate candidates would already access/call those candidates.
None of these paths is used by the new binding gate.

`lib/syscalls.py` supplies a syscall-11 stub named kexec; the installed callback
handler comes from the existing console environment. The observed Context-Probe
proved that a no-argument callback could read registers and return. It did not
establish the handler's thread/lock/interrupt contract, allocation capability,
or any symbol/base address. In particular CR3 `0x0B28B000` is a physical page
table root, not the kernel image base. No argument ABI is inferred from a
callback that ignores its arguments.

## Enforced refusal and exact missing evidence

`pwl_ps4_memory_bind` clears output, records the requested firmware and returns
`PWL_ERR_UNSUPPORTED`. No production profiles are enabled, including 13.52.
`pwl_ps4_memory_api_validate` runs before allocation and before release. Filling
the struct with non-null function pointers, an unknown firmware, or host ID 0
does not bypass the production gate. Candidate addresses are documentation
only; the binder neither reads them nor turns them into callable pointers.

Host integration tests alone compile `PWL_PS4_MEMORY_HOST_TEST` and use firmware
ID 0 with synthetic callbacks. The macro is forbidden in a freestanding build;
the production-gate test is compiled without it. This is a build separation,
not a security boundary against arbitrary modifications to the source.

Release refuses a malformed owner or invalid binding without calling free or
erasing ownership. Workspace cleanup propagates that refusal and retains spans
for diagnosis/retry. Successful cleanup calls the void free once and clears the
owner. A trap/panic/non-return in the real kernel is not recoverable by this C
API; host tests do not simulate it as a normal error return.

Before enabling a profile, the missing inputs are:

1. Exact installed 13.52 kernel build identity and a trustworthy KASLR anchor
   with its derivation, not just the displayed firmware version or old CR3.
2. Same-build evidence identifying **all five** memory symbols above: reviewed
   disassembly/call sites or equivalent authoritative symbol evidence, including
   prototype/flag semantics, map indirection and wired-allocation lifetime.
3. Expected immutable bytes/fingerprints and a justified, fault-contained read
   mechanism to check the running image before converting any address to a call.
   Guessed prologues or unchecked reads at candidates are not sufficient.
4. The actual installed syscall-11 callback handler contract: stack/ABI,
   current thread, interrupt and lock state, resident code/data, and permission
   to use VM locks/allocate/free and return without kernel patches.

After those inputs are reviewed, a separate minimal payload can allocate e.g.
32 KiB, verify PA continuity, release the original KVA and return to CPL3.
Its build ID, binding result, allocation/PA check, free-return and user-return
must be visible in notifications even without USB logging. That payload is
**not built yet**; no CR3 switch, EFI entry, device changes or low-memory writes
are authorized by a passing memory test. Windows boot remains a later milestone.


## Next observations

A separate [Anchor-Probe](ANCHOR_PROBE.md) now collects live LSTAR, callback
entry flags/alignment and user-process kernel version strings. Its result is
not a profile approval: no candidate address is read or called, no kernel base
is derived, and the handler/VM contract and five symbols remain unverified.
The previously described allocation/free payload is still not built.


## Installed-kernel dump evidence — 2026-09-30

A privately supplied returning-dumper capture has 42,158,832 bytes and SHA-256
`566b5d65a4d94d2392fa85e6bb573478fb9cbc844a807e5eba18ac3793dc0116`.
It contains ELF64 little-endian AMD64 metadata and the exact version string
`r228995/release_branches/release_13.520 Jun 11 2026 05:25:24`.
Its lowest PT_LOAD VA is `0xFFFFFFFF8433C000`, agreeing with the photographed
LSTAR minus 0x1C0. Its highest PT_LOAD end minus base equals the capture length.
This file is a **memory-layout capture**: byte offset equals VA minus base.
The second PT_LOAD's original ELF file offset is 0xD20000 but its memory offset
is 0x1520000. Do not use ordinary ELF file-offset mapping for disassembly or data.
No dump or runtime page-table contents are committed to the repository.

| Candidate | Observed same-build evidence | Assessment |
| --- | --- | --- |
| kmem_alloc_contig +0x24D4F0 | Eight-argument wrapper inserts domain zero and calls +0x24D520; body rounds size to 16 KiB, calls physical page allocation +0x2D66A0, references vm_contig.c at +0x7BE00E and compares map with the pointer at +0x22D1D50 | Strong static identification; no allocation executed |
| kmem_free +0x466460 | Aligns start down and end (KVA+size) up to 16 KiB, tail-jumps to +0x3005B0 | Consistent three-argument VM removal wrapper; no free executed |
| pmap_extract +0x573D0 | Locks supplied object, loads page-table root from object+0x20, walks present/large-page entries, returns physical address or zero, unlocks; references amd64/pmap.c at +0x783B43 | Strong static identification; lock/context contract remains |
| kernel_map +0x22D1D50 | Pointer variable loaded/compared by contiguous allocator at +0x24D606/+0x24D61F; 145 candidate RIP-relative references in executable range | Pointer-variable interpretation corroborated |
| kernel_pmap_store +0x1B2C3A0 | 47 candidate RIP-relative references; initialization at +0x561DB takes object address directly and references nearby page-table globals; object+0x20 holds a canonical root pointer | Object-address interpretation corroborated; full layout not approved |

Reference counts are byte-pattern candidate scans, not independently decoded
instruction counts. Code windows above were separately disassembled with GNU
objdump. This evidence increases confidence in the candidates without proving
all ABI details, live object lifetime, CPU affinity, VM-lock eligibility, or
allocator cleanup. The production binding gate remains **UNSUPPORTED**.

The syscall-11 entry at base+0x1102B70+11*48 contains argument count 2 and handler
`0xFFFFFFFF843896D0`, corroborating the GoldHEN resolver result. Its installed
handler begins with an indirect jump (`FF 26`); treating the original body as
an unmodified FreeBSD handler would be incorrect. Successful dump callbacks
show the SDK argument path worked for copying in this run, not that VM
allocation is permitted under its locks or scheduling context.

Next: trace the installed syscall-11 handler and its indirection, determine
thread/lock/preemption constraints, then design a returning one-page allocation,
translation and release diagnostic with independent error reporting. Do not
switch CR3, stop APs, or enter Microsoft code during that diagnostic.


## Guarded returning experiment

The exact installed handler FF 26 instruction tail-dispatches through the first
argument at RSI; there is no additional indirection to a separate GoldHEN code
trampoline. The candidate mutex entry +0x378A80 obtains current thread from GS:0,
rejects a nonzero DWORD at thread+0x128, and increments a WORD at thread+0xFC.
The experimental [Memory-Probe](MEMORY_PROBE.md) conservatively checks both
counters are zero, callback TD matches GS:0, CPL/IF/DF, observed version/code,
map/root pointer shape and user-payload mlock before attempting one page.
These guards narrow the experiment; they do not approve the production binding
or prove the full scheduler/VM contract. No hardware result is recorded yet.
