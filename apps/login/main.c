/**
 * @file main.c
 * @brief Login Screen Entry Point
 */

#include "login.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    printf("Scarlett OS Login Screen v1.0\n");

    // TODO: Connect to compositor via IPC
    // For now, create a local compositor instance
    compositor_ctx_t* compositor = compositor_create(1920, 1080);
    if (!compositor) {
        fprintf(stderr, "Failed to create compositor\n");
        return 1;
    }

    // Create login screen
    login_ctx_t* login = login_create(compositor);
    if (!login) {
        fprintf(stderr, "Failed to create login screen\n");
        compositor_destroy(compositor);
        return 1;
    }

    printf("Login screen initialized\n");

    // Run login screen
    login_run(login);

    // Cleanup
    login_destroy(login);
    compositor_destroy(compositor);

    return 0;
}

