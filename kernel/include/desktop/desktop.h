/**
 * @file desktop.h
 * @brief Desktop environment interface
 */

#ifndef KERNEL_DESKTOP_DESKTOP_H
#define KERNEL_DESKTOP_DESKTOP_H

#include "../types.h"
#include "../errors.h"
#include "../graphics/framebuffer.h"

// Desktop state
typedef struct {
    void* wallpaper_buffer;      // Wallpaper framebuffer
    bool initialized;
} desktop_state_t;

// Desktop functions
error_code_t desktop_init(void);
error_code_t desktop_render(void);
error_code_t desktop_handle_input(void);

#endif // KERNEL_DESKTOP_DESKTOP_H

