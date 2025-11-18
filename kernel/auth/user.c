/**
 * @file user.c
 * @brief User and group management implementation
 */

#include "../include/types.h"
#include "../include/auth/user.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/mm/heap.h"
#include "../include/errors.h"

// User and group databases
static user_t users[MAX_USERS];
static group_t groups[MAX_GROUPS];
static uint32_t user_count = 0;
static uint32_t group_count = 0;
static uid_t next_uid = 1000;  // Start UIDs at 1000 (0 is root)
static gid_t next_gid = 1000;  // Start GIDs at 1000 (0 is root)

// Current user/group (per-process, simplified to global for now)
static uid_t current_uid = ROOT_UID;
static gid_t current_gid = ROOT_GID;

/**
 * Simple password hashing (placeholder - should use bcrypt/scrypt)
 */
static void hash_password(const char* password, char* hash) {
    // TODO: Implement proper bcrypt/scrypt hashing
    // For now, simple placeholder
    strncpy(hash, password, MAX_PASSWORD_HASH_LEN - 1);
    hash[MAX_PASSWORD_HASH_LEN - 1] = '\0';
}

/**
 * Verify password
 */
static bool verify_password(const char* password, const char* hash) {
    // TODO: Implement proper password verification
    // For now, simple comparison (NOT SECURE - placeholder)
    return strcmp(password, hash) == 0;
}

/**
 * Initialize user system
 */
error_code_t user_init(void) {
    kinfo("Initializing user system...\n");
    
    memset(users, 0, sizeof(users));
    memset(groups, 0, sizeof(groups));
    user_count = 0;
    group_count = 0;
    
    // Create root user
    user_t* root = &users[0];
    root->uid = ROOT_UID;
    root->gid = ROOT_GID;
    strcpy(root->username, "root");
    hash_password("root", root->password_hash);
    root->active = true;
    user_count = 1;
    
    // Create root group
    group_t* root_group = &groups[0];
    root_group->gid = ROOT_GID;
    strcpy(root_group->groupname, "root");
    root_group->members[0] = ROOT_UID;
    root_group->member_count = 1;
    group_count = 1;
    
    kinfo("User system initialized (root user created)\n");
    return ERR_OK;
}

/**
 * Create new user
 */
error_code_t user_create(const char* username, const char* password, uid_t* uid) {
    if (!username || !password || !uid) {
        return ERR_INVALID_ARG;
    }
    
    if (user_count >= MAX_USERS) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Check if username already exists
    if (user_get_by_username(username) != NULL) {
        return ERR_ALREADY_EXISTS;
    }
    
    user_t* user = &users[user_count];
    user->uid = next_uid++;
    user->gid = user->uid;  // Create group with same ID
    strncpy(user->username, username, MAX_USERNAME_LEN - 1);
    user->username[MAX_USERNAME_LEN - 1] = '\0';
    hash_password(password, user->password_hash);
    user->active = true;
    
    *uid = user->uid;
    user_count++;
    
    kinfo("User created: %s (UID: %u)\n", username, user->uid);
    return ERR_OK;
}

/**
 * Delete user
 */
error_code_t user_delete(uid_t uid) {
    if (uid == ROOT_UID) {
        return ERR_PERMISSION_DENIED;  // Cannot delete root
    }
    
    for (uint32_t i = 0; i < user_count; i++) {
        if (users[i].uid == uid) {
            users[i].active = false;
            kinfo("User deleted: UID %u\n", uid);
            return ERR_OK;
        }
    }
    
    return ERR_NOT_FOUND;
}

/**
 * Get user by UID
 */
user_t* user_get_by_uid(uid_t uid) {
    for (uint32_t i = 0; i < user_count; i++) {
        if (users[i].uid == uid && users[i].active) {
            return &users[i];
        }
    }
    return NULL;
}

/**
 * Get user by username
 */
user_t* user_get_by_username(const char* username) {
    if (!username) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, username) == 0 && users[i].active) {
            return &users[i];
        }
    }
    return NULL;
}

/**
 * Authenticate user
 */
error_code_t user_authenticate(const char* username, const char* password, uid_t* uid) {
    if (!username || !password || !uid) {
        return ERR_INVALID_ARG;
    }
    
    user_t* user = user_get_by_username(username);
    if (!user) {
        return ERR_NOT_FOUND;
    }
    
    if (!verify_password(password, user->password_hash)) {
        return ERR_PERMISSION_DENIED;
    }
    
    *uid = user->uid;
    return ERR_OK;
}

