/**
 * @file syscall.h
 * @brief System call wrapper definitions for user-space
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

// System call numbers (must match kernel/include/syscall/syscall.h)
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

// IPC message structure (must match kernel/include/ipc/ipc.h)
typedef struct {
    uint64_t sender_tid;
    uint64_t msg_id;
    uint32_t type;
    uint32_t inline_size;
    uint8_t inline_data[64];
    void* buffer;
    size_t buffer_size;
} ipc_message_t;

// System call wrapper (architecture-specific)
static inline uint64_t syscall(uint64_t num, uint64_t arg1, uint64_t arg2,
                               uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    uint64_t ret;
    #if defined(__x86_64__)
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r10"(arg4), "r8"(arg5)
        : "rcx", "r11", "memory"
    );
    #elif defined(__aarch64__)
    __asm__ volatile(
        "mov x8, %1\n"
        "mov x0, %2\n"
        "mov x1, %3\n"
        "mov x2, %4\n"
        "mov x3, %5\n"
        "mov x4, %6\n"
        "svc #0\n"
        "mov %0, x0"
        : "=r"(ret)
        : "r"(num), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4), "r"(arg5)
        : "x0", "x1", "x2", "x3", "x4", "x8", "memory"
    );
    #elif defined(__riscv)
    __asm__ volatile(
        "ecall"
        : "=a"(ret)
        : "a"(num), "a1"(arg1), "a2"(arg2), "a3"(arg3), "a4"(arg4), "a5"(arg5)
        : "memory"
    );
    #else
    #error "Unsupported architecture"
    #endif
    return ret;
}

// Convenience wrappers
static inline void sys_exit(int status) {
    syscall(SYS_EXIT, status, 0, 0, 0, 0);
    __builtin_unreachable();
}

static inline ssize_t sys_write(int fd, const void* buf, size_t count) {
    return (ssize_t)syscall(SYS_WRITE, fd, (uint64_t)buf, count, 0, 0);
}

static inline ssize_t sys_read(int fd, void* buf, size_t count) {
    return (ssize_t)syscall(SYS_READ, fd, (uint64_t)buf, count, 0, 0);
}

static inline int sys_open(const char* path, int flags, int mode) {
    return (int)syscall(SYS_OPEN, (uint64_t)path, flags, mode, 0, 0);
}

static inline int sys_close(int fd) {
    return (int)syscall(SYS_CLOSE, fd, 0, 0, 0, 0);
}

static inline void sys_sleep(uint64_t ms) {
    syscall(SYS_SLEEP, ms, 0, 0, 0, 0);
}

static inline void sys_yield(void) {
    syscall(SYS_YIELD, 0, 0, 0, 0, 0);
}

static inline int sys_ipc_send(uint64_t port_id, ipc_message_t* msg) {
    return (int)syscall(SYS_IPC_SEND, port_id, (uint64_t)msg, 0, 0, 0);
}

static inline int sys_ipc_receive(uint64_t port_id, ipc_message_t* msg) {
    return (int)syscall(SYS_IPC_RECEIVE, port_id, (uint64_t)msg, 0, 0, 0);
}

static inline pid_t sys_getpid(void) {
    return (pid_t)syscall(SYS_GETPID, 0, 0, 0, 0, 0);
}

static inline uid_t sys_getuid(void) {
    return (uid_t)syscall(SYS_GETUID, 0, 0, 0, 0, 0);
}

static inline void* sys_mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return (void*)syscall(SYS_MMAP, (uint64_t)addr, length, prot, flags, fd, offset);
}

static inline int sys_munmap(void* addr, size_t length) {
    return (int)syscall(SYS_MUNMAP, (uint64_t)addr, length, 0, 0, 0);
}

static inline uint64_t sys_ipc_create_port(void) {
    return syscall(SYS_IPC_CREATE_PORT, 0, 0, 0, 0, 0);
}

static inline int sys_ipc_destroy_port(uint64_t port_id) {
    return (int)syscall(SYS_IPC_DESTROY_PORT, port_id, 0, 0, 0, 0);
}

static inline uint32_t sys_pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return (uint32_t)syscall(SYS_PCI_READ_CONFIG, bus, device, function, offset, 0);
}

static inline uint64_t sys_capability_create(uint32_t type, uint64_t resource_id, uint32_t rights) {
    return syscall(SYS_CAPABILITY_CREATE, type, resource_id, rights, 0, 0);
}

static inline bool sys_capability_check(uint64_t cap_id, uint32_t right) {
    return (bool)syscall(SYS_CAPABILITY_CHECK, cap_id, right, 0, 0, 0);
}

static inline int sys_pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    return (int)syscall(SYS_PCI_WRITE_CONFIG, bus, device, function, offset, value);
}

static inline int sys_irq_register(uint8_t irq, void (*handler)(void*), void* context) {
    return (int)syscall(SYS_IRQ_REGISTER, irq, (uint64_t)handler, (uint64_t)context, 0, 0);
}

static inline int sys_irq_unregister(uint8_t irq, void (*handler)(void*)) {
    return (int)syscall(SYS_IRQ_UNREGISTER, irq, (uint64_t)handler, 0, 0, 0);
}

static inline int sys_irq_enable(uint8_t irq) {
    return (int)syscall(SYS_IRQ_ENABLE, irq, 0, 0, 0, 0);
}

static inline int sys_irq_disable(uint8_t irq) {
    return (int)syscall(SYS_IRQ_DISABLE, irq, 0, 0, 0, 0);
}

static inline void* sys_dma_alloc(size_t size) {
    return (void*)syscall(SYS_DMA_ALLOC, size, 0, 0, 0, 0);
}

static inline int sys_dma_free(void* addr, size_t size) {
    return (int)syscall(SYS_DMA_FREE, (uint64_t)addr, size, 0, 0, 0);
}

static inline void* sys_mmio_map(uint64_t paddr, size_t size) {
    return (void*)syscall(SYS_MMIO_MAP, paddr, size, 0, 0, 0);
}

static inline int sys_mmio_unmap(void* vaddr, size_t size) {
    return (int)syscall(SYS_MMIO_UNMAP, (uint64_t)vaddr, size, 0, 0, 0);
}

#define SYS_SET_PROCESS_IPC_PORT 48
#define SYS_IO_READ 49
#define SYS_IO_WRITE 50
#define SYS_STAT 51

// IPC message structure (must match kernel/include/ipc/ipc.h)

// ... (existing code)

static inline int sys_set_process_ipc_port(uint64_t port_id) {
    return (int)syscall(SYS_SET_PROCESS_IPC_PORT, port_id, 0, 0, 0, 0);
}

#endif // SYSCALL_H

