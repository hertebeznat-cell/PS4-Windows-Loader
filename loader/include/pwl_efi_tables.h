#ifndef PWL_EFI_TABLES_H
#define PWL_EFI_TABLES_H
#include "pwl_handoff.h"

/* AMD64 wire layout. Integer addresses are destination PAs, never host/KVA
 * pointers. This is an incomplete preparation fixture, not callable firmware.
 */
#define PWL_EFI_BOOT_SLOTS 44U
#define PWL_EFI_PREPARED_CALLBACKS 41U
#define PWL_EFI_BOOT_CALLBACKS 29U
typedef struct pwl_efi_header {
    uint64_t signature;
    uint32_t revision, header_size, crc32, reserved;
} pwl_efi_header_t;
typedef struct pwl_efi_system_table {
    pwl_efi_header_t header;
    uint64_t vendor;
    uint32_t firmware_revision, padding;
    uint64_t console_in_handle, console_in;
    uint64_t console_out_handle, console_out;
    uint64_t console_error_handle, console_error;
    uint64_t runtime_services, boot_services;
    uint64_t configuration_count, configuration_tables;
} pwl_efi_system_table_t;
typedef struct pwl_efi_boot_table {
    pwl_efi_header_t header;
    uint64_t functions[PWL_EFI_BOOT_SLOTS];
} pwl_efi_boot_table_t;
typedef struct pwl_efi_prepared_tables {
    pwl_efi_system_table_t system;
    pwl_efi_boot_table_t boot;
} pwl_efi_prepared_tables_t;
/* Offsets in order: RaiseTPL, RestoreTPL, AllocatePages, FreePages,
 * GetMemoryMap, ExitBootServices, CalculateCrc32, CopyMem, SetMem,
 * AllocatePool, FreePool, InstallProtocolInterface, ReinstallProtocolInterface,
 * UninstallProtocolInterface, HandleProtocol, LocateHandle, LocateProtocol.
 * The remaining eleven offsets are filesystem protocol methods: OpenVolume,
 * Open, Close, Delete, Read, Write, GetPosition, SetPosition, GetInfo, SetInfo,
 * Flush. They are not Boot Services slots.
 * Offsets 28/29: OpenProtocol, CloseProtocol (query modes only).
 * Offset 30: assembly EFI_UNSUPPORTED adapter for unimplemented Boot Services.
 * Offsets 31..36: CreateEvent, SignalEvent, CloseEvent, CheckEvent,
 * CreateEventEx, WaitForEvent (cooperative, non-timer events).
 * Offsets 37..40: LocateHandleBuffer, ProtocolsPerHandle,
 * OpenProtocolInformation, InstallConfigurationTable.
 * Caller must independently establish code provenance, relocations and ABI.
 */
typedef struct pwl_efi_table_spec {
    uint64_t code_pa, code_bytes, data_pa, data_bytes;
    uint64_t callback_offsets[PWL_EFI_PREPARED_CALLBACKS];
} pwl_efi_table_spec_t;
uint32_t pwl_efi_crc32(const void *bytes, size_t size);
pwl_status_t pwl_efi_tables_prepare(const pwl_efi_table_spec_t *spec,
                                    pwl_efi_prepared_tables_t *out);
pwl_status_t pwl_efi_tables_validate(const pwl_efi_table_spec_t *spec,
                                    const pwl_efi_prepared_tables_t *tables);
#endif
