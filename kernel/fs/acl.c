/**
 * @file acl.c
 * @brief Access Control List (ACL) implementation
 */

#include "../include/fs/acl.h"
#include "../include/fs/permissions.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/sync/spinlock.h"

static bool acl_initialized = false;
static spinlock_t acl_lock = SPINLOCK_INIT;

/**
 * Initialize ACL system
 */
error_code_t acl_init(void) {
    if (acl_initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing ACL system...\n");
    spinlock_init(&acl_lock);
    acl_initialized = true;
    kinfo("ACL system initialized\n");
    return ERR_OK;
}

/**
 * Create a new ACL
 */
error_code_t acl_create(acl_t* acl) {
    if (!acl) {
        return ERR_INVALID_ARG;
    }
    
    memset(acl, 0, sizeof(acl_t));
    acl->entry_count = 0;
    return ERR_OK;
}

/**
 * Add entry to ACL
 */
error_code_t acl_add_entry(acl_t* acl, uint8_t type, uint32_t id, uint8_t perms) {
    if (!acl) {
        return ERR_INVALID_ARG;
    }
    
    if (acl->entry_count >= MAX_ACL_ENTRIES) {
        kerror("ACL: Maximum entries reached\n");
        return ERR_OUT_OF_MEMORY;
    }
    
    // Check if entry already exists
    for (uint32_t i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].type == type && acl->entries[i].id == id) {
            // Update existing entry
            acl->entries[i].perms = perms;
            return ERR_OK;
        }
    }
    
    // Add new entry
    acl_entry_t* entry = &acl->entries[acl->entry_count++];
    entry->type = type;
    entry->id = id;
    entry->perms = perms;
    
    return ERR_OK;
}

/**
 * Remove entry from ACL
 */
error_code_t acl_remove_entry(acl_t* acl, uint8_t type, uint32_t id) {
    if (!acl) {
        return ERR_INVALID_ARG;
    }
    
    for (uint32_t i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].type == type && acl->entries[i].id == id) {
            // Remove entry by shifting remaining entries
            for (uint32_t j = i; j < acl->entry_count - 1; j++) {
                acl->entries[j] = acl->entries[j + 1];
            }
            acl->entry_count--;
            return ERR_OK;
        }
    }
    
    return ERR_NOT_FOUND;
}

/**
 * Get entry from ACL
 */
error_code_t acl_get_entry(const acl_t* acl, uint8_t type, uint32_t id, acl_entry_t* entry) {
    if (!acl || !entry) {
        return ERR_INVALID_ARG;
    }
    
    for (uint32_t i = 0; i < acl->entry_count; i++) {
        if (acl->entries[i].type == type && acl->entries[i].id == id) {
            *entry = acl->entries[i];
            return ERR_OK;
        }
    }
    
    return ERR_NOT_FOUND;
}

/**
 * Check access using ACL
 */
error_code_t acl_check_access(const acl_t* acl, uint32_t uid, uint32_t gid, uint8_t requested_perms) {
    if (!acl) {
        return ERR_INVALID_ARG;
    }
    
    // Check user entry first
    acl_entry_t user_entry;
    if (acl_get_entry(acl, ACL_ENTRY_USER, uid, &user_entry) == ERR_OK) {
        if ((user_entry.perms & requested_perms) == requested_perms) {
            return ERR_OK;  // Access granted
        }
        return ERR_PERMISSION_DENIED;  // Access denied
    }
    
    // Check group entry
    acl_entry_t group_entry;
    if (acl_get_entry(acl, ACL_ENTRY_GROUP, gid, &group_entry) == ERR_OK) {
        // Check mask if present
        acl_entry_t mask_entry;
        uint8_t effective_perms = group_entry.perms;
        if (acl_get_entry(acl, ACL_ENTRY_MASK, 0, &mask_entry) == ERR_OK) {
            effective_perms &= mask_entry.perms;
        }
        
        if ((effective_perms & requested_perms) == requested_perms) {
            return ERR_OK;  // Access granted
        }
    }
    
    // Check other entry
    acl_entry_t other_entry;
    if (acl_get_entry(acl, ACL_ENTRY_OTHER, 0, &other_entry) == ERR_OK) {
        if ((other_entry.perms & requested_perms) == requested_perms) {
            return ERR_OK;  // Access granted
        }
    }
    
    return ERR_PERMISSION_DENIED;
}

/**
 * Set default ACL from mode bits
 */
error_code_t acl_set_default(acl_t* acl, uint16_t mode, uint32_t uid, uint32_t gid) {
    if (!acl) {
        return ERR_INVALID_ARG;
    }
    
    acl_create(acl);
    
    // Owner permissions
    uint8_t owner_perms = 0;
    if (mode & PERM_OWNER_READ) owner_perms |= ACL_READ;
    if (mode & PERM_OWNER_WRITE) owner_perms |= ACL_WRITE;
    if (mode & PERM_OWNER_EXECUTE) owner_perms |= ACL_EXECUTE;
    acl_add_entry(acl, ACL_ENTRY_USER, uid, owner_perms);
    
    // Group permissions
    uint8_t group_perms = 0;
    if (mode & PERM_GROUP_READ) group_perms |= ACL_READ;
    if (mode & PERM_GROUP_WRITE) group_perms |= ACL_WRITE;
    if (mode & PERM_GROUP_EXECUTE) group_perms |= ACL_EXECUTE;
    acl_add_entry(acl, ACL_ENTRY_GROUP, gid, group_perms);
    
    // Other permissions
    uint8_t other_perms = 0;
    if (mode & PERM_OTHER_READ) other_perms |= ACL_READ;
    if (mode & PERM_OTHER_WRITE) other_perms |= ACL_WRITE;
    if (mode & PERM_OTHER_EXECUTE) other_perms |= ACL_EXECUTE;
    acl_add_entry(acl, ACL_ENTRY_OTHER, 0, other_perms);
    
    return ERR_OK;
}

