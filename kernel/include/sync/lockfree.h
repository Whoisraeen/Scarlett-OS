/**
 * @file lockfree.h
 * @brief Lock-free data structures
 * 
 * Provides lock-free implementations for high-performance scenarios.
 */

#ifndef KERNEL_SYNC_LOCKFREE_H
#define KERNEL_SYNC_LOCKFREE_H

#include "../types.h"

// Lock-free queue node
typedef struct lf_queue_node {
    void* data;
    struct lf_queue_node* next;
} lf_queue_node_t;

// Lock-free queue
typedef struct {
    lf_queue_node_t* head;
    lf_queue_node_t* tail;
    volatile uint32_t size;
} lf_queue_t;

// Lock-free stack
typedef struct {
    lf_queue_node_t* top;
    volatile uint32_t size;
} lf_stack_t;

// Lock-free atomic counter
typedef volatile uint32_t lf_counter_t;

// Lock-free queue functions
void lf_queue_init(lf_queue_t* queue);
void lf_queue_enqueue(lf_queue_t* queue, void* data);
void* lf_queue_dequeue(lf_queue_t* queue);
bool lf_queue_is_empty(lf_queue_t* queue);
uint32_t lf_queue_size(lf_queue_t* queue);

// Lock-free stack functions
void lf_stack_init(lf_stack_t* stack);
void lf_stack_push(lf_stack_t* stack, void* data);
void* lf_stack_pop(lf_stack_t* stack);
bool lf_stack_is_empty(lf_stack_t* stack);
uint32_t lf_stack_size(lf_stack_t* stack);

// Lock-free counter functions
void lf_counter_init(lf_counter_t* counter, uint32_t initial);
uint32_t lf_counter_increment(lf_counter_t* counter);
uint32_t lf_counter_decrement(lf_counter_t* counter);
uint32_t lf_counter_add(lf_counter_t* counter, uint32_t value);
uint32_t lf_counter_get(lf_counter_t* counter);

#endif // KERNEL_SYNC_LOCKFREE_H

