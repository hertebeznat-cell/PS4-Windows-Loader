# Returning native CPU call

Implemented in `native_call.c`, `native_call.S` and the Stage 5 entry preparation
transaction. **No PS4 execution of this transition has occurred.** The normal
console entry still performs its old preflight and production memory binding
is still refused. This is an executable returning transition component, not a
complete platform backend or complete Windows boot.

## Call construction

The preparation transaction snapshots the old root, constructs the separate
root, audits the resident image/environment and binds the private image mapping
service. It then constructs an owned call record, aligned FP save buffer and
EFI arguments for the resident StartImage wrapper. The callback is the existing
Microsoft ABI adapter, so all 64 bits of EFI_STATUS remain in the EFI context.
The SysV callback's integer result is a separate transport result. A valid
return report does not mean EFI_STATUS is successful or that Windows booted.

The call preparer verifies identical supervisor physical translations and
permissions for its assembly text, EFI adapter, suspended caller stack, new
stack and complete control allocation under both roots. It checks the control
allocation's actual recorded physical placement and WB/PAT0 encoding, excludes
arena/table overlap, and rejects physical aliasing between code, control and
the two stacks. New table storage is also forbidden from overlapping any old
table frame before construction writes begin. Its checks do not establish effective MTRR cache types or RAM
ownership. The old stack must include save/helper scratch space and the caller's
return address; live RSP bounds are checked immediately before state changes.

Stage 5 means construction succeeded. It is not a readiness certificate.
Failure invalidates the new root and newly installed mapping-service binding;
the output pointer and control bytes remain unchanged. An existing binding is
not overwritten. All preparation inputs/output buffers must be distinct from
the control allocation and mutually valid for their lifetimes.

## State sequence

1. Reject CPL3 before reading the argument or executing CLI/CR/RDMSR.
2. Save caller registers/flags; mask IRQs; compare live CR0/3/4/EFER, XCR0 and
   CPUID save-area size with the prepared record. Refuse DF, unsupported mode,
   missing SSE support, PCID/LA57 and PKE/CET/PKS. Original TS/PGE are allowed.
3. Zero the save area, clear TS, save the complete enabled x87/SSE/AVX state
   with XSAVE64, or FXSAVE64 without OSXSAVE. Initialize x87 and MXCSR for EFI.
4. Clear PGE to invalidate old global translations, install the new CR3, then
   the new stack. Both stack extents must therefore exist under both roots.
5. Call the resident StartImage path through the ABI adapter.
6. On an ordinary return, restore old CR3 and stack, restore FP state and original
   CR4/CR0 including PGE/TS, then caller registers and interrupt flags.

CPUID/XGETBV discovery reads the full enabled host user-state mask. Native entry
currently restricts that mask to x87/SSE (3) or x87/SSE/AVX (7); additional
extensions are not silently discarded. No XCR0 write is used. Save space is
64-byte aligned and bounded at 64 KiB. Reserved XSAVE header bytes are zeroed
before saving. Save/restore primitives are shared with real processor tests.

The callback must return in the same CPU mode with XCR0, descriptors/GS,
ownership and mappings intact, and IRQs masked. This assembly is not exception
recovery and cannot recover if Boot Manager takes ownership permanently.
CLI does not exclude NMI/MCE, stop other CPUs or stop DMA. Platform-specific
exception mappings/handlers, FP ownership, AP coordination, device ownership,
PAT/MTRR evidence and the complete memory inventory remain required.

## Evidence

Native host tests execute a copied RX call body with unreadable inputs and
confirm CPL3 refusal before reads or privileged operations. Shared FP assembly
actually saves, clobbers and restores x87 control/data, MXCSR, XMM0/XMM15 and
enabled YMM state on the host CPU. Both FXSAVE and the real CPUID-sized full-mask
XSAVE path are exercised, without an emulator. Synthetic page-table integration
checks Stage 5 construction, mapping failure rollback, output preservation,
stack alias rejection and result checks. Synthetic result records are not
evidence of a CR3 switch. The build audit rejects imports/relocations/writable
globals and verifies the early privilege guard. No Microsoft code runs in these
tests, and no new console test is requested.

Architecture references: [AMD64 System Programming](https://docs.amd.com/v/u/en-US/24593_3.45_APM_Vol2_PUB),
[AMD64 instruction reference](https://docs.amd.com/v/u/en-US/24594_3.38_APM_Vol3_PUB)
and [Intel software developer manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html),
Volumes 1/2 (XSAVE/XRSTOR, CPUID, XGETBV, FXSAVE/FXRSTOR) and Volume 3
(CR0.TS, CR4.PGE and translation invalidation). The actual target is AMD64;
these common instruction contracts do not replace target platform validation.
