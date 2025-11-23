/**
 * @file user.c
 * @brief User and group management implementation
 */

#include "../include/types.h"
#include "../include/auth/user.h"
#include "../include/auth/password_hash.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/mm/heap.h"
#include "../include/errors.h"

// Forward declaration for persistence functions
extern error_code_t user_save_to_disk(void);
extern error_code_t user_load_from_disk(void);
extern error_code_t group_save_to_disk(void);
extern error_code_t group_load_from_disk(void);
extern error_code_t create_home_directory(const char* username, uid_t uid, gid_t gid);

// User and group databases
static user_t users[MAX_USERS];
static group_t groups[MAX_GROUPS];
static uint32_t user_count = 0;
static uint32_t group_count = 0;
static uid_t next_uid = 1000;  // Start UIDs at 1000 (0 is root)
static gid_t next_gid = 1000;  // Start GIDs at 1000 (0 is root)

// Current user/group - Now stored in process_t, globals removed

/**
 * Get current UID
 */
uid_t get_current_uid(void) {
    extern process_t* process_get_current(void);
    process_t* proc = process_get_current();
    if (proc) {
        return proc->uid;
    }
    // Fallback for early boot or kernel threads (running as root)
    return ROOT_UID;
}

/**
 * Get current GID
 */
gid_t get_current_gid(void) {
    extern process_t* process_get_current(void);
    process_t* proc = process_get_current();
    if (proc) {
        return proc->gid;
    }
    return ROOT_GID;
}

/**
 * Set current UID
 */
error_code_t set_current_uid(uid_t uid) {
    extern process_t* process_get_current(void);
    process_t* proc = process_get_current();
    
    if (!proc) return ERR_INVALID_STATE;
    
    // Check permissions (only root can change UID arbitrarily)
    // Real implementation needs complex setuid rules (saved set-user-ID, etc.)
    // For now: only root or setting to self (no-op)
    if (proc->uid != ROOT_UID && proc->uid != uid) {
        return ERR_PERMISSION_DENIED;
    }
    
    proc->uid = uid;
    return ERR_OK;
}

/**
 * Set current GID
 */
error_code_t set_current_gid(gid_t gid) {
    extern process_t* process_get_current(void);
    process_t* proc = process_get_current();
    
    if (!proc) return ERR_INVALID_STATE;
    
    // Check permissions
    if (proc->uid != ROOT_UID && proc->gid != gid) {
        return ERR_PERMISSION_DENIED;
    }
    
    proc->gid = gid;
    return ERR_OK;
}

