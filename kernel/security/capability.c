/**
 * @file capability.c
 * @brief Capability-based security implementation
 */

#include "../include/types.h"
#include "../include/security/capability.h"
#include "../include/ipc/ipc.h"
#include "../include/process.h"
#include "../include/mm/heap.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/sync/spinlock.h"

// Capability structure
typedef struct capability {
    uint64_t cap_id;
    uint32_t type;
    uint64_t resource_id;
    uint32_t rights;  // Bitmask of allowed operations
    // No 'next' pointer needed in array-based table, but useful if we switch to lists later
} capability_t;

// Capability table per process
typedef struct capability_table {
    capability_t* capabilities; // Dynamic array
    size_t count;
    size_t capacity;
    spinlock_t lock;
    bool initialized;
} capability_table_t;

#define MAX_CAPABILITIES_PER_PROCESS 256
#define INITIAL_CAPACITY 16

// Global capability tables (one per process)
// Use a simple array indexed by PID (for now - could use hash table for scalability)
// Note: PIDs can be large, so a direct array might be sparse. 
// In a real system, this should be part of process_t.
// Since I cannot easily modify process_t header without triggering potential recompilation issues across the board,
// I will use a static array indexed by PID modulo MAX_PROCESSES or just MAX_PROCESSES.
// Assuming PIDs are allocated sequentially and recycled.

#define MAX_PROCESSES 32768
static capability_table_t capability_tables[MAX_PROCESSES];
static bool capability_system_initialized = false;
static spinlock_t system_lock = SPINLOCK_INIT;
static uint64_t next_cap_id = 1;

// Helper to get table for a specific PID
static capability_table_t* get_process_table(pid_t pid) {
    if (pid < 0 || pid >= MAX_PROCESSES) return NULL;
    return &capability_tables[pid];
}

/**
 * Initialize capability system
 */
void capability_init(void) {
    kinfo("Initializing capability system...\n");
    
    spinlock_init(&system_lock);
    next_cap_id = 1;
    
    // Initialize per-process capability tables
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        capability_tables[i].capabilities = NULL;
        capability_tables[i].count = 0;
        capability_tables[i].capacity = 0;
        capability_tables[i].initialized = false;
        spinlock_init(&capability_tables[i].lock);
    }
    
    capability_system_initialized = true;
    kinfo("Capability system initialized (Per-process tables active)\n");
}

/**
 * Ensure capability table exists for process
 */
static int ensure_table_initialized(capability_table_t* table) {
    if (table->initialized && table->capabilities) return 0;
    
    spinlock_lock(&table->lock);
    if (!table->initialized) {
        table->capacity = INITIAL_CAPACITY;
        table->capabilities = (capability_t*)kmalloc(sizeof(capability_t) * table->capacity);
        if (!table->capabilities) {
            spinlock_unlock(&table->lock);
            return -1;
        }
        table->count = 0;
        table->initialized = true;
    }
    spinlock_unlock(&table->lock);
    return 0;
}

/**
 * Create a new capability for current process
 */
uint64_t capability_create(uint32_t type, uint64_t resource_id, uint32_t rights) {
    extern process_t* process_get_current(void);
    process_t* proc = process_get_current();
    if (!proc) return 0;
    
    capability_table_t* table = get_process_table(proc->pid);
    if (!table) return 0;
    
    if (ensure_table_initialized(table) != 0) return 0;
    
    spinlock_lock(&table->lock);
    
    // Resize if needed
    if (table->count >= table->capacity) {
        size_t new_cap = table->capacity * 2;
        if (new_cap > MAX_CAPABILITIES_PER_PROCESS) new_cap = MAX_CAPABILITIES_PER_PROCESS;
        if (table->count >= new_cap) {
            spinlock_unlock(&table->lock);
            return 0; // Limit reached
        }
        
        capability_t* new_arr = (capability_t*)kmalloc(sizeof(capability_t) * new_cap);
        if (!new_arr) {
            spinlock_unlock(&table->lock);
            return 0;
        }
        
        // Copy old
        for (size_t i=0; i < table->count; i++) new_arr[i] = table->capabilities[i];
        kfree(table->capabilities);
        table->capabilities = new_arr;
        table->capacity = new_cap;
    }
    
    // Add capability
    uint64_t id = 0;
    
    spinlock_lock(&system_lock);
    id = next_cap_id++;
    spinlock_unlock(&system_lock);
    
    capability_t* slot = &table->capabilities[table->count++];
    slot->cap_id = id;
    slot->type = type;
    slot->resource_id = resource_id;
    slot->rights = rights;
    
    spinlock_unlock(&table->lock);
    
    return id;
}

