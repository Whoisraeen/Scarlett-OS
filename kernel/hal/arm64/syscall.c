/**
 * @file syscall.c
 * @brief ARM64 system call handling
 */

#include "../../include/types.h"
#include "../../include/hal/hal.h"
#include "../../include/errors.h"
#include "../../include/kprintf.h"
#include "../../include/process.h"
#include "../../include/fs/vfs.h"
#include "../../include/ipc/ipc.h"
#include "../../include/mm/vmm.h"

// Syscall numbers (should match kernel/include/syscall/syscall.h)
#define SYS_EXIT        0
#define SYS_WRITE       1
#define SYS_READ        2
#define SYS_OPEN        3
#define SYS_CLOSE       4
#define SYS_IPC_SEND    9
#define SYS_IPC_RECEIVE 10

// Saved register state from syscall
typedef struct {
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7;
    uint64_t x8;  // Syscall number
    uint64_t x9, x10, x11, x12, x13, x14, x15, x16, x17, x18;
    uint64_t x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    uint64_t x29, x30;  // FP, LR
    uint64_t sp, pc, pstate;
} syscall_regs_t;

// Validate user pointer
static bool validate_user_ptr(void* ptr, size_t size) {
    // Check if pointer is in user space (ARM64: lower 48 bits, user space typically < 0x0000800000000000)
    uint64_t addr = (uint64_t)ptr;
    if (addr >= 0x0000800000000000ULL) {
        return false;  // Kernel space
    }
    // Check if range is valid (simple check - should be enhanced)
    if (addr + size < addr) {
        return false;  // Overflow
    }
    return true;
}

// Syscall handler called from vectors.S
uint64_t arm64_syscall_handler(uint64_t syscall_num, syscall_regs_t* regs) {
    uint64_t result = 0;

    switch (syscall_num) {
        case SYS_EXIT: {
            // arg1 (x0) = exit code
            int exit_code = (int)regs->x0;
            process_t* current = process_get_current();
            if (current) {
                process_exit(current, exit_code);
            } else {
                // If no process, just halt
                __asm__ volatile("wfi");
            }
            result = 0;  // Never reached
            break;
        }

        case SYS_READ: {
            // arg1 (x0) = fd, arg2 (x1) = buffer, arg3 (x2) = size
            int fd = (int)regs->x0;
            void* buf = (void*)regs->x1;
            size_t size = (size_t)regs->x2;
            
            // Validate arguments
            if (!validate_user_ptr(buf, size)) {
                result = (uint64_t)ERR_INVALID_ARG;
                break;
            }
            
            // Special file descriptor (stdin)
            if (fd == 0) {
                extern char serial_getc(void);
                char* cbuf = (char*)buf;
                size_t read = 0;
                for (size_t i = 0; i < size; i++) {
                    cbuf[i] = serial_getc();
                    read++;
                    if (cbuf[i] == '\n' || cbuf[i] == '\r') {
                        break;
                    }
                }
                result = read;
                break;
            }
            
            // Regular file - use VFS
            size_t bytes_read = 0;
            error_code_t err = vfs_read((int32_t)fd, buf, size, &bytes_read);
            if (err != ERR_OK) {
                result = (uint64_t)err;
            } else {
                result = bytes_read;
            }
            break;
        }

        case SYS_WRITE: {
            // arg1 (x0) = fd, arg2 (x1) = buffer, arg3 (x2) = size
            int fd = (int)regs->x0;
            void* buf = (void*)regs->x1;
            size_t size = (size_t)regs->x2;
            
            // Validate arguments
            if (!validate_user_ptr(buf, size)) {
                result = (uint64_t)ERR_INVALID_ARG;
                break;
            }
            
            // Special file descriptors (stdout, stderr)
            if (fd == 1 || fd == 2) {
                char* cbuf = (char*)buf;
                for (size_t i = 0; i < size; i++) {
                    kputc(cbuf[i]);
                }
                result = size;
                break;
            }
            
            // Regular file - use VFS
            size_t bytes_written = 0;
            error_code_t err = vfs_write((int32_t)fd, buf, size, &bytes_written);
            if (err != ERR_OK) {
                result = (uint64_t)err;
            } else {
                result = bytes_written;
            }
            break;
        }

        case SYS_OPEN: {
            // arg1 (x0) = path, arg2 (x1) = flags, arg3 (x2) = mode
            const char* path = (const char*)regs->x0;
            uint64_t flags = regs->x1;
            (void)regs->x2;  // mode unused for now
            
            // Validate path
            if (!validate_user_ptr((void*)path, 256)) {
                result = (uint64_t)ERR_INVALID_ARG;
                break;
            }
            
            // Open file via VFS
            int32_t fd;
            error_code_t err = vfs_open(path, flags, &fd);
            if (err != ERR_OK) {
                result = (uint64_t)err;
            } else {
                result = (uint64_t)fd;
            }
            break;
        }

        case SYS_CLOSE: {
            // arg1 (x0) = fd
            int fd = (int)regs->x0;
            
            // Close file via VFS
            error_code_t err = vfs_close((int32_t)fd);
            result = (uint64_t)err;
            break;
        }

        case SYS_IPC_SEND: {
            // arg1 (x0) = port_id, arg2 (x1) = message
            if (!validate_user_ptr((void*)regs->x1, sizeof(ipc_message_t))) {
                result = (uint64_t)ERR_INVALID_ARG;
                break;
            }
            result = (uint64_t)ipc_send(regs->x0, (ipc_message_t*)regs->x1);
            break;
        }

        case SYS_IPC_RECEIVE: {
            // arg1 (x0) = port_id, arg2 (x1) = message
            if (!validate_user_ptr((void*)regs->x1, sizeof(ipc_message_t))) {
                result = (uint64_t)ERR_INVALID_ARG;
                break;
            }
            result = (uint64_t)ipc_receive(regs->x0, (ipc_message_t*)regs->x1);
            break;
        }

        default:
            kwarn("Unknown syscall: %lu\n", syscall_num);
            result = (uint64_t)ERR_INVALID_SYSCALL;
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
