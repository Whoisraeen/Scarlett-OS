/**
 * @file bootsplash.h
 * @brief Boot splash screen interface
 */

#ifndef KERNEL_DESKTOP_BOOTSPLASH_H
#define KERNEL_DESKTOP_BOOTSPLASH_H

#include "../types.h"
#include "../errors.h"

// Boot splash screen
typedef struct {
    bool visible;
    char message[128];
    uint32_t progress;  // 0-100
    bool initialized;
} bootsplash_t;

// Boot splash functions
error_code_t bootsplash_init(void);
error_code_t bootsplash_show(void);
error_code_t bootsplash_hide(void);
error_code_t bootsplash_set_message(const char* message);
error_code_t bootsplash_set_progress(uint32_t percent);
error_code_t bootsplash_render(void);
bootsplash_t* bootsplash_get(void);

#endif // KERNEL_DESKTOP_BOOTSPLASH_H

