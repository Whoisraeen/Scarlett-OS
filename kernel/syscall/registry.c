/**
 * @file registry.c
 * @brief System call registry implementation
 */

#include "../include/types.h"
#include "../include/syscall/registry.h"
#include "../include/syscall/syscall.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

// System call registry
static const syscall_info_t syscall_registry[] = {
    {SYS_EXIT, "exit", 1, true, "Terminate the calling process"},
    {SYS_WRITE, "write", 3, true, "Write to a file descriptor"},
    {SYS_READ, "read", 3, true, "Read from a file descriptor"},
    {SYS_OPEN, "open", 3, false, "Open a file (needs filesystem)"},
    {SYS_CLOSE, "close", 1, false, "Close a file descriptor (needs filesystem)"},
    {SYS_SLEEP, "sleep", 1, true, "Sleep for specified milliseconds"},
    {SYS_YIELD, "yield", 0, true, "Yield CPU to another thread"},
    {SYS_THREAD_CREATE, "thread_create", 3, true, "Create a new thread"},
    {SYS_THREAD_EXIT, "thread_exit", 0, true, "Exit current thread"},
    {SYS_IPC_SEND, "ipc_send", 2, true, "Send IPC message"},
    {SYS_IPC_RECEIVE, "ipc_receive", 2, true, "Receive IPC message"},
    {SYS_MMAP, "mmap", 6, true, "Map memory into address space"},
    {SYS_MUNMAP, "munmap", 2, true, "Unmap memory from address space"},
    {SYS_GETPID, "getpid", 0, true, "Get current process ID"},
    {SYS_GETUID, "getuid", 0, true, "Get current user ID"},
    {SYS_FORK, "fork", 0, true, "Create a child process"},
    {SYS_EXEC, "exec", 3, true, "Execute a new program"},
    {SYS_WAIT, "wait", 2, false, "Wait for a child process"},
    {SYS_BRK, "brk", 1, false, "Change data segment size"},
    {SYS_GETCWD, "getcwd", 2, false, "Get current working directory (needs filesystem)"},
    {SYS_CHDIR, "chdir", 1, false, "Change current directory (needs filesystem)"},
    {0, NULL, 0, false, NULL}  // Sentinel
};

/**
 * Get syscall information
 */
const syscall_info_t* syscall_get_info(uint32_t syscall_num) {
    for (int i = 0; syscall_registry[i].name != NULL; i++) {
        if (syscall_registry[i].number == syscall_num) {
            return &syscall_registry[i];
        }
    }
    return NULL;
}

/**
 * Get syscall name
 */
const char* syscall_get_name(uint32_t syscall_num) {
    const syscall_info_t* info = syscall_get_info(syscall_num);
    return info ? info->name : "unknown";
}

/**
 * Check if syscall is implemented
 */
bool syscall_is_implemented(uint32_t syscall_num) {
    const syscall_info_t* info = syscall_get_info(syscall_num);
    return info ? info->implemented : false;
}

/**
 * List all syscalls
 */
void syscall_list_all(void) {
    kinfo("System Call Registry:\n");
    kinfo("====================\n");
    
    for (int i = 0; syscall_registry[i].name != NULL; i++) {
        const syscall_info_t* info = &syscall_registry[i];
        kinfo("  %2u: %-16s (%u args) %s - %s\n",
              info->number,
              info->name,
              info->num_args,
              info->implemented ? "[IMPLEMENTED]" : "[NOT IMPLEMENTED]",
              info->description);
    }
}

