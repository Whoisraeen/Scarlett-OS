/**
 * @file main.c
 * @brief Login Screen Entry Point
 */

#include "login.h"
#include <stdio.h>
#include <stdlib.h>
#include "../../libs/libgui/include/compositor_ipc.h" // Needed for compositor_get_screen_info

int main(int argc, char* argv[]) {
    printf("Scarlett OS Login Screen v1.0\n");

    // Create login screen
    login_ctx_t* login = login_create(NULL); // Pass NULL as compositor is handled via IPC
    if (!login) {
        fprintf(stderr, "Failed to create login screen\n");
        return 1;
    }

    printf("Login screen initialized\n");

    // Run login screen
    login_run(login);

    // Cleanup
    login_destroy(login);

    return 0;
}

