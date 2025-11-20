/**
 * @file unistd.c
 * @brief POSIX-like system call implementations
 */

#include "unistd.h"
#include "syscall.h"

pid_t getpid(void) {
    return sys_getpid();
}

uid_t getuid(void) {
    return sys_getuid();
}

void sleep(unsigned int seconds) {
    sys_sleep(seconds * 1000);
}

void usleep(unsigned int useconds) {
    sys_sleep(useconds / 1000);
}

void yield(void) {
    sys_yield();
}

