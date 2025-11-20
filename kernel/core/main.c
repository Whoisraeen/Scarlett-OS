/**
 * @file main.c
 * @brief Kernel main entry point
 *
 * This is where the kernel begins execution after the bootloader
 * hands control over to us.
 */

#include "../include/types.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/config.h"
#include "../include/errors.h"
#include "../include/cpu.h"
#include "../include/mm/vmm.h"
#include "../include/mm/pmm.h"
#include "../include/desktop/desktop.h"
#include "../include/desktop/taskbar.h"
#include "../include/desktop/login.h"
#include "../include/window/window.h"
#include "../include/graphics/graphics.h"
#include "../../bootloader/common/boot_info.h"
#include "../include/desktop/desktop.h"
#include "../include/desktop/taskbar.h"
#include "../include/graphics/graphics.h"
#include "../../bootloader/limine/limine.h"

// Limine requests - these MUST be in the .limine_requests section
__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(2);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

// External symbols from linker script
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];
extern uint8_t _bss_start[];
extern uint8_t _bss_end[];

// Forward declarations
static void print_banner(void);
static void verify_boot_info(boot_info_t* boot_info);
static void print_memory_map(boot_info_t* boot_info);

// x86 I/O port functions
static inline void __outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t __inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Initialize serial port for early debug output
static void early_serial_init(void) {
    __outb(0x3F8 + 1, 0x00);    // Disable all interrupts
    __outb(0x3F8 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    __outb(0x3F8 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    __outb(0x3F8 + 1, 0x00);    //                  (hi byte)
    __outb(0x3F8 + 3, 0x03);    // 8 bits, no parity, one stop bit
    __outb(0x3F8 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    __outb(0x3F8 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

// Simple early serial write (before serial_init)
// Just write directly like entry.S does - no waiting
static void early_serial_write(const char* str) {
    while (*str) {
        __outb(0x3F8, *str);
        str++;
    }
}

/**
 * Main kernel entry point
 */
void kernel_main(boot_info_t* boot_info) {
    // VERY FIRST: Initialize serial port for debug output
    early_serial_init();

    // Print to serial to confirm we reached C code
    early_serial_write("MAIN\r\n");

    // Initialize VGA first for visual debugging
    extern void vga_init(void);
    extern void vga_writestring(const char*);
    vga_init();
    vga_writestring("Scarlett OS - Booting...\n");

    early_serial_write("VGA_INIT\r\n");

    // Initialize serial console for debugging
    extern void serial_init(void);
    serial_init();

    early_serial_write("SERIAL_INIT\r\n");

    // Print kernel banner
    print_banner();
    vga_writestring("Serial initialized\n");

    // Handle Multiboot2 boot info (passed in RDI/EBX)
    // If we were booted by Limine, RDI would be NULL (as we cleared it in entry.S for now)
    // But wait! We need to pass the Multiboot2 info pointer from entry.S
    
    // Define a static boot_info structure for Multiboot2
    static boot_info_t mb2_boot_info;
    
    // If the pointer looks like a valid physical address (not NULL), try to parse it
    if (boot_info != NULL) {
        // Check if it's likely a Multiboot2 structure (starts with size)
        // For now, we assume if it's not NULL, it's what we passed from entry.S
        
        extern void multiboot2_parse(uint64_t addr, boot_info_t* info);
        multiboot2_parse((uint64_t)boot_info, &mb2_boot_info);
        
        // Use our parsed info
        boot_info = &mb2_boot_info;
    } else {
        early_serial_write("WARNING: boot_info is NULL\r\n");
        vga_writestring("WARNING: Running without boot info\n");
        // We can't really proceed safely without memory map, but we'll try
    }

    if (boot_info) {
        verify_boot_info(boot_info);
    }

skip_boot_info:
    
    // Print system information
    kinfo("Kernel loaded at: 0x%016lx - 0x%016lx\n",
          (uint64_t)_kernel_start, (uint64_t)_kernel_end);
          
    if ((uint64_t)_kernel_start == 0) {
        kerror("CRITICAL: _kernel_start symbol is 0! Linker script issue?\n");
    }
    
    kinfo("Kernel size: %lu KB\n",
          ((uint64_t)_kernel_end - (uint64_t)_kernel_start) / 1024);
    kinfo("BSS section: 0x%016lx - 0x%016lx\n",
          (uint64_t)_bss_start, (uint64_t)_bss_end);
    
    // Print bootloader information
    kinfo("Bootloader: %s v%u.%u\n",
          boot_info->bootloader_name,
          boot_info->bootloader_version >> 16,
          boot_info->bootloader_version & 0xFFFF);
    
    // Initialize framebuffer
    extern error_code_t framebuffer_init(framebuffer_info_t*);
    if (boot_info->framebuffer.base != 0) {
        kinfo("Framebuffer: 0x%016lx (%ux%u @ %u bpp)\n",
              boot_info->framebuffer.base,
              boot_info->framebuffer.width,
              boot_info->framebuffer.height,
              boot_info->framebuffer.bpp);
        framebuffer_init(&boot_info->framebuffer);
        
        // Initialize boot splash screen early
        extern error_code_t bootsplash_init(void);
        kinfo("Initializing boot splash screen...\n");
        bootsplash_init();
        
        // Note: Delay bootsplash_render until after GDT/IDT are initialized
        // Graphics functions may need proper CPU state setup
        extern error_code_t bootsplash_set_message(const char* message);
        bootsplash_set_message("Initializing kernel...");
    } else {
        kwarn("No framebuffer available\n");
    }
    
    // Print memory map
    print_memory_map(boot_info);
    
    // Initialize GDT
    extern void gdt_init(void);
    gdt_init();
    
    // Initialize IDT
    extern void idt_init(void);
    idt_init();
    
    // Initialize CPU detection
    extern error_code_t cpu_init(void);
    kinfo("Initializing CPU subsystem...\n");
    cpu_init();

    // Initialize PIC and interrupts (but not APIC yet - needs VMM)
    extern void interrupts_init(void);
    interrupts_init();

    // Initialize timer
    extern void timer_init(void);
    timer_init();

    // Enable interrupts
    kinfo("About to enable interrupts with sti...\n");
    __asm__ volatile("sti");
    kinfo("Interrupts enabled\n");

    // Initialize physical memory manager
    extern void pmm_init(boot_info_t*);
    pmm_init(boot_info);
    
    // Note: Boot splash rendering skipped during early boot for speed
    // Will be rendered later after heap is initialized

    kinfo("\n========================================\n");
    kinfo("Phase 1 initialization complete!\n");
    kinfo("========================================\n\n");

    // ========================================================================
    // Phase 2 Initialization
    // ========================================================================
    kinfo("=== Phase 2 Initialization ===\n");

    // Virtual Memory Manager
    extern void vmm_init(void);
    kinfo("[MAIN] About to call vmm_init()...\n");
    vmm_init();
    kinfo("[MAIN] vmm_init() returned, continuing...\n");

    // Initialize APIC after VMM (needs physical memory mapping)
    // Initialize APIC after VMM (needs physical memory mapping)
    extern error_code_t apic_init(void);
    kinfo("Initializing APIC...\n");
    apic_init();
    // kinfo("APIC initialization skipped (PHYS_MAP_BASE not available)\n");

    // Start Application Processors (Multi-core boot)
    kinfo("\n========================================\n");
    kinfo("Starting Application Processors...\n");
    kinfo("========================================\n");

    extern error_code_t ap_startup(uint32_t apic_id);
    extern cpu_topology_t* cpu_get_topology(void);
    extern cpu_info_t* cpu_get_info(uint32_t cpu_id);

    cpu_topology_t* topo = cpu_get_topology();
    uint32_t ap_count = 0;

    // Start all Application Processors
    for (uint32_t i = 0; i < MAX_CPUS; i++) {
        cpu_info_t* cpu_info = cpu_get_info(i);
        if (cpu_info && !cpu_info->is_bsp && cpu_info->apic_id != 0) {
            kinfo("Starting AP %u (APIC ID %u)...\n", i, cpu_info->apic_id);
            error_code_t result = ap_startup(cpu_info->apic_id);

            if (result == ERR_OK) {
                ap_count++;
                kinfo("AP %u started successfully\n", i);

                // Brief delay to let AP initialize (10ms @ ~1000 iterations/ms)
                for (volatile uint32_t delay = 0; delay < 10000; delay++);
            } else {
                kwarn("Failed to start AP %u (error: %d)\n", i, result);
            }
        }
    }

    kinfo("Started %u Application Processor(s)\n", ap_count);
    kinfo("Total CPUs online: %u\n", topo->num_cpus);
    kinfo("========================================\n\n");

    // Skip VMM mapping test - causes hangs with kprintf formatting
    // TODO: Fix kprintf %016lx formatting issue
    kinfo("Skipping VMM mapping test\n");

    // Kernel Heap Allocator
    extern void heap_init(void);
    kinfo("Initializing Kernel Heap...\n");
    heap_init();
    kinfo("Heap init returned successfully\n");
    
    // Memory Mapping System
    extern error_code_t mmap_init(void);
    kinfo("Initializing Memory Mapping System...\n");
    mmap_init();

    // Thread Scheduler
    extern void scheduler_init(void);
    kinfo("Initializing Scheduler...\n");
    scheduler_init();
    kinfo("Scheduler init returned\n");

    // Enable scheduler ticks in timer interrupt
    // TODO: Debug why this causes hang - disabled for now
    // kinfo("Enabling scheduler ticks...\n");
    // extern void timer_enable_scheduler(void);
    // timer_enable_scheduler();
    kinfo("Scheduler ticks DISABLED for debugging\n");

    // IPC System
    extern void ipc_init(void);
    kinfo("Initializing IPC System...\n");
    ipc_init();

    // System Calls
    extern void syscall_init(void);
    kinfo("Initializing System Calls...\n");
    syscall_init();

    kinfo("\n========================================\n");
    kinfo("Phase 2 initialization complete!\n");
    kinfo("========================================\n");
    
    // ========================================================================
    // Phase 3 Initialization (Userspace Foundation)
    // ========================================================================
    kinfo("=== Phase 3 Initialization (Userspace) ===\n");
    
    // Process Management
    extern void process_init(void);
    kinfo("Initializing Process Management...\n");
    process_init();
    
    // Block Device System
    extern error_code_t block_device_init(void);
    kinfo("Initializing block device system...\n");
    block_device_init();
    
    // PCI enumeration (needed for AHCI and network cards)
    extern error_code_t pci_init(void);
    kinfo("Initializing PCI subsystem...\n");
    pci_init();
    
    // Network stack (needed before ethernet driver registration)
    extern error_code_t network_init(void);
    kinfo("Initializing Network Stack...\n");
    network_init();
    
    // Ethernet driver (probes PCI for network cards)
    extern error_code_t ethernet_driver_init(void);
    kinfo("Initializing Ethernet driver...\n");
    ethernet_driver_init();
    
    // Storage Drivers
    extern error_code_t ata_init(void);
    kinfo("Initializing ATA driver...\n");
    ata_init();
    
    extern error_code_t ahci_init(void);
    kinfo("Initializing AHCI driver...\n");
    ahci_init();
    
    // Input Event System
    extern error_code_t input_event_init(void);
    kinfo("Initializing input event system...\n");
    input_event_init();
    
    // Input Drivers
    extern error_code_t keyboard_init(void);
    kinfo("Initializing keyboard...\n");
    keyboard_init();
    
    extern error_code_t mouse_init(void);
    kinfo("Initializing mouse...\n");
    mouse_init();
    
    // User & Group System
    extern error_code_t user_init(void);
    kinfo("Initializing user system...\n");
    user_init();
    
    // Memory Protection
    extern error_code_t memory_protection_init(void);
    kinfo("Initializing memory protection...\n");
    memory_protection_init();
    
    // Virtual File System
    extern error_code_t vfs_init(void);
    kinfo("Initializing VFS...\n");
    vfs_init();
    
    // Register FAT32 filesystem
    extern error_code_t fat32_register_vfs(void);
    kinfo("Registering FAT32 filesystem...\n");
    fat32_register_vfs();
    
    // Shell (initialize but don't run yet - will launch in userspace)
    extern void shell_init(void);
    kinfo("Initializing Shell...\n");
    shell_init();
    
    // Theme System
    extern error_code_t theme_init(void);
    kinfo("Initializing Theme System...\n");
    theme_init();
    
    // Window Manager
    extern error_code_t window_manager_init(void);
    kinfo("Initializing Window Manager...\n");
    window_manager_init();
    
    // 2D Graphics Acceleration
    extern error_code_t gfx_accel_init(void);
    kinfo("Initializing 2D Graphics Acceleration...\n");
    gfx_accel_init();

    // Initialize double buffering for smooth rendering
    extern void gfx_init_double_buffer(void);
    kinfo("Initializing Double Buffering...\n");
    gfx_init_double_buffer();
    
    // Note: VirtIO GPU initialization would happen via PCI enumeration
    // In QEMU, VirtIO devices appear as PCI devices that can be detected
    // The acceleration layer will use software fallback if no GPU is found
    
    // Network Stack (already initialized earlier, before ethernet driver)
    extern error_code_t arp_init(void);
    kinfo("Initializing ARP...\n");
    arp_init();
    
    extern error_code_t icmp_init(void);
    kinfo("Initializing ICMP...\n");
    icmp_init();
    
    extern error_code_t tcp_init(void);
    kinfo("Initializing TCP...\n");
    tcp_init();
    
    extern error_code_t socket_init(void);
    kinfo("Initializing Socket System...\n");
    socket_init();
    
    // Desktop Environment
    extern error_code_t desktop_init(void);
    kinfo("Initializing Desktop Environment...\n");
    desktop_init();
    
    extern error_code_t taskbar_init(void);
    kinfo("Initializing Taskbar...\n");
    taskbar_init();
    
    extern error_code_t launcher_init(void);
    kinfo("Initializing Application Launcher...\n");
    launcher_init();
    
    extern error_code_t login_screen_init(void);
    kinfo("Initializing Login Screen...\n");
    login_screen_init();
    
    // Update boot splash
    extern error_code_t bootsplash_set_message(const char* message);
    extern error_code_t bootsplash_set_progress(uint32_t percent);
    bootsplash_set_message("Booting complete!");
    bootsplash_set_progress(100);
    
    kinfo("\n========================================\n");
    kinfo("Phase 3 initialization complete!\n");
    kinfo("========================================\n");
    
    // Run kernel tests
    extern void run_all_tests(void);
    run_all_tests();
    
    // ========================================================================
    // Desktop Environment Main Loop
    // ========================================================================
    kinfo("\n========================================\n");
    kinfo("Starting Desktop Environment...\n");
    kinfo("========================================\n");
    
    // Show login screen initially
    extern error_code_t login_screen_show(void);
    extern bool login_screen_is_logged_in(void);
    extern error_code_t login_screen_render(void);
    extern error_code_t login_screen_handle_input(void);
    
    login_screen_show();
    
    // Import desktop rendering functions
    extern error_code_t desktop_render(void);
    extern error_code_t taskbar_render(void);
    extern error_code_t window_manager_render_all(void);
    extern error_code_t window_manager_handle_input(void);
    extern void gfx_swap_buffers(void);
    
    kinfo("Entering main desktop loop...\n");
    
    // Show login screen
    extern error_code_t login_screen_show(void);
    login_screen_show();
    
    kinfo("\n========================================\n");
    kinfo("Kernel initialization complete!\n");
    kinfo("Desktop will run in userspace (Ring 3)\n");
    kinfo("========================================\n\n");
    
    // Launch userspace shell (which drives the desktop)
    extern error_code_t launch_shell_userspace(void);
    launch_shell_userspace();

    kinfo("Entering kernel idle loop...\n");
    
    // Kernel Idle Loop
    // Desktop rendering now happens in userspace via syscalls
    while (1) {
        __asm__ volatile("hlt");
    }
}

/**
 * Print kernel banner
 */
static void print_banner(void) {
    kprintf("\n");
    kprintf("====================================================\n");
    kprintf("                  Scarlett OS - DEBUG BUILD         \n");
    kprintf("        A Modern Microkernel Operating System      \n");
    kprintf("====================================================\n");
    kprintf("Version: 0.1.0 (Phase 1 - Development)\n");
    kprintf("Architecture: x86_64\n");
    kprintf("Build: %s %s\n", __DATE__, __TIME__);
    kprintf("====================================================\n");
    kprintf("\n");
}

/**
 * Verify boot info structure
 */
static void verify_boot_info(boot_info_t* boot_info) {
    kinfo("Verifying boot information...\n");
    
    // Check magic number
    if (boot_info->magic != BOOT_INFO_MAGIC) {
        kerror("Invalid boot info magic: 0x%016lx\n", boot_info->magic);
        kpanic("Boot info verification failed!");
    }
    
    // Check memory map
    if (boot_info->memory_map_count == 0) {
        kpanic("No memory regions in memory map!");
    }
    
    if (boot_info->memory_map_count > MAX_MEMORY_REGIONS) {
        kerror("Too many memory regions: %u\n", boot_info->memory_map_count);
        kpanic("Memory map overflow!");
    }
    
    kinfo("Boot info verified successfully\n");
}

/**
 * Print memory map
 */
static void print_memory_map(boot_info_t* boot_info) {
    kinfo("\nMemory Map (%u regions):\n", boot_info->memory_map_count);
    kprintf("  %-18s %-18s %-12s %s\n", "Base", "Length", "Pages", "Type");
    kprintf("  %s\n", "---------------------------------------------------------------");
    
    uint64_t total_memory = 0;
    uint64_t usable_memory = 0;
    
    for (uint32_t i = 0; i < boot_info->memory_map_count; i++) {
        memory_region_t* region = &boot_info->memory_map[i];
        uint64_t pages = region->length / 4096;
        
        const char* type_name = "Unknown";
        switch (region->type) {
            case MEMORY_TYPE_CONVENTIONAL:
                type_name = "Available";
                usable_memory += region->length;
                break;
            case MEMORY_TYPE_RESERVED:
                type_name = "Reserved";
                break;
            case MEMORY_TYPE_ACPI_RECLAIM:
                type_name = "ACPI Reclaim";
                break;
            case MEMORY_TYPE_ACPI_NVS:
                type_name = "ACPI NVS";
                break;
            case MEMORY_TYPE_UNUSABLE:
                type_name = "Unusable";
                break;
            case MEMORY_TYPE_LOADER_CODE:
                type_name = "Loader Code";
                break;
            case MEMORY_TYPE_LOADER_DATA:
                type_name = "Loader Data";
                break;
            case MEMORY_TYPE_BOOT_SERVICES_CODE:
                type_name = "Boot Code";
                break;
            case MEMORY_TYPE_BOOT_SERVICES_DATA:
                type_name = "Boot Data";
                break;
            default:
                break;
        }
        
        kprintf("Region %u: 0x%016lx 0x%016lx %-12lu %s\n",
                i, region->base, region->length, pages, type_name);
        
        total_memory += region->length;
    }
    
    kprintf("  %s\n", "---------------------------------------------------------------");
    kprintf("  Total Memory:   %lu MB\n", total_memory / (1024 * 1024));
    kprintf("  Usable Memory:  %lu MB\n", usable_memory / (1024 * 1024));
    kprintf("\n");
}

/**
 * Kernel panic - print message and halt
 */
void kpanic(const char* msg) {
    kprintf("\n");
    kprintf("************************* KERNEL PANIC *************************\n");
    kprintf("* %s\n", msg);
    kprintf("****************************************************************\n");
    kprintf("\n");
    kprintf("System halted.\n");
    
    // Disable interrupts and halt
    __asm__ volatile("cli");
    while (1) {
        __asm__ volatile("hlt");
    }
    
    __builtin_unreachable();
}

