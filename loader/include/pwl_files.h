#ifndef PWL_FILES_H
#define PWL_FILES_H
#include "pwl_firmware.h"
#define PWL_FILES_MAX 128U
#define PWL_FILES_PATH 256U
#define PWL_FILES_HEADER 24U
#define PWL_FILES_RECORD 536U
#define PWL_FILES_DIRECTORY 1U
/* Immutable preloaded file archive; all byte accesses are extent checked.
 * Paths are absolute UTF-16 with backslashes. No process file handles survive.
 */
typedef struct pwl_file_view {
    uint64_t offset,size;
    uint32_t directory,index;
} pwl_file_view_t;
pwl_status_t pwl_files_validate(const void *archive,size_t bytes);
uint64_t pwl_files_open(const void *archive,size_t bytes,const uint16_t *path,
                        pwl_file_view_t *file);
uint64_t pwl_files_read(const void *archive,size_t bytes,const pwl_file_view_t *file,
                        uint64_t *position,size_t *size,void *buffer);
uint64_t pwl_files_at(const void *archive,size_t bytes,uint32_t index,
                     pwl_file_view_t *file,uint16_t path[PWL_FILES_PATH]);
#endif
