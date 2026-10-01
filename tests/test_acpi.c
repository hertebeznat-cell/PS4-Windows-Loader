#include "acpi_fixture.h"
#include <stdio.h>
static acpi_fixture_t fixture;
static unsigned char storage[65536];
static void failed_capture(pwl_acpi_source_t *source,pwl_status_t expected)
{
    pwl_acpi_snapshot_t result;memset(&result,0xa5,sizeof(result));
    assert(pwl_acpi_capture(source,ACPI_BASE+16,storage,sizeof(storage),&result)==expected);
    pwl_acpi_snapshot_t zero={0};assert(!memcmp(&zero,&result,sizeof(result)));
}
static void success(unsigned revision)
{
    acpi_fixture_init(&fixture,revision);pwl_acpi_source_t source=acpi_fixture_source(&fixture);
    uint64_t address=0;
    assert(pwl_acpi_find_rsdp(&source,ACPI_BASE,4096,&address)==PWL_OK && address==ACPI_BASE+16);
    pwl_acpi_snapshot_t snapshot;
    assert(pwl_acpi_capture(&source,address,storage,sizeof(storage),&snapshot)==PWL_OK);
    assert(snapshot.count==(revision==2?8U:7U));
    assert(pwl_acpi_snapshot_validate(&snapshot)==PWL_OK);
    assert(pwl_acpi_snapshot_recheck(&source,&snapshot)==PWL_OK);
    int high=0;for(size_t i=0;i<snapshot.count;i++)high|=snapshot.tables[i].physical_address==ACPI_HIGH;
    assert(high==(revision==2));
    pwl_x64_alias_range_t ranges[128];size_t count=0;
    assert(pwl_acpi_ranges(&snapshot,NULL,0,&count)==PWL_ERR_BUFFER_TOO_SMALL && count>0);
    memset(ranges,0xa5,sizeof(ranges));pwl_x64_alias_range_t sentinel=ranges[0];
    assert(pwl_acpi_ranges(&snapshot,ranges,count-1,&count)==PWL_ERR_BUFFER_TOO_SMALL);
    assert(!memcmp(&sentinel,&ranges[0],sizeof(sentinel)));
    assert(pwl_acpi_ranges(&snapshot,ranges,128,&count)==PWL_OK);
    static uint64_t entries[16][512];pwl_x64_table_page_t tables[16];
    for(size_t i=0;i<16;i++)tables[i]=(pwl_x64_table_page_t){UINT64_C(0x30000000)+4096*i,entries[i]};
    size_t used;assert(pwl_x64_alias_tables_build(ranges,count,tables,16,&used)==PWL_OK);
    assert(pwl_acpi_mappings_validate(&snapshot,tables,used,tables[0].physical_address)==PWL_OK);
    ranges[0].executable=1;
    assert(pwl_x64_alias_tables_build(ranges,count,tables,16,&used)==PWL_OK);
    assert(pwl_acpi_mappings_validate(&snapshot,tables,used,tables[0].physical_address)==PWL_ERR_BAD_IMAGE);
    fixture.low[24576+16]^=1; /* Same physical Global Lock, never a relocated clone. */
    assert(pwl_acpi_snapshot_recheck(&source,&snapshot)==PWL_ERR_BAD_IMAGE);
    fixture.low[24576+16]^=1;
    pwl_acpi_snapshot_t bad=snapshot;bad.tables[0].bytes=1;
    assert(pwl_acpi_snapshot_validate(&bad)==PWL_ERR_BAD_IMAGE);
    bad=snapshot;bad.tables[1].offset=0;
    assert(pwl_acpi_snapshot_validate(&bad)==PWL_ERR_BAD_IMAGE);
    bad=snapshot;bad.tables[1].physical_address=bad.tables[0].physical_address;
    assert(pwl_acpi_snapshot_validate(&bad)==PWL_ERR_BAD_IMAGE);
}
int main(void)
{
    success(0);success(2);
    acpi_fixture_init(&fixture,2);pwl_acpi_source_t source=acpi_fixture_source(&fixture);
    uint64_t found=UINT64_MAX;fixture.permitted_end=ACPI_BASE+36;
    assert(pwl_acpi_find_rsdp(&source,ACPI_BASE+16,20,&found)==PWL_ERR_NOT_FOUND && found==UINT64_MAX);
    fixture.permitted_end=0;
    unsigned char *f=fixture.low+12288,*x=fixture.low+8192,*r=fixture.low+16;
    fixture.high[40]^=1;failed_capture(&source,PWL_ERR_BAD_IMAGE);fixture.high[40]^=1;
    fixture.mutate_header=1;failed_capture(&source,PWL_ERR_BAD_IMAGE);fixture.mutate_header=0;
    fixture.reads=0;fixture.fail_read=1;failed_capture(&source,PWL_ERR_IO);fixture.fail_read=0;
    acpi_put64(f+140,UINT64_C(0x400000000));acpi_sum(f,276,9);
    unsigned before=fixture.reads;failed_capture(&source,PWL_ERR_ACCESS_DENIED);
    assert(fixture.reads>before); /* Reader must never receive the bad PA. */
    acpi_put64(f+140,ACPI_HIGH);acpi_sum(f,276,9);
    acpi_put64(x+36,ACPI_BASE+8192);acpi_sum(x,60,9);failed_capture(&source,PWL_ERR_BAD_IMAGE);
    acpi_put64(x+36,ACPI_BASE+12288);acpi_sum(x,60,9);
    acpi_put32(x+4,37);acpi_sum(x,37,9);failed_capture(&source,PWL_ERR_BAD_IMAGE);
    acpi_put32(x+4,60);acpi_sum(x,60,9);
    acpi_put32(f+4,PWL_ACPI_MAX_TABLE_BYTES+1);failed_capture(&source,PWL_ERR_BAD_IMAGE);
    acpi_put32(f+4,276);acpi_sum(f,276,9);
    acpi_put32(r+20,40);acpi_sum(r,36,32);failed_capture(&source,PWL_ERR_BAD_IMAGE);
    acpi_put32(r+20,36);acpi_sum(r,36,32);
    acpi_put64(f+132,0);acpi_put32(f+36,0);acpi_sum(f,276,9);failed_capture(&source,PWL_ERR_BAD_IMAGE);
    acpi_put32(f+112,1U<<20);acpi_sum(f,276,9);
    pwl_acpi_snapshot_t snapshot;
    assert(pwl_acpi_capture(&source,ACPI_BASE+16,storage,sizeof(storage),&snapshot)==PWL_OK && snapshot.count==7);
    assert(pwl_acpi_capture(&source,ACPI_BASE+16,storage,32,&snapshot)==PWL_ERR_BUFFER_TOO_SMALL && !snapshot.count);
    source.extent_count=0;before=fixture.reads;
    failed_capture(&source,PWL_ERR_INVALID_ARGUMENT);assert(fixture.reads==before);
    /* FACS can share a page with a read-only description table, but not its
     * actual bytes. The whole shared page must be writable and still NX. */
    acpi_fixture_init(&fixture,2);source=acpi_fixture_source(&fixture);
    memcpy(fixture.low+12288+320,fixture.low+24576,64);
    acpi_put64(f+132,ACPI_BASE+12288+320);acpi_sum(f,276,9);
    assert(pwl_acpi_capture(&source,ACPI_BASE+16,storage,sizeof(storage),&snapshot)==PWL_OK);
    pwl_x64_alias_range_t ranges[128];size_t count=0;int shared=0;
    assert(pwl_acpi_ranges(&snapshot,ranges,128,&count)==PWL_OK);
    for(size_t i=0;i<count;i++)if(ACPI_BASE+12288>=ranges[i].physical_address &&
       ACPI_BASE+12288-ranges[i].physical_address<ranges[i].bytes) {
        assert(ranges[i].writable && !ranges[i].executable);shared=1;
    }
    assert(shared);
    for(size_t i=0;i<snapshot.count;i++)for(unsigned n=0;n<36;n++) {
        pwl_acpi_snapshot_t bad=snapshot;bad.tables[i].bytes=n;
        assert(pwl_acpi_snapshot_validate(&bad)!=PWL_OK);
    }
    puts("ACPI: bounded physical reads, both roots, preferred >4GiB DSDT, FACS, checksums, graph and mapping refusals passed");
    return 0;
}