/**
 * Set user password
 */
error_code_t user_set_password(uid_t uid, const char* password) {
    if (!password) {
        return ERR_INVALID_ARG;
    }
    
    user_t* user = user_get_by_uid(uid);
    if (!user) {
        return ERR_NOT_FOUND;
    }
    
    hash_password(password, user->password_hash);
    return ERR_OK;
}

/**
 * Initialize group system
 */
error_code_t group_init(void) {
    // Already initialized in user_init
    return ERR_OK;
}

/**
 * Create new group
 */
error_code_t group_create(const char* groupname, gid_t* gid) {
    if (!groupname || !gid) {
        return ERR_INVALID_ARG;
    }
    
    if (group_count >= MAX_GROUPS) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Check if group already exists
    if (group_get_by_name(groupname) != NULL) {
        return ERR_ALREADY_EXISTS;
    }
    
    group_t* group = &groups[group_count];
    group->gid = next_gid++;
    strncpy(group->groupname, groupname, MAX_GROUPNAME_LEN - 1);
    group->groupname[MAX_GROUPNAME_LEN - 1] = '\0';
    group->member_count = 0;
    
    *gid = group->gid;
    group_count++;
    
    kinfo("Group created: %s (GID: %u)\n", groupname, group->gid);
    return ERR_OK;
}

/**
 * Delete group
 */
error_code_t group_delete(gid_t gid) {
    if (gid == ROOT_GID) {
        return ERR_PERMISSION_DENIED;  // Cannot delete root group
    }
    
    for (uint32_t i = 0; i < group_count; i++) {
        if (groups[i].gid == gid) {
            // Mark as deleted (simplified)
            groups[i].gid = 0;
            kinfo("Group deleted: GID %u\n", gid);
            return ERR_OK;
        }
    }
    
    return ERR_NOT_FOUND;
}

/**
 * Get group by GID
 */
group_t* group_get_by_gid(gid_t gid) {
    for (uint32_t i = 0; i < group_count; i++) {
        if (groups[i].gid == gid) {
            return &groups[i];
        }
    }
    return NULL;
}

/**
 * Get group by name
 */
group_t* group_get_by_name(const char* groupname) {
    if (!groupname) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < group_count; i++) {
        if (strcmp(groups[i].groupname, groupname) == 0) {
            return &groups[i];
        }
    }
    return NULL;
}

/**
 * Add member to group
 */
error_code_t group_add_member(gid_t gid, uid_t uid) {
    group_t* group = group_get_by_gid(gid);
    if (!group) {
        return ERR_NOT_FOUND;
    }
    
    if (group->member_count >= 32) {
        return ERR_OUT_OF_MEMORY;
    }
    
    // Check if already a member
    for (uint32_t i = 0; i < group->member_count; i++) {
        if (group->members[i] == uid) {
            return ERR_ALREADY_EXISTS;
        }
    }
    
    group->members[group->member_count++] = uid;
    return ERR_OK;
}

/**
 * Remove member from group
 */
error_code_t group_remove_member(gid_t gid, uid_t uid) {
    group_t* group = group_get_by_gid(gid);
    if (!group) {
        return ERR_NOT_FOUND;
    }
    
    for (uint32_t i = 0; i < group->member_count; i++) {
        if (group->members[i] == uid) {
            // Remove by shifting
            for (uint32_t j = i; j < group->member_count - 1; j++) {
                group->members[j] = group->members[j + 1];
            }
            group->member_count--;
            return ERR_OK;
        }
    }
    
    return ERR_NOT_FOUND;
}

/**
 * Get current UID
 */
uid_t get_current_uid(void) {
    return current_uid;
}

/**
 * Get current GID
 */
gid_t get_current_gid(void) {
    return current_gid;
}

/**
 * Set current UID
 */
error_code_t set_current_uid(uid_t uid) {
    // TODO: Check permissions (only root or setuid)
    current_uid = uid;
    return ERR_OK;
}

/**
 * Set current GID
 */
error_code_t set_current_gid(gid_t gid) {
    // TODO: Check permissions
    current_gid = gid;
    return ERR_OK;
}

