/**
 * @file main.c
 * @brief Application Launcher Entry Point
 */

#include "launcher.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    printf("Scarlett OS Application Launcher v1.0\n");

    // TODO: Connect to compositor via IPC
    compositor_ctx_t* compositor = compositor_create(1920, 1080);
    if (!compositor) {
        fprintf(stderr, "Failed to connect to compositor\n");
        return 1;
    }

    // Create launcher
    launcher_ctx_t* launcher = launcher_create(compositor);
    if (!launcher) {
        fprintf(stderr, "Failed to create launcher\n");
        compositor_destroy(compositor);
        return 1;
    }

    printf("Launcher initialized\n");

    // Show launcher window
    launcher_show(launcher);

    // Run launcher
    launcher_run(launcher);

    // Cleanup
    launcher_destroy(launcher);
    compositor_destroy(compositor);

    return 0;
}
