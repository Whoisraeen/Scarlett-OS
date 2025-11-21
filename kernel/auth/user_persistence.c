/**
 * @file user_persistence.c
 * @brief Persistent storage for user and group databases
 */

#include "../include/types.h"
#include "../include/auth/user.h"
#include "../include/fs/vfs.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/stdio.h"  // For strtoul
#include "../include/mm/heap.h"
#include "../include/errors.h"

// Paths for user database files
#define PASSWD_PATH "/etc/passwd"
#define GROUP_PATH "/etc/group"
#define HOME_BASE "/home"

// Forward declarations
extern user_t users[MAX_USERS];
extern group_t groups[MAX_GROUPS];
extern uint32_t user_count;
extern uint32_t group_count;
extern uid_t next_uid;
extern gid_t next_gid;

/**
 * Convert unsigned integer to string
 */
static void uint_to_str(uint32_t val, char* buf, size_t buf_size) {
    if (buf_size == 0) return;
    
    char* ptr = buf + buf_size - 1;
    *ptr = '\0';
    
    if (val == 0) {
        *--ptr = '0';
    } else {
        while (val > 0 && ptr > buf) {
            *--ptr = '0' + (val % 10);
            val /= 10;
        }
    }
    
    // Move result to start of buffer
    size_t len = strlen(ptr);
    if (ptr != buf) {
        memmove(buf, ptr, len + 1);
    }
}

/**
 * Create home directory for a user
 */
error_code_t create_home_directory(const char* username, uid_t uid, gid_t gid) {
    char home_path[256];
    strcpy(home_path, HOME_BASE);
    strcat(home_path, "/");
    strcat(home_path, username);
    
    // Create /home directory if it doesn't exist
    vfs_stat_t stat;
    error_code_t err = vfs_stat(HOME_BASE, &stat);
    if (err != ERR_OK) {
        // /home doesn't exist, create it
        err = vfs_mkdir(HOME_BASE);
        if (err != ERR_OK) {
            kerror("Failed to create /home directory: %d\n", err);
            return err;
        }
        kinfo("Created /home directory\n");
    }
    
    // Create user's home directory
    err = vfs_mkdir(home_path);
    if (err != ERR_OK && err != ERR_ALREADY_EXISTS) {
        kerror("Failed to create home directory for %s: %d\n", username, err);
        return err;
    }
    
    if (err == ERR_OK) {
        kinfo("Created home directory: %s\n", home_path);
    }
    
    return ERR_OK;
}

/**
 * Save user database to /etc/passwd
 */
error_code_t user_save_to_disk(void) {
    kinfo("Saving user database to %s...\n", PASSWD_PATH);
    
    fd_t fd;
    error_code_t err = vfs_open(PASSWD_PATH, VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &fd);
    if (err != ERR_OK) {
        // Try creating /etc directory first
        err = vfs_mkdir("/etc");
        if (err != ERR_OK && err != ERR_ALREADY_EXISTS) {
            kerror("Failed to create /etc directory: %d\n", err);
            return err;
        }
        
        err = vfs_open(PASSWD_PATH, VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &fd);
        if (err != ERR_OK) {
            kerror("Failed to open %s for writing: %d\n", PASSWD_PATH, err);
            return err;
        }
    }
    
    // Write each user in passwd format: username:password_hash:uid:gid:comment:home:shell
    for (uint32_t i = 0; i < user_count; i++) {
        if (!users[i].active) {
            continue;
        }
        
        char line[512];
        char uid_str[16], gid_str[16];
        uint_to_str(users[i].uid, uid_str, sizeof(uid_str));
        uint_to_str(users[i].gid, gid_str, sizeof(gid_str));
        
        strcpy(line, users[i].username);
        strcat(line, ":");
        strcat(line, users[i].password_hash);
        strcat(line, ":");
        strcat(line, uid_str);
        strcat(line, ":");
        strcat(line, gid_str);
        strcat(line, "::");
        strcat(line, HOME_BASE);
        strcat(line, "/");
        strcat(line, users[i].username);
        strcat(line, ":/bin/sh\n");
        
        size_t bytes_written;
        err = vfs_write(fd, line, strlen(line), &bytes_written);
        if (err != ERR_OK) {
            kerror("Failed to write user entry: %d\n", err);
            vfs_close(fd);
            return err;
        }
    }
    
    vfs_close(fd);
    kinfo("User database saved (%u users)\n", user_count);
    return ERR_OK;
}