/**
 * Check if current process has capability right
 */
bool capability_check(uint64_t cap_id, uint32_t right) {
    extern process_t* process_get_current(void);
    process_t* proc = process_get_current();
    if (!proc) return false;
    
    capability_table_t* table = get_process_table(proc->pid);
    if (!table || !table->initialized) return false;
    
    bool allowed = false;
    
    spinlock_lock(&table->lock);
    for (size_t i = 0; i < table->count; i++) {
        if (table->capabilities[i].cap_id == cap_id) {
            if ((table->capabilities[i].rights & right) == right) {
                allowed = true;
            }
            break;
        }
    }
    spinlock_unlock(&table->lock);
    
    return allowed;
}

/**
 * Transfer capability in IPC message
 */
int capability_transfer(ipc_message_t* msg, uint64_t cap_id) {
    if (!msg) return -1;
    
    // Verify ownership
    if (!capability_check(cap_id, CAP_RIGHT_TRANSFER)) return -1;
    
    // TODO: In a real implementation, we need to:
    // 1. Look up the source capability details.
    // 2. Create a "pending transfer" or actually copy it to the target process immediately if known.
    // Since IPC usually buffers messages, we should attach the capability metadata to the message kernel-side.
    // The recipient will "claim" it upon receive, generating a NEW cap_id for their table.
    
    // For this implementation, we'll just validate checking passed.
    // We add the ID to the message. The receiver logic (not shown here fully) needs to import it.
    
    if (msg->inline_size + 8 <= IPC_INLINE_SIZE) {
        uint64_t* cap_ptr = (uint64_t*)(msg->inline_data + msg->inline_size);
        *cap_ptr = cap_id;
        msg->inline_size += 8;
    } else {
        return -1;
    }
    
    return 0;
}

/**
 * Revoke capability
 */
int capability_revoke(uint64_t cap_id) {
    extern process_t* process_get_current(void);
    process_t* proc = process_get_current();
    if (!proc) return -1;
    
    capability_table_t* table = get_process_table(proc->pid);
    if (!table || !table->initialized) return -1;
    
    spinlock_lock(&table->lock);
    for (size_t i = 0; i < table->count; i++) {
        if (table->capabilities[i].cap_id == cap_id) {
            // Found it. Remove by swapping with last element.
            table->capabilities[i] = table->capabilities[table->count - 1];
            table->count--;
            spinlock_unlock(&table->lock);
            return 0;
        }
    }
    spinlock_unlock(&table->lock);
    
    return -1;
}

/**
 * Find capability for a port (helper function for IPC)
 */
uint64_t capability_find_for_port(uint64_t port_id) {
    extern process_t* process_get_current(void);
    process_t* proc = process_get_current();
    if (!proc) return 0;
    
    capability_table_t* table = get_process_table(proc->pid);
    if (!table || !table->initialized) return 0;
    
    uint64_t found_id = 0;
    
    spinlock_lock(&table->lock);
    for (size_t i = 0; i < table->count; i++) {
        if (table->capabilities[i].type == CAP_TYPE_IPC_PORT && 
            table->capabilities[i].resource_id == port_id) {
            found_id = table->capabilities[i].cap_id;
            break;
        }
    }
    spinlock_unlock(&table->lock);
    
    return found_id;
}