/**
 * @file syscall.h
 * @brief System call interface
 */

#ifndef KERNEL_SYSCALL_SYSCALL_H
#define KERNEL_SYSCALL_SYSCALL_H

#include "../types.h"

// System call numbers
#define SYS_EXIT        0
#define SYS_WRITE       1
#define SYS_READ        2
#define SYS_OPEN        3
#define SYS_CLOSE       4
#define SYS_SLEEP       5
#define SYS_YIELD       6
#define SYS_THREAD_CREATE 7
#define SYS_THREAD_EXIT 8
#define SYS_IPC_SEND    9
#define SYS_IPC_RECEIVE 10
#define SYS_MMAP        11
#define SYS_MUNMAP      12
#define SYS_GETPID      13
#define SYS_GETUID      14
#define SYS_FORK        15
#define SYS_EXEC        16
#define SYS_WAIT        17
#define SYS_BRK         18
#define SYS_GETCWD      19
#define SYS_CHDIR       20
#define SYS_SET_AFFINITY 21
#define SYS_GET_AFFINITY 22
#define SYS_DESKTOP_RENDER 23
#define SYS_TASKBAR_RENDER 24
#define SYS_GFX_SWAP_BUFFERS 25
#define SYS_IPC_CREATE_PORT 26
#define SYS_IPC_DESTROY_PORT 27
#define SYS_PCI_READ_CONFIG 28
#define SYS_PCI_WRITE_CONFIG 29
#define SYS_IRQ_REGISTER 30
#define SYS_IRQ_UNREGISTER 31
#define SYS_IRQ_ENABLE 32
#define SYS_IRQ_DISABLE 33
#define SYS_DMA_ALLOC 34
#define SYS_DMA_FREE 35
#define SYS_MMIO_MAP 36
#define SYS_MMIO_UNMAP 37
#define SYS_CAPABILITY_CREATE 38
#define SYS_CAPABILITY_CHECK 39
#define SYS_SHM_CREATE 40
#define SYS_SHM_MAP 41
#define SYS_SHM_UNMAP 42
#define SYS_SHM_DESTROY 43
#define SYS_SHM_GET_INFO 44

// Maximum syscall number
#define SYS_MAX         44

/**
 * Initialize system call handling
 */
void syscall_init(void);

/**
 * System call handler (called from assembly)
 */
uint64_t syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, 
                         uint64_t arg3, uint64_t arg4, uint64_t arg5);

#endif // KERNEL_SYSCALL_SYSCALL_H

