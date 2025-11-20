/**
 * @file main.cpp
 * @brief Compositor service entry point
 * 
 * The compositor is a user-space service that manages window composition
 * and rendering to the display.
 */

#include "compositor.hpp"
#include <cstdint>

extern "C" {
    // System call wrappers (to be implemented)
    extern int syscall_ipc_send(uint64_t port_id, void* msg);
    extern int syscall_ipc_receive(uint64_t port_id, void* msg);
}

/**
 * Entry point for compositor service
 */
int main() {
    Compositor compositor;
    
    // Initialize compositor
    if (!compositor.init()) {
        return 1;
    }
    
    // Main compositor loop
    compositor.run();
    
    return 0;
}

