#ifndef PWL_BOOT_SOURCE_H
#define PWL_BOOT_SOURCE_H
#include "pwl_native_workspace.h"
/* Preparation-side I/O only. open success transfers one stream to the caller;
 * read returns its actual count (zero means EOF). Errors own no new resources.
 * None of these callbacks or the stream are copied to resident firmware. */
typedef struct pwl_boot_source_io {
    void *context;
    pwl_status_t (*open)(void *,const char *,uint64_t *,uint64_t *);
    pwl_status_t (*read)(void *,uint64_t,void *,size_t,size_t *);
    pwl_status_t (*close)(void *,uint64_t);
    void *(*allocate)(void *,size_t);
    void (*release)(void *,void *);
} pwl_boot_source_io_t;
enum pwl_boot_source_stage { PWL_BOOT_SOURCE_OPEN=1,PWL_BOOT_SOURCE_READ,
    PWL_BOOT_SOURCE_CLOSE,PWL_BOOT_SOURCE_VALIDATE,PWL_BOOT_SOURCE_PREPARE,
    PWL_BOOT_SOURCE_READY };
typedef struct pwl_boot_source_report {
    unsigned stage;
    pwl_status_t status,close_status;
    uint64_t advertised_bytes,read_bytes;
} pwl_boot_source_report_t;
/* One transaction: read a bounded archive, close the stream, validate, prepare
 * the owned image/environment, discard staging bytes. request media/boot fields
 * must be empty and workspace must be unowned. READY means preparation only. */
pwl_status_t pwl_boot_source_prepare(const pwl_boot_source_io_t *io,
    const char *archive_path,uint64_t maximum_bytes,const uint16_t *boot_path,
    const pwl_ps4_memory_api_t *api,const pwl_native_request_t *request,
    const pwl_resident_image_t *image,pwl_native_workspace_t *workspace,
    pwl_boot_source_report_t *report);
#endif
