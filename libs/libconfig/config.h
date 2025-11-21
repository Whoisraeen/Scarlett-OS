/**
 * @file config.h
 * @brief System Configuration Management
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#define CONFIG_MAX_KEY_LEN 64
#define CONFIG_MAX_VALUE_LEN 256
#define CONFIG_MAX_ENTRIES 256

// Configuration entry
typedef struct {
    char key[CONFIG_MAX_KEY_LEN];
    char value[CONFIG_MAX_VALUE_LEN];
    bool is_set;
} config_entry_t;

// Configuration categories
typedef enum {
    CONFIG_SYSTEM,
    CONFIG_DISPLAY,
    CONFIG_NETWORK,
    CONFIG_AUDIO,
    CONFIG_SECURITY,
    CONFIG_USER
} config_category_t;

// Configuration management
int config_init(void);
int config_load(const char* filename);
int config_save(const char* filename);

// Get/Set configuration
const char* config_get(const char* key);
int config_set(const char* key, const char* value);
int config_get_int(const char* key, int default_value);
bool config_get_bool(const char* key, bool default_value);

// Default configurations
void config_set_defaults(void);
void config_validate(void);

// Configuration migration
int config_migrate(int from_version, int to_version);

#endif // CONFIG_H
