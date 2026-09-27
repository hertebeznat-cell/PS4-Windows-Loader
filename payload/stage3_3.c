#define main stage3_2_base_main
#include "stage3_2.c"
#undef main

int main(void)
{
    void *iface = 0;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_32 *fs;
    EFI_FILE_PROTOCOL_32 *root = 0;
    EFI_FILE_PROTOCOL_32 *bcd = 0;
    UINTN n;
    u8 header[4];
    u64 pos = 0;
    static CHAR16 bcd_path[] = {
        '\\','E','F','I','\\','M','i','c','r','o','s','o','f','t',
        '\\','B','o','o','t','\\','B','C','D',0
    };

    notify("PS4 Windows Loader: Stage 3.3 started");
    build_efi_shim();
    if (build32() != 0) {
        notify("Stage 3.3: EFI environment build FAILED");
        return 1;
    }
    notify("Stage 3.3: EFI filesystem bridge constructed");

    if (bs32.LocateProtocol((EFI_GUID *)&simple_fs_guid32, 0, &iface) != EFI_SUCCESS ||
        iface != &fs32) {
        notify("Stage 3.3: LocateProtocol(SimpleFS) FAILED");
        return 1;
    }
    notify("Stage 3.3: LocateProtocol(SimpleFS) OK");
    fs = (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_32 *)iface;

    if (fs->OpenVolume(fs, &root) != EFI_SUCCESS || !root) {
        notify("Stage 3.3: OpenVolume FAILED");
        return 1;
    }
    notify("Stage 3.3: OpenVolume OK");

    if (root->Open(root, &bcd, bcd_path, EFI_FILE_MODE_READ, 0) != EFI_SUCCESS || !bcd) {
        notify("Stage 3.3: EFI File Open BCD FAILED");
        return 1;
    }
    notify("Stage 3.3: EFI File Open BCD OK");

    n = sizeof(header);
    if (bcd->Read(bcd, &n, header) != EFI_SUCCESS || n != sizeof(header)) {
        bcd->Close(bcd);
        notify("Stage 3.3: BCD Read FAILED");
        return 1;
    }
    if (header[0] != 'r' || header[1] != 'e' || header[2] != 'g' || header[3] != 'f') {
        bcd->Close(bcd);
        notify("Stage 3.3: BCD header is not regf");
        return 1;
    }
    notify("Stage 3.3: BCD registry hive header regf OK");

    if (bcd->GetPosition(bcd, &pos) != EFI_SUCCESS || pos != 4) {
        bcd->Close(bcd);
        notify("Stage 3.3: BCD GetPosition FAILED");
        return 1;
    }
    if (bcd->SetPosition(bcd, 0) != EFI_SUCCESS) {
        bcd->Close(bcd);
        notify("Stage 3.3: BCD rewind FAILED");
        return 1;
    }
    notify("Stage 3.3: BCD seek/rewind OK");

    if (bcd->Close(bcd) != EFI_SUCCESS) {
        notify("Stage 3.3: BCD Close FAILED");
        return 1;
    }
    notify("Stage 3.3: BCD Close OK");
    notify("PS4 Windows Loader: Stage 3.3 BCD EFI read self-test OK");

    munmap(arena32, 16U * 1024U * 1024U);
    return 0;
}
