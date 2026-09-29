# Firmware 13.52 memory binding audit

Status: **UNVERIFIED / REFUSED**, not a hardware-ready allocator.
Audit baseline: project `59b0f8a9d20f6b4707e88658fb6fe14fd68d41d3`.
There is no new console payload. Do not run Native-Core, repeat Context-Probe,
or disable Stage 4.8 preflight to test this work.

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
