/**
 * @file rbac.c
 * @brief Role-Based Access Control (RBAC) implementation
 */

#include "../include/security/rbac.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/mm/heap.h"
#include "../include/sync/spinlock.h"

// RBAC state
static struct {
    rbac_role_t roles[MAX_ROLES];
    rbac_user_roles_t user_roles[256];  // Support up to 256 users
    uint32_t role_count;
    uint32_t user_count;
    spinlock_t lock;
    bool initialized;
} rbac_state = {0};

/**
 * Initialize RBAC system
 */
error_code_t rbac_init(void) {
    if (rbac_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing RBAC system...\n");
    
    memset(&rbac_state, 0, sizeof(rbac_state));
    spinlock_init(&rbac_state.lock);
    rbac_state.initialized = true;
    
    kinfo("RBAC system initialized\n");
    return ERR_OK;
}

/**
 * Create a new role
 */
error_code_t rbac_create_role(uint32_t role_id, const char* name) {
    if (!name) {
        return ERR_INVALID_ARG;
    }
    
    if (!rbac_state.initialized) {
        rbac_init();
    }
    
    spinlock_lock(&rbac_state.lock);
    
    // Check if role already exists
    for (uint32_t i = 0; i < rbac_state.role_count; i++) {
        if (rbac_state.roles[i].role_id == role_id) {
            spinlock_unlock(&rbac_state.lock);
            return ERR_ALREADY_EXISTS;
        }
    }
    
    if (rbac_state.role_count >= MAX_ROLES) {
        spinlock_unlock(&rbac_state.lock);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Create new role
    rbac_role_t* role = &rbac_state.roles[rbac_state.role_count++];
    role->role_id = role_id;
    strncpy(role->name, name, sizeof(role->name) - 1);
    role->name[sizeof(role->name) - 1] = '\0';
    role->permission_count = 0;
    
    spinlock_unlock(&rbac_state.lock);
    
    kinfo("RBAC: Created role %u (%s)\n", role_id, name);
    return ERR_OK;
}

/**
 * Add permission to role
 */
error_code_t rbac_add_permission_to_role(uint32_t role_id, uint32_t permission) {
    if (!rbac_state.initialized) {
        rbac_init();
    }
    
    spinlock_lock(&rbac_state.lock);
    
    // Find role
    rbac_role_t* role = NULL;
    for (uint32_t i = 0; i < rbac_state.role_count; i++) {
        if (rbac_state.roles[i].role_id == role_id) {
            role = &rbac_state.roles[i];
            break;
        }
    }
    
    if (!role) {
        spinlock_unlock(&rbac_state.lock);
        return ERR_NOT_FOUND;
    }
    
    // Check if permission already exists
    for (uint32_t i = 0; i < role->permission_count; i++) {
        if (role->permissions[i] == permission) {
            spinlock_unlock(&rbac_state.lock);
            return ERR_OK;  // Already has permission
        }
    }
    
    if (role->permission_count >= MAX_PERMISSIONS_PER_ROLE) {
        spinlock_unlock(&rbac_state.lock);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Add permission
    role->permissions[role->permission_count++] = permission;
    
    spinlock_unlock(&rbac_state.lock);
    return ERR_OK;
}

/**
 * Assign role to user
 */
error_code_t rbac_assign_role_to_user(uint32_t uid, uint32_t role_id) {
    if (!rbac_state.initialized) {
        rbac_init();
    }
    
    spinlock_lock(&rbac_state.lock);
    
    // Find or create user entry
    rbac_user_roles_t* user_roles = NULL;
    for (uint32_t i = 0; i < rbac_state.user_count; i++) {
        if (rbac_state.user_roles[i].uid == uid) {
            user_roles = &rbac_state.user_roles[i];
            break;
        }
    }
    
    if (!user_roles) {
        if (rbac_state.user_count >= 256) {
            spinlock_unlock(&rbac_state.lock);
            return ERR_OUT_OF_MEMORY;
        }
        user_roles = &rbac_state.user_roles[rbac_state.user_count++];
        user_roles->uid = uid;
        user_roles->role_count = 0;
    }
    
    // Check if role already assigned
    for (uint32_t i = 0; i < user_roles->role_count; i++) {
        if (user_roles->roles[i] == role_id) {
            spinlock_unlock(&rbac_state.lock);
            return ERR_OK;  // Already assigned
        }
    }
    
    if (user_roles->role_count >= MAX_ROLES_PER_USER) {
        spinlock_unlock(&rbac_state.lock);
        return ERR_OUT_OF_MEMORY;
    }
    
    // Assign role
    user_roles->roles[user_roles->role_count++] = role_id;
    
    spinlock_unlock(&rbac_state.lock);
    kinfo("RBAC: Assigned role %u to user %u\n", role_id, uid);
    return ERR_OK;
}

/**
 * Remove role from user
 */
error_code_t rbac_remove_role_from_user(uint32_t uid, uint32_t role_id) {
    if (!rbac_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    spinlock_lock(&rbac_state.lock);
    
    // Find user
    rbac_user_roles_t* user_roles = NULL;
    for (uint32_t i = 0; i < rbac_state.user_count; i++) {
        if (rbac_state.user_roles[i].uid == uid) {
            user_roles = &rbac_state.user_roles[i];
            break;
        }
    }
    
    if (!user_roles) {
        spinlock_unlock(&rbac_state.lock);
        return ERR_NOT_FOUND;
    }
    
    // Remove role
    for (uint32_t i = 0; i < user_roles->role_count; i++) {
        if (user_roles->roles[i] == role_id) {
            // Shift remaining roles
            for (uint32_t j = i; j < user_roles->role_count - 1; j++) {
                user_roles->roles[j] = user_roles->roles[j + 1];
            }
            user_roles->role_count--;
            spinlock_unlock(&rbac_state.lock);
            return ERR_OK;
        }
    }
    
    spinlock_unlock(&rbac_state.lock);
    return ERR_NOT_FOUND;
}

/**
 * Check if user has permission (via roles)
 */
bool rbac_user_has_permission(uint32_t uid, uint32_t permission) {
    if (!rbac_state.initialized) {
        return false;
    }
    
    spinlock_lock(&rbac_state.lock);
    
    // Find user
    rbac_user_roles_t* user_roles = NULL;
    for (uint32_t i = 0; i < rbac_state.user_count; i++) {
        if (rbac_state.user_roles[i].uid == uid) {
            user_roles = &rbac_state.user_roles[i];
            break;
        }
    }
    
    if (!user_roles) {
        spinlock_unlock(&rbac_state.lock);
        return false;
    }
    
    // Check all user's roles for the permission
    for (uint32_t i = 0; i < user_roles->role_count; i++) {
        uint32_t role_id = user_roles->roles[i];
        
        // Find role
        for (uint32_t j = 0; j < rbac_state.role_count; j++) {
            if (rbac_state.roles[j].role_id == role_id) {
                // Check permissions in this role
                for (uint32_t k = 0; k < rbac_state.roles[j].permission_count; k++) {
                    if (rbac_state.roles[j].permissions[k] == permission) {
                        spinlock_unlock(&rbac_state.lock);
                        return true;
                    }
                }
                break;
            }
        }
    }
    
    spinlock_unlock(&rbac_state.lock);
    return false;
}

/**
 * Check if user has role
 */
bool rbac_user_has_role(uint32_t uid, uint32_t role_id) {
    if (!rbac_state.initialized) {
        return false;
    }
    
    spinlock_lock(&rbac_state.lock);
    
    // Find user
    for (uint32_t i = 0; i < rbac_state.user_count; i++) {
        if (rbac_state.user_roles[i].uid == uid) {
            // Check roles
            for (uint32_t j = 0; j < rbac_state.user_roles[i].role_count; j++) {
                if (rbac_state.user_roles[i].roles[j] == role_id) {
                    spinlock_unlock(&rbac_state.lock);
                    return true;
                }
            }
            break;
        }
    }
    
    spinlock_unlock(&rbac_state.lock);
    return false;
}

/**
 * Get user roles
 */
error_code_t rbac_get_user_roles(uint32_t uid, uint32_t* roles, uint32_t* count) {
    if (!roles || !count) {
        return ERR_INVALID_ARG;
    }
    
    if (!rbac_state.initialized) {
        return ERR_INVALID_STATE;
    }
    
    spinlock_lock(&rbac_state.lock);
    
    // Find user
    for (uint32_t i = 0; i < rbac_state.user_count; i++) {
        if (rbac_state.user_roles[i].uid == uid) {
            uint32_t copy_count = *count < rbac_state.user_roles[i].role_count ?
                                  *count : rbac_state.user_roles[i].role_count;
            memcpy(roles, rbac_state.user_roles[i].roles, copy_count * sizeof(uint32_t));
            *count = rbac_state.user_roles[i].role_count;
            spinlock_unlock(&rbac_state.lock);
            return ERR_OK;
        }
    }
    
    *count = 0;
    spinlock_unlock(&rbac_state.lock);
    return ERR_NOT_FOUND;
}

