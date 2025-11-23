#include "../kernel/include/types.h"
#include "../kernel/include/kprintf.h"
#include "framework/test.h"

void test_syscall_dispatch(void) {
    TEST_START("Syscall Dispatch");
    TEST_ASSERT(1 == 1); // Placeholder
    TEST_PASS();
}

void run_syscall_tests(void) {
    test_syscall_dispatch();
}
