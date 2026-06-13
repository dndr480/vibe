typedef unsigned long long UINTN;
typedef unsigned int UINT32;
typedef unsigned short CHAR16;
typedef void *EFI_HANDLE;
typedef unsigned long long EFI_STATUS;

typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef EFI_STATUS(__attribute__((ms_abi)) *EFI_TEXT_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *,
    CHAR16 *);
typedef EFI_STATUS(__attribute__((ms_abi)) *EFI_TEXT_CLEAR_SCREEN)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *);

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *Reset;
    EFI_TEXT_STRING OutputString;
    void *TestString;
    void *QueryMode;
    void *SetMode;
    void *SetAttribute;
    EFI_TEXT_CLEAR_SCREEN ClearScreen;
};

typedef struct {
    unsigned long long Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;
    void *RuntimeServices;
    void *BootServices;
    UINTN NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

EFI_STATUS __attribute__((ms_abi)) efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    (void)image;

    if (st && st->ConOut) {
        st->ConOut->ClearScreen(st->ConOut);
        st->ConOut->OutputString(st->ConOut, L"\r\nhello from pxenode1 PXE\r\n");
        st->ConOut->OutputString(st->ConOut, L"\r\nTiny UEFI program loaded over PXE/iPXE.\r\n");
    }

    for (;;) {
        __asm__ __volatile__("pause");
    }

    return 0;
}
