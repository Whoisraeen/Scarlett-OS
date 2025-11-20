/**
 * @file test_services.c
 * @brief Test user-space service startup and IPC
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/ipc/ipc.h"
#include "../../kernel/include/syscall/syscall.h"
#include "../../kernel/include/kprintf.h"
#include "../../kernel/include/debug.h"
#include "../../kernel/include/string.h"

/**
 * Test IPC port creation
 */
void test_ipc_ports(void) {
    kinfo("=== Testing IPC Ports ===\n");
    
    // Create IPC ports
    uint64_t port1 = ipc_create_port();
    uint64_t port2 = ipc_create_port();
    
    if (port1 == 0 || port2 == 0) {
        kerror("[FAIL] IPC port creation failed\n");
        return;
    }
    
    kinfo("[PASS] IPC ports created: %lu, %lu\n", port1, port2);
    
    // Test port destruction
    if (ipc_destroy_port(port1) != 0) {
        kerror("[FAIL] IPC port destruction failed\n");
        return;
    }
    
    kinfo("[PASS] IPC port destroyed\n");
    
    // Cleanup
    ipc_destroy_port(port2);
    kinfo("[PASS] IPC port test complete\n");
}

/**
 * Test IPC message sending
 */
void test_ipc_messages(void) {
    kinfo("=== Testing IPC Messages ===\n");
    
    // Create ports
    uint64_t port1 = ipc_create_port();
    uint64_t port2 = ipc_create_port();
    
    if (port1 == 0 || port2 == 0) {
        kerror("[FAIL] Port creation failed\n");
        return;
    }
    
    // Create test message
    ipc_message_t msg;
    msg.sender_tid = 1;
    msg.msg_id = 1;
    msg.type = IPC_MSG_REQUEST;
    msg.inline_size = 4;
    uint32_t test_data = 0x12345678;
    memcpy(msg.inline_data, &test_data, 4);
    msg.buffer = NULL;
    msg.buffer_size = 0;
    
    // Send message
    if (ipc_send(port2, &msg) != 0) {
        kerror("[FAIL] IPC send failed\n");
        ipc_destroy_port(port1);
        ipc_destroy_port(port2);
        return;
    }
    
    kinfo("[PASS] IPC message sent\n");
    
    // Receive message
    ipc_message_t received;
    if (ipc_receive(port2, &received) != 0) {
        kerror("[FAIL] IPC receive failed\n");
        ipc_destroy_port(port1);
        ipc_destroy_port(port2);
        return;
    }
    
    // Verify message
    if (received.msg_id != 1 || received.type != IPC_MSG_REQUEST) {
        kerror("[FAIL] Message verification failed\n");
        ipc_destroy_port(port1);
        ipc_destroy_port(port2);
        return;
    }
    
    uint32_t received_data;
    memcpy(&received_data, received.inline_data, 4);
    if (received_data != test_data) {
        kerror("[FAIL] Message data mismatch\n");
        ipc_destroy_port(port1);
        ipc_destroy_port(port2);
        return;
    }
    
    kinfo("[PASS] IPC message received and verified\n");
    
    // Cleanup
    ipc_destroy_port(port1);
    ipc_destroy_port(port2);
    kinfo("[PASS] IPC message test complete\n");
}

/**
 * Test service discovery (placeholder)
 */
void test_service_discovery(void) {
    kinfo("=== Testing Service Discovery ===\n");
    
    // TODO: Test service registration
    // TODO: Test service lookup
    // TODO: Test service communication
    
    kinfo("[INFO] Service discovery test (placeholder - needs init service)\n");
}

