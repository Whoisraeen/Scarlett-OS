/**
 * @file ipc.h
 * @brief ScarlettOS IPC Interface
 */

#ifndef SCARLETTOS_IPC_H
#define SCARLETTOS_IPC_H

#include "types.h"

#define IPC_MAX_MSG_SIZE 4096

// IPC message structure
typedef struct {
    msg_id_t id;
    pid_t sender;
    port_t port;
    uint32_t size;
    uint8_t data[IPC_MAX_MSG_SIZE];
} ipc_msg_t;

// IPC functions
int ipc_create_port(port_t* port);
int ipc_destroy_port(port_t port);
int ipc_send(port_t port, const void* data, uint32_t size);
int ipc_recv(port_t port, ipc_msg_t* msg, uint32_t timeout_ms);
int ipc_reply(msg_id_t msg_id, const void* data, uint32_t size);

#endif // SCARLETTOS_IPC_H
