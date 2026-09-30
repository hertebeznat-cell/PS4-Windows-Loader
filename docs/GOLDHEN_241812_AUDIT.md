# User-supplied GoldHEN v2.4b18.12: initial static audit

Status: callback and memory binding remain unverified. No console run requested.

## Input identity

The user supplied `GoldHEN_v2.4b18.12.7z`, referring to
[SiSTRo's release page](https://ko-fi.com/s/85ea22bd2f).
This establishes provenance reported by the user; no publisher signature or
independently published digest was verified.

- Archive SHA-256: `81d83c7f99adee484b7be570d6782831cffa4798858a2d00b6aa9b751eea0bc8`.
- Extracted `goldhen.bin`: 293120 bytes.
- Binary SHA-256: `df3f27c1b35bc7c40e3a08caab948930914dc7d0301a73b68945cf6ffe40ea12`.
- Other entries: README.md, CHANGELOG.md, CHEATMENU.md, LICENSES.txt, THANKS.md.
- No source files or symbol map were included.

The supplied README lists firmware 13.52 among supported targets and says the
project source is private. Its changelog attributes 13.52 support to v2.4b18.11;
v2.4b18.12 adds 13.02, 13.04 and 13.50. Compatibility is a release claim, not
confirmation of this project's memory symbols or same-build kernel bytes.
The proprietary binary is not copied into this repository.

## Reproducible static observations

Offsets below are **file offsets in the supplied binary**, not kernel offsets
or console addresses. Disassembly used GNU objdump, raw x86-64 mode. The entry
at file offset zero jumps to `0xE3B`. No uploaded code was executed.

| File offset | Observation | Limit |
| --- | --- | --- |
| `0xE74`–`0xE8F` | Reads MSR `0xC0000082`, combines EDX:EAX, subtracts `0x1C0`, returns | Confirms this binary uses the candidate arithmetic, not the identity of live entry code |
| `0x1961` | Compares firmware selector to `0x548` (1352 decimal) | Establishes a 13.52 selector branch |
| `0x196A` | Relative call targets `0x3840` | Links selector to this binary's table construction routine |
| `0x3844` | Packs DWORDs `0x1C0` and `0x2E0510`, stored to the output table | Corroborates syscall anchor / printf candidates; function roles also require use-site review |
| `0x3A17`, stored at `0x3A43` | Packs DWORD `0x22D1D50` into table offset `0xF8` | Candidate value exists in the selected table; not an arbitrary byte match alone |

The LSTAR routine's exact 28 bytes and the 13.52 compare/call target were checked
against the extracted bytes, independently of display formatting. These facts
support the [photographed Anchor-Probe result](ANCHOR_PROBE.md), without enabling
the production binding.

A literal little-endian DWORD search did not find the existing candidates
`0x24D4F0`, `0x466460`, `0x573D0` or `0x1B2C3A0` in the file. This is **not** proof
that they are absent from the implementation: constants can be represented by
arithmetic, different encodings or embedded/transformed content. No replacement
symbol identities are inferred from anonymous table slots.

## Remaining analysis

The observed syscall-11 invocation in the installer is a caller, not sufficient
identification of the handler used by our diagnostic. Trace handler installation,
all relevant table use sites and the actual callback path before asserting
argument ABI, thread state, VM lock state or fault recovery. Independently
identify all five memory symbols for the observed installed kernel build.

The uploaded file removes the uncertainty about the artifact to inspect. It
does not establish that its bytes match the live installed image, nor supply a
kernel dump. No kernel reads, allocations, CPU-control writes or Windows entry
were attempted as part of this offline analysis.

## Internal image extraction and syscall registration

Further offline review identified a zlib stream at outer file offset `0x6900`,
length 264595 (length DWORD at `0x47294`). Python's zlib decoder verified the
stream, producing 636760 bytes. Internal SHA-256:
`4e2864fb568707470bbc0dcf28ade7ed2096c3ea42ea87dd5ff13613db91abee`.

`tools/inspect_goldhen.py goldhen.bin --extract-to inner.bin` reproduces this
extraction for the exact reviewed input only. It refuses different hashes,
invalid streams and existing output files. No payload code is executed.
The proprietary output is not committed. Extraction and modified-input refusal
were checked locally.

Offsets in the following observations refer to the **decompressed image**:

- Initialization at `0x25F8` calls syscall registration at `0x244E`.
- Registration loads the handler from internal global `0x9B8A0`, sets syscall
  number 11 and argument count 2, and calls the entry writer at `0x2410`.
- The entry writer uses a 48-byte stride from global `0x9B8C0`, zeroes the entry,
  stores argument count at +0, handler pointer at +8 and DWORD 1 at +44.
- The resolver at `0x12EB3` loads a table DWORD at stack offset `0x124`, adds
  the proposed kernel base and stores it to `0x9B8A0`. Thus the registered
  syscall-11 target is resolved from a kernel-offset table; registration itself
  does not supply the target function body.

This narrows the next task to the selected table value, its exact role and the
installed kernel handler implementation. The entry writer's CR0 writes are
observations of GoldHEN code, not operations added to our loader. No allocation
payload is authorized by this analysis, and the production binding stays refused.

## Resolved 13.52 syscall-11 target

The resolver at `0x12A22` passes `RSP+0xC` to selector `0x64D8`. That selector's
1352 branch at `0x6726` calls table builder `0xB377`. This is distinct from the
patch table builder at `0xB5F7`; conflating them gives incorrect field identities.

| Role established by use site | Table field | Kernel-relative value |
| --- | --- | --- |
| Syscall-11 target stored to `0x9B8A0` | `0x118` (resolver stack `0x124`) | `0x4D6D0` |
| Syscall table used by 48-byte entry writer | `0x108` (resolver stack `0x114`) | `0x1102B70` |

At `0xB592`, `movabs` loads `0x026473500004D6D0` into RDX; the store at `0xB5BE`
writes it to table field `0x118`. The resolver consumes the low DWORD. At
`0xB569`, RSI receives `0x026542C001102B70`, stored to field `0x108` at `0xB59C`.
The offline inspector now reports these values from their immediate bytes.

Combining the photographed LSTAR with GoldHEN's base arithmetic yields candidate
base `0xFFFFFFFF8433C000`, target `0xFFFFFFFF843896D0`, and syscall table
`0xFFFFFFFF8543EB70`. These are **derived candidates, not live address checks**.
No new console reads or calls were made.

The installation path resolves the handler into the external kernel image.
Following that pointer cannot be completed from the uploaded GoldHEN image alone:
the pointed-to kernel code has not been supplied or observed. The upload
therefore identifies which kernel-relative handler to investigate, but does not
prove its callback argument behavior, thread/lock contract or allocation safety.
Next evidence must include independently verifiable code for that exact installed
kernel target (and memory symbols), rather than another register-only probe.
