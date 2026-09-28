# Standalone CPU report

For the first CPU check, use the `PS4-Windows-Loader-CPU-Probe` artifact from a
successful [main CI run](https://github.com/hertebeznat-cell/PS4-Windows-Loader/actions/workflows/ci.yml).
It is a separate PS4 user-process payload: it opens one USB log, runs CPUID,
writes the register values, closes the log and exits. It does not read a Windows
file, allocate an EFI image, or call Microsoft Boot Manager.

1. Compare the artifact's `COMMIT.txt` with the selected CI run and check
   `PS4WindowsLoader-CPU-Probe.bin` against `SHA256SUMS.txt`. From the unpacked
   artifact folder, `sha256sum -c SHA256SUMS.txt` checks both the `.bin` and
   `.elf` files; the manifest lists filenames without CI workspace paths.
2. With the test USB drive mounted at `/mnt/usb0`, load the CPU probe `.bin`
   using the same payload launcher previously used for this PS4. No
   `bootmgfw.efi` or Windows installation media is needed.
3. Save `/mnt/usb0/PS4WL_CPU.LOG` after the payload exits. Confirm the `BUILD:`
   commit, `MODE: CPU_ONLY; no EFI or Boot Manager entry`, both `CPU48: CPUID`
   markers and `CPU probe finished`.
4. In a checkout of this repository, run
   `python3 tools/check_cpu_log.py PS4WL_CPU.LOG` to summarize the Server 2025
   CPU instruction flags. Share the complete log to distinguish the console's
   result from the CI host's CPU test.

This report cannot establish CPU clock, Windows 11 processor eligibility,
physical RAM ownership, TPM, firmware, drivers or Windows bootability. If the
log is absent or incomplete, retain the exact artifact and report that result.