/**
 * Load user database from /etc/passwd
 */
error_code_t user_load_from_disk(void) {
    kinfo("Loading user database from %s...\n", PASSWD_PATH);
    
    fd_t fd;
    error_code_t err = vfs_open(PASSWD_PATH, VFS_MODE_READ, &fd);
    if (err != ERR_OK) {
        kinfo("User database file not found, using defaults\n");
        return ERR_OK;  // Not an error if file doesn't exist (first boot)
    }
    
    // Read file line by line
    char line[512];
    size_t bytes_read;
    size_t line_pos = 0;
    
    while (1) {
        char c;
        err = vfs_read(fd, &c, 1, &bytes_read);
        if (err != ERR_OK || bytes_read == 0) {
            break;  // EOF or error
        }
        
        if (c == '\n' || line_pos >= sizeof(line) - 1) {
            line[line_pos] = '\0';
            
            if (line_pos > 0) {
                // Parse passwd line: username:password_hash:uid:gid:comment:home:shell
                char* fields[7];
                int field_count = 0;
                char* token = line;
                
                // Split by ':'
                for (int i = 0; i < 7 && token; i++) {
                    fields[i] = token;
                    char* colon = strchr(token, ':');
                    if (colon) {
                        *colon = '\0';
                        token = colon + 1;
                    } else {
                        token = NULL;
                    }
                    field_count++;
                }
                
                if (field_count >= 4) {
                    // Parse fields
                    const char* username = fields[0];
                    const char* password_hash = fields[1];
                    uid_t uid = (uid_t)strtoul(fields[2], NULL, 10);
                    gid_t gid = (gid_t)strtoul(fields[3], NULL, 10);
                    
                    // Add user to database
                    if (user_count < MAX_USERS) {
                        user_t* user = &users[user_count];
                        user->uid = uid;
                        user->gid = gid;
                        strncpy(user->username, username, MAX_USERNAME_LEN - 1);
                        user->username[MAX_USERNAME_LEN - 1] = '\0';
                        strncpy(user->password_hash, password_hash, MAX_PASSWORD_HASH_LEN - 1);
                        user->password_hash[MAX_PASSWORD_HASH_LEN - 1] = '\0';
                        user->active = true;
                        
                        user_count++;
                        
                        // Update next_uid/gid
                        if (uid >= next_uid) {
                            next_uid = uid + 1;
                        }
                        if (gid >= next_gid) {
                            next_gid = gid + 1;
                        }
                    }
                }
            }
            
            line_pos = 0;
        } else {
            line[line_pos++] = c;
        }
    }
    
    vfs_close(fd);
    kinfo("User database loaded (%u users)\n", user_count);
    return ERR_OK;
}

/**
 * Save group database to /etc/group
 */
