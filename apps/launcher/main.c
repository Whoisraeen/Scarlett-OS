/**
 * @file main.c
 * @brief Application Launcher Entry Point
 */

#include "launcher.h"
#include <stdio.h>
#include <stdlib.h>
#include "../../libs/libgui/include/compositor_ipc.h" // Needed for compositor_get_screen_info

int main(int argc, char* argv[]) {
    printf("Scarlett OS Application Launcher v1.0\n");

    // Create launcher
    launcher_ctx_t* launcher = launcher_create(NULL); // Pass NULL as compositor is handled via IPC
    if (!launcher) {
        fprintf(stderr, "Failed to create launcher\n");
        return 1;
    }

    printf("Launcher initialized\n");

    // Show launcher window
    launcher_show(launcher);

    // Run launcher
    launcher_run(launcher);

    // Cleanup
    launcher_destroy(launcher);

    return 0;
}
