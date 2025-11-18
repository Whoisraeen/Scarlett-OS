/**
 * @file permissions.c
 * @brief File permissions implementation
 */

#include "../include/types.h"
#include "../include/fs/permissions.h"
#include "../include/kprintf.h"

// For now, assume root user (UID 0) and root group (GID 0)
// TODO: Integrate with user/group system when available
static uint32_t current_uid = 0;
static uint32_t current_gid = 0;

/**
 * Check if user has read permission
 */
bool permissions_check_read(file_permissions_t* perms, uint32_t uid, uint32_t gid) {
    if (!perms) {
        return false;
    }
    
    // Root (UID 0) always has permission
    if (uid == 0) {
        return true;
    }
    
    // Check owner permissions
    if (uid == perms->uid) {
        return (perms->mode & PERM_OWNER_READ) != 0;
    }
    
    // Check group permissions
    if (gid == perms->gid) {
        return (perms->mode & PERM_GROUP_READ) != 0;
    }
    
    // Check other permissions
    return (perms->mode & PERM_OTHER_READ) != 0;
}

/**
 * Check if user has write permission
 */
bool permissions_check_write(file_permissions_t* perms, uint32_t uid, uint32_t gid) {
    if (!perms) {
        return false;
    }
    
    // Root (UID 0) always has permission
    if (uid == 0) {
        return true;
    }
    
    // Check owner permissions
    if (uid == perms->uid) {
        return (perms->mode & PERM_OWNER_WRITE) != 0;
    }
    
    // Check group permissions
    if (gid == perms->gid) {
        return (perms->mode & PERM_GROUP_WRITE) != 0;
    }
    
    // Check other permissions
    return (perms->mode & PERM_OTHER_WRITE) != 0;
}

/**
 * Check if user has execute permission
 */
bool permissions_check_execute(file_permissions_t* perms, uint32_t uid, uint32_t gid) {
    if (!perms) {
        return false;
    }
    
    // Root (UID 0) always has permission
    if (uid == 0) {
        return true;
    }
    
    // Check owner permissions
    if (uid == perms->uid) {
        return (perms->mode & PERM_OWNER_EXECUTE) != 0;
    }
    
    // Check group permissions
    if (gid == perms->gid) {
        return (perms->mode & PERM_GROUP_EXECUTE) != 0;
    }
    
    // Check other permissions
    return (perms->mode & PERM_OTHER_EXECUTE) != 0;
}

/**
 * Set permission mode
 */
void permissions_set_mode(file_permissions_t* perms, uint16_t mode) {
    if (!perms) {
        return;
    }
    
    // Only keep lower 12 bits (3 for type, 9 for permissions)
    perms->mode = mode & 0x0FFF;
}

/**
 * Set file owner
 */
void permissions_set_owner(file_permissions_t* perms, uint32_t uid, uint32_t gid) {
    if (!perms) {
        return;
    }
    
    perms->uid = uid;
    perms->gid = gid;
}

/**
 * Get current user ID (placeholder - will integrate with user system)
 */
uint32_t permissions_get_current_uid(void) {
    return current_uid;
}

/**
 * Get current group ID (placeholder - will integrate with user system)
 */
uint32_t permissions_get_current_gid(void) {
    return current_gid;
}

