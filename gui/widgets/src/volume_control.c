/**
 * @file volume_control.c
 * @brief Volume Control Widget Implementation
 */

#include "volume_control.h"
#include <stdlib.h>
#include <string.h>

volume_control_t* volume_control_create(uint32_t x, uint32_t y) {
    volume_control_t* ctrl = (volume_control_t*)calloc(1, sizeof(volume_control_t));
    
    ctrl->x = x;
    ctrl->y = y;
    ctrl->width = 32;
    ctrl->height = 32;
    
    ctrl->volume = 75;
    ctrl->muted = false;
    ctrl->popup_visible = false;
    
    ctrl->popup_width = 40;
    ctrl->popup_height = 150;
    ctrl->popup_x = x - (ctrl->popup_width / 2) + (ctrl->width / 2);
    ctrl->popup_y = y - ctrl->popup_height - 10;
    
    ctrl->dragging = false;
    
    // Initialize audio
    audio_init();
    
    // TODO: Connect to audio server
    // ctrl->audio = audio_open_playback("VolumeControl", NULL);
    
    return ctrl;
}

void volume_control_destroy(volume_control_t* ctrl) {
    if (ctrl->audio) {
        // audio_close_playback(ctrl->audio);
    }
    free(ctrl);
}

void volume_control_handle_click(volume_control_t* ctrl, int32_t x, int32_t y) {
    // Check if click is on icon
    if (x >= (int32_t)ctrl->x && x < (int32_t)(ctrl->x + ctrl->width) &&
        y >= (int32_t)ctrl->y && y < (int32_t)(ctrl->y + ctrl->height)) {
        
        // Toggle popup
        ctrl->popup_visible = !ctrl->popup_visible;
    }
    
    // Check if click is on popup slider
    if (ctrl->popup_visible &&
        x >= (int32_t)ctrl->popup_x && x < (int32_t)(ctrl->popup_x + ctrl->popup_width) &&
        y >= (int32_t)ctrl->popup_y && y < (int32_t)(ctrl->popup_y + ctrl->popup_height)) {
        
        // Calculate volume from click position
        int32_t rel_y = y - ctrl->popup_y;
        uint32_t new_volume = 100 - ((rel_y * 100) / ctrl->popup_height);
        if (new_volume > 100) new_volume = 100;
        
        volume_control_set_volume(ctrl, new_volume);
        ctrl->dragging = true;
    }
}

void volume_control_handle_mouse_move(volume_control_t* ctrl, int32_t x, int32_t y) {
    if (ctrl->dragging && ctrl->popup_visible) {
        // Update volume based on mouse position
        int32_t rel_y = y - ctrl->popup_y;
        uint32_t new_volume = 100 - ((rel_y * 100) / ctrl->popup_height);
        if (new_volume > 100) new_volume = 100;
        
        volume_control_set_volume(ctrl, new_volume);
    }
}

void volume_control_handle_mouse_up(volume_control_t* ctrl) {
    ctrl->dragging = false;
}

void volume_control_handle_scroll(volume_control_t* ctrl, int32_t delta) {
    int32_t new_volume = (int32_t)ctrl->volume + (delta * 5);
    if (new_volume < 0) new_volume = 0;
    if (new_volume > 100) new_volume = 100;
    
    volume_control_set_volume(ctrl, (uint32_t)new_volume);
}

void volume_control_set_volume(volume_control_t* ctrl, uint32_t volume) {
    if (volume > 100) volume = 100;
    
    ctrl->volume = volume;
    
    // Update audio server
    if (ctrl->audio) {
        // audio_set_volume(ctrl->audio, volume);
    }
    
    // Unmute if volume changed from 0
    if (volume > 0 && ctrl->muted) {
        ctrl->muted = false;
    }
}

uint32_t volume_control_get_volume(volume_control_t* ctrl) {
    return ctrl->volume;
}

void volume_control_toggle_mute(volume_control_t* ctrl) {
    ctrl->muted = !ctrl->muted;
    
    if (ctrl->audio) {
        // audio_set_mute(ctrl->audio, ctrl->muted);
    }
}

void volume_control_render(volume_control_t* ctrl, void* framebuffer) {
    // TODO: Render volume icon
    // - Show speaker icon
    // - Show mute icon if muted
    // - Show volume level indicator
    
    if (ctrl->popup_visible) {
        volume_control_render_popup(ctrl, framebuffer);
    }
}

void volume_control_render_popup(volume_control_t* ctrl, void* framebuffer) {
    // TODO: Render volume slider popup
    // - Draw background panel
    // - Draw slider track
    // - Draw slider thumb at current volume position
    // - Draw volume percentage text
}
