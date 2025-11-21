/**
 * @file test_capability.c
 * @brief Capability System Unit Tests
 */

#include "../test_framework.h"

// Mock capability functions
extern int cap_create(uint32_t permissions);
extern int cap_grant(int cap_id, uint32_t pid);
extern int cap_revoke(int cap_id);
extern int cap_check(int cap_id, uint32_t permission);

#define CAP_READ  0x01
#define CAP_WRITE 0x02
#define CAP_EXEC  0x04

TEST(cap_create_revoke) {
    int cap = cap_create(CAP_READ | CAP_WRITE);
    ASSERT(cap >= 0);
    
    int ret = cap_revoke(cap);
    ASSERT_EQ(ret, 0);
}

TEST(cap_permissions) {
    int cap = cap_create(CAP_READ);
    ASSERT(cap >= 0);
    
    int ret = cap_check(cap, CAP_READ);
    ASSERT_EQ(ret, 1);
    
    ret = cap_check(cap, CAP_WRITE);
    ASSERT_EQ(ret, 0);
    
    cap_revoke(cap);
}

TEST(cap_grant) {
    int cap = cap_create(CAP_READ | CAP_WRITE);
    ASSERT(cap >= 0);
    
    int ret = cap_grant(cap, 1234); // Grant to PID 1234
    ASSERT_EQ(ret, 0);
    
    cap_revoke(cap);
}

TEST(cap_invalid) {
    int ret = cap_check(9999, CAP_READ);
    ASSERT_EQ(ret, 0);
}

int main(void) {
    test_init();
    
    printf("=== Capability System Tests ===\n");
    RUN_TEST(cap_create_revoke);
    RUN_TEST(cap_permissions);
    RUN_TEST(cap_grant);
    RUN_TEST(cap_invalid);
    
    test_print_results();
    return test_get_exit_code();
}
