/**
 * @file main.c
 * @brief Terminal Emulator Entry Point
 */

#include "terminal.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    printf("Scarlett OS Terminal Emulator v1.0\n");

    // Connect to compositor via IPC
    compositor_ctx_t* compositor = compositor_create(1920, 1080);
    if (!compositor) {
        fprintf(stderr, "Failed to connect to compositor\n");
        return 1;
    }

    // Create terminal
    terminal_ctx_t* term = terminal_create(compositor);
    if (!term) {
        fprintf(stderr, "Failed to create terminal\n");
        compositor_destroy(compositor);
        return 1;
    }

    printf("Terminal emulator initialized\n");

    // Run terminal
    terminal_run(term);

    // Cleanup
    terminal_destroy(term);
    compositor_destroy(compositor);

    return 0;
}
