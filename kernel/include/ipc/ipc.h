/**
 * @file ipc.h
 * @brief Inter-Process Communication interface
 */

#ifndef KERNEL_IPC_IPC_H
#define KERNEL_IPC_IPC_H

#include "../types.h"

// IPC message types
#define IPC_MSG_DATA 0
#define IPC_MSG_REQUEST 1
#define IPC_MSG_RESPONSE 2
#define IPC_MSG_NOTIFICATION 3

// Maximum inline message size
#define IPC_INLINE_SIZE 64

// IPC message structure
typedef struct ipc_message {
    uint64_t sender_tid;
    uint64_t msg_id;
    uint32_t type;
    uint32_t inline_size;
    uint8_t inline_data[IPC_INLINE_SIZE];
    void* buffer;
    size_t buffer_size;
} ipc_message_t;

// IPC port (endpoint for communication)
typedef struct ipc_port {
    uint64_t port_id;
    uint64_t owner_tid;
    struct ipc_message* message_queue;
    size_t queue_size;
    size_t queue_max;
    struct ipc_port* next;
} ipc_port_t;

/**
 * Initialize IPC system
 */
void ipc_init(void);

/**
 * Create a new IPC port
 * @return Port ID or 0 on error
 */
uint64_t ipc_create_port(void);

/**
 * Destroy an IPC port
 */
int ipc_destroy_port(uint64_t port_id);

/**
 * Send a message (synchronous)
 * @param port_id Target port
 * @param msg Message to send
 * @return 0 on success, -1 on error
 */
int ipc_send(uint64_t port_id, ipc_message_t* msg);

/**
 * Receive a message (blocking)
 * @param port_id Port to receive from
 * @param msg Buffer to receive message
 * @return 0 on success, -1 on error
 */
int ipc_receive(uint64_t port_id, ipc_message_t* msg);

/**
 * Try to receive a message (non-blocking)
 * @param port_id Port to receive from
 * @param msg Buffer to receive message
 * @return 0 on success, -1 if no message available
 */
int ipc_try_receive(uint64_t port_id, ipc_message_t* msg);

/**
 * Send and receive (call/reply pattern)
 * @param port_id Target port
 * @param request Request message
 * @param response Buffer for response
 * @return 0 on success, -1 on error
 */
int ipc_call(uint64_t port_id, ipc_message_t* request, ipc_message_t* response);

#endif // KERNEL_IPC_IPC_H

