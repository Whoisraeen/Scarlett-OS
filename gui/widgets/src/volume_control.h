/**
 * @file volume_control.h
 * @brief Volume Control Widget for Taskbar
 *
 * Integrates with audio server for system-wide volume control
 */

#ifndef GUI_VOLUME_CONTROL_H
#define GUI_VOLUME_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "../../libs/libaudio/libaudio.h"

// Volume control widget
typedef struct {
    uint32_t x, y;
    uint32_t width, height;
    
    uint32_t volume;         // 0-100
    bool muted;
    bool popup_visible;
    
    // Popup slider
    uint32_t popup_x, popup_y;
    uint32_t popup_width, popup_height;
    bool dragging;
    
    // Audio handle
    audio_handle_t* audio;
} volume_control_t;

// Create volume control widget
volume_control_t* volume_control_create(uint32_t x, uint32_t y);
void volume_control_destroy(volume_control_t* ctrl);

// Event handling
void volume_control_handle_click(volume_control_t* ctrl, int32_t x, int32_t y);
void volume_control_handle_mouse_move(volume_control_t* ctrl, int32_t x, int32_t y);
void volume_control_handle_mouse_up(volume_control_t* ctrl);
void volume_control_handle_scroll(volume_control_t* ctrl, int32_t delta);

// Volume operations
void volume_control_set_volume(volume_control_t* ctrl, uint32_t volume);
uint32_t volume_control_get_volume(volume_control_t* ctrl);
void volume_control_toggle_mute(volume_control_t* ctrl);

// Rendering
void volume_control_render(volume_control_t* ctrl, void* framebuffer);
void volume_control_render_popup(volume_control_t* ctrl, void* framebuffer);

#endif // GUI_VOLUME_CONTROL_H
