/**
 * @file main.c
 * @brief File Manager Entry Point
 */

#include "filemanager.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    printf("Scarlett OS File Manager v1.0\n");

    // TODO: Connect to compositor via IPC
    compositor_ctx_t* compositor = compositor_create(1920, 1080);
    if (!compositor) {
        fprintf(stderr, "Failed to connect to compositor\n");
        return 1;
    }

    // Create file manager
    filemanager_ctx_t* fm = filemanager_create(compositor);
    if (!fm) {
        fprintf(stderr, "Failed to create file manager\n");
        compositor_destroy(compositor);
        return 1;
    }

    printf("File Manager initialized\n");

    // Run file manager
    filemanager_run(fm);

    // Cleanup
    filemanager_destroy(fm);
    compositor_destroy(compositor);

    return 0;
}
