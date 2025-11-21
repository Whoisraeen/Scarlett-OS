/**
 * @file acl.h
 * @brief Access Control List (ACL) interface
 */

#ifndef KERNEL_FS_ACL_H
#define KERNEL_FS_ACL_H

#include "../types.h"
#include "../errors.h"

// ACL entry types
#define ACL_ENTRY_USER      0x01
#define ACL_ENTRY_GROUP     0x02
#define ACL_ENTRY_OTHER     0x04
#define ACL_ENTRY_MASK      0x08

// ACL permissions
#define ACL_READ    0x04
#define ACL_WRITE   0x02
#define ACL_EXECUTE 0x01

// Maximum ACL entries per resource
#define MAX_ACL_ENTRIES 32

// ACL entry structure
typedef struct {
    uint8_t type;      // Entry type (USER, GROUP, OTHER, MASK)
    uint32_t id;       // User ID or Group ID (0 for OTHER)
    uint8_t perms;     // Permissions (READ, WRITE, EXECUTE)
} acl_entry_t;

// ACL structure
typedef struct {
    acl_entry_t entries[MAX_ACL_ENTRIES];
    uint32_t entry_count;
} acl_t;

// ACL functions
error_code_t acl_init(void);
error_code_t acl_create(acl_t* acl);
error_code_t acl_add_entry(acl_t* acl, uint8_t type, uint32_t id, uint8_t perms);
error_code_t acl_remove_entry(acl_t* acl, uint8_t type, uint32_t id);
error_code_t acl_check_access(const acl_t* acl, uint32_t uid, uint32_t gid, uint8_t requested_perms);
error_code_t acl_get_entry(const acl_t* acl, uint8_t type, uint32_t id, acl_entry_t* entry);
error_code_t acl_set_default(acl_t* acl, uint16_t mode, uint32_t uid, uint32_t gid);

#endif // KERNEL_FS_ACL_H

