/**
 * @file audit.h
 * @brief Audit subsystem interface
 */

#ifndef KERNEL_SECURITY_AUDIT_H
#define KERNEL_SECURITY_AUDIT_H

#include "../types.h"
#include "../errors.h"

// Audit event types
typedef enum {
    AUDIT_EVENT_LOGIN,              // User login
    AUDIT_EVENT_LOGOUT,             // User logout
    AUDIT_EVENT_FILE_OPEN,          // File opened
    AUDIT_EVENT_FILE_WRITE,         // File written
    AUDIT_EVENT_FILE_DELETE,        // File deleted
    AUDIT_EVENT_PROCESS_CREATE,     // Process created
    AUDIT_EVENT_PROCESS_EXIT,       // Process exited
    AUDIT_EVENT_IPC_SEND,           // IPC message sent
    AUDIT_EVENT_IPC_RECEIVE,        // IPC message received
    AUDIT_EVENT_CAPABILITY_USE,     // Capability used
    AUDIT_EVENT_PERMISSION_DENIED, // Permission denied
    AUDIT_EVENT_SYSCALL,            // System call executed
    AUDIT_EVENT_MOUNT,              // Filesystem mounted
    AUDIT_EVENT_UNMOUNT,            // Filesystem unmounted
    AUDIT_EVENT_NETWORK_CONNECT,    // Network connection
    AUDIT_EVENT_NETWORK_DISCONNECT, // Network disconnection
    AUDIT_EVENT_CONFIG_CHANGE,      // System configuration changed
    AUDIT_EVENT_MAX
} audit_event_type_t;

// Audit event structure
typedef struct {
    uint64_t timestamp;             // Event timestamp
    audit_event_type_t event_type;  // Type of event
    uint32_t uid;                   // User ID
    uint32_t gid;                   // Group ID
    uint32_t pid;                   // Process ID
    uint32_t result;                // Result (0 = success, error code = failure)
    char subject[64];               // Subject (username, process name, etc.)
    char object[256];               // Object (file path, resource, etc.)
    char action[64];                // Action performed
    char details[512];              // Additional details
} audit_event_t;

// Audit log entry (with sequence number)
typedef struct {
    uint64_t sequence;              // Sequence number
    audit_event_t event;            // Event data
} audit_log_entry_t;

// Maximum audit log size
#define AUDIT_LOG_MAX_ENTRIES 10000
#define AUDIT_LOG_FILE "/var/log/audit.log"

// Initialize audit subsystem
error_code_t audit_init(void);

// Log an audit event
error_code_t audit_log(audit_event_type_t event_type, uint32_t uid, uint32_t gid, 
                       uint32_t pid, uint32_t result, const char* subject,
                       const char* object, const char* action, const char* details);

// Query audit log
error_code_t audit_query(uint64_t start_time, uint64_t end_time, 
                        audit_event_type_t event_type, uint32_t uid,
                        audit_log_entry_t* results, size_t max_results, size_t* result_count);

// Get audit log statistics
error_code_t audit_get_stats(uint64_t* total_events, uint64_t* failed_events,
                             uint64_t* last_event_time);

// Flush audit log to disk
error_code_t audit_flush(void);

// Clear audit log
error_code_t audit_clear(void);

// Enable/disable audit logging
error_code_t audit_enable(void);
error_code_t audit_disable(void);
bool audit_is_enabled(void);

#endif // KERNEL_SECURITY_AUDIT_H

