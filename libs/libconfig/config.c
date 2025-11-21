/**
 * @file config.c
 * @brief System Configuration Implementation
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static config_entry_t g_config[CONFIG_MAX_ENTRIES];
static int g_config_count = 0;

int config_init(void) {
    memset(g_config, 0, sizeof(g_config));
    g_config_count = 0;
    
    // Set default configuration
    config_set_defaults();
    
    return 0;
}

int config_load(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Config: Failed to open %s\n", filename);
        return -1;
    }
    
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        // Parse key=value
        char* equals = strchr(line, '=');
        if (!equals) {
            continue;
        }
        
        *equals = '\0';
        char* key = line;
        char* value = equals + 1;
        
        // Trim newline
        char* newline = strchr(value, '\n');
        if (newline) {
            *newline = '\0';
        }
        
        config_set(key, value);
    }
    
    fclose(fp);
    printf("Config: Loaded from %s\n", filename);
    return 0;
}

int config_save(const char* filename) {
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        printf("Config: Failed to save to %s\n", filename);
        return -1;
    }
    
    fprintf(fp, "# ScarlettOS Configuration\n");
    fprintf(fp, "# Auto-generated - do not edit manually\n\n");
    
    for (int i = 0; i < g_config_count; i++) {
        if (g_config[i].is_set) {
            fprintf(fp, "%s=%s\n", g_config[i].key, g_config[i].value);
        }
    }
    
    fclose(fp);
    printf("Config: Saved to %s\n", filename);
    return 0;
}

const char* config_get(const char* key) {
    for (int i = 0; i < g_config_count; i++) {
        if (strcmp(g_config[i].key, key) == 0 && g_config[i].is_set) {
            return g_config[i].value;
        }
    }
    return NULL;
}

int config_set(const char* key, const char* value) {
    // Check if key already exists
    for (int i = 0; i < g_config_count; i++) {
        if (strcmp(g_config[i].key, key) == 0) {
            strncpy(g_config[i].value, value, CONFIG_MAX_VALUE_LEN - 1);
            g_config[i].is_set = true;
            return 0;
        }
    }
    
    // Add new entry
    if (g_config_count >= CONFIG_MAX_ENTRIES) {
        printf("Config: Too many entries\n");
        return -1;
    }
    
    strncpy(g_config[g_config_count].key, key, CONFIG_MAX_KEY_LEN - 1);
    strncpy(g_config[g_config_count].value, value, CONFIG_MAX_VALUE_LEN - 1);
    g_config[g_config_count].is_set = true;
    g_config_count++;
    
    return 0;
}

int config_get_int(const char* key, int default_value) {
    const char* value = config_get(key);
    if (!value) {
        return default_value;
    }
    return atoi(value);
}

bool config_get_bool(const char* key, bool default_value) {
    const char* value = config_get(key);
    if (!value) {
        return default_value;
    }
    return (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
}

void config_set_defaults(void) {
    // System
    config_set("system.hostname", "scarlettos");
    config_set("system.timezone", "UTC");
    config_set("system.language", "en_US");
    
    // Display
    config_set("display.resolution", "1920x1080");
    config_set("display.refresh_rate", "60");
    config_set("display.vsync", "true");
    
    // Network
    config_set("network.dhcp", "true");
    config_set("network.dns", "8.8.8.8");
    
    // Audio
    config_set("audio.volume", "50");
    config_set("audio.sample_rate", "48000");
    
    // Security
    config_set("security.firewall", "true");
    config_set("security.encryption", "true");
    
    // User
    config_set("user.shell", "/bin/sh");
    config_set("user.home", "/home/user");
}

void config_validate(void) {
    // Validate display resolution
    const char* res = config_get("display.resolution");
    if (res && strchr(res, 'x') == NULL) {
        printf("Config: Invalid resolution, using default\n");
        config_set("display.resolution", "1920x1080");
    }
    
    // Validate audio volume (0-100)
    int volume = config_get_int("audio.volume", 50);
    if (volume < 0 || volume > 100) {
        printf("Config: Invalid volume, using default\n");
        config_set("audio.volume", "50");
    }
    
    printf("Config: Validation complete\n");
}

int config_migrate(int from_version, int to_version) {
    printf("Config: Migrating from v%d to v%d\n", from_version, to_version);
    
    // Example migration logic
    if (from_version == 1 && to_version == 2) {
        // Add new keys introduced in v2
        if (!config_get("system.language")) {
            config_set("system.language", "en_US");
        }
    }
    
    printf("Config: Migration complete\n");
    return 0;
}
