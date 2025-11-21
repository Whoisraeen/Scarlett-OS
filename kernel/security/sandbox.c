/**
 * @file sandbox.c
 * @brief Sandboxing support implementation
 */

#include "../include/security/sandbox.h"
#include "../include/process.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/sync/spinlock.h"
#include "../include/string.h"

// Maximum sandboxes
#define MAX_SANDBOXES 256

// Sandbox state
static struct {
    sandbox_t sandboxes[MAX_SANDBOXES];
    uint32_t sandbox_count;
    spinlock_t lock;
    bool initialized;
} sandbox_state = {0};

/**
 * Initialize sandbox system
 */
error_code_t sandbox_init(void) {
    if (sandbox_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing sandbox system...\n");
    
    memset(&sandbox_state, 0, sizeof(sandbox_state));
    spinlock_init(&sandbox_state.lock);
    sandbox_state.initialized = true;
    
    kinfo("Sandbox system initialized\n");
    return ERR_OK;
}

/**
 * Create sandbox for process
 */
error_code_t sandbox_create(pid_t pid, const sandbox_limits_t* limits) {
    if (!limits) {
        return ERR_INVALID_ARG;
    }
    
    if (!sandbox_state.initialized) {
        sandbox_init();
    }
    
    spinlock_lock(&sandbox_state.lock);
    
    // Check if sandbox already exists
    for (uint32_t i = 0; i < sandbox_state.sandbox_count; i++) {
        if (sandbox_state.sandboxes[i].pid == pid) {
            // Update existing sandbox
            sandbox_state.sandboxes[i].limits = *limits;
            sandbox_state.sandboxes[i].active = true;
            spinlock_unlock(&sandbox_state.lock);
            return ERR_OK;
        }
    }
    
    if (sandbox_state.sandbox_count >= MAX_SANDBOXES) {
        spinlock_unlock(&sandbox_state.lock);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Create new sandbox
    sandbox_t* sandbox = &sandbox_state.sandboxes[sandbox_state.sandbox_count++];
    sandbox->pid = pid;
    sandbox->limits = *limits;
    sandbox->current_memory = 0;
    sandbox->current_files = 0;
    sandbox->current_processes = 0;
    sandbox->current_fds = 0;
    sandbox->active = true;
    
    spinlock_unlock(&sandbox_state.lock);
    
    kinfo("Sandbox: Created for PID %d (memory: %zu, files: %zu, processes: %u)\n",
          pid, limits->max_memory, limits->max_files, limits->max_processes);
    return ERR_OK;
}

/**
 * Destroy sandbox
 */
error_code_t sandbox_destroy(pid_t pid) {
    if (!sandbox_state.initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    spinlock_lock(&sandbox_state.lock);
    
    for (uint32_t i = 0; i < sandbox_state.sandbox_count; i++) {
        if (sandbox_state.sandboxes[i].pid == pid) {
            // Remove sandbox by shifting
            for (uint32_t j = i; j < sandbox_state.sandbox_count - 1; j++) {
                sandbox_state.sandboxes[j] = sandbox_state.sandboxes[j + 1];
            }
            sandbox_state.sandbox_count--;
            spinlock_unlock(&sandbox_state.lock);
            return ERR_OK;
        }
    }
    
    spinlock_unlock(&sandbox_state.lock);
    return ERR_NOT_FOUND;
}

/**
 * Get sandbox for process
 */
sandbox_t* sandbox_get(pid_t pid) {
    if (!sandbox_state.initialized) {
        return NULL;
    }
    
    spinlock_lock(&sandbox_state.lock);
    
    for (uint32_t i = 0; i < sandbox_state.sandbox_count; i++) {
        if (sandbox_state.sandboxes[i].pid == pid && sandbox_state.sandboxes[i].active) {
            sandbox_t* sandbox = &sandbox_state.sandboxes[i];
            spinlock_unlock(&sandbox_state.lock);
            return sandbox;
        }
    }
    
    spinlock_unlock(&sandbox_state.lock);
    return NULL;
}

/**
 * Check memory limit
 */
error_code_t sandbox_check_memory(pid_t pid, size_t requested) {
    sandbox_t* sandbox = sandbox_get(pid);
    if (!sandbox) {
        return ERR_OK;  // No sandbox, allow
    }
    
    if (sandbox->current_memory + requested > sandbox->limits.max_memory) {
        return ERR_OUT_OF_MEMORY;
    }
    
    return ERR_OK;
}

/**
 * Check file limit
 */
error_code_t sandbox_check_file(pid_t pid) {
    sandbox_t* sandbox = sandbox_get(pid);
    if (!sandbox) {
        return ERR_OK;  // No sandbox, allow
    }
    
    if (sandbox->current_files >= sandbox->limits.max_files) {
        return ERR_OUT_OF_MEMORY;
    }
    
    return ERR_OK;
}

/**
 * Check process limit
 */
error_code_t sandbox_check_process(pid_t pid) {
    sandbox_t* sandbox = sandbox_get(pid);
    if (!sandbox) {
        return ERR_OK;  // No sandbox, allow
    }
    
    if (sandbox->current_processes >= sandbox->limits.max_processes) {
        return ERR_OUT_OF_MEMORY;
    }
    
    return ERR_OK;
}

/**
 * Check file descriptor limit
 */
error_code_t sandbox_check_fd(pid_t pid) {
    sandbox_t* sandbox = sandbox_get(pid);
    if (!sandbox) {
        return ERR_OK;  // No sandbox, allow
    }
    
    if (sandbox->current_fds >= sandbox->limits.max_fds) {
        return ERR_OUT_OF_MEMORY;
    }
    
    return ERR_OK;
}

/**
 * Check sandbox flag
 */
error_code_t sandbox_check_flag(pid_t pid, uint32_t flag) {
    sandbox_t* sandbox = sandbox_get(pid);
    if (!sandbox) {
        return ERR_OK;  // No sandbox, allow
    }
    
    if (!(sandbox->limits.flags & flag)) {
        return ERR_PERMISSION_DENIED;
    }
    
    return ERR_OK;
}

/**
 * Update memory usage
 */
error_code_t sandbox_update_memory(pid_t pid, size_t delta) {
    sandbox_t* sandbox = sandbox_get(pid);
    if (!sandbox) {
        return ERR_OK;  // No sandbox
    }
    
    spinlock_lock(&sandbox_state.lock);
    
    if (delta > 0 && sandbox->current_memory + delta > sandbox->limits.max_memory) {
        spinlock_unlock(&sandbox_state.lock);
        return ERR_OUT_OF_MEMORY;
    }
    
    if (delta < 0 && sandbox->current_memory < (size_t)(-delta)) {
        sandbox->current_memory = 0;
    } else {
        sandbox->current_memory += delta;
    }
    
    spinlock_unlock(&sandbox_state.lock);
    return ERR_OK;
}

/**
 * Update file count
 */
error_code_t sandbox_update_files(pid_t pid, int delta) {
    sandbox_t* sandbox = sandbox_get(pid);
    if (!sandbox) {
        return ERR_OK;  // No sandbox
    }
    
    spinlock_lock(&sandbox_state.lock);
    
    if (delta > 0 && sandbox->current_files + delta > sandbox->limits.max_files) {
        spinlock_unlock(&sandbox_state.lock);
        return ERR_OUT_OF_MEMORY;
    }
    
    if (delta < 0 && sandbox->current_files < (uint32_t)(-delta)) {
        sandbox->current_files = 0;
    } else {
        sandbox->current_files += delta;
    }
    
    spinlock_unlock(&sandbox_state.lock);
    return ERR_OK;
}

/**
 * Update process count
 */
error_code_t sandbox_update_processes(pid_t pid, int delta) {
    sandbox_t* sandbox = sandbox_get(pid);
    if (!sandbox) {
        return ERR_OK;  // No sandbox
    }
    
    spinlock_lock(&sandbox_state.lock);
    
    if (delta > 0 && sandbox->current_processes + delta > sandbox->limits.max_processes) {
        spinlock_unlock(&sandbox_state.lock);
        return ERR_OUT_OF_MEMORY;
    }
    
    if (delta < 0 && sandbox->current_processes < (uint32_t)(-delta)) {
        sandbox->current_processes = 0;
    } else {
        sandbox->current_processes += delta;
    }
    
    spinlock_unlock(&sandbox_state.lock);
    return ERR_OK;
}

/**
 * Update file descriptor count
 */
error_code_t sandbox_update_fds(pid_t pid, int delta) {
    sandbox_t* sandbox = sandbox_get(pid);
    if (!sandbox) {
        return ERR_OK;  // No sandbox
    }
    
    spinlock_lock(&sandbox_state.lock);
    
    if (delta > 0 && sandbox->current_fds + delta > sandbox->limits.max_fds) {
        spinlock_unlock(&sandbox_state.lock);
        return ERR_OUT_OF_MEMORY;
    }
    
    if (delta < 0 && sandbox->current_fds < (uint32_t)(-delta)) {
        sandbox->current_fds = 0;
    } else {
        sandbox->current_fds += delta;
    }
    
    spinlock_unlock(&sandbox_state.lock);
    return ERR_OK;
}

