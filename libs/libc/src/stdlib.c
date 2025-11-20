/**
 * @file stdlib.c
 * @brief Standard library implementations
 */

#include "stdlib.h"
#include "syscall.h"

void exit(int status) {
    sys_exit(status);
    __builtin_unreachable();
}

void abort(void) {
    // TODO: Send signal or panic
    exit(1);
}

