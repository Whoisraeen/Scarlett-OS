/**
 * @file test_all.c
 * @brief Run all boot sequence tests
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kprintf.h"

// Test function declarations
extern void test_scheduler_init(void);
extern void test_process_creation(void);
extern void test_ipc_ports(void);
extern void test_ipc_messages(void);
extern void test_service_discovery(void);
extern void test_vfs_init(void);
extern void test_file_operations(void);
extern void test_directory_operations(void);
extern void test_network_init(void);
extern void test_ip_protocol(void);
extern void test_tcp_connection(void);
extern void test_framebuffer(void);
extern void test_graphics_rendering(void);

/**
 * Run all boot sequence tests
 */
void run_boot_tests(void) {
    kinfo("\n");
    kinfo("========================================\n");
    kinfo("  Boot Sequence Test Suite\n");
    kinfo("========================================\n\n");
    
    // 1. Continue Boot Sequence
    kinfo("=== 1. Continue Boot Sequence ===\n\n");
    test_scheduler_init();
    kinfo("\n");
    test_process_creation();
    kinfo("\n");
    test_service_discovery();
    kinfo("\n");
    
    // 2. Service Testing
    kinfo("=== 2. Service Testing ===\n\n");
    test_ipc_ports();
    kinfo("\n");
    test_ipc_messages();
    kinfo("\n");
    test_vfs_init();
    kinfo("\n");
    test_network_init();
    kinfo("\n");
    
    // 3. Functional Testing
    kinfo("=== 3. Functional Testing ===\n\n");
    test_file_operations();
    kinfo("\n");
    test_directory_operations();
    kinfo("\n");
    test_ip_protocol();
    kinfo("\n");
    test_tcp_connection();
    kinfo("\n");
    test_framebuffer();
    kinfo("\n");
    test_graphics_rendering();
    kinfo("\n");
    
    kinfo("========================================\n");
    kinfo("  All Boot Tests Complete\n");
    kinfo("========================================\n");
}

