/**
 * @file capability.h
 * @brief Capability-based security interface
 */

#ifndef KERNEL_SECURITY_CAPABILITY_H
#define KERNEL_SECURITY_CAPABILITY_H

#include "../types.h"
#include "../ipc/ipc.h"

// Capability types
#define CAP_TYPE_IPC_PORT    1
#define CAP_TYPE_MEMORY      2
#define CAP_TYPE_FILE        3
#define CAP_TYPE_DEVICE      4
#define CAP_TYPE_SERVICE     5

// Capability rights
#define CAP_RIGHT_READ        (1 << 0)
#define CAP_RIGHT_WRITE       (1 << 1)
#define CAP_RIGHT_EXECUTE     (1 << 2)
#define CAP_RIGHT_DELETE      (1 << 3)
#define CAP_RIGHT_TRANSFER    (1 << 4)

/**
 * Initialize capability system
 */
void capability_init(void);

/**
 * Create a new capability
 */
uint64_t capability_create(uint32_t type, uint64_t resource_id, uint32_t rights);

/**
 * Check if capability grants right
 */
bool capability_check(uint64_t cap_id, uint32_t right);

/**
 * Transfer capability in IPC message
 */
int capability_transfer(ipc_message_t* msg, uint64_t cap_id);

/**
 * Revoke capability
 */
int capability_revoke(uint64_t cap_id);

/**
 * Find capability for a port (helper for IPC)
 */
uint64_t capability_find_for_port(uint64_t port_id);

#endif // KERNEL_SECURITY_CAPABILITY_H

