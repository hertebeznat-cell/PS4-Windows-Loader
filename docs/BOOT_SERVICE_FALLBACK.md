# Explicit missing-service errors

The prepared Boot Services table previously left every unimplemented method at
address zero. A caller attempting one of those methods would branch through a
null address. The table now points the remaining 7 unimplemented method slots to a copied
resident assembly adapter returning the complete AMD64 EFI_UNSUPPORTED value
0x8000000000000003. The 36 dedicated Boot Services destinations keep their own
addresses, including the existing explicit ExitBootServices refusal.
Reserved slot 17 stays zero as required by the Boot Services table layout.

The adapter ignores all parameters, does not dereference output pointers, does
not touch resident state and has no imports or privileged instructions. It uses
assembly rather than a C function with a mismatched prototype for the different
method signatures. The linked image has 67 entry symbols: the previous 30
callbacks, this error adapter at index 30, and ten new event/discovery/configuration
callbacks at 31..40, four variable callbacks at 41..44, and SetTimer/Stall/
watchdog-disable callbacks at 45..47, image lifecycle at 48..51, private mapping/
preloaded-entry adapters at 52/53, graphics/text protocol methods at 54..65,
and boot-only observed GetTime at 66 (the runtime table remains unpublished).
Existing callback indices and the All30 diagnostic mask are
unchanged. See [event/discovery services](RESIDENT_EVENT_SERVICES.md) and
[variables and conditional timers](RESIDENT_VARIABLES_TIMERS.md).

The table CRC and the exact reconstruction verifier include these new pointers.
Tests call every fallback slot using six Microsoft AMD64 arguments on actual
copied RX code, verify the full error value, and confirm the reserved field.
Malformed table pointers or offsets remain rejected. The complete host/native
build tests passed. This updated image is not yet tested on the console; the
previous All30 hardware result applies to the earlier image only.

This prevents null Boot Services calls, not missing-function failures. It does
NOT implement interrupt-driven timers, complete runtime services or hardware
handoff. Child image services are now implemented in the separate
[resident application layer](RESIDENT_IMAGES.md). Conditional
[graphics/text protocols](RESIDENT_GRAPHICS.md) require a verified external
framebuffer before publication. Runtime remains unpublished and the console
stays absent in normal builds. This is still an incomplete firmware fixture.
No Boot Manager readiness or new console binary is claimed.

Table layout reference:
https://uefi.org/specs/UEFI/2.10/04_EFI_System_Table.html#efi-boot-services-table
