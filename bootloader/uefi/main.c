/**
 * @file main.c
 * @brief UEFI Bootloader main entry point (GNU-EFI version)
 */

#include <efi.h>
#include <efilib.h>
#include "../common/boot_info.h"

// Boot info to pass to kernel
static boot_info_t boot_info = {0};

// Forward declarations
EFI_STATUS load_kernel_file(EFI_HANDLE ImageHandle, CHAR16 *FileName, VOID **Buffer, UINTN *Size);
UINT64 load_elf(void *elf_data);
void setup_page_tables(void);

/**
 * UEFI Application Entry Point (GNU-EFI)
 * Explicitly use MS ABI calling convention
 */
EFI_STATUS
__attribute__((ms_abi))
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    VOID *KernelBuffer = NULL;
    UINTN KernelSize = 0;
    UINT64 KernelEntry;
    
    // Skip InitializeLib and manually set globals to avoid corruption
    ST = SystemTable;
    BS = SystemTable->BootServices;

    // Clear screen and print banner - use SystemTable directly to avoid global variable issues
    uefi_call_wrapper(SystemTable->ConOut->ClearScreen, 1, SystemTable->ConOut);

    // Use direct console output since Print() relies on ST global
    CHAR16 *msg = L"Scarlett OS UEFI Bootloader\r\n";
    uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, msg);
    msg = L"===========================\r\n\r\n";
    uefi_call_wrapper(SystemTable->ConOut->OutputString, 2, SystemTable->ConOut, msg);
    
    // Load kernel.elf from disk
    Print(L"Loading kernel.elf...\\n");
    Status = load_kernel_file(ImageHandle, L"\\\\kernel.elf", &KernelBuffer, &KernelSize);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Failed to load kernel.elf (Status: %r)\\n", Status);
        while(1);
    }
    Print(L"Kernel loaded: %d bytes\\n", KernelSize);
    
    // Parse ELF and get entry point
    Print(L"Parsing ELF...\\n");
    KernelEntry = load_elf(KernelBuffer);
    if (KernelEntry == 0) {
        Print(L"ERROR: Failed to parse ELF\\n");
        while(1);
    }
    Print(L"Kernel entry point: 0x%lx\\n", KernelEntry);
    
    // Get memory map
    Print(L"Getting memory map...\\n");
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;
    UINTN MapSize = sizeof(boot_info.memory_map);
    
    Status = uefi_call_wrapper(BS->GetMemoryMap, 5,
                               &MapSize,
                               (EFI_MEMORY_DESCRIPTOR*)boot_info.memory_map,
                               &MapKey,
                               &DescriptorSize,
                               &DescriptorVersion);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Failed to get memory map (Status: %r)\\n", Status);
        while(1);
    }
    boot_info.memory_map_count = MapSize / DescriptorSize;
    
    // Get framebuffer info
    Print(L"Getting framebuffer info...\\n");
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    Status = uefi_call_wrapper(BS->LocateProtocol, 3, &GopGuid, NULL, (VOID**)&Gop);
    if (!EFI_ERROR(Status)) {
        boot_info.framebuffer.base = Gop->Mode->FrameBufferBase;
        boot_info.framebuffer.width = Gop->Mode->Info->HorizontalResolution;
        boot_info.framebuffer.height = Gop->Mode->Info->VerticalResolution;
        boot_info.framebuffer.pitch = Gop->Mode->Info->PixelsPerScanLine * 4;
        boot_info.framebuffer.bpp = 32;
        Print(L"Framebuffer: %dx%d @ 0x%lx\\n", 
              boot_info.framebuffer.width, 
              boot_info.framebuffer.height,
              boot_info.framebuffer.base);
    }
    
    // Setup page tables
    Print(L"Setting up page tables...\\n");
    setup_page_tables();
    
    // Exit boot services
    Print(L"Exiting boot services...\\n");
    Status = uefi_call_wrapper(BS->ExitBootServices, 2, ImageHandle, MapKey);
    if (EFI_ERROR(Status)) {
        Print(L"ERROR: Failed to exit boot services (Status: %r)\\n", Status);
        while(1);
    }
    
    // Jump to kernel
    typedef void (*kernel_entry_t)(boot_info_t*);
    kernel_entry_t kernel_main = (kernel_entry_t)KernelEntry;
    kernel_main(&boot_info);
    
    // Should never reach here
    while(1);
    return EFI_SUCCESS;
}

/**
 * Load a file from the EFI System Partition
 */
EFI_STATUS load_kernel_file(EFI_HANDLE ImageHandle, CHAR16 *FileName, VOID **Buffer, UINTN *Size)
{
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE_PROTOCOL *Root, *File;
    EFI_FILE_INFO *FileInfo;
    UINTN FileInfoSize;
    
    // Get loaded image protocol
    EFI_GUID LoadedImageGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, &LoadedImageGuid, (VOID**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;
    
    // Get file system protocol
    EFI_GUID FileSystemGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle, &FileSystemGuid, (VOID**)&FileSystem);
    if (EFI_ERROR(Status)) return Status;
    
    // Open root directory
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    if (EFI_ERROR(Status)) return Status;
    
    // Open file
    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(Root->Close, 1, Root);
        return Status;
    }
    
    // Get file size
    FileInfoSize = SIZE_OF_EFI_FILE_INFO + 256;
    FileInfo = AllocatePool(FileInfoSize);
    Status = uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
    if (EFI_ERROR(Status)) {
        uefi_call_wrapper(File->Close, 1, File);
        uefi_call_wrapper(Root->Close, 1, Root);
        return Status;
    }
    
    *Size = FileInfo->FileSize;
    FreePool(FileInfo);
    
    // Allocate buffer
    *Buffer = AllocatePool(*Size);
    if (!*Buffer) {
        uefi_call_wrapper(File->Close, 1, File);
        uefi_call_wrapper(Root->Close, 1, Root);
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Read file
    Status = uefi_call_wrapper(File->Read, 3, File, Size, *Buffer);
    
    // Close file and root
    uefi_call_wrapper(File->Close, 1, File);
    uefi_call_wrapper(Root->Close, 1, Root);
    
    return Status;
}
