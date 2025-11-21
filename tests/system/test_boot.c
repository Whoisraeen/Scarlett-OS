/**
 * @file test_boot.c
 * @brief System Boot Test
 */

#include "../test_framework.h"

// System state checks
extern int check_kernel_initialized(void);
extern int check_memory_manager_ready(void);
extern int check_scheduler_running(void);
extern int check_ipc_ready(void);
extern int check_vfs_mounted(void);
extern int check_network_up(void);

TEST(kernel_initialized) {
    ASSERT_EQ(check_kernel_initialized(), 1);
}

TEST(memory_manager_ready) {
    ASSERT_EQ(check_memory_manager_ready(), 1);
}

TEST(scheduler_running) {
    ASSERT_EQ(check_scheduler_running(), 1);
}

TEST(ipc_ready) {
    ASSERT_EQ(check_ipc_ready(), 1);
}

TEST(vfs_mounted) {
    ASSERT_EQ(check_vfs_mounted(), 1);
}

TEST(network_up) {
    ASSERT_EQ(check_network_up(), 1);
}

int main(void) {
    test_init();
    
    printf("=== System Boot Test ===\n");
    RUN_TEST(kernel_initialized);
    RUN_TEST(memory_manager_ready);
    RUN_TEST(scheduler_running);
    RUN_TEST(ipc_ready);
    RUN_TEST(vfs_mounted);
    RUN_TEST(network_up);
    
    test_print_results();
    return test_get_exit_code();
}
