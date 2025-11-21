/**
 * @file test_ipc.c
 * @brief IPC System Tests
 */

#include "../test_framework.h"

// Mock IPC functions
extern int ipc_create_port(uint32_t* port);
extern int ipc_destroy_port(uint32_t port);
extern int ipc_send(uint32_t port, const void* data, uint32_t size);
extern int ipc_recv(uint32_t port, void* buffer, uint32_t size, uint32_t timeout);

TEST(ipc_create_destroy_port) {
    uint32_t port;
    int ret = ipc_create_port(&port);
    ASSERT_EQ(ret, 0);
    
    ret = ipc_destroy_port(port);
    ASSERT_EQ(ret, 0);
}

TEST(ipc_send_recv) {
    uint32_t port;
    ipc_create_port(&port);
    
    const char* msg = "Hello, IPC!";
    int ret = ipc_send(port, msg, 12);
    ASSERT_EQ(ret, 0);
    
    char buffer[64];
    ret = ipc_recv(port, buffer, 64, 1000);
    ASSERT_EQ(ret, 12);
    
    ipc_destroy_port(port);
}

TEST(ipc_multiple_messages) {
    uint32_t port;
    ipc_create_port(&port);
    
    for (int i = 0; i < 10; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Message %d", i);
        int ret = ipc_send(port, msg, strlen(msg) + 1);
        ASSERT_EQ(ret, 0);
    }
    
    ipc_destroy_port(port);
}

int main(void) {
    test_init();
    
    printf("=== IPC Tests ===\n");
    RUN_TEST(ipc_create_destroy_port);
    RUN_TEST(ipc_send_recv);
    RUN_TEST(ipc_multiple_messages);
    
    test_print_results();
    return test_get_exit_code();
}
