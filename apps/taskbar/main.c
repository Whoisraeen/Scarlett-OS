/**
 * @file main.c
 * @brief Taskbar Entry Point
 */

#include "taskbar.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    printf("Scarlett OS Taskbar v1.0\n");

    // TODO: Connect to compositor via IPC
    compositor_ctx_t* compositor = compositor_create(1920, 1080);
    if (!compositor) {
        fprintf(stderr, "Failed to connect to compositor\n");
        return 1;
    }

    // Create taskbar
    taskbar_ctx_t* taskbar = taskbar_create(compositor);
    if (!taskbar) {
        fprintf(stderr, "Failed to create taskbar\n");
        compositor_destroy(compositor);
        return 1;
    }

    printf("Taskbar initialized\n");

    // Run taskbar
    taskbar_run(taskbar);

    // Cleanup
    taskbar_destroy(taskbar);
    compositor_destroy(compositor);

    return 0;
}
