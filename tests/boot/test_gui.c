/**
 * @file test_gui.c
 * @brief Test GUI rendering
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/graphics/graphics.h"
#include "../../kernel/include/kprintf.h"
#include "../../kernel/include/debug.h"

/**
 * Test framebuffer access
 */
void test_framebuffer(void) {
    kinfo("=== Testing Framebuffer ===\n");
    
    // Get framebuffer info
    extern void* g_framebuffer;
    extern uint32_t g_framebuffer_width;
    extern uint32_t g_framebuffer_height;
    extern uint32_t g_framebuffer_bpp;
    
    if (!g_framebuffer) {
        kerror("[FAIL] Framebuffer not initialized\n");
        return;
    }
    
    kinfo("[PASS] Framebuffer: %p, %ux%u @ %u bpp\n", 
          g_framebuffer, g_framebuffer_width, g_framebuffer_height, g_framebuffer_bpp);
    
    // Test basic drawing (if graphics functions available)
    kinfo("[INFO] Framebuffer access test complete\n");
}

/**
 * Test graphics rendering (placeholder)
 */
void test_graphics_rendering(void) {
    kinfo("=== Testing Graphics Rendering ===\n");
    
    // TODO: Test basic drawing primitives
    // TODO: Test text rendering
    // TODO: Test window rendering
    
    kinfo("[INFO] Graphics rendering test (placeholder - needs GUI service)\n");
}

