/**
 * @file main.cpp
 * @brief Window Manager service entry point
 */

#include "window_manager.hpp"

int main() {
    WindowManager wm;
    
    if (!wm.init()) {
        return 1;
    }
    
    wm.run();
    
    return 0;
}

