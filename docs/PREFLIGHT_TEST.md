# Stage 4.8 preflight check

This check verifies file loading, PE validation, mapping and relocation on the
PS4 Slim CUH-2208B. It does **not** start Boot Manager or Windows 11/Server
2025. The current build exits before calling Microsoft's EFI entry point.

1. Open the successful [main-branch CI run](https://github.com/hertebeznat-cell/PS4-Windows-Loader/actions/workflows/ci.yml)
   and download its `PS4-Windows-Loader-Latest` artifact. The archive contains
   `PS4WindowsLoader-latest.bin`, `COMMIT.txt`, `STAGE.txt` and `SHA256SUMS.txt`.
   Check that `STAGE.txt` says `stage4_8` and that `COMMIT.txt` matches the
   intended CI run's commit. Verify the binary against `SHA256SUMS.txt` before
   copying it to removable test media.
2. Put your own `bootmgfw.efi` at
   `/mnt/usb0/EFI/Microsoft/Boot/bootmgfw.efi` on the test USB drive. Use the
   same known working payload-launch method as in the prior Stage 4.8 checks.
   Keep the internal system drive out of the test procedure.
3. Save the resulting `/mnt/usb0/PS4WL_STAGE48.LOG` and read it before any
   further test. It should identify the exact `BUILD:` commit, include
   `MODE: PREFLIGHT_ONLY; Boot Manager entry disabled`, and show
   `CPU48: CPUID raw registers begin`, `CPU48: CPUID raw registers end`, and
   `PREFLIGHT48: image mapped; Boot Manager entry disabled`. A clean return
   after those markers confirms only this preflight milestone. Preserve every
   `CPU48:` line: each leaf/subleaf is followed by EAX, EBX, ECX and EDX.
   The values identify processor capabilities reported to this process; they
   do not by themselves prove Windows compatibility or a viable OS handoff.

If the log is missing, the build marker differs, the PE validation fails, or
the console does not return normally, stop this test and preserve the complete
log and the exact artifact. Do not switch to an earlier artifact that calls
the Boot Manager entry point. The previous hardware traces already identify
the two privileged instructions that stop that path in a PS4 user process.
