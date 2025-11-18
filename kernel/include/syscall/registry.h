/**
 * @file registry.h
 * @brief System call registry
 */

#ifndef KERNEL_SYSCALL_REGISTRY_H
#define KERNEL_SYSCALL_REGISTRY_H

#include "../types.h"

// System call information
typedef struct {
    uint32_t number;            // Syscall number
    const char* name;           // Syscall name
    uint32_t num_args;          // Number of arguments
    bool implemented;           // Is it implemented?
    const char* description;   // Description
} syscall_info_t;

// Get syscall information
const syscall_info_t* syscall_get_info(uint32_t syscall_num);
const char* syscall_get_name(uint32_t syscall_num);
bool syscall_is_implemented(uint32_t syscall_num);

// List all syscalls
void syscall_list_all(void);

#endif // KERNEL_SYSCALL_REGISTRY_H

