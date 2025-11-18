/**
 * @file main.c
 * @brief UEFI Bootloader main entry point
 * 
 * This is the main bootloader for Scarlett OS.
 * It loads the kernel, sets up page tables, and passes control to the kernel.
 */

#include "uefi.h"
#include "../common/boot_info.h"

// Global pointers
static EFI_SYSTEM_TABLE* systab = NULL;
static EFI_BOOT_SERVICES* bs = NULL;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* cout = NULL;

// Boot info to pass to kernel
static boot_info_t boot_info = {0};

/**
 * Print a string to UEFI console
 */
static void print(const char* str) {
    if (!cout) return;
    
    uint16_t buf[256];
    for (int i = 0; i < 255 && str[i]; i++) {
        buf[i] = str[i];
        buf[i + 1] = 0;
    }
    
    cout->OutputString(cout, buf);
}

/**
 * Print a newline
 */
static void println(const char* str) {
    print(str);
    print("\r\n");
}

/**
 * Convert number to hex string
 */
static void print_hex(uint64_t num) {
    char buf[20] = "0x";
    char hex[] = "0123456789ABCDEF";
    int i = 2;
    
    for (int shift = 60; shift >= 0; shift -= 4) {
        buf[i++] = hex[(num >> shift) & 0xF];
    }
    buf[i] = 0;
    
    print(buf);
}

/**
 * Get memory map from UEFI
 */
static EFI_STATUS get_memory_map(void) {
    uint64_t mmap_size = sizeof(EFI_MEMORY_DESCRIPTOR) * MAX_MEMORY_REGIONS;
    EFI_MEMORY_DESCRIPTOR* mmap = NULL;
    uint64_t map_key = 0;
    uint64_t desc_size = 0;
    uint32_t desc_version = 0;
    EFI_STATUS status;
    
    // Allocate buffer for memory map
    status = bs->AllocatePool(EfiLoaderData, mmap_size, (void**)&mmap);
    if (status != EFI_SUCCESS) {
        println("ERROR: Failed to allocate memory for memory map");
        return status;
    }
    
    // Get memory map
    status = bs->GetMemoryMap(&mmap_size, mmap, &map_key, &desc_size, &desc_version);
    if (status != EFI_SUCCESS) {
        println("ERROR: Failed to get memory map");
        bs->FreePool(mmap);
        return status;
    }
    
    // Copy memory map to boot info
    uint32_t num_entries = mmap_size / desc_size;
    if (num_entries > MAX_MEMORY_REGIONS) {
        num_entries = MAX_MEMORY_REGIONS;
    }
    
    boot_info.memory_map_count = num_entries;
    
    for (uint32_t i = 0; i < num_entries; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)mmap + (i * desc_size));
        
        boot_info.memory_map[i].base = desc->PhysicalStart;
        boot_info.memory_map[i].length = desc->NumberOfPages * 4096;
        boot_info.memory_map[i].type = desc->Type;
    }
    
    bs->FreePool(mmap);
    
    println("Memory map retrieved successfully");
    return EFI_SUCCESS;
}

/**
 * Get framebuffer information
 */
static EFI_STATUS get_framebuffer_info(void) {
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    EFI_STATUS status;
    
    // Locate Graphics Output Protocol
    status = bs->HandleProtocol(systab->ConsoleOutHandle, &gop_guid, (void**)&gop);
    if (status != EFI_SUCCESS || !gop) {
        println("WARNING: Graphics Output Protocol not found");
        return status;
    }
    
    // Get current mode information
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE* mode = gop->Mode;
    if (!mode || !mode->Info) {
        println("WARNING: No graphics mode information");
        return EFI_NOT_FOUND;
    }
    
    // Fill in framebuffer info
    boot_info.framebuffer.base = mode->FrameBufferBase;
    boot_info.framebuffer.width = mode->Info->HorizontalResolution;
    boot_info.framebuffer.height = mode->Info->VerticalResolution;
    boot_info.framebuffer.pitch = mode->Info->PixelsPerScanLine * 4;
    boot_info.framebuffer.bpp = 32;
    
    if (mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
        boot_info.framebuffer.red_mask = 0x00FF0000;
        boot_info.framebuffer.green_mask = 0x0000FF00;
        boot_info.framebuffer.blue_mask = 0x000000FF;
        boot_info.framebuffer.reserved_mask = 0xFF000000;
    } else if (mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        boot_info.framebuffer.red_mask = 0x000000FF;
        boot_info.framebuffer.green_mask = 0x0000FF00;
        boot_info.framebuffer.blue_mask = 0x00FF0000;
        boot_info.framebuffer.reserved_mask = 0xFF000000;
    }
    
    print("Framebuffer: ");
    print_hex(boot_info.framebuffer.base);
    println("");
    
    return EFI_SUCCESS;
}

/**
 * Initialize boot info structure
 */
