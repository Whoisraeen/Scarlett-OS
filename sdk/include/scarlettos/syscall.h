/**
 * @file syscall.h
 * @brief ScarlettOS System Call Interface
 */

#ifndef SCARLETTOS_SYSCALL_H
#define SCARLETTOS_SYSCALL_H

#include "types.h"

// System call numbers
#define SYS_EXIT            0
#define SYS_FORK            1
#define SYS_READ            2
#define SYS_WRITE           3
#define SYS_OPEN            4
#define SYS_CLOSE           5
#define SYS_MMAP            6
#define SYS_MUNMAP          7
#define SYS_IPC_SEND        8
#define SYS_IPC_RECV        9
#define SYS_THREAD_CREATE   10
#define SYS_THREAD_EXIT     11
#define SYS_SLEEP           12
#define SYS_GETPID          13
#define SYS_GETTID          14

// System call wrapper
static inline uint64_t syscall(uint64_t num, uint64_t arg1, uint64_t arg2, 
                                uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    uint64_t ret;
    register uint64_t r10 __asm__("r10") = arg4;
    register uint64_t r8 __asm__("r8") = arg5;
    
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10), "r"(r8)
        : "rcx", "r11", "memory"
    );
    
    return ret;
}

// Convenience wrappers
static inline void sys_exit(int code) {
    syscall(SYS_EXIT, code, 0, 0, 0, 0);
}

static inline pid_t sys_fork(void) {
    return (pid_t)syscall(SYS_FORK, 0, 0, 0, 0, 0);
}

static inline ssize_t sys_read(fd_t fd, void* buf, size_t count) {
    return (ssize_t)syscall(SYS_READ, fd, (uint64_t)buf, count, 0, 0);
}

static inline ssize_t sys_write(fd_t fd, const void* buf, size_t count) {
    return (ssize_t)syscall(SYS_WRITE, fd, (uint64_t)buf, count, 0, 0);
}

static inline pid_t sys_getpid(void) {
    return (pid_t)syscall(SYS_GETPID, 0, 0, 0, 0, 0);
}

static inline tid_t sys_gettid(void) {
    return (tid_t)syscall(SYS_GETTID, 0, 0, 0, 0, 0);
}

#endif // SCARLETTOS_SYSCALL_H
