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
#include "../include/sync/spinlock.h"
#include "../include/security/capability.h"

#define MAX_PORTS 256
#define MAX_QUEUE_SIZE 32

// Message queue node
typedef struct message_node {
    ipc_message_t message;
    struct message_node* next;
} message_node_t;

// Thread waiting list node
typedef struct waiting_thread {
    thread_t* thread;
    struct waiting_thread* next;
} waiting_thread_t;

// Enhanced port structure with waiting lists
typedef struct ipc_port_internal {
    uint64_t port_id;
    uint64_t owner_tid;
    
    // Message queue
    message_node_t* queue_head;
    message_node_t* queue_tail;
    size_t queue_size;
    size_t queue_max;
    
    // Thread waiting lists
    waiting_thread_t* waiting_receivers;  // Threads waiting to receive
    waiting_thread_t* waiting_senders;    // Threads waiting to send (queue full)
    
    // Lock for this port
    spinlock_t lock;
    
    struct ipc_port_internal* next;
} ipc_port_internal_t;

// Port table
static ipc_port_internal_t* port_table[MAX_PORTS];
static uint64_t next_port_id = 1;
static spinlock_t port_table_lock = SPINLOCK_INIT;

/**
 * Initialize IPC system
 */
void ipc_init(void) {
    kinfo("Initializing IPC system...\n");
    
    spinlock_init(&port_table_lock);
    
    for (int i = 0; i < MAX_PORTS; i++) {
        port_table[i] = NULL;
    }
    
    kinfo("IPC system initialized\n");
}

/**
 * Create a new IPC port
 */
uint64_t ipc_create_port(void) {
    spinlock_lock(&port_table_lock);
    
    if (next_port_id >= MAX_PORTS) {
        spinlock_unlock(&port_table_lock);
        return 0;
    }
    
    ipc_port_internal_t* port = (ipc_port_internal_t*)kzalloc(sizeof(ipc_port_internal_t));
    if (!port) {
        spinlock_unlock(&port_table_lock);
        return 0;
    }
    
    port->port_id = next_port_id++;
    port->owner_tid = thread_current()->tid;
    port->queue_head = NULL;
    port->queue_tail = NULL;
    port->queue_size = 0;
    port->queue_max = MAX_QUEUE_SIZE;
    port->waiting_receivers = NULL;
    port->waiting_senders = NULL;
    port->next = NULL;
    spinlock_init(&port->lock);
    
    port_table[port->port_id] = port;
    
    spinlock_unlock(&port_table_lock);
    
    return port->port_id;
}

/**
 * Destroy an IPC port
 */
int ipc_destroy_port(uint64_t port_id) {
    if (port_id >= MAX_PORTS) {
        return -1;
    }
    
    spinlock_lock(&port_table_lock);
    
    ipc_port_internal_t* port = port_table[port_id];
    if (!port) {
        spinlock_unlock(&port_table_lock);
        return -1;
    }
    
    spinlock_lock(&port->lock);
    
    // Free queued messages
    message_node_t* msg_node = port->queue_head;
    while (msg_node) {
        message_node_t* next = msg_node->next;
        kfree(msg_node);
        msg_node = next;
    }
    
    // Wake up all waiting threads
    waiting_thread_t* waiting = port->waiting_receivers;
    while (waiting) {
        waiting_thread_t* next = waiting->next;
        thread_unblock(waiting->thread);
        kfree(waiting);
        waiting = next;
    }
    
    waiting = port->waiting_senders;
    while (waiting) {
        waiting_thread_t* next = waiting->next;
        thread_unblock(waiting->thread);
        kfree(waiting);
        waiting = next;
    }
    
    spinlock_unlock(&port->lock);
    
    port_table[port_id] = NULL;
    kfree(port);
    
    spinlock_unlock(&port_table_lock);
    
    return 0;
}

/**
 * Send a message
 */
