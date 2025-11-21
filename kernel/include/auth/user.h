/**
 * @file user.h
 * @brief User and group management
 */

#ifndef KERNEL_AUTH_USER_H
#define KERNEL_AUTH_USER_H

#include "../types.h"
#include "../errors.h"

// User ID and Group ID types
typedef uint32_t uid_t;
typedef uint32_t gid_t;

// Maximum username and group name lengths
#define MAX_USERNAME_LEN 32
#define MAX_GROUPNAME_LEN 32
#define MAX_PASSWORD_HASH_LEN 64

// User structure
typedef struct {
    uid_t uid;                          // User ID
    gid_t gid;                          // Primary group ID
    char username[MAX_USERNAME_LEN];    // Username
    char password_hash[MAX_PASSWORD_HASH_LEN];  // Password hash (bcrypt/scrypt)
    bool active;                        // Is user active?
} user_t;

// Group structure
typedef struct {
    gid_t gid;                          // Group ID
    char groupname[MAX_GROUPNAME_LEN];  // Group name
    uid_t members[32];                  // Member UIDs (simplified)
    uint32_t member_count;              // Number of members
} group_t;

// Maximum users and groups
#define MAX_USERS 256
#define MAX_GROUPS 64

// Special UIDs/GIDs
#define ROOT_UID 0
#define ROOT_GID 0
#define NOBODY_UID 65534
#define NOBODY_GID 65534

// User/Group functions
error_code_t user_init(void);
error_code_t user_create(const char* username, const char* password, uid_t* uid);
error_code_t user_delete(uid_t uid);
user_t* user_get_by_uid(uid_t uid);
user_t* user_get_by_username(const char* username);
error_code_t user_authenticate(const char* username, const char* password, uid_t* uid);
error_code_t user_set_password(uid_t uid, const char* password);

// Group functions
error_code_t group_init(void);
error_code_t group_create(const char* groupname, gid_t* gid);
error_code_t group_delete(gid_t gid);
group_t* group_get_by_gid(gid_t gid);
group_t* group_get_by_name(const char* groupname);
error_code_t group_add_member(gid_t gid, uid_t uid);
error_code_t group_remove_member(gid_t gid, uid_t uid);

// Current user/group
uid_t get_current_uid(void);
gid_t get_current_gid(void);
error_code_t set_current_uid(uid_t uid);
error_code_t set_current_gid(gid_t gid);

// Persistence functions
error_code_t user_save_to_disk(void);
error_code_t user_load_from_disk(void);
error_code_t group_save_to_disk(void);
error_code_t group_load_from_disk(void);
error_code_t create_home_directory(const char* username, uid_t uid, gid_t gid);

#endif // KERNEL_AUTH_USER_H

