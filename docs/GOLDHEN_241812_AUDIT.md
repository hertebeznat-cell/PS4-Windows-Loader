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
