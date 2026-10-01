# Observed boot-time UTC and resident GetTime

Native-Prepare now collects five console `sceKernelGettimeofday` observations,
each bracketed by serialized TSC reads and APIC identity checks, spaced by
10 ms waits. Actual CPUID invariant-TSC support is required. Calibration
rejects CPU migration, reversed/non-increasing clock values, long observation
windows, implausible frequencies and interval rates differing by more than
1 percent. The median observed rate seeds the last UTC observation. Neither
the frequency nor the date is a guessed constant.

After checked memory binding, the CPL0 callback captures CPU environment and
requires the calibrated CPU identity to match before installing the seed into
prepared resident data. Clock failure is reported independently and leaves
timing unavailable; successful memory preparation is still allowed. This new
capture/calibration path has been built, not verified on the console.

The copied RX image now has 67 linked entries. Offset 66 implements Microsoft
AMD64 `GetTime`; Runtime Services slot 0 points to it in the prepared table.
The callback converts observed UTC/TSC to EFI_TIME, including Gregorian leap
years, nanosecond carry and the year 9999 limit. The clock checks CPU identity
around each TSC observation; migration disables it. Output remains unchanged
on error. Unseeded clocks, retired memory services and requests for hardware
RTC capabilities return EFI_UNSUPPORTED. No accuracy, persistence or hardware
resolution is invented.

This is a boot-time source only. It is not a persistent RTC, SetTime,
SetVirtualAddressMap, ConvertPointer, ResetSystem, or a complete runtime
lifetime implementation. The System Table still does not publish its Runtime
Services pointer: reset/virtualization/lifetime and required runtime capability
decisions remain incomplete. Thus adding this callback does not yet provide
time to Boot Manager through a published Runtime Services table.

Host tests call the actual copied RX callback under its Microsoft ABI with a
real TSC and synthetic UTC seed. Pure calibration fixtures cover jitter,
excessive bracket uncertainty and migration; calendar fixtures cover leap day
and the final supported second. They do not measure the console clock or
prove a persistent runtime time source. No emulator or new console probe is
required by this change.
