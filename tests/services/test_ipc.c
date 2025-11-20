/**
 * @file test_ipc.c
 * @brief IPC communication test between services
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/ipc/ipc.h"
#include "../../kernel/include/syscall/syscall.h"
#include "../../kernel/include/kprintf.h"

/**
 * Test IPC communication between services
 */
void test_ipc_communication(void) {
    kinfo("Testing IPC communication...\n");
    
    // Create IPC ports
    uint64_t port1 = ipc_create_port();
    uint64_t port2 = ipc_create_port();
    
    if (port1 == 0 || port2 == 0) {
        kerror("Failed to create IPC ports\n");
        return;
    }
    
    kinfo("Created ports: %lu, %lu\n", port1, port2);
    
    // Create test message
    ipc_message_t msg;
    msg.sender_tid = 0;
    msg.msg_id = 1;
    msg.type = IPC_MSG_REQUEST;
    msg.inline_size = 4;
    uint32_t test_data = 0x12345678;
    memcpy(msg.inline_data, &test_data, 4);
    msg.buffer = NULL;
    msg.buffer_size = 0;
    
    // Send message
    if (ipc_send(port2, &msg) != 0) {
        kerror("Failed to send IPC message\n");
        return;
    }
    
    kinfo("Sent message to port %lu\n", port2);
    
    // Receive message
    ipc_message_t received;
    if (ipc_receive(port2, &received) != 0) {
        kerror("Failed to receive IPC message\n");
        return;
    }
    
    // Verify message
    if (received.msg_id != 1 || received.type != IPC_MSG_REQUEST) {
        kerror("Received message doesn't match sent message\n");
        return;
    }
    
    uint32_t received_data;
    memcpy(&received_data, received.inline_data, 4);
    if (received_data != test_data) {
        kerror("Received data doesn't match sent data\n");
        return;
    }
    
    kinfo("IPC communication test PASSED\n");
    
    // Cleanup
    ipc_destroy_port(port1);
    ipc_destroy_port(port2);
}

/**
 * Test capability enforcement
 */
void test_capability_enforcement(void) {
    kinfo("Testing capability enforcement...\n");
    
    // Create capability for IPC port
    uint64_t port = ipc_create_port();
    if (port == 0) {
        kerror("Failed to create IPC port\n");
        return;
    }
    
    // Create capability with read right only
    uint64_t cap_id = capability_create(CAP_TYPE_IPC_PORT, port, CAP_RIGHT_READ);
    if (cap_id == 0) {
        kerror("Failed to create capability\n");
        return;
    }
    
    kinfo("Created capability %lu for port %lu\n", cap_id, port);
    
    // Test capability check
    if (!capability_check(cap_id, CAP_RIGHT_READ)) {
        kerror("Capability check failed for read right\n");
        return;
    }
    
    if (capability_check(cap_id, CAP_RIGHT_WRITE)) {
        kerror("Capability incorrectly grants write right\n");
        return;
    }
    
    kinfo("Capability enforcement test PASSED\n");
    
    // Cleanup
    capability_revoke(cap_id);
    ipc_destroy_port(port);
}

