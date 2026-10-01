/* Included by the real owned-workspace integration test. */
typedef struct native_table_fixture {
    pwl_x64_table_page_t *tables;size_t count;
} native_table_fixture_t;
static pwl_status_t native_fixture_read(void *context,uint64_t pa,uint64_t out[512])
{
    native_table_fixture_t *f=context;
    for(size_t i=0;i<f->count;i++)if(f->tables[i].physical_address==pa) {
        memcpy(out,f->tables[i].entries,4096);return PWL_OK;
    }
    return PWL_ERR_NOT_FOUND;
}
static void native_ranges_sort(pwl_x64_alias_range_t *r,size_t n)
{
    for(size_t i=1;i<n;i++) {
        pwl_x64_alias_range_t x=r[i];size_t j=i;
        while(j && r[j-1].virtual_address>x.virtual_address){r[j]=r[j-1];j--;}
        r[j]=x;
    }
}
static void test_native_call_transaction(pwl_native_workspace_t *w)
{
    void *control=aligned_alloc(4096,4096),*new_bytes=aligned_alloc(4096,64*4096);
    void *old_bytes=aligned_alloc(4096,64*4096),*snapshot_bytes=aligned_alloc(4096,64*4096);
    assert(control && new_bytes && old_bytes && snapshot_bytes);
    pwl_x64_table_page_t old[64],tables[64],snapshot[64];
    for(size_t i=0;i<64;i++) {
        old[i]=(pwl_x64_table_page_t){UINT64_C(0x21000000)+i*4096,(uint64_t *)((unsigned char *)old_bytes+i*4096)};
        snapshot[i]=(pwl_x64_table_page_t){0,(uint64_t *)((unsigned char *)snapshot_bytes+i*4096)};
    }
    uintptr_t start,adapter;
    int (*call_fn)(pwl_native_call_t *)=pwl_x64_native_call;
    int (*adapter_fn)(void *)=pwl_x64_efi_entry_call;
    memcpy(&start,&call_fn,sizeof(start));memcpy(&adapter,&adapter_fn,sizeof(adapter));
    uint64_t low=(start<adapter ? start : adapter)&~UINT64_C(4095);
    uint64_t aend=(uintptr_t)pwl_x64_native_call_end,bend=(uintptr_t)pwl_x64_efi_entry_call_end;
    uint64_t high=((aend>bend ? aend : bend)+4095)&~UINT64_C(4095);
    assert(high>low && high-low<65536);
    pwl_native_call_storage_t storage={{control,UINT64_C(0x51000000),4096},UINT64_C(0x70000000),16384};
    pwl_x64_alias_range_t deps[]={
        {low,UINT64_C(0x40000000),high-low,0,1,0},
        {(uintptr_t)control,storage.control.physical_address,4096,1,0,0},
        {storage.old_stack_base,UINT64_C(0x50000000),storage.old_stack_bytes,1,0,0}};
    pwl_x64_alias_range_t ranges[PWL_TRANSITION_MAX_RANGES];size_t n=0,old_count=0;
    for(size_t i=0;i<w->mapping_count;i++) {
        pwl_x64_identity_range_t *m=&w->mappings[i];
        ranges[n++]=(pwl_x64_alias_range_t){m->base,m->base,m->size,m->writable,m->executable,0};
    }
    for(size_t i=0;i<3;i++)ranges[n++]=deps[i];
    native_ranges_sort(ranges,n);
    assert(pwl_x64_alias_tables_build(ranges,n,old,64,&old_count)==PWL_OK);
    native_table_fixture_t reader={old,old_count};
    pwl_owned_span_t span={new_bytes,UINT64_C(0x20000000),64*4096};
    pwl_x64_cpu_state_t cpu={UINT64_C(0x8005003b),old[0].physical_address,0x406f0,0xd01};
    pwl_fp_layout_t fp={832,7,PWL_FP_XSAVE};
    pwl_native_transition_plan_t plan={0};pwl_entry_pipeline_report_t report;
    pwl_native_call_t *call=(void *)(uintptr_t)1;
    pwl_native_data_t *data=w->data.prepare_address;
    assert(!data->image_mapping.root);
    memset(control,0xaa,4096);
    storage.control.physical_address+=4096;
    assert(pwl_native_entry_call_prepare(w,&resident_image,&cpu,native_fixture_read,&reader,
        snapshot,64,deps,3,&span,tables,64,&plan,&fp,&storage,&call,&report)==PWL_ERR_BAD_IMAGE);
    assert(call==(void *)(uintptr_t)1 && !plan.root && !data->image_mapping.root && report.stage==4);
    for(size_t i=0;i<4096;i++)assert(((unsigned char *)control)[i]==0xaa);
    storage.control.physical_address-=4096;
    assert(pwl_native_entry_call_prepare(w,&resident_image,&cpu,native_fixture_read,&reader,
        snapshot,64,deps,3,&span,tables,64,&plan,&fp,&storage,&call,&report)==PWL_OK);
    assert(report.stage==5 && call==control && data->image_mapping.root==plan.root);
    assert(call->root==plan.root && call->fp_address==(uintptr_t)control+256 &&
        call->fp_bytes==832 && call->fp_mask==7 && call->fp_kind==PWL_FP_XSAVE);
    pwl_efi_entry_context_t *entry=(void *)(uintptr_t)call->context;
    assert(entry->entry==w->firmware.physical_address+resident_image.callbacks[53]);
    assert(!entry->returned && !call->returned && !call->fp_saved && !call->fp_restored);
    assert(pwl_native_call_result_validate(call)==PWL_ERR_BAD_IMAGE);
    /* Synthetic result validation is not a host CR3 execution claim. */
    call->addresses=(pwl_address_call_report_t){cpu.cr3,plan.root,cpu.cr3,
        storage.old_stack_base+8192,call->stack_top,storage.old_stack_base+8192};
    call->cr0_entered=cpu.cr0&~UINT64_C(8);call->cr4_entered=cpu.cr4&~UINT64_C(128);
    call->fp_saved=call->fp_restored=call->returned=entry->returned=1;
    entry->status=UINT64_C(0x800000000000000e);
    assert(pwl_native_call_result_validate(call)==PWL_OK && entry->status==UINT64_C(0x800000000000000e));
    call->addresses.root_after+=4096;
    assert(pwl_native_call_result_validate(call)==PWL_ERR_BAD_IMAGE);
    call->addresses.root_after-=4096;
    call->fp_restored=0;assert(pwl_native_call_result_validate(call)==PWL_ERR_BAD_IMAGE);
    pwl_native_call_t *previous=call;
    assert(pwl_native_entry_call_prepare(w,&resident_image,&cpu,native_fixture_read,&reader,
        snapshot,64,deps,3,&span,tables,64,&plan,&fp,&storage,&call,&report)==PWL_ERR_ACCESS_DENIED);
    assert(call==previous && data->image_mapping.root==plan.root);
    data->image_mapping.root=data->image_mapping.tables_base=data->image_mapping.tables_bytes=0;
    /* Alias the caller stack to the new stack: both roots agree, but the
     * suspended return state would be overwritten. Must fail transactionally. */
    deps[2].physical_address=w->stack.physical_address;
    n=0;
    for(size_t i=0;i<w->mapping_count;i++) {
        pwl_x64_identity_range_t *m=&w->mappings[i];
        ranges[n++]=(pwl_x64_alias_range_t){m->base,m->base,m->size,m->writable,m->executable,0};
    }
    for(size_t i=0;i<3;i++)ranges[n++]=deps[i];
    native_ranges_sort(ranges,n);
    assert(pwl_x64_alias_tables_build(ranges,n,old,64,&old_count)==PWL_OK);reader.count=old_count;
    assert(pwl_native_entry_call_prepare(w,&resident_image,&cpu,native_fixture_read,&reader,
        snapshot,64,deps,3,&span,tables,64,&plan,&fp,&storage,&call,&report)!=PWL_OK);
    assert(!plan.root && !data->image_mapping.root && call==previous);
    /* Same-permission aliases are accepted by the root constructor. The
     * call preparer must reject a stack alias to control/FP storage despite
     * identical RW/NX translations under both roots. */
    deps[2].physical_address=storage.control.physical_address;
    n=0;
    for(size_t i=0;i<w->mapping_count;i++) {
        pwl_x64_identity_range_t *m=&w->mappings[i];
        ranges[n++]=(pwl_x64_alias_range_t){m->base,m->base,m->size,m->writable,m->executable,0};
    }
    for(size_t i=0;i<3;i++)ranges[n++]=deps[i];
    native_ranges_sort(ranges,n);
    assert(pwl_x64_alias_tables_build(ranges,n,old,64,&old_count)==PWL_OK);reader.count=old_count;
    assert(pwl_native_entry_call_prepare(w,&resident_image,&cpu,native_fixture_read,&reader,
        snapshot,64,deps,3,&span,tables,64,&plan,&fp,&storage,&call,&report)==PWL_ERR_INVALID_ARGUMENT);
    assert(report.stage==4 && !plan.root && !data->image_mapping.root && call==previous);
    free(control);free(new_bytes);free(old_bytes);free(snapshot_bytes);
}
