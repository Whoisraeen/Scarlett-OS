/**
 * @file audit.c
 * @brief Audit subsystem implementation
 */

#include "../include/security/audit.h"
#include "../include/fs/vfs.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"
#include "../include/sync/spinlock.h"
#include "../include/time.h"

// Audit log buffer (circular buffer)
static audit_log_entry_t audit_log_buffer[AUDIT_LOG_MAX_ENTRIES];
static uint64_t audit_log_head = 0;  // Next write position
static uint64_t audit_log_tail = 0;  // Oldest entry
static uint64_t audit_sequence = 0;  // Sequence counter
static bool audit_enabled_flag = true;
static spinlock_t audit_lock;
static bool audit_initialized = false;

// Statistics
static uint64_t audit_total_events = 0;
static uint64_t audit_failed_events = 0;
static uint64_t audit_last_event_time = 0;

/**
 * Initialize audit subsystem
 */
error_code_t audit_init(void) {
    if (audit_initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing audit subsystem...\n");
    
    memset(audit_log_buffer, 0, sizeof(audit_log_buffer));
    audit_log_head = 0;
    audit_log_tail = 0;
    audit_sequence = 0;
    audit_enabled_flag = true;
    spinlock_init(&audit_lock);
    audit_total_events = 0;
    audit_failed_events = 0;
    audit_last_event_time = 0;
    
    audit_initialized = true;
    kinfo("Audit subsystem initialized\n");
    return ERR_OK;
}

/**
 * Log an audit event
 */
error_code_t audit_log(audit_event_type_t event_type, uint32_t uid, uint32_t gid,
                       uint32_t pid, uint32_t result, const char* subject,
                       const char* object, const char* action, const char* details) {
    if (!audit_initialized || !audit_enabled_flag) {
        return ERR_OK;  // Silently ignore if not initialized or disabled
    }
    
    if (event_type >= AUDIT_EVENT_MAX) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_lock(&audit_lock);
    
    // Get current time
    uint64_t timestamp = time_get_uptime_ms();  // Use uptime for now
    
    // Create audit event
    audit_event_t event;
    memset(&event, 0, sizeof(event));
    event.timestamp = timestamp;
    event.event_type = event_type;
    event.uid = uid;
    event.gid = gid;
    event.pid = pid;
    event.result = result;
    
    if (subject) {
        strncpy(event.subject, subject, sizeof(event.subject) - 1);
    }
    if (object) {
        strncpy(event.object, object, sizeof(event.object) - 1);
    }
    if (action) {
        strncpy(event.action, action, sizeof(event.action) - 1);
    }
    if (details) {
        strncpy(event.details, details, sizeof(event.details) - 1);
    }
    
    // Add to circular buffer
    uint64_t next_head = (audit_log_head + 1) % AUDIT_LOG_MAX_ENTRIES;
    
    if (next_head == audit_log_tail) {
        // Buffer full, advance tail (overwrite oldest entry)
        audit_log_tail = (audit_log_tail + 1) % AUDIT_LOG_MAX_ENTRIES;
    }
    
    audit_log_entry_t* entry = &audit_log_buffer[audit_log_head];
    entry->sequence = audit_sequence++;
    entry->event = event;
    
    audit_log_head = next_head;
    
    // Update statistics
    audit_total_events++;
    if (result != 0) {
        audit_failed_events++;
    }
    audit_last_event_time = timestamp;
    
    spinlock_unlock(&audit_lock);
    
    return ERR_OK;
}

/**
 * Query audit log
 */
error_code_t audit_query(uint64_t start_time, uint64_t end_time,
                        audit_event_type_t event_type, uint32_t uid,
                        audit_log_entry_t* results, size_t max_results, size_t* result_count) {
    if (!audit_initialized || !results || !result_count) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_lock(&audit_lock);
    
    size_t count = 0;
    uint64_t current = audit_log_tail;
    
    // Iterate through circular buffer
    while (current != audit_log_head && count < max_results) {
        audit_log_entry_t* entry = &audit_log_buffer[current];
        
        // Check filters
        bool match = true;
        
        if (start_time > 0 && entry->event.timestamp < start_time) {
            match = false;
        }
        if (end_time > 0 && entry->event.timestamp > end_time) {
            match = false;
        }
        if (event_type < AUDIT_EVENT_MAX && entry->event.event_type != event_type) {
            match = false;
        }
        if (uid != 0 && entry->event.uid != uid) {
            match = false;
        }
        
        if (match) {
            results[count] = *entry;
            count++;
        }
        
        current = (current + 1) % AUDIT_LOG_MAX_ENTRIES;
    }
    
    *result_count = count;
    
    spinlock_unlock(&audit_lock);
    
    return ERR_OK;
}

/**
 * Get audit log statistics
 */
error_code_t audit_get_stats(uint64_t* total_events, uint64_t* failed_events,
                             uint64_t* last_event_time) {
    if (!audit_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    spinlock_lock(&audit_lock);
    
    if (total_events) {
        *total_events = audit_total_events;
    }
    if (failed_events) {
        *failed_events = audit_failed_events;
    }
    if (last_event_time) {
        *last_event_time = audit_last_event_time;
    }
    
    spinlock_unlock(&audit_lock);
    
    return ERR_OK;
}

/**
 * Flush audit log to disk
 */
error_code_t audit_flush(void) {
    if (!audit_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    // Open audit log file
    fd_t fd;
    error_code_t err = vfs_open(AUDIT_LOG_FILE, VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &fd);
    if (err != ERR_OK) {
        // Try creating /var/log directory first
        vfs_mkdir("/var");
        vfs_mkdir("/var/log");
        err = vfs_open(AUDIT_LOG_FILE, VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &fd);
        if (err != ERR_OK) {
            return err;
        }
    }
    
    spinlock_lock(&audit_lock);
    
    // Write all entries in buffer
    uint64_t current = audit_log_tail;
    size_t bytes_written;
    
    while (current != audit_log_head) {
        audit_log_entry_t* entry = &audit_log_buffer[current];
        err = vfs_write(fd, entry, sizeof(audit_log_entry_t), &bytes_written);
        if (err != ERR_OK || bytes_written != sizeof(audit_log_entry_t)) {
            break;
        }
        current = (current + 1) % AUDIT_LOG_MAX_ENTRIES;
    }
    
    spinlock_unlock(&audit_lock);
    
    vfs_close(fd);
    
    return ERR_OK;
}

/**
 * Clear audit log
 */
error_code_t audit_clear(void) {
    if (!audit_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    spinlock_lock(&audit_lock);
    
    memset(audit_log_buffer, 0, sizeof(audit_log_buffer));
    audit_log_head = 0;
    audit_log_tail = 0;
    audit_sequence = 0;
    audit_total_events = 0;
    audit_failed_events = 0;
    audit_last_event_time = 0;
    
    spinlock_unlock(&audit_lock);
    
    return ERR_OK;
}

/**
 * Enable audit logging
 */
error_code_t audit_enable(void) {
    if (!audit_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    audit_enabled_flag = true;
    return ERR_OK;
}

/**
 * Disable audit logging
 */
error_code_t audit_disable(void) {
    if (!audit_initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    audit_enabled_flag = false;
    return ERR_OK;
}

/**
 * Check if audit logging is enabled
 */
bool audit_is_enabled(void) {
    return audit_initialized && audit_enabled_flag;
}

