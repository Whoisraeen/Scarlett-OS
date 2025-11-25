/**
 * @file main.c
 * @brief Settings Application Entry Point
 */

#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../libs/libgui/include/compositor_ipc.h" // For compositor_connect/disconnect

int main(int argc, char** argv) {
    // Connect to compositor
    uint64_t compositor_port_id = compositor_connect(); // Connect to compositor service
    if (compositor_port_id == 0) {
        fprintf(stderr, "Failed to connect to compositor\n");
        return 1;
    }
    
    // Create settings application
    settings_ctx_t* settings = settings_create(NULL); // compositor is now handled via IPC
    
    // Open specific panel if requested
    if (argc > 1) {
        if (strcmp(argv[1], "display") == 0) {
            settings_switch_panel(settings, PANEL_DISPLAY);
        } else if (strcmp(argv[1], "appearance") == 0) {
            settings_switch_panel(settings, PANEL_APPEARANCE);
        } else if (strcmp(argv[1], "input") == 0) {
            settings_switch_panel(settings, PANEL_INPUT);
        } else if (strcmp(argv[1], "network") == 0) {
            settings_switch_panel(settings, PANEL_NETWORK);
        } else if (strcmp(argv[1], "sound") == 0) {
            settings_switch_panel(settings, PANEL_SOUND);
        } else if (strcmp(argv[1], "power") == 0) {
            settings_switch_panel(settings, PANEL_POWER);
        } else if (strcmp(argv[1], "users") == 0) {
            settings_switch_panel(settings, PANEL_USERS_SECURITY);
        } else if (strcmp(argv[1], "apps") == 0) {
            settings_switch_panel(settings, PANEL_APPLICATIONS);
        } else if (strcmp(argv[1], "updates") == 0) {
            settings_switch_panel(settings, PANEL_SYSTEM_UPDATES);
        }
    }
    
    // Run settings (simplified - actual implementation would have event loop)
    printf("Settings Application initialized\n");
    printf("Active panel: %d\n", settings->active_panel);
    printf("Display: %ux%u @ %uHz\n",
           settings->display.resolution_width,
           settings->display.resolution_height,
           settings->display.refresh_rate);
    printf("Theme: %s (Dark mode: %s)\n",
           settings->appearance.theme_name,
           settings->appearance.dark_mode ? "Yes" : "No");
    printf("Sound: Master volume %u%%\n", settings->sound.master_volume);
    printf("Power: %s plan\n", settings->power.power_plan);
    
    // Run the settings app main loop
    settings_run(settings);

    // Cleanup
    settings_destroy(settings);
    compositor_disconnect(); // Disconnect from compositor
    
    return 0;
}
