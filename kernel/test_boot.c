/**
 * Minimal test kernel to verify Limine boot
 */

#include <stdint.h>
#include "../bootloader/limine/limine.h"

// Limine base revision
__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(2);

// Framebuffer request
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// Request markers
__attribute__((used, section(".limine_requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

// Halt and catch fire
static void hcf(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

// Simple function to write to serial port
static void serial_write(const char *str) {
    uint8_t status;
    while (*str) {
        // Wait for transmit to be ready (check bit 5 of LSR at 0x3FD)
        do {
            __asm__ volatile("inb $0x3FD, %0" : "=a"(status));
        } while ((status & 0x20) == 0);

        // Write character to data port at 0x3F8
        __asm__ volatile("outb %0, $0x3F8" : : "a"(*str));
        str++;
    }
}

// Entry point (called from test_entry.S)
void test_main(void) {
    // Write to serial immediately
    serial_write("LIMINE BOOT TEST: Kernel _start() reached!\r\n");

    // Check if we have a framebuffer
    if (framebuffer_request.response &&
        framebuffer_request.response->framebuffer_count >= 1) {
        serial_write("Framebuffer available!\r\n");

        // Get first framebuffer
        struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

        // Fill screen with blue (simple test)
        uint32_t *fb_ptr = (uint32_t*)fb->address;
        for (uint64_t i = 0; i < fb->width * fb->height; i++) {
            fb_ptr[i] = 0x0000FF00; // Green
        }

        serial_write("Screen filled with green!\r\n");
    } else {
        serial_write("No framebuffer available\r\n");
    }

    serial_write("Test kernel complete - halting\r\n");

    // Halt
    hcf();
}