error_code_t group_save_to_disk(void) {
    kinfo("Saving group database to %s...\n", GROUP_PATH);
    
    fd_t fd;
    error_code_t err = vfs_open(GROUP_PATH, VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &fd);
    if (err != ERR_OK) {
        // Try creating /etc directory first
        err = vfs_mkdir("/etc");
        if (err != ERR_OK && err != ERR_ALREADY_EXISTS) {
            kerror("Failed to create /etc directory: %d\n", err);
            return err;
        }
        
        err = vfs_open(GROUP_PATH, VFS_MODE_WRITE | VFS_MODE_CREATE | VFS_MODE_TRUNC, &fd);
        if (err != ERR_OK) {
            kerror("Failed to open %s for writing: %d\n", GROUP_PATH, err);
            return err;
        }
    }
    
    // Write each group in group format: groupname:password:gid:members
    for (uint32_t i = 0; i < group_count; i++) {
        if (groups[i].gid == 0 && i > 0) {
            continue;  // Skip deleted groups
        }
        
        char line[512];
        char members[256] = "";
        
        // Build member list
        for (uint32_t j = 0; j < groups[i].member_count; j++) {
            if (j > 0) {
                strcat(members, ",");
            }
            char uid_str[16];
            uint_to_str(groups[i].members[j], uid_str, sizeof(uid_str));
            strcat(members, uid_str);
        }
        
        char gid_str[16];
        uint_to_str(groups[i].gid, gid_str, sizeof(gid_str));
        
        strcpy(line, groups[i].groupname);
        strcat(line, ":x:");
        strcat(line, gid_str);
        strcat(line, ":");
        strcat(line, members);
        strcat(line, "\n");
        
        size_t bytes_written;
        err = vfs_write(fd, line, strlen(line), &bytes_written);
        if (err != ERR_OK) {
            kerror("Failed to write group entry: %d\n", err);
            vfs_close(fd);
            return err;
        }
    }
    
    vfs_close(fd);
    kinfo("Group database saved (%u groups)\n", group_count);
    return ERR_OK;
}

/**
 * Load group database from /etc/group
 */
error_code_t group_load_from_disk(void) {
    kinfo("Loading group database from %s...\n", GROUP_PATH);
    
    fd_t fd;
    error_code_t err = vfs_open(GROUP_PATH, VFS_MODE_READ, &fd);
    if (err != ERR_OK) {
        kinfo("Group database file not found, using defaults\n");
        return ERR_OK;  // Not an error if file doesn't exist (first boot)
    }
    
    // Read file line by line
    char line[512];
    size_t bytes_read;
    size_t line_pos = 0;
    
    while (1) {
        char c;
        err = vfs_read(fd, &c, 1, &bytes_read);
        if (err != ERR_OK || bytes_read == 0) {
            break;  // EOF or error
        }
        
        if (c == '\n' || line_pos >= sizeof(line) - 1) {
            line[line_pos] = '\0';
            
            if (line_pos > 0) {
                // Parse group line: groupname:password:gid:members
                char* fields[4];
                int field_count = 0;
                char* token = line;
                
                // Split by ':'
                for (int i = 0; i < 4 && token; i++) {
                    fields[i] = token;
                    char* colon = strchr(token, ':');
                    if (colon) {
                        *colon = '\0';
                        token = colon + 1;
                    } else {
                        token = NULL;
                    }
                    field_count++;
                }
                
                if (field_count >= 3) {
                    // Parse fields
                    const char* groupname = fields[0];
                    gid_t gid = (gid_t)strtoul(fields[2], NULL, 10);
                    
                    // Add group to database
                    if (group_count < MAX_GROUPS) {
                        group_t* group = &groups[group_count];
                        group->gid = gid;
                        strncpy(group->groupname, groupname, MAX_GROUPNAME_LEN - 1);
                        group->groupname[MAX_GROUPNAME_LEN - 1] = '\0';
                        group->member_count = 0;
                        
                        // Parse members if present
                        if (field_count >= 4 && fields[3] && strlen(fields[3]) > 0) {
                            char* members = fields[3];
                            char* member_token = members;
                            while (member_token && group->member_count < 32) {
                                char* comma = strchr(member_token, ',');
                                if (comma) {
                                    *comma = '\0';
                                }
                                
                                uid_t uid = (uid_t)strtoul(member_token, NULL, 10);
                                group->members[group->member_count++] = uid;
                                
                                if (comma) {
                                    member_token = comma + 1;
                                } else {
                                    break;
                                }
                            }
                        }
                        
                        group_count++;
                        
                        // Update next_gid
                        if (gid >= next_gid) {
                            next_gid = gid + 1;
                        }
                    }
                }
            }
            
            line_pos = 0;
        } else {
            line[line_pos++] = c;
        }
    }
    
    vfs_close(fd);
    kinfo("Group database loaded (%u groups)\n", group_count);
    return ERR_OK;
}

