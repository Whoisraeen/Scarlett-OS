/**
 * @file compositor.hpp
 * @brief Window compositor class
 */

#ifndef COMPOSITOR_HPP
#define COMPOSITOR_HPP

#include <cstdint>
#include <vector>

class Window;

/**
 * Compositor class
 * Manages window composition and rendering
 */
class Compositor {
public:
    Compositor();
    ~Compositor();
    
    /**
     * Initialize the compositor
     */
    bool init();
    
    /**
     * Main compositor loop
     */
    void run();
    
    /**
     * Register a window for composition
     */
    void register_window(Window* window);
    
    /**
     * Unregister a window
     */
    void unregister_window(Window* window);
    
    /**
     * Composite all windows to the framebuffer
     */
    void composite();
    
    /**
     * Swap buffers (present to display)
     */
    void swap_buffers();

private:
    std::vector<Window*> windows_;
    uint32_t* framebuffer_;
    uint32_t width_;
    uint32_t height_;
    bool initialized_;
    
    /**
     * Initialize framebuffer
     */
    bool init_framebuffer();
    
    /**
     * Clear framebuffer
     */
    void clear_framebuffer(uint32_t color);
};

#endif // COMPOSITOR_HPP

