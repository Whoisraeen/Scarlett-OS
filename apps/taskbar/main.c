/**
 * @file main.c
 * @brief Taskbar Entry Point
 */

#include "taskbar.h"
#include <stdio.h>
#include <stdlib.h>
#include "../../libs/libgui/include/compositor_ipc.h" // Needed for compositor_get_screen_info

int main(int argc, char* argv[]) {
    printf("Scarlett OS Taskbar v1.0\n");

    // Create taskbar
    taskbar_ctx_t* taskbar = taskbar_create(NULL); // Pass NULL as compositor is handled via IPC
    if (!taskbar) {
        fprintf(stderr, "Failed to create taskbar\n");
        return 1;
    }

    printf("Taskbar initialized\n");

    // Run taskbar
    taskbar_run(taskbar);

    // Cleanup
    taskbar_destroy(taskbar);

    return 0;
}
