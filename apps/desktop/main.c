/**
 * @file main.c
 * @brief Desktop Shell Entry Point
 */

#include "desktop.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    printf("Scarlett OS Desktop Shell v1.0\n");

    // TODO: Connect to compositor via IPC
    // For now, create a local compositor instance
    compositor_ctx_t* compositor = compositor_create(1920, 1080);
    if (!compositor) {
        fprintf(stderr, "Failed to create compositor\n");
        return 1;
    }

    // Create desktop shell
    desktop_ctx_t* desktop = desktop_create(compositor);
    if (!desktop) {
        fprintf(stderr, "Failed to create desktop shell\n");
        compositor_destroy(compositor);
        return 1;
    }

    // Load configuration
    desktop_load_config(desktop, "/etc/desktop/config.ini");

    printf("Desktop shell initialized\n");

    // Run desktop shell
    desktop_run(desktop);

    // Cleanup
    desktop_destroy(desktop);
    compositor_destroy(compositor);

    return 0;
}
