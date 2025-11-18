/**
 * @file ipc.c
 * @brief Inter-Process Communication implementation
 */

#include "../include/types.h"
#include "../include/ipc/ipc.h"
#include "../include/sched/scheduler.h"
#include "../include/mm/heap.h"
#include "../include/kprintf.h"
#include "../include/debug.h"

#define MAX_PORTS 256
#define MAX_QUEUE_SIZE 32

// Port table
static ipc_port_t* port_table[MAX_PORTS];
static uint64_t next_port_id = 1;

/**
 * Initialize IPC system
 */
void ipc_init(void) {
    kinfo("Initializing IPC system...\n");
    
    for (int i = 0; i < MAX_PORTS; i++) {
        port_table[i] = NULL;
    }
    
    kinfo("IPC system initialized\n");
}

/**
 * Create a new IPC port
 */
uint64_t ipc_create_port(void) {
    if (next_port_id >= MAX_PORTS) {
        return 0;
    }
    
    ipc_port_t* port = (ipc_port_t*)kzalloc(sizeof(ipc_port_t));
    if (!port) {
        return 0;
    }
    
    port->port_id = next_port_id++;
    port->owner_tid = thread_current()->tid;
    port->message_queue = NULL;
    port->queue_size = 0;
    port->queue_max = MAX_QUEUE_SIZE;
    port->next = NULL;
    
    port_table[port->port_id] = port;
    
    return port->port_id;
}

/**
 * Destroy an IPC port
 */
int ipc_destroy_port(uint64_t port_id) {
    if (port_id >= MAX_PORTS || !port_table[port_id]) {
        return -1;
    }
    
    ipc_port_t* port = port_table[port_id];
    
    // Free queued messages
    // TODO: Implement message queue cleanup
    
    port_table[port_id] = NULL;
    kfree(port);
    
    return 0;
}

/**
 * Send a message
 */
int ipc_send(uint64_t port_id, ipc_message_t* msg) {
    if (port_id >= MAX_PORTS || !port_table[port_id]) {
        return -1;
    }
    
    ipc_port_t* port = port_table[port_id];
    
    // Check queue full
    if (port->queue_size >= port->queue_max) {
        return -1;
    }
    
    // Copy message
    ipc_message_t* queued_msg = (ipc_message_t*)kmalloc(sizeof(ipc_message_t));
    if (!queued_msg) {
        return -1;
    }
    
    *queued_msg = *msg;
    queued_msg->sender_tid = thread_current()->tid;
    
    // Add to queue (simple linked list for now)
    // TODO: Implement proper message queue
    
    port->queue_size++;
    
    // Wake up waiting thread if any
    // TODO: Implement thread waiting list
    
    return 0;
}

/**
 * Receive a message (blocking)
 */
int ipc_receive(uint64_t port_id, ipc_message_t* msg) {
    if (port_id >= MAX_PORTS || !port_table[port_id]) {
        return -1;
    }
    
    ipc_port_t* port = port_table[port_id];
    
    // Wait for message
    while (port->queue_size == 0) {
        thread_block();
    }
    
    // Get message from queue
    // TODO: Implement proper queue dequeue
    
    port->queue_size--;
    
    return 0;
}

/**
 * Try to receive a message (non-blocking)
 */
int ipc_try_receive(uint64_t port_id, ipc_message_t* msg) {
    if (port_id >= MAX_PORTS || !port_table[port_id]) {
        return -1;
    }
    
    ipc_port_t* port = port_table[port_id];
    
    if (port->queue_size == 0) {
        return -1;  // No message available
    }
    
    // Get message from queue
    // TODO: Implement proper queue dequeue
    
    port->queue_size--;
    
    return 0;
}

/**
 * Send and receive (call/reply pattern)
 */
int ipc_call(uint64_t port_id, ipc_message_t* request, ipc_message_t* response) {
    // Send request
    if (ipc_send(port_id, request) != 0) {
        return -1;
    }
    
    // Create reply port
    uint64_t reply_port = ipc_create_port();
    if (reply_port == 0) {
        return -1;
    }
    
    // Wait for response
    int result = ipc_receive(reply_port, response);
    
    // Cleanup
    ipc_destroy_port(reply_port);
    
    return result;
}

