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
    
    extern process_t* process_get_current(void);
    process_t* proc = process_get_current();
    if (!proc) return -1;
    
    capability_table_t* table = get_process_table(proc->pid);
    if (!table || !table->initialized) return -1;
    
    // Lookup and verify capability
    capability_t source_cap = {0};
    bool found = false;
    
    spinlock_lock(&table->lock);
    for (size_t i = 0; i < table->count; i++) {
        if (table->capabilities[i].cap_id == cap_id) {
            if ((table->capabilities[i].rights & CAP_RIGHT_TRANSFER) == CAP_RIGHT_TRANSFER) {
                source_cap = table->capabilities[i];
                found = true;
            }
            break;
        }
    }
    spinlock_unlock(&table->lock);
    
    if (!found) return -1; // Not found or no transfer rights
    
    // In a full implementation, we would attach this metadata to the kernel-side message object.
    // Since `ipc_message_t` here refers to the user-facing struct which is copied to kernel buffer,
    // we will pack the capability details into the message data as a "transferred capability descriptor".
    // The receiver's IPC handler in kernel will detect this and insert it into receiver's table.
    
    // Assuming msg has space for capability descriptor (cap_id, type, resource, rights)
    // We use the last 32 bytes of inline_data as a hidden area or special field if supported.
    // For this implementation, we will append a special tag if space permits.
    
    // Check space: we need sizeof(uint64_t) * 4 (type, resource, rights, padding)
    if (msg->inline_size + 32 <= IPC_INLINE_SIZE) {
        // Write capability data to end of inline buffer
        uint64_t* ptr = (uint64_t*)(msg->inline_data + msg->inline_size);
        ptr[0] = 0xCAFEBABECAFEBABE; // Magic signature for transferred cap
        ptr[1] = (uint64_t)source_cap.type;
        ptr[2] = source_cap.resource_id;
        ptr[3] = (uint64_t)source_cap.rights;
        
        msg->inline_size += 32;
        return 0;
    }
    
    return -1; // No space
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