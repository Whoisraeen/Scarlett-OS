/**
 * @file rbac.h
 * @brief Role-Based Access Control (RBAC) interface
 */

#ifndef KERNEL_SECURITY_RBAC_H
#define KERNEL_SECURITY_RBAC_H

#include "../types.h"
#include "../errors.h"
#include "capability.h"

// Maximum roles per user
#define MAX_ROLES_PER_USER 16
// Maximum permissions per role
#define MAX_PERMISSIONS_PER_ROLE 64
// Maximum roles in system
#define MAX_ROLES 128

// Role structure
typedef struct {
    uint32_t role_id;
    char name[32];
    uint32_t permissions[MAX_PERMISSIONS_PER_ROLE];
    uint32_t permission_count;
} rbac_role_t;

// User role assignment
typedef struct {
    uint32_t uid;
    uint32_t roles[MAX_ROLES_PER_USER];
    uint32_t role_count;
} rbac_user_roles_t;

// RBAC functions
error_code_t rbac_init(void);
error_code_t rbac_create_role(uint32_t role_id, const char* name);
error_code_t rbac_add_permission_to_role(uint32_t role_id, uint32_t permission);
error_code_t rbac_assign_role_to_user(uint32_t uid, uint32_t role_id);
error_code_t rbac_remove_role_from_user(uint32_t uid, uint32_t role_id);
bool rbac_user_has_permission(uint32_t uid, uint32_t permission);
bool rbac_user_has_role(uint32_t uid, uint32_t role_id);
error_code_t rbac_get_user_roles(uint32_t uid, uint32_t* roles, uint32_t* count);

#endif // KERNEL_SECURITY_RBAC_H

