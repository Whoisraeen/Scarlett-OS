/**
 * @file fat32_utils.c
 * @brief FAT32 utility functions (path parsing, date conversion, etc.)
 */

#include "../include/types.h"
#include "../include/fs/fat32.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"
#include "../include/errors.h"

/**
 * Parse path into components
 */
error_code_t fat32_parse_path(const char* path, char components[][12], uint32_t* component_count) {
    if (!path || !components || !component_count) {
        return ERR_INVALID_ARG;
    }
    
    *component_count = 0;
    
    // Skip leading slash
    const char* p = path;
    if (*p == '/') {
        p++;
    }
    
    // Parse components
    while (*p && *component_count < 32) {
        // Find next slash or end
        const char* end = p;
        while (*end && *end != '/') {
            end++;
        }
        
        size_t len = end - p;
        if (len == 0) {
            break;  // Empty component
        }
        
        if (len > 11) {
            return ERR_INVALID_ARG;  // Component too long
        }
        
        // Copy component
        memcpy(components[*component_count], p, len);
        components[*component_count][len] = '\0';
        (*component_count)++;
        
        // Move to next component
        if (*end == '/') {
            p = end + 1;
        } else {
            break;
        }
    }
    
    return ERR_OK;
}

/**
 * Convert FAT32 date to Unix timestamp
 */
uint64_t fat32_date_to_unix(uint16_t fat_date, uint16_t fat_time) {
    // FAT32 date format: YYYYYYY MMMM DDDDD (year from 1980, month 1-12, day 1-31)
    // FAT32 time format: HHHHH MMMMM SSSSS (hour 0-23, minute 0-59, second*2 0-58)
    
    uint32_t year = 1980 + ((fat_date >> 9) & 0x7F);
    uint32_t month = (fat_date >> 5) & 0x0F;
    uint32_t day = fat_date & 0x1F;
    
    uint32_t hour = (fat_time >> 11) & 0x1F;
    uint32_t minute = (fat_time >> 5) & 0x3F;
    uint32_t second = (fat_time & 0x1F) * 2;
    
    // Simple conversion (not accounting for leap years, etc.)
    // Days since 1980-01-01
    uint64_t days = 0;
    for (uint32_t y = 1980; y < year; y++) {
        days += 365;
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
            days++;  // Leap year
        }
    }
    
    // Days in current year
    uint32_t days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        days_per_month[1] = 29;  // Leap year
    }
    
    for (uint32_t m = 1; m < month; m++) {
        days += days_per_month[m - 1];
    }
    days += day - 1;
    
    // Convert to seconds
    uint64_t seconds = days * 86400ULL + hour * 3600ULL + minute * 60ULL + second;
    
    // Unix epoch starts at 1970-01-01, FAT32 starts at 1980-01-01
    // Difference: 10 years = 315532800 seconds (approximately)
    return seconds + 315532800ULL;
}

/**
 * Convert Unix timestamp to FAT32 date
 */
void fat32_unix_to_date(uint64_t unix_time, uint16_t* fat_date, uint16_t* fat_time) {
    // Convert from Unix epoch (1970) to FAT32 epoch (1980)
    uint64_t fat_seconds = unix_time - 315532800ULL;
    
    uint64_t days = fat_seconds / 86400ULL;
    uint64_t seconds_in_day = fat_seconds % 86400ULL;
    
    // Calculate year
    uint32_t year = 1980;
    while (days >= 365) {
        bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        uint32_t year_days = is_leap ? 366 : 365;
        if (days < year_days) {
            break;
        }
        days -= year_days;
        year++;
    }
    
    // Calculate month and day
    uint32_t month = 1;
    uint32_t days_per_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool is_leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    if (is_leap) {
        days_per_month[1] = 29;
    }
    
    uint32_t day = (uint32_t)days + 1;
    for (uint32_t m = 0; m < 12; m++) {
        if (day <= days_per_month[m]) {
            month = m + 1;
            break;
        }
        day -= days_per_month[m];
    }
    
    // Calculate time
    uint32_t hour = (uint32_t)(seconds_in_day / 3600ULL);
    uint32_t minute = (uint32_t)((seconds_in_day % 3600ULL) / 60ULL);
    uint32_t second = (uint32_t)(seconds_in_day % 60ULL);
    
    // Pack into FAT32 format
    *fat_date = ((year - 1980) << 9) | (month << 5) | day;
    *fat_time = (hour << 11) | (minute << 5) | (second / 2);
}

/**
 * Find file in directory by name
 */
error_code_t fat32_find_in_dir(fat32_fs_t* fs, uint32_t cluster, const char* name, fat32_dir_entry_t* entry) {
    if (!fs || !name || !entry) {
        return ERR_INVALID_ARG;
    }
    
    // Format name to 8.3
    char formatted[11];
    memset(formatted, ' ', 11);
    
    const char* dot = strchr(name, '.');
    const char* ext = dot ? (dot + 1) : NULL;
    size_t name_len = dot ? (dot - name) : strlen(name);
    size_t ext_len = ext ? strlen(ext) : 0;
    
    // Copy name (up to 8 chars, uppercase)
    for (size_t i = 0; i < name_len && i < 8; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
        formatted[i] = c;
    }
    
    // Copy extension (up to 3 chars, uppercase)
    if (ext && ext_len > 0) {
        for (size_t i = 0; i < ext_len && i < 3; i++) {
            char c = ext[i];
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            }
            formatted[8 + i] = c;
        }
    }
    
    // Search directory
    uint32_t current_cluster = cluster;
    uint8_t* cluster_data = (uint8_t*)kmalloc(fs->bytes_per_cluster);
    if (!cluster_data) {
        return ERR_OUT_OF_MEMORY;
    }
    
    while (current_cluster >= 2 && current_cluster < FAT32_CLUSTER_EOF_MIN) {
        error_code_t err = fat32_read_cluster(fs, current_cluster, cluster_data);
        if (err != ERR_OK) {
            kfree(cluster_data);
            return err;
        }
        
        fat32_dir_entry_t* entries = (fat32_dir_entry_t*)cluster_data;
        size_t entries_per_cluster = fs->bytes_per_cluster / sizeof(fat32_dir_entry_t);
        
        for (size_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00) {
                // End of directory
                kfree(cluster_data);
                return ERR_NOT_FOUND;
            }
            
            if (entries[i].name[0] == 0xE5) {
                continue;  // Deleted entry
            }
            
            // Compare names
            bool match = true;
            for (int j = 0; j < 11; j++) {
                if (entries[i].name[j] != formatted[j]) {
                    match = false;
                    break;
                }
            }
            
            if (match) {
                memcpy(entry, &entries[i], sizeof(fat32_dir_entry_t));
                kfree(cluster_data);
                return ERR_OK;
            }
        }
        
        // Get next cluster
        current_cluster = fat32_get_next_cluster(fs, current_cluster);
    }
    
    kfree(cluster_data);
    return ERR_NOT_FOUND;
}

