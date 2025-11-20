/**
 * @file syscall.c
 * @brief System call implementation
 */

#include "../include/types.h"
#include "../include/syscall/syscall.h"
#include "../include/sched/scheduler.h"
#include "../include/ipc/ipc.h"
#include "../include/process.h"
#include "../include/mm/vmm.h"
#include "../include/mm/mmap.h"
#include "../include/fs/vfs.h"
#include "../include/errors.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/security/capability.h"
#include "../drivers/pci/pci.h"
#include "../include/hal/irq_handler.h"
#include "../include/mm/pmm.h"
#include "../include/mm/vmm.h"
#include "../include/process.h"

/**
 * Initialize system calls
 */
void syscall_init(void) {
    kinfo("Initializing system calls...\n");
    
    // Set up MSR for syscall/sysret
    // IA32_STAR MSR (0xC0000081)
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x18 << 48);
    __asm__ volatile("wrmsr" :: "a"((uint32_t)star), "d"((uint32_t)(star >> 32)), "c"(0xC0000081));
    
    // IA32_LSTAR MSR (0xC0000082) - syscall entry point
    extern void syscall_entry(void);
    uint64_t lstar = (uint64_t)syscall_entry;
    __asm__ volatile("wrmsr" :: "a"((uint32_t)lstar), "d"((uint32_t)(lstar >> 32)), "c"(0xC0000082));
    
    // IA32_FMASK MSR (0xC0000084) - RFLAGS mask
    uint64_t fmask = 0x200;  // Clear IF (disable interrupts)
    __asm__ volatile("wrmsr" :: "a"((uint32_t)fmask), "d"((uint32_t)(fmask >> 32)), "c"(0xC0000084));
    
    // Enable syscall/sysret in EFER
    uint32_t efer_lo, efer_hi;
    __asm__ volatile("rdmsr" : "=a"(efer_lo), "=d"(efer_hi) : "c"(0xC0000080));
    efer_lo |= 1;  // Set SCE (System Call Extensions)
    __asm__ volatile("wrmsr" :: "a"(efer_lo), "d"(efer_hi), "c"(0xC0000080));
    
    kinfo("System calls initialized\n");
}

/**
 * Validate syscall number
 */
static bool validate_syscall(uint64_t syscall_num) {
    if (syscall_num > SYS_MAX) {
        return false;
    }
    return true;
}

/**
 * Validate user pointer (check if in user space)
 */
static bool validate_user_ptr(void* ptr, size_t size) {
    uint64_t addr = (uint64_t)ptr;
    // User space is lower half (0x0000000000000000 - 0x00007FFFFFFFFFFF)
    if (addr >= 0x0000800000000000ULL) {
        return false;  // Kernel space
    }
    // Check for overflow
    if (addr + size < addr) {
        return false;
    }
    if (addr + size > 0x0000800000000000ULL) {
        return false;  // Would overflow into kernel space
    }
    return true;
}

/**
 * System call handler
 */
