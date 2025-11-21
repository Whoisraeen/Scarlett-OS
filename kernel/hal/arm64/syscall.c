/**
 * @file syscall.c
 * @brief ARM64 system call handling
 */

#include "../../include/types.h"
#include "../../include/hal/hal.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"

// Syscall numbers (should match userspace)
#define SYS_EXIT        1
#define SYS_READ        2
#define SYS_WRITE       3
#define SYS_OPEN        4
#define SYS_CLOSE       5
#define SYS_IPC_SEND    10
#define SYS_IPC_RECEIVE 11

// Saved register state from syscall
typedef struct {
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7;
    uint64_t x8;  // Syscall number
    uint64_t x9, x10, x11, x12, x13, x14, x15, x16, x17, x18;
    uint64_t x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    uint64_t x29, x30;  // FP, LR
    uint64_t sp, pc, pstate;
} syscall_regs_t;

// Syscall handler called from vectors.S
uint64_t arm64_syscall_handler(uint64_t syscall_num, syscall_regs_t* regs) {
    uint64_t result = 0;

    switch (syscall_num) {
        case SYS_EXIT:
            // TODO: Implement process exit
            kinfo("Syscall: exit(%lu)\n", regs->x0);
            result = 0;
            break;

        case SYS_READ:
            // TODO: Implement read
            kinfo("Syscall: read(%lu, %p, %lu)\n", regs->x0, (void*)regs->x1, regs->x2);
            result = 0;
            break;

        case SYS_WRITE:
            // TODO: Implement write
            kinfo("Syscall: write(%lu, %p, %lu)\n", regs->x0, (void*)regs->x1, regs->x2);
            result = regs->x2;  // Return bytes written
            break;

        case SYS_OPEN:
            // TODO: Implement open
            kinfo("Syscall: open(%p, %lu)\n", (void*)regs->x0, regs->x1);
            result = 3;  // Return fake FD
            break;

        case SYS_CLOSE:
            // TODO: Implement close
            kinfo("Syscall: close(%lu)\n", regs->x0);
            result = 0;
            break;

        case SYS_IPC_SEND:
            // TODO: Implement IPC send
            kinfo("Syscall: ipc_send(%lu, %p)\n", regs->x0, (void*)regs->x1);
            result = 0;
            break;

        case SYS_IPC_RECEIVE:
            // TODO: Implement IPC receive
            kinfo("Syscall: ipc_receive(%lu, %p)\n", regs->x0, (void*)regs->x1);
            result = 0;
            break;

        default:
            kwarn("Unknown syscall: %lu\n", syscall_num);
            result = (uint64_t)-1;  // Error
            break;
    }

    // Return value goes in x0
    regs->x0 = result;
    return result;
}

error_code_t arm64_syscall_init(void) {
    kinfo("ARM64 syscall interface initialized\n");
    // Syscall handling is set up in vectors.S
    return ERR_OK;
}
