/**
 * @file lockfree.c
 * @brief Lock-free data structures implementation
 * 
 * Uses compare-and-swap (CAS) operations for thread-safe, lock-free data structures.
 */

#include "../include/types.h"
#include "../include/sync/lockfree.h"
#include "../include/mm/heap.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/string.h"

// Atomic operations (using GCC builtins)
static inline bool atomic_compare_exchange(volatile void* ptr, void* expected, void* desired) {
    return __sync_bool_compare_and_swap((volatile void**)ptr, expected, desired);
}

static inline void* atomic_exchange(volatile void* ptr, void* value) {
    return __sync_lock_test_and_set((volatile void**)ptr, value);
}

static inline void atomic_store(volatile void* ptr, void* value) {
    __sync_synchronize();
    *(volatile void**)ptr = value;
    __sync_synchronize();
}

static inline void* atomic_load(volatile void* ptr) {
    __sync_synchronize();
    void* value = *(volatile void**)ptr;
    __sync_synchronize();
    return value;
}

static inline uint32_t atomic_fetch_add(volatile uint32_t* ptr, uint32_t value) {
    return __sync_fetch_and_add(ptr, value);
}

static inline uint32_t atomic_fetch_sub(volatile uint32_t* ptr, uint32_t value) {
    return __sync_fetch_and_sub(ptr, value);
}

// Memory barrier
static inline void memory_barrier(void) {
    __sync_synchronize();
}

/**
 * Initialize lock-free queue
 */
void lf_queue_init(lf_queue_t* queue) {
    if (!queue) {
        return;
    }
    
    // Create dummy node
    lf_queue_node_t* dummy = (lf_queue_node_t*)kmalloc(sizeof(lf_queue_node_t));
    if (!dummy) {
        kerror("Lock-free queue: Failed to allocate dummy node\n");
        return;
    }
    
    dummy->data = NULL;
    dummy->next = NULL;
    
    queue->head = dummy;
    queue->tail = dummy;
    queue->size = 0;
}

/**
 * Enqueue item into lock-free queue
 */
void lf_queue_enqueue(lf_queue_t* queue, void* data) {
    if (!queue || !data) {
        return;
    }
    
    // Allocate new node
    lf_queue_node_t* node = (lf_queue_node_t*)kmalloc(sizeof(lf_queue_node_t));
    if (!node) {
        kerror("Lock-free queue: Failed to allocate node\n");
        return;
    }
    
    node->data = data;
    node->next = NULL;
    
    // Lock-free enqueue using CAS
    lf_queue_node_t* tail;
    lf_queue_node_t* next;
    
    while (1) {
        tail = (lf_queue_node_t*)atomic_load((volatile void*)&queue->tail);
        next = (lf_queue_node_t*)atomic_load((volatile void*)&tail->next);
        
        // Check if tail is still valid
        if (tail == (lf_queue_node_t*)atomic_load((volatile void*)&queue->tail)) {
            if (next == NULL) {
                // Try to link new node
                if (atomic_compare_exchange((volatile void*)&tail->next, NULL, node)) {
                    // Success - update tail
                    atomic_compare_exchange((volatile void*)&queue->tail, tail, node);
                    atomic_fetch_add(&queue->size, 1);
                    return;
                }
            } else {
                // Tail is behind, help advance it
                atomic_compare_exchange((volatile void*)&queue->tail, tail, next);
            }
        }
    }
}

/**
 * Dequeue item from lock-free queue
 */
void* lf_queue_dequeue(lf_queue_t* queue) {
    if (!queue) {
        return NULL;
    }
    
    void* data;
    lf_queue_node_t* head;
    lf_queue_node_t* tail;
    lf_queue_node_t* next;
    
    while (1) {
        head = (lf_queue_node_t*)atomic_load((volatile void*)&queue->head);
        tail = (lf_queue_node_t*)atomic_load((volatile void*)&queue->tail);
        next = (lf_queue_node_t*)atomic_load((volatile void*)&head->next);
        
        // Check if head is still valid
        if (head == (lf_queue_node_t*)atomic_load((volatile void*)&queue->head)) {
            if (head == tail) {
                // Queue is empty (or has only dummy node)
                if (next == NULL) {
                    return NULL;
                }
                // Help advance tail
                atomic_compare_exchange((volatile void*)&queue->tail, tail, next);
            } else {
                // Read data before CAS (next might be freed)
                data = next->data;
                
                // Try to move head forward
                if (atomic_compare_exchange((volatile void*)&queue->head, head, next)) {
                    // Success - free old head (dummy node)
                    kfree(head);
                    atomic_fetch_sub(&queue->size, 1);
                    return data;
                }
            }
        }
    }
}

