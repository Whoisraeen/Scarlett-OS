/**
 * @file input.c
 * @brief Input event system implementation
 */

#include "../include/input/input.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/sync/spinlock.h"

// Input event queue
#define INPUT_EVENT_QUEUE_SIZE 256

static struct {
    input_event_t queue[INPUT_EVENT_QUEUE_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    spinlock_t lock;
    bool initialized;
} input_queue = {0};

/**
 * Initialize input event system
 */
error_code_t input_event_init(void) {
    if (input_queue.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing input event system...\n");
    
    input_queue.head = 0;
    input_queue.tail = 0;
    input_queue.count = 0;
    spinlock_init(&input_queue.lock);
    input_queue.initialized = true;
    
    kinfo("Input event system initialized\n");
    return ERR_OK;
}

/**
 * Enqueue an input event
 */
error_code_t input_event_enqueue(input_event_t* event) {
    if (!input_queue.initialized || !event) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_acquire(&input_queue.lock);
    
    // Check if queue is full
    if (input_queue.count >= INPUT_EVENT_QUEUE_SIZE) {
        spinlock_release(&input_queue.lock);
        return ERR_DEVICE_BUSY;  // Queue full
    }
    
    // Add event to queue
    input_queue.queue[input_queue.tail] = *event;
    input_queue.tail = (input_queue.tail + 1) % INPUT_EVENT_QUEUE_SIZE;
    input_queue.count++;
    
    spinlock_release(&input_queue.lock);
    
    return ERR_OK;
}

/**
 * Dequeue an input event
 */
error_code_t input_event_dequeue(input_event_t* event) {
    if (!input_queue.initialized || !event) {
        return ERR_INVALID_ARG;
    }
    
    spinlock_acquire(&input_queue.lock);
    
    // Check if queue is empty
    if (input_queue.count == 0) {
        spinlock_release(&input_queue.lock);
        return ERR_NOT_FOUND;
    }
    
    // Remove event from queue
    *event = input_queue.queue[input_queue.head];
    input_queue.head = (input_queue.head + 1) % INPUT_EVENT_QUEUE_SIZE;
    input_queue.count--;
    
    spinlock_release(&input_queue.lock);
    
    return ERR_OK;
}

/**
 * Check if events are available
 */
bool input_event_available(void) {
    if (!input_queue.initialized) {
        return false;
    }
    
    spinlock_acquire(&input_queue.lock);
    bool available = (input_queue.count > 0);
    spinlock_release(&input_queue.lock);
    
    return available;
}

/**
 * Clear input event queue
 */
void input_event_clear(void) {
    if (!input_queue.initialized) {
        return;
    }
    
    spinlock_acquire(&input_queue.lock);
    input_queue.head = 0;
    input_queue.tail = 0;
    input_queue.count = 0;
    spinlock_release(&input_queue.lock);
}

/**
 * Handle keyboard event (called by keyboard driver)
 */
void input_handle_keyboard(key_event_t* event) {
    if (!event) {
        return;
    }
    
    input_event_t input_event = {0};
    input_event.type = INPUT_EVENT_KEYBOARD;
    input_event.data.keyboard = *event;
    
    input_event_enqueue(&input_event);
}

/**
 * Handle mouse event (called by mouse driver)
 */
void input_handle_mouse(mouse_event_t* event) {
    if (!event) {
        return;
    }
    
    input_event_t input_event = {0};
    input_event.type = INPUT_EVENT_MOUSE;
    input_event.data.mouse = *event;
    
    input_event_enqueue(&input_event);
}