static void init_boot_info(void) {
    boot_info.magic = BOOT_INFO_MAGIC;
    
    // Set bootloader info
    const char* name = "Scarlett UEFI Bootloader";
    for (int i = 0; i < 63 && name[i]; i++) {
        boot_info.bootloader_name[i] = name[i];
    }
    boot_info.bootloader_version = 0x00010000; // Version 1.0
    
    // Set kernel addresses (will be updated after loading kernel)
    boot_info.kernel_physical_base = 0;
    boot_info.kernel_virtual_base = 0xFFFFFFFF80000000ULL; // Higher half
    boot_info.kernel_size = 0;
    
    // ACPI (RSDP address will be filled later)
    boot_info.rsdp_address = 0;
}

/**
 * Main bootloader entry point
 */
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE* system_table) {
    EFI_STATUS status;
    
    // Save system table pointers
    systab = system_table;
    bs = systab->BootServices;
    cout = systab->ConOut;
    
    // Clear screen
    if (cout && cout->ClearScreen) {
        cout->ClearScreen(cout);
    }
    
    // Print banner
    println("===========================================");
    println("   Scarlett OS - UEFI Bootloader v1.0");
    println("===========================================");
    println("");
    
    // Initialize boot info
    init_boot_info();
    println("Boot info structure initialized");
    
    // Get memory map
    status = get_memory_map();
    if (status != EFI_SUCCESS) {
        println("FATAL: Could not get memory map");
        goto error;
    }
    
    // Get framebuffer info
    status = get_framebuffer_info();
    // Non-fatal if this fails
    
    // Load kernel from disk
    println("Loading kernel...");
    
    // Get loaded image protocol to access filesystem
    EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    void* loaded_image_proto;
    status = bs->HandleProtocol(image, &loaded_image_guid, &loaded_image_proto);
    if (status != EFI_SUCCESS) {
        println("ERROR: Could not get loaded image protocol");
        goto error;
    }
    
    // For now, kernel must be loaded by QEMU with -kernel flag
    // Full UEFI filesystem loading would go here
    println("Note: Using kernel loaded by QEMU -kernel flag");
    
    // In a full implementation, we would:
    // 1. Get EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
    // 2. Open volume
    // 3. Open file "kernel.elf"
    // 4. Read file into memory
    // 5. Parse and load ELF
    
    // For now, assume kernel is already loaded by bootloader stub
    // and we're just setting up the environment
    
    // Set up page tables
    println("Setting up page tables...");
    
    uint64_t pml4_addr = 0;
    extern EFI_STATUS setup_page_tables(uint64_t*, uint64_t, uint64_t, 
                                        uint64_t, uint64_t, EFI_BOOT_SERVICES*);
    
    // Assume kernel is at 0x100000 (1MB) and is about 2MB
    uint64_t kernel_phys_start = 0x100000;
    uint64_t kernel_phys_end = 0x300000;
    
    status = setup_page_tables(&pml4_addr,
                               kernel_phys_start,
                               kernel_phys_end,
                               boot_info.framebuffer.base,
                               boot_info.framebuffer.height * boot_info.framebuffer.pitch,
                               bs);
    
    if (status != EFI_SUCCESS) {
        println("ERROR: Could not set up page tables");
        goto error;
    }
    
    print("Page tables at: ");
    print_hex(pml4_addr);
    println("");
    
    // Get final memory map and exit boot services
    println("Exiting boot services...");
    
    uint64_t mmap_size = sizeof(EFI_MEMORY_DESCRIPTOR) * MAX_MEMORY_REGIONS;
    EFI_MEMORY_DESCRIPTOR* mmap = NULL;
    uint64_t map_key = 0;
    uint64_t desc_size = 0;
    uint32_t desc_version = 0;
    
    status = bs->AllocatePool(EfiLoaderData, mmap_size, (void**)&mmap);
    if (status != EFI_SUCCESS) {
        println("ERROR: Could not allocate memory for final memory map");
        goto error;
    }
    
    status = bs->GetMemoryMap(&mmap_size, mmap, &map_key, &desc_size, &desc_version);
    if (status != EFI_SUCCESS) {
        println("ERROR: Could not get final memory map");
        goto error;
    }
    
    // Exit boot services (point of no return!)
    status = bs->ExitBootServices(image, map_key);
    if (status != EFI_SUCCESS) {
        // Try again with updated memory map
        bs->GetMemoryMap(&mmap_size, mmap, &map_key, &desc_size, &desc_version);
        status = bs->ExitBootServices(image, map_key);
        if (status != EFI_SUCCESS) {
            // Can't print anymore, just hang
            while(1) __asm__ volatile("hlt");
        }
    }
    
    // Boot services are now unavailable!
    // Load new page tables
    __asm__ volatile("mov %0, %%cr3" :: "r"(pml4_addr));
    
    // Jump to kernel
    // Kernel entry point is at virtual address 0xFFFFFFFF80100000
    void (*kernel_entry)(boot_info_t*) = (void*)0xFFFFFFFF80100000ULL;
    
    // Call kernel with boot info
    kernel_entry(&boot_info);
    
    // Should never return
    while (1) {
        __asm__ volatile("hlt");
    }
    
    return EFI_SUCCESS;
    
error:
    println("Bootloader failed!");
    while (1) {
        __asm__ volatile("hlt");
    }
    return status;
}