int ipc_send(uint64_t port_id, ipc_message_t* msg) {
    if (port_id >= MAX_PORTS || !msg) {
        return -1;
    }
    
    spinlock_lock(&port_table_lock);
    ipc_port_internal_t* port = port_table[port_id];
    spinlock_unlock(&port_table_lock);
    
    if (!port) {
        return -1;
    }
    
    // Capability check: verify sender has capability to send to this port
    // TODO: Look up capability for this port in sender's capability table
    // For now, we'll check if port owner matches or if there's a capability
    // This is a simplified check - real implementation would use capability system
    extern uint64_t capability_find_for_port(uint64_t port_id);
    uint64_t cap_id = capability_find_for_port(port_id);
    if (cap_id == 0) {
        // No capability found - check if sender is port owner
        if (port->owner_tid != thread_current()->tid) {
            return -1;  // Not authorized
        }
    } else {
        // Check if capability grants write right
        if (!capability_check(cap_id, CAP_RIGHT_WRITE)) {
            return -1;  // No write right
        }
    }
    
    spinlock_lock(&port->lock);
    
    // Check if queue is full
    if (port->queue_size >= port->queue_max) {
        // Block until space available
        waiting_thread_t* waiting = (waiting_thread_t*)kmalloc(sizeof(waiting_thread_t));
        if (!waiting) {
            spinlock_unlock(&port->lock);
            return -1;
        }
        
        waiting->thread = thread_current();
        waiting->next = port->waiting_senders;
        port->waiting_senders = waiting;
        
        spinlock_unlock(&port->lock);
        thread_block();
        spinlock_lock(&port->lock);
    }
    
    // Create message node
    message_node_t* node = (message_node_t*)kmalloc(sizeof(message_node_t));
    if (!node) {
        spinlock_unlock(&port->lock);
        return -1;
    }
    
    // Copy message
    node->message = *msg;
    node->message.sender_tid = thread_current()->tid;
    node->next = NULL;
    
    // Add to queue
    if (port->queue_tail) {
        port->queue_tail->next = node;
        port->queue_tail = node;
    } else {
        port->queue_head = node;
        port->queue_tail = node;
    }
    
    port->queue_size++;
    
    // Wake up a waiting receiver if any
    if (port->waiting_receivers) {
        waiting_thread_t* waiting = port->waiting_receivers;
        port->waiting_receivers = waiting->next;
        
        thread_unblock(waiting->thread);
        kfree(waiting);
    }
    
    spinlock_unlock(&port->lock);
    
    return 0;
}

/**
 * Receive a message (blocking)
 */
int ipc_receive(uint64_t port_id, ipc_message_t* msg) {
    if (port_id >= MAX_PORTS || !msg) {
        return -1;
    }
    
    spinlock_lock(&port_table_lock);
    ipc_port_internal_t* port = port_table[port_id];
    spinlock_unlock(&port_table_lock);
    
    if (!port) {
        return -1;
    }
    
    // Capability check: verify receiver has capability to receive from this port
    // TODO: Look up capability for this port in receiver's capability table
    extern uint64_t capability_find_for_port(uint64_t port_id);
    uint64_t cap_id = capability_find_for_port(port_id);
    if (cap_id == 0) {
        // No capability found - check if receiver is port owner
        if (port->owner_tid != thread_current()->tid) {
            return -1;  // Not authorized
        }
    } else {
        // Check if capability grants read right
        if (!capability_check(cap_id, CAP_RIGHT_READ)) {
            return -1;  // No read right
        }
    }
    
    spinlock_lock(&port->lock);
    
    // Wait for message if queue is empty
    while (port->queue_size == 0) {
        waiting_thread_t* waiting = (waiting_thread_t*)kmalloc(sizeof(waiting_thread_t));
        if (!waiting) {
            spinlock_unlock(&port->lock);
            return -1;
        }
        
        waiting->thread = thread_current();
        waiting->next = port->waiting_receivers;
        port->waiting_receivers = waiting;
        
        spinlock_unlock(&port->lock);
        thread_block();
        spinlock_lock(&port->lock);
    }
    
    // Dequeue message
    message_node_t* node = port->queue_head;
    if (!node) {
        spinlock_unlock(&port->lock);
        return -1;
    }
    
    port->queue_head = node->next;
    if (!port->queue_head) {
        port->queue_tail = NULL;
    }
    
    port->queue_size--;
    
    // Copy message to output
    *msg = node->message;
    kfree(node);
    
    // Wake up a waiting sender if any
    if (port->waiting_senders) {
        waiting_thread_t* waiting = port->waiting_senders;
        port->waiting_senders = waiting->next;
        
        thread_unblock(waiting->thread);
        kfree(waiting);
    }
    
    spinlock_unlock(&port->lock);
    
    return 0;
}

/**
 * Try to receive a message (non-blocking)
 */
int ipc_try_receive(uint64_t port_id, ipc_message_t* msg) {
    if (port_id >= MAX_PORTS || !msg) {
        return -1;
    }
    
    spinlock_lock(&port_table_lock);
    ipc_port_internal_t* port = port_table[port_id];
    spinlock_unlock(&port_table_lock);
    
    if (!port) {
        return -1;
    }
    
    spinlock_lock(&port->lock);
    
    if (port->queue_size == 0) {
        spinlock_unlock(&port->lock);
        return -1;  // No message available
    }
    
    // Dequeue message
    message_node_t* node = port->queue_head;
    port->queue_head = node->next;
    if (!port->queue_head) {
        port->queue_tail = NULL;
    }
    
    port->queue_size--;
    
    // Copy message to output
    *msg = node->message;
    kfree(node);
    
    // Wake up a waiting sender if any
    if (port->waiting_senders) {
        waiting_thread_t* waiting = port->waiting_senders;
        port->waiting_senders = waiting->next;
        
        thread_unblock(waiting->thread);
        kfree(waiting);
    }
    
    spinlock_unlock(&port->lock);
    
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