uint64_t syscall_handler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2,
                         uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    // Validate syscall number
    if (!validate_syscall(syscall_num)) {
        kwarn("Invalid syscall number: %lu\n", syscall_num);
        return (uint64_t)ERR_INVALID_SYSCALL;
    }
    
    switch (syscall_num) {
        case SYS_EXIT: {
            // arg1 = exit code
            int exit_code = (int)arg1;
            process_t* current = process_get_current();
            if (current) {
                process_exit(current, exit_code);
            } else {
                thread_exit();
            }
            return 0;  // Never reached
        }
        
        case SYS_WRITE: {
            // arg1 = fd, arg2 = buffer, arg3 = size
            int fd = (int)arg1;
            void* buf = (void*)arg2;
            size_t size = (size_t)arg3;
            
            // Validate arguments
            if (!validate_user_ptr(buf, size)) {
                return (uint64_t)ERR_INVALID_ARG;
            }
            
            // Special file descriptors (stdout, stderr)
            if (fd == 1 || fd == 2) {
                char* cbuf = (char*)buf;
                for (size_t i = 0; i < size; i++) {
                    kputc(cbuf[i]);
                }
                return size;
            }
            
            // Regular file - use VFS
            extern error_code_t vfs_write(int32_t fd, const void* buf, size_t count, size_t* bytes_written);
            size_t bytes_written = 0;
            error_code_t err = vfs_write((int32_t)fd, buf, size, &bytes_written);
            if (err != ERR_OK) {
                return (uint64_t)err;
            }
            
            return bytes_written;
        }
        
        case SYS_READ: {
            // arg1 = fd, arg2 = buffer, arg3 = size
            int fd = (int)arg1;
            void* buf = (void*)arg2;
            size_t size = (size_t)arg3;
            
            // Validate arguments
            if (!validate_user_ptr(buf, size)) {
                return (uint64_t)ERR_INVALID_ARG;
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
                return read;
            }
            
            // Regular file - use VFS
            extern error_code_t vfs_read(int32_t fd, void* buf, size_t count, size_t* bytes_read);
            size_t bytes_read = 0;
            error_code_t err = vfs_read((int32_t)fd, buf, size, &bytes_read);
            if (err != ERR_OK) {
                return (uint64_t)err;
            }
            
            return bytes_read;
        }
        
        case SYS_OPEN: {
            // arg1 = path, arg2 = flags, arg3 = mode
            const char* path = (const char*)arg1;
            uint64_t flags = arg2;
            (void)arg3;  // mode unused for now
            
            // Validate path
            if (!validate_user_ptr((void*)path, 256)) {
                return (uint64_t)ERR_INVALID_ARG;
            }
            
            // Open file via VFS
            extern error_code_t vfs_open(const char* path, uint64_t flags, int32_t* fd);
            int32_t fd;
            error_code_t err = vfs_open(path, flags, &fd);
            if (err != ERR_OK) {
                return (uint64_t)err;
            }
            
            return (uint64_t)fd;
        }
        
        case SYS_CLOSE: {
            // arg1 = fd
            int fd = (int)arg1;
            
            // Close file via VFS
            extern error_code_t vfs_close(int32_t fd);
            error_code_t err = vfs_close((int32_t)fd);
            return (uint64_t)err;
        }
        
        case SYS_SLEEP: {
            // arg1 = milliseconds
            uint64_t ms = arg1;
            thread_sleep(ms);
            return 0;
        }
        
        case SYS_YIELD: {
            thread_yield();
            return 0;
        }
        
        case SYS_GETPID: {
            process_t* current = process_get_current();
            if (current) {
                return (uint64_t)current->pid;
            }
            return 0;  // Kernel process
        }
        
        case SYS_GETUID: {
            // TODO: Implement when user system is ready
            return 0;  // Root user for now
        }
        
        case SYS_FORK: {
            // Fork current process
            process_t* current = process_get_current();
            if (!current) {
                return (uint64_t)ERR_INVALID_ARG;
            }
            
            extern pid_t process_fork(process_t* parent);
            pid_t child_pid = process_fork(current);
            if (child_pid < 0) {
                return (uint64_t)ERR_OUT_OF_MEMORY;
            }
            
            // Return child PID to parent, 0 to child
            // In a real fork, we'd need to handle the return value differently
            // For now, return child PID (parent will get it, child will get 0)
            return (uint64_t)child_pid;
        }
        
        case SYS_EXEC: {
            // arg1 = path, arg2 = argv, arg3 = envp
            const char* path = (const char*)arg1;
            const char** argv = (const char**)arg2;
            const char** envp = (const char**)arg3;
            
            // Validate arguments
            if (!validate_user_ptr((void*)path, 256)) {  // Assume max path length
                return (uint64_t)ERR_INVALID_ARG;
            }
            
            process_t* current = process_get_current();
            if (!current) {
                return (uint64_t)ERR_INVALID_ARG;
            }
            
            extern error_code_t process_exec(process_t* process, const char* path, char* const* argv, char* const* envp);
            error_code_t err = process_exec(current, path, argv, envp);
            
            // If exec succeeds, it doesn't return (process is replaced)
            // If it fails, return error
            return (uint64_t)err;
        }
        
        case SYS_WAIT: {
            // arg1 = pid, arg2 = status
            // TODO: Implement process waiting
            kwarn("SYS_WAIT not yet implemented\n");
            return (uint64_t)ERR_NOT_SUPPORTED;
        }
        
        case SYS_MMAP: {
            // arg1 = addr (hint, can be NULL), arg2 = length, arg3 = prot, arg4 = flags, arg5 = fd, offset not used (would be arg6)
            vaddr_t addr __attribute__((unused)) = (vaddr_t)arg1;  // Hint address (currently ignored)
            size_t length = (size_t)arg2;
            uint64_t prot = arg3;
            uint64_t flags = arg4;
            int fd = (int)arg5;
            uint64_t offset = 0;  // Offset not available in current syscall interface
            
            // Validate arguments
            if (length == 0) {
                return (uint64_t)ERR_INVALID_ARG;
            }
            
            // Get current process
            process_t* current = process_get_current();
            if (!current || !current->address_space) {
                return (uint64_t)ERR_INVALID_ARG;
            }
            
            extern vaddr_t mmap_alloc(address_space_t* as, size_t size, uint64_t prot, uint64_t flags, int fd, uint64_t offset);
            vaddr_t result = mmap_alloc(current->address_space, length, prot, flags, fd, offset);
            
            if (is_error((error_code_t)result)) {
                return (uint64_t)result;
            }
            
            return (uint64_t)result;
        }
        
        case SYS_MUNMAP: {
            // arg1 = addr, arg2 = length
            vaddr_t addr = (vaddr_t)arg1;
            size_t length = (size_t)arg2;
            
            // Validate arguments
            if (addr == 0 || length == 0) {
                return (uint64_t)ERR_INVALID_ARG;
            }
            
            // Get current process
            process_t* current = process_get_current();
            if (!current || !current->address_space) {
                return (uint64_t)ERR_INVALID_ARG;
            }
            
            extern error_code_t mmap_free(address_space_t* as, vaddr_t addr, size_t size);
            error_code_t err = mmap_free(current->address_space, addr, length);
            
            return (uint64_t)err;
        }
        
        case SYS_BRK: {
            // arg1 = new_brk
            // TODO: Implement heap expansion
            kwarn("SYS_BRK not yet implemented\n");
            return (uint64_t)ERR_NOT_SUPPORTED;
        }
        
        case SYS_GETCWD: {
            // arg1 = buf, arg2 = size
            // TODO: Implement when filesystem is ready
            kwarn("SYS_GETCWD not yet implemented (filesystem needed)\n");
            return (uint64_t)ERR_NOT_SUPPORTED;
        }
        
        case SYS_CHDIR: {
            // arg1 = path
            // TODO: Implement when filesystem is ready
            kwarn("SYS_CHDIR not yet implemented (filesystem needed)\n");
            return (uint64_t)ERR_NOT_SUPPORTED;
        }
        
        case SYS_THREAD_CREATE: {
            // arg1 = entry, arg2 = arg, arg3 = priority
            void (*entry)(void*) = (void (*)(void*))arg1;
            void* arg = (void*)arg2;
            uint8_t priority = (uint8_t)arg3;
            
            // Validate entry point is in user space
            if (!validate_user_ptr((void*)arg1, 1)) {
                return (uint64_t)ERR_INVALID_ARG;
            }
            
            return (uint64_t)thread_create(entry, arg, priority, "user thread");
        }
        
        case SYS_THREAD_EXIT: {
            thread_exit();
            return 0;  // Never reached
        }
        
        case SYS_IPC_SEND: {
            // arg1 = port_id, arg2 = message
            if (!validate_user_ptr((void*)arg2, sizeof(ipc_message_t))) {
                return (uint64_t)ERR_INVALID_ARG;
            }
            return (uint64_t)ipc_send(arg1, (ipc_message_t*)arg2);
        }
        
        case SYS_IPC_RECEIVE: {
            // arg1 = port_id, arg2 = message
            if (!validate_user_ptr((void*)arg2, sizeof(ipc_message_t))) {
                return (uint64_t)ERR_INVALID_ARG;
            }
            return (uint64_t)ipc_receive(arg1, (ipc_message_t*)arg2);
        }
        
        case SYS_DESKTOP_RENDER: {
            // Render desktop from userspace
            extern error_code_t desktop_render(void);
            return (uint64_t)desktop_render();
        }
        
        case SYS_TASKBAR_RENDER: {
            // Render taskbar from userspace
            extern error_code_t taskbar_render(void);
            return (uint64_t)taskbar_render();
        }
        
        case SYS_GFX_SWAP_BUFFERS: {
            // Swap graphics buffers from userspace
            extern void gfx_swap_buffers(void);
            gfx_swap_buffers();
            return 0;
        }
        
        case SYS_IPC_CREATE_PORT: {
            // Create IPC port
            uint64_t port_id = ipc_create_port();
            return (uint64_t)port_id;
        }
        
        case SYS_IPC_DESTROY_PORT: {
            // Destroy IPC port
            uint64_t port_id = arg1;
            return (uint64_t)ipc_destroy_port(port_id);
        }
        
        case SYS_PCI_READ_CONFIG: {
            // Read PCI configuration space (for device manager)
            extern uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
            uint8_t bus = (uint8_t)arg1;
            uint8_t device = (uint8_t)arg2;
            uint8_t function = (uint8_t)arg3;
            uint8_t offset = (uint8_t)arg4;
            return (uint64_t)pci_read_config(bus, device, function, offset);
        }
        
        case SYS_PCI_WRITE_CONFIG: {
            // Write PCI configuration space
            extern void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
            uint8_t bus = (uint8_t)arg1;
            uint8_t device = (uint8_t)arg2;
            uint8_t function = (uint8_t)arg3;
            uint8_t offset = (uint8_t)arg4;
            uint32_t value = (uint32_t)arg5;
            pci_write_config(bus, device, function, offset, value);
            return 0;
        }
        
        case SYS_IRQ_REGISTER: {
            // Register IRQ handler
            extern int irq_register(uint8_t irq, void (*handler)(void*), void* context);
            uint8_t irq = (uint8_t)arg1;
            void (*handler)(void*) = (void (*)(void*))arg2;
            void* context = (void*)arg3;
            return (uint64_t)irq_register(irq, handler, context);
        }
        
        case SYS_IRQ_UNREGISTER: {
            // Unregister IRQ handler
            extern int irq_unregister(uint8_t irq, void (*handler)(void*));
            uint8_t irq = (uint8_t)arg1;
            void (*handler)(void*) = (void (*)(void*))arg2;
            return (uint64_t)irq_unregister(irq, handler);
        }
        
        case SYS_IRQ_ENABLE: {
            // Enable IRQ
            extern void irq_enable(uint8_t irq);
            uint8_t irq = (uint8_t)arg1;
            irq_enable(irq);
            return 0;
        }
        
        case SYS_IRQ_DISABLE: {
            // Disable IRQ
            extern void irq_disable(uint8_t irq);
            uint8_t irq = (uint8_t)arg1;
            irq_disable(irq);
            return 0;
        }
        
        case SYS_DMA_ALLOC: {
            // Allocate DMA buffer (contiguous physical memory)
            extern paddr_t pmm_alloc_pages(size_t count);
            size_t size = (size_t)arg1;
            size_t pages = (size + 4095) / 4096;  // Round up to pages
            paddr_t paddr = pmm_alloc_pages(pages);
            if (paddr == 0) {
                return 0;  // Allocation failed
            }
            // Map to user-space virtual address
            extern int vmm_map_pages(address_space_t* as, vaddr_t vaddr, paddr_t paddr, size_t count, uint64_t flags);
            extern address_space_t* process_get_address_space(process_t* proc);
            extern process_t* process_get_current(void);
            process_t* proc = process_get_current();
            if (!proc) {
                return 0;
            }
            address_space_t* as = process_get_address_space(proc);
            if (!as) {
                return 0;
            }
            // Allocate virtual address (simplified - would use proper allocator)
            vaddr_t vaddr = 0x40000000;  // User-space DMA region
            uint64_t flags = VMM_PRESENT | VMM_WRITE | VMM_USER | VMM_NOCACHE;
            if (vmm_map_pages(as, vaddr, paddr, pages, flags) != 0) {
                return 0;
            }
            return (uint64_t)vaddr;
        }
        
        case SYS_DMA_FREE: {
            // Free DMA buffer
            vaddr_t vaddr = (vaddr_t)arg1;
            size_t size = (size_t)arg2;
            size_t pages = (size + 4095) / 4096;
            extern address_space_t* process_get_address_space(process_t* proc);
            extern process_t* process_get_current(void);
            extern paddr_t vmm_get_physical(address_space_t* as, vaddr_t vaddr);
            extern void pmm_free_pages(paddr_t base, size_t count);
            extern int vmm_unmap_pages(address_space_t* as, vaddr_t vaddr, size_t count);
            process_t* proc = process_get_current();
            if (!proc) {
                return (uint64_t)-1;
            }
            address_space_t* as = process_get_address_space(proc);
            if (!as) {
                return (uint64_t)-1;
            }
            paddr_t paddr = vmm_get_physical(as, vaddr);
            if (paddr == 0) {
                return (uint64_t)-1;
            }
            vmm_unmap_pages(as, vaddr, pages);
            pmm_free_pages(paddr, pages);
            return 0;
        }
        
        case SYS_MMIO_MAP: {
            // Map MMIO region to user-space
            paddr_t paddr = (paddr_t)arg1;
            size_t size = (size_t)arg2;
            size_t pages = (size + 4095) / 4096;
            extern int vmm_map_pages(address_space_t* as, vaddr_t vaddr, paddr_t paddr, size_t count, uint64_t flags);
            extern address_space_t* process_get_address_space(process_t* proc);
            extern process_t* process_get_current(void);
            process_t* proc = process_get_current();
            if (!proc) {
                return 0;
            }
            address_space_t* as = process_get_address_space(proc);
            if (!as) {
                return 0;
            }
            vaddr_t vaddr = 0x50000000;  // User-space MMIO region
            uint64_t flags = VMM_PRESENT | VMM_WRITE | VMM_USER | VMM_NOCACHE | VMM_WRITETHROUGH;
            if (vmm_map_pages(as, vaddr, paddr, pages, flags) != 0) {
                return 0;
            }
            return (uint64_t)vaddr;
        }
        
        case SYS_MMIO_UNMAP: {
            // Unmap MMIO region
            vaddr_t vaddr = (vaddr_t)arg1;
            size_t size = (size_t)arg2;
            size_t pages = (size + 4095) / 4096;
            extern address_space_t* process_get_address_space(process_t* proc);
            extern process_t* process_get_current(void);
            extern int vmm_unmap_pages(address_space_t* as, vaddr_t vaddr, size_t count);
            process_t* proc = process_get_current();
            if (!proc) {
                return (uint64_t)-1;
            }
            address_space_t* as = process_get_address_space(proc);
            if (!as) {
                return (uint64_t)-1;
            }
            return (uint64_t)vmm_unmap_pages(as, vaddr, pages);
        }
        
        case SYS_CAPABILITY_CREATE: {
            // Create capability
            extern uint64_t capability_create(uint32_t type, uint64_t resource_id, uint32_t rights);
            uint32_t type = (uint32_t)arg1;
            uint64_t resource_id = arg2;
            uint32_t rights = (uint32_t)arg3;
            return capability_create(type, resource_id, rights);
        }
        
        case SYS_CAPABILITY_CHECK: {
            // Check capability right
            extern bool capability_check(uint64_t cap_id, uint32_t right);
            uint64_t cap_id = arg1;
            uint32_t right = (uint32_t)arg2;
            return (uint64_t)capability_check(cap_id, right);
        }
        
        default:
            kwarn("Unknown system call: %lu\n", syscall_num);
            return (uint64_t)ERR_INVALID_SYSCALL;
    }
}

