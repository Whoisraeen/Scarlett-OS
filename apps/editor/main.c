/**
 * @file main.c
 * @brief Text Editor Entry Point
 */

#include "editor.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    printf("Scarlett OS Text Editor v1.0\n");

    // TODO: Connect to compositor via IPC
    compositor_ctx_t* compositor = compositor_create(1920, 1080);
    if (!compositor) {
        fprintf(stderr, "Failed to connect to compositor\n");
        return 1;
    }

    // Create editor
    editor_ctx_t* editor = editor_create(compositor);
    if (!editor) {
        fprintf(stderr, "Failed to create text editor\n");
        compositor_destroy(compositor);
        return 1;
    }

    // Load file if specified
    if (argc > 1) {
        editor_create_tab(editor, argv[1]);
    }

    printf("Text editor initialized\n");

    // Run editor
    editor_run(editor);

    // Cleanup
    editor_destroy(editor);
    compositor_destroy(compositor);

    return 0;
}
