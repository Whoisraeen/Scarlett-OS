/**
 * @file dhcp.c
 * @brief DHCP client implementation
 */

#include "../include/net/dhcp.h"
#include "../include/net/udp.h"
#include "../include/net/ip.h"
#include "../include/net/network.h"
#include "../include/kprintf.h"
#include "../include/debug.h"
#include "../include/mm/heap.h"
#include "../include/string.h"
#include "../include/sync/spinlock.h"

// DHCP state
static struct {
    uint32_t next_xid;
    spinlock_t lock;
    bool initialized;
} dhcp_state = {0};

/**
 * Initialize DHCP client
 */
error_code_t dhcp_init(void) {
    if (dhcp_state.initialized) {
        return ERR_OK;
    }
    
    kinfo("Initializing DHCP client...\n");
    
    dhcp_state.next_xid = 1;
    spinlock_init(&dhcp_state.lock);
    dhcp_state.initialized = true;
    
    kinfo("DHCP client initialized\n");
    return ERR_OK;
}

/**
 * Add DHCP option to message
 */
static size_t dhcp_add_option(uint8_t* options, size_t offset, uint8_t code, const void* data, size_t data_len) {
    if (offset + 2 + data_len >= 312) {
        return offset;  // No space
    }
    
    options[offset++] = code;
    options[offset++] = (uint8_t)data_len;
    if (data && data_len > 0) {
        memcpy(options + offset, data, data_len);
        offset += data_len;
    }
    
    return offset;
}

/**
 * Request DHCP configuration
 */
error_code_t dhcp_request_config(net_device_t* device, dhcp_config_t* config) {
    if (!device || !config) {
        return ERR_INVALID_ARG;
    }
    
    if (!dhcp_state.initialized) {
        dhcp_init();
    }
    
    kinfo("DHCP: Requesting configuration for device %s\n", device->name);
    
    // Create DHCP DISCOVER message
    dhcp_message_t* discover = (dhcp_message_t*)kmalloc(sizeof(dhcp_message_t));
    if (!discover) {
        return ERR_OUT_OF_MEMORY;
    }
    
    memset(discover, 0, sizeof(dhcp_message_t));
    
    spinlock_lock(&dhcp_state.lock);
    discover->xid = dhcp_state.next_xid++;
    spinlock_unlock(&dhcp_state.lock);
    
    discover->op = 1;  // BOOTREQUEST
    discover->htype = 1;  // Ethernet
    discover->hlen = 6;   // MAC address length
    discover->hops = 0;
    discover->secs = 0;
    discover->flags = 0x8000;  // Broadcast flag
    
    // Client hardware address
    memcpy(discover->chaddr, device->mac_address, 6);
    
    // Magic cookie (99.130.83.99)
    discover->options[0] = 99;
    discover->options[1] = 130;
    discover->options[2] = 83;
    discover->options[3] = 99;
    
    // DHCP message type: DISCOVER
    size_t opt_offset = 4;
    uint8_t msg_type = DHCP_DISCOVER;
    opt_offset = dhcp_add_option(discover->options, opt_offset, DHCP_OPT_MESSAGE_TYPE, &msg_type, 1);
    
    // Parameter request list
    uint8_t param_list[] = {DHCP_OPT_SUBNET_MASK, DHCP_OPT_ROUTER, DHCP_OPT_DNS_SERVER};
    opt_offset = dhcp_add_option(discover->options, opt_offset, DHCP_OPT_PARAMETER_REQUEST, param_list, sizeof(param_list));
    
    // End option
    discover->options[opt_offset++] = DHCP_OPT_END;
    
    // Send DISCOVER to broadcast
    uint32_t broadcast_ip = 0xFFFFFFFF;  // 255.255.255.255
    error_code_t err = udp_send(broadcast_ip, DHCP_SERVER_PORT, DHCP_CLIENT_PORT, discover, sizeof(dhcp_message_t));
    
    if (err != ERR_OK) {
        kerror("DHCP: Failed to send DISCOVER\n");
        kfree(discover);
        return err;
    }
    
    kinfo("DHCP: DISCOVER sent, waiting for OFFER...\n");
    
    // Wait for OFFER (simplified - would use proper timeout and retry)
    // For now, we'll use a simple timeout loop
    uint8_t* response = (uint8_t*)kmalloc(sizeof(dhcp_message_t));
    if (!response) {
        kfree(discover);
        return ERR_OUT_OF_MEMORY;
    }
    
    // TODO: Implement proper receive loop with timeout
    // For now, return error indicating manual configuration needed
    kfree(discover);
    kfree(response);
    
    kinfo("DHCP: Manual configuration required (receive loop not implemented)\n");
    return ERR_NOT_IMPLEMENTED;
}

/**
 * Release DHCP configuration
 */
error_code_t dhcp_release_config(net_device_t* device) {
    if (!device) {
        return ERR_INVALID_ARG;
    }
    
    if (!dhcp_state.initialized) {
        return ERR_NOT_INITIALIZED;
    }
    
    kinfo("DHCP: Releasing configuration for device %s\n", device->name);
    
    // TODO: Send DHCP RELEASE message
    return ERR_NOT_IMPLEMENTED;
}

