# Experimental returning kernel dump for firmware 13.52

This is a new diagnostic, not a Windows launcher or allocator test. Hardware
behavior is untested. Calling the external SDK's candidate copyout address can
still crash: checking its return cannot recover a wrong function pointer.
The installed syscall-11 argument contract is assumed from the existing SDK;
Anchor-Probe verified only a callback that ignored arguments. No verified memory
binding or exception containment is claimed by this artifact. Callback residency
and VM-lock compatibility on this GoldHEN build are also unverified.

SDK source pinned to Scene-Collective/ps4-payload-sdk commit
`b7326416c23ce14639e9180a24093bdc5eedb579`. The SDK supplies LSTAR minus `0x1C0`
and firmware-13.52 copyout offset `0x2BD6A0`. These are external candidates.
This implementation does not call jailbreak(), change credentials/control
registers, allocate kernel memory or enter Windows. Only the SDK base/dump
callbacks are used; their base-copy error propagation is repaired at build time.

## What changed from the upstream dumper

- Reject unsupported firmware, absent HEN, invalid base, failed mapping/open.
- Read a bounded ELF header window; validate ELF64 little endian AMD64 metadata,
  complete program-header table, load ranges/overflow and power-of-two alignment.
- Cap the dump at 64 MiB. Recognize only live base or known legacy link base.
  Reject other formats instead of reading an arbitrary extent.
- Stop on every nonzero memory-read result. Clear the destination before reads.
- Loop on short USB writes; stop on zero/negative or impossible results.
- Check fsync, close and rename; retain `.partial` after a failure.
- Refuse an existing final dump; open the partial exclusively. Use a single
  dumper instance and do not create/replace those paths concurrently.
- No success marker is written independently of the data. The final filename
  appears only after complete transfer, sync and close.

The host test covers ELF truncation, wrong magic, excessive header count,
invalid alignment, oversized extent, address overflow and an unrelated base.
This is parser testing, not a hardware validation of copyout or USB durability.

## One console test, once the artifact is supplied

Use a separate USB drive with at least 100 MiB free, accessible as `/mnt/usb0`.
Run from the same GoldHEN v2.4b18.12 environment used for Anchor-Probe. Send
only `PS4WindowsLoader-Kernel-Dump.bin` using your existing payload sender.
Do not send the relocatable Native-Core object or disable Stage 4.8 preflight.

On success: `PS4WL Dump: complete, ... bytes; PS4WL_KERNEL.bin`. Wait for the
payload to return before disconnecting USB. Send `PS4WL_KERNEL.bin` privately
for analysis together with the build notification. It may contain sensitive
runtime data; do not commit or publish the dump in GitHub.

On failure, photograph the error and retain `PS4WL_KERNEL.partial`. Do not
retry immediately or substitute offsets. A header rejection can mean that this
kernel does not expose the expected in-memory ELF layout; it is not proof that
the existing kernel memory binding is correct or wrong.

A separate USB dump file's existence is not validation of all five allocator
symbols. Review contents and function/data identities before enabling calls.

## Reproduce

```
git clone https://github.com/Scene-Collective/ps4-payload-sdk.git dumper-sdk
git -C dumper-sdk checkout b7326416c23ce14639e9180a24093bdc5eedb579
sh tools/build_kernel_dump.sh dumper-sdk project-commit-id
```
