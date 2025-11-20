/**
 * @file dhcp.h
 * @brief DHCP client interface
 */

#ifndef KERNEL_NET_DHCP_H
#define KERNEL_NET_DHCP_H

#include "../types.h"
#include "../errors.h"

// DHCP ports
#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

// DHCP message types
#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST 3
#define DHCP_DECLINE 4
#define DHCP_ACK     5
#define DHCP_NAK     6
#define DHCP_RELEASE 7
#define DHCP_INFORM  8

// DHCP options
#define DHCP_OPT_PAD               0
#define DHCP_OPT_SUBNET_MASK       1
#define DHCP_OPT_ROUTER            3
#define DHCP_OPT_DNS_SERVER        6
#define DHCP_OPT_HOSTNAME          12
#define DHCP_OPT_DOMAIN_NAME       15
#define DHCP_OPT_REQUESTED_IP       50
#define DHCP_OPT_LEASE_TIME        51
#define DHCP_OPT_MESSAGE_TYPE      53
#define DHCP_OPT_SERVER_ID         54
#define DHCP_OPT_PARAMETER_REQUEST  55
#define DHCP_OPT_END               255

// DHCP message structure
typedef struct {
    uint8_t op;          // Message op code (1 = BOOTREQUEST, 2 = BOOTREPLY)
    uint8_t htype;       // Hardware address type (1 = Ethernet)
    uint8_t hlen;        // Hardware address length (6 for Ethernet)
    uint8_t hops;        // Hops (set to 0 by client)
    uint32_t xid;        // Transaction ID
    uint16_t secs;       // Seconds elapsed since client began address acquisition
    uint16_t flags;      // Flags
    uint32_t ciaddr;     // Client IP address
    uint32_t yiaddr;     // Your IP address
    uint32_t siaddr;     // Server IP address
    uint32_t giaddr;     // Gateway IP address
    uint8_t chaddr[16];  // Client hardware address
    uint8_t sname[64];   // Server host name
    uint8_t file[128];   // Boot file name
    uint8_t options[312]; // Options
} __attribute__((packed)) dhcp_message_t;

// DHCP configuration
typedef struct {
    uint32_t ip_address;
    uint32_t subnet_mask;
    uint32_t gateway;
    uint32_t dns_server;
    uint32_t lease_time;
    bool configured;
} dhcp_config_t;

// DHCP functions
error_code_t dhcp_init(void);
error_code_t dhcp_request_config(net_device_t* device, dhcp_config_t* config);
error_code_t dhcp_release_config(net_device_t* device);

#endif // KERNEL_NET_DHCP_H

