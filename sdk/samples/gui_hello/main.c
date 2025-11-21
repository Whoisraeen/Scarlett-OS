/**
 * GUI Hello World - ScarlettOS Sample GUI Application
 * 
 * Demonstrates basic GUI window creation
 */

#include <scarlettos.h>
#include <stdio.h>

// Forward declarations (GUI API would be fully defined in scarlettos/gui.h)
typedef struct window window_t;

extern window_t* window_create(const char* title, int width, int height);
extern void window_show(window_t* window);
extern void window_set_text(window_t* window, const char* text);
extern void window_run(window_t* window);
extern void window_destroy(window_t* window);

int main(int argc, char** argv) {
    // Create window
    window_t* window = window_create("Hello ScarlettOS", 400, 300);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        return 1;
    }
    
    // Set content
    window_set_text(window, "Hello from ScarlettOS GUI!");
    
    // Show window
    window_show(window);
    
    // Run event loop
    window_run(window);
    
    // Cleanup
    window_destroy(window);
    
    return 0;
}
