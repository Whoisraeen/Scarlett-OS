/**
 * @file permissions.h
 * @brief File permissions system
 */

#ifndef KERNEL_FS_PERMISSIONS_H
#define KERNEL_FS_PERMISSIONS_H

#include "../types.h"

// Permission bits (Unix-style)
#define PERM_READ    0x04  // 100 (binary)
#define PERM_WRITE   0x02  // 010 (binary)
#define PERM_EXECUTE 0x01  // 001 (binary)

// Permission masks for owner, group, other
#define PERM_OWNER_READ    (PERM_READ << 6)     // 0400
#define PERM_OWNER_WRITE   (PERM_WRITE << 6)    // 0200
#define PERM_OWNER_EXECUTE (PERM_EXECUTE << 6)  // 0100
#define PERM_GROUP_READ    (PERM_READ << 3)     // 0040
#define PERM_GROUP_WRITE   (PERM_WRITE << 3)    // 0020
#define PERM_GROUP_EXECUTE (PERM_EXECUTE << 3)  // 0010
#define PERM_OTHER_READ    (PERM_READ << 0)     // 0004
#define PERM_OTHER_WRITE   (PERM_WRITE << 0)    // 0002
#define PERM_OTHER_EXECUTE (PERM_EXECUTE << 0)  // 0001

// Common permission combinations
#define PERM_OWNER_RW      (PERM_OWNER_READ | PERM_OWNER_WRITE)      // 0600
#define PERM_OWNER_RWX     (PERM_OWNER_READ | PERM_OWNER_WRITE | PERM_OWNER_EXECUTE)  // 0700
#define PERM_ALL_RW        (PERM_OWNER_RW | PERM_GROUP_READ | PERM_GROUP_WRITE | PERM_OTHER_READ | PERM_OTHER_WRITE)  // 0666
#define PERM_ALL_RWX       (PERM_OWNER_RWX | PERM_GROUP_READ | PERM_GROUP_WRITE | PERM_GROUP_EXECUTE | PERM_OTHER_READ | PERM_OTHER_WRITE | PERM_OTHER_EXECUTE)  // 0777

// File permissions structure
typedef struct {
    uint16_t mode;      // Permission bits (12 bits: 3 for type, 9 for permissions)
    uint32_t uid;       // Owner user ID
    uint32_t gid;       // Owner group ID
} file_permissions_t;

// Permission checking functions
bool permissions_check_read(file_permissions_t* perms, uint32_t uid, uint32_t gid);
bool permissions_check_write(file_permissions_t* perms, uint32_t uid, uint32_t gid);
bool permissions_check_execute(file_permissions_t* perms, uint32_t uid, uint32_t gid);

// Permission setting functions
void permissions_set_mode(file_permissions_t* perms, uint16_t mode);
void permissions_set_owner(file_permissions_t* perms, uint32_t uid, uint32_t gid);

// Default permissions
#define PERM_DEFAULT_FILE  PERM_OWNER_RW | PERM_GROUP_READ | PERM_OTHER_READ  // 0644
#define PERM_DEFAULT_DIR   PERM_OWNER_RWX | PERM_GROUP_READ | PERM_GROUP_EXECUTE | PERM_OTHER_READ | PERM_OTHER_EXECUTE  // 0755

#endif // KERNEL_FS_PERMISSIONS_H

