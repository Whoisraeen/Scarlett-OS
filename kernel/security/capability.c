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
    struct capability* next;
} capability_t;

// Capability table per process
typedef struct capability_table {
    capability_t* capabilities;
    size_t count;
    size_t max_count;
    spinlock_t lock;
} capability_table_t;

#define MAX_CAPABILITIES_PER_PROCESS 256

// Global capability tables (one per process)
// Use a simple array indexed by PID (for now - could use hash table for scalability)
#define MAX_PROCESSES 32768
static capability_table_t capability_tables[MAX_PROCESSES];
static bool capability_tables_initialized = false;
static spinlock_t global_lock = SPINLOCK_INIT;

// Capability storage (simplified - one table for all processes)
#define MAX_CAPABILITIES 1024
static capability_t capabilities[MAX_CAPABILITIES];
static uint32_t capability_count = 0;
static uint64_t next_cap_id = 1;

/**
 * Get capability table for current process
 */
static capability_table_t* get_capability_table(void) {
    // Get table for current process
    extern process_t* process_get_current(void);
    process_t* proc = process_get_current();
    if (!proc || !capability_tables_initialized) {
        return NULL;
    }
    
    // Allocate per-process capability table if not already allocated
    pid_t pid = proc->pid;
    if (pid < 0 || pid >= MAX_PROCESSES) {
        return NULL;
    }
    
    capability_table_t* table = &capability_tables[pid];
    
    // Initialize table if needed (first access)
    if (table->capabilities == NULL && table->count == 0) {
        // Allocate capability list for this process
        table->capabilities = (capability_t*)kmalloc(sizeof(capability_t) * MAX_CAPABILITIES_PER_PROCESS);
        if (!table->capabilities) {
            return NULL;
        }
        
        // Initialize all capabilities to zero
        for (size_t i = 0; i < MAX_CAPABILITIES_PER_PROCESS; i++) {
            table->capabilities[i].cap_id = 0;
            table->capabilities[i].type = 0;
            table->capabilities[i].resource_id = 0;
            table->capabilities[i].rights = 0;
            table->capabilities[i].next = NULL;
        }
        
        table->count = 0;
        table->max_count = MAX_CAPABILITIES_PER_PROCESS;
    }
    
    return table;
}

/**
 * Find capability by ID
 */
static capability_t* find_capability(uint64_t cap_id) {
    spinlock_lock(&global_lock);
    
    for (uint32_t i = 0; i < capability_count; i++) {
        if (capabilities[i].cap_id == cap_id) {
            spinlock_unlock(&global_lock);
            return &capabilities[i];
        }
    }
    
    spinlock_unlock(&global_lock);
    return NULL;
}

/**
 * Initialize capability system
 */
void capability_init(void) {
    kinfo("Initializing capability system...\n");
    
    spinlock_init(&global_lock);
    capability_count = 0;
    next_cap_id = 1;
    
    // Clear capability array
    for (uint32_t i = 0; i < MAX_CAPABILITIES; i++) {
        capabilities[i].cap_id = 0;
        capabilities[i].type = 0;
        capabilities[i].resource_id = 0;
        capabilities[i].rights = 0;
        capabilities[i].next = NULL;
    }
    
    // Initialize per-process capability tables
    for (size_t i = 0; i < MAX_PROCESSES; i++) {
        capability_tables[i].capabilities = NULL;
        capability_tables[i].count = 0;
        capability_tables[i].max_count = MAX_CAPABILITIES_PER_PROCESS;
        spinlock_init(&capability_tables[i].lock);
    }
    
    capability_tables_initialized = true;
    kinfo("Capability system initialized\n");
}

/**
 * Create a new capability
 */
uint64_t capability_create(uint32_t type, uint64_t resource_id, uint32_t rights) {
    if (capability_count >= MAX_CAPABILITIES) {
        return 0;  // Out of capabilities
    }
    
    spinlock_lock(&global_lock);
    
    // Find free slot
    uint32_t idx = capability_count;
    capability_t* cap = &capabilities[idx];
    
    cap->cap_id = next_cap_id++;
    cap->type = type;
    cap->resource_id = resource_id;
    cap->rights = rights;
    cap->next = NULL;
    
    capability_count++;
    
    spinlock_unlock(&global_lock);
    
    return cap->cap_id;
}

/**
 * Check if capability grants right
 */
bool capability_check(uint64_t cap_id, uint32_t right) {
    capability_t* cap = find_capability(cap_id);
    if (!cap) {
        return false;  // Capability not found
    }
    
    // Check if right is granted
    return (cap->rights & right) != 0;
}

/**
 * Transfer capability in IPC message
 */
int capability_transfer(ipc_message_t* msg, uint64_t cap_id) {
    if (!msg) {
        return -1;
    }
    
    // Verify capability exists and can be transferred
    capability_t* cap = find_capability(cap_id);
    if (!cap) {
        return -1;  // Capability not found
    }
    
    // Check if capability has transfer right
    if (!capability_check(cap_id, CAP_RIGHT_TRANSFER)) {
        return -1;  // No transfer right
    }
    
    // Add capability ID to message inline data
    // For now, we'll store it in the first 8 bytes if there's space
    if (msg->inline_size + 8 <= IPC_INLINE_SIZE) {
        uint64_t* cap_ptr = (uint64_t*)(msg->inline_data + msg->inline_size);
        *cap_ptr = cap_id;
        msg->inline_size += 8;
    } else {
        // Would need to use buffer for larger messages
        return -1;
    }
    
    return 0;
}

/**
 * Revoke capability
 */
int capability_revoke(uint64_t cap_id) {
    capability_t* cap = find_capability(cap_id);
    if (!cap) {
        return -1;  // Capability not found
    }
    
    spinlock_lock(&global_lock);
    
    // Mark capability as revoked (set ID to 0)
    cap->cap_id = 0;
    cap->type = 0;
    cap->resource_id = 0;
    cap->rights = 0;
    
    // TODO: Notify processes using this capability
    // TODO: Remove from process capability tables
    
    spinlock_unlock(&global_lock);
    
    return 0;
}

/**
 * Find capability for a port (helper function for IPC)
 */
uint64_t capability_find_for_port(uint64_t port_id) {
    // TODO: Look up capability in current process's capability table
    // For now, search global capability list
    spinlock_lock(&global_lock);
    
    for (uint32_t i = 0; i < capability_count; i++) {
        if (capabilities[i].cap_id != 0 &&
            capabilities[i].type == CAP_TYPE_IPC_PORT &&
            capabilities[i].resource_id == port_id) {
            uint64_t cap_id = capabilities[i].cap_id;
            spinlock_unlock(&global_lock);
            return cap_id;
        }
    }
    
    spinlock_unlock(&global_lock);
    return 0;  // No capability found
}