/**
 * Check if queue is empty
 */
bool lf_queue_is_empty(lf_queue_t* queue) {
    if (!queue) {
        return true;
    }
    
    lf_queue_node_t* head = (lf_queue_node_t*)atomic_load((volatile void*)&queue->head);
    lf_queue_node_t* tail = (lf_queue_node_t*)atomic_load((volatile void*)&queue->tail);
    
    return (head == tail) && (head->next == NULL);
}

/**
 * Get queue size (approximate)
 */
uint32_t lf_queue_size(lf_queue_t* queue) {
    if (!queue) {
        return 0;
    }
    
    return atomic_load((volatile void*)&queue->size);
}

/**
 * Initialize lock-free stack
 */
void lf_stack_init(lf_stack_t* stack) {
    if (!stack) {
        return;
    }
    
    stack->top = NULL;
    stack->size = 0;
}

/**
 * Push item onto lock-free stack
 */
void lf_stack_push(lf_stack_t* stack, void* data) {
    if (!stack || !data) {
        return;
    }
    
    // Allocate new node
    lf_queue_node_t* node = (lf_queue_node_t*)kmalloc(sizeof(lf_queue_node_t));
    if (!node) {
        kerror("Lock-free stack: Failed to allocate node\n");
        return;
    }
    
    node->data = data;
    
    // Lock-free push using CAS
    lf_queue_node_t* old_top;
    do {
        old_top = (lf_queue_node_t*)atomic_load((volatile void*)&stack->top);
        node->next = old_top;
    } while (!atomic_compare_exchange((volatile void*)&stack->top, old_top, node));
    
    atomic_fetch_add(&stack->size, 1);
}

/**
 * Pop item from lock-free stack
 */
void* lf_stack_pop(lf_stack_t* stack) {
    if (!stack) {
        return NULL;
    }
    
    lf_queue_node_t* old_top;
    lf_queue_node_t* new_top;
    void* data;
    
    do {
        old_top = (lf_queue_node_t*)atomic_load((volatile void*)&stack->top);
        if (old_top == NULL) {
            return NULL;  // Stack is empty
        }
        
        new_top = old_top->next;
        data = old_top->data;
    } while (!atomic_compare_exchange((volatile void*)&stack->top, old_top, new_top));
    
    // Free node
    kfree(old_top);
    atomic_fetch_sub(&stack->size, 1);
    
    return data;
}

/**
 * Check if stack is empty
 */
bool lf_stack_is_empty(lf_stack_t* stack) {
    if (!stack) {
        return true;
    }
    
    return atomic_load((volatile void*)&stack->top) == NULL;
}

/**
 * Get stack size (approximate)
 */
uint32_t lf_stack_size(lf_stack_t* stack) {
    if (!stack) {
        return 0;
    }
    
    return atomic_load((volatile void*)&stack->size);
}

/**
 * Initialize lock-free counter
 */
void lf_counter_init(lf_counter_t* counter, uint32_t initial) {
    if (!counter) {
        return;
    }
    
    *counter = initial;
    memory_barrier();
}

/**
 * Increment lock-free counter
 */
uint32_t lf_counter_increment(lf_counter_t* counter) {
    if (!counter) {
        return 0;
    }
    
    return atomic_fetch_add(counter, 1) + 1;
}

/**
 * Decrement lock-free counter
 */
uint32_t lf_counter_decrement(lf_counter_t* counter) {
    if (!counter) {
        return 0;
    }
    
    return atomic_fetch_sub(counter, 1) - 1;
}

/**
 * Add value to lock-free counter
 */
uint32_t lf_counter_add(lf_counter_t* counter, uint32_t value) {
    if (!counter) {
        return 0;
    }
    
    return atomic_fetch_add(counter, value) + value;
}

/**
 * Get lock-free counter value
 */
uint32_t lf_counter_get(lf_counter_t* counter) {
    if (!counter) {
        return 0;
    }
    
    memory_barrier();
    return *counter;
}

