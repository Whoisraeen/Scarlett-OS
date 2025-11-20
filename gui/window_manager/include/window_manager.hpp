/**
 * @file window_manager.hpp
 * @brief Window Manager class
 */

#ifndef WINDOW_MANAGER_HPP
#define WINDOW_MANAGER_HPP

#include <cstdint>
#include <vector>
#include <memory>

class Window;

/**
 * Window Manager class
 * Manages window creation, positioning, focus, and input routing
 */
class WindowManager {
public:
    WindowManager();
    ~WindowManager();
    
    /**
     * Initialize the window manager
     */
    bool init();
    
    /**
     * Main window manager loop
     */
    void run();
    
    /**
     * Create a new window
     */
    Window* create_window(uint32_t width, uint32_t height, const char* title);
    
    /**
     * Destroy a window
     */
    void destroy_window(Window* window);
    
    /**
     * Set focused window
     */
    void set_focus(Window* window);
    
    /**
     * Handle input event
     */
    void handle_input(uint32_t type, uint32_t code, int32_t value);

private:
    std::vector<std::unique_ptr<Window>> windows_;
    Window* focused_window_;
    uint64_t compositor_port_;
    bool initialized_;
    
    /**
     * Connect to compositor service
     */
    bool connect_to_compositor();
    
    /**
     * Send window update to compositor
     */
    void notify_compositor(Window* window);
};

#endif // WINDOW_MANAGER_HPP

