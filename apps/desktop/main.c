/**
 * @file main.c
 * @brief Desktop Shell Entry Point
 */

#include "desktop.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    printf("Scarlett OS Desktop Shell v1.0\n");

    // Connect to compositor via IPC (implicitly handled by window_create)
    // We pass NULL as the compositor context since we use IPC
    
    // Create desktop shell
    desktop_ctx_t* desktop = desktop_create(NULL);
    if (!desktop) {
        fprintf(stderr, "Failed to create desktop shell\n");
        return 1;
    }

    // Load configuration
    desktop_load_config(desktop, "/etc/desktop/config.ini");

    printf("Desktop shell initialized\n");

    // Run desktop shell
    desktop_run(desktop);

    // Cleanup
    desktop_destroy(desktop);

    return 0;
}
