#include "pwl_boot_source.h"
pwl_status_t pwl_boot_source_prepare(const pwl_boot_source_io_t *io,
    const char *path,uint64_t limit,const uint16_t *boot_path,
    const pwl_ps4_memory_api_t *api,const pwl_native_request_t *r,
    const pwl_resident_image_t *image,pwl_native_workspace_t *w,
    pwl_boot_source_report_t *report)
{
    if (!report) return PWL_ERR_INVALID_ARGUMENT;
    *report=(pwl_boot_source_report_t){0};
    pwl_status_t status=PWL_ERR_INVALID_ARGUMENT;
    void *buffer=NULL;uint64_t stream=0,bytes=0;
    if (!io || !io->open || !io->read || !io->close || !io->allocate ||
        !io->release || !path || !*path || !boot_path || !api || !r || !w ||
        w->arena.kernel_address || w->arena.size || r->disk_image || r->disk_bytes ||
        r->boot_image || r->boot_image_bytes || limit<512 ||
        pwl_resident_image_validate(image)!=PWL_OK) goto done;
    report->stage=PWL_BOOT_SOURCE_OPEN;
    status=io->open(io->context,path,&stream,&bytes);
    if (status!=PWL_OK) goto done;
    report->advertised_bytes=bytes;
    if (bytes<512 || bytes%512) status=PWL_ERR_BAD_IMAGE;
    else if (bytes>limit || bytes>SIZE_MAX) status=PWL_ERR_OUT_OF_RESOURCES;
    else {
        buffer=io->allocate(io->context,(size_t)bytes);
        status=buffer ? PWL_OK : PWL_ERR_OUT_OF_RESOURCES;
    }
    if (status==PWL_OK) {
        report->stage=PWL_BOOT_SOURCE_READ;
        while (report->read_bytes<bytes) {
            size_t count=0;
            uint64_t remaining=bytes-report->read_bytes;
            size_t wanted=remaining>65536 ? 65536 : (size_t)remaining;
            status=io->read(io->context,stream,
                (unsigned char *)buffer+(size_t)report->read_bytes,wanted,&count);
            if (status!=PWL_OK) break;
            if (!count || count>wanted) { status=PWL_ERR_IO;break; }
            report->read_bytes+=count;
        }
        if (status==PWL_OK) {
            unsigned char extra;size_t count=0;
            status=io->read(io->context,stream,&extra,1,&count);
            if (status==PWL_OK && count) status=PWL_ERR_IO;
        }
    }
    report->close_status=io->close(io->context,stream);
    if (status==PWL_OK && report->close_status!=PWL_OK) {
        report->stage=PWL_BOOT_SOURCE_CLOSE;status=report->close_status;
    }
    if (status==PWL_OK) {
        report->stage=PWL_BOOT_SOURCE_VALIDATE;
        status=pwl_files_validate(buffer,(size_t)bytes);
    }
    if (status==PWL_OK) {
        pwl_native_request_t request=*r;
        request.disk_image=buffer;request.disk_bytes=(size_t)bytes;
        report->stage=PWL_BOOT_SOURCE_PREPARE;
        status=pwl_native_boot_prepare(api,&request,image,boot_path,w);
        if (status==PWL_OK) report->stage=PWL_BOOT_SOURCE_READY;
    }
    if (buffer) io->release(io->context,buffer);
done:
    report->status=status;
    return status;
}
