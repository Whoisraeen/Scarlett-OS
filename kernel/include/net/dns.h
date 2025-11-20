/**
 * @file dns.h
 * @brief DNS resolver interface
 */

#ifndef KERNEL_NET_DNS_H
#define KERNEL_NET_DNS_H

#include "../types.h"
#include "../errors.h"

// DNS port
#define DNS_PORT 53

// DNS message structure
typedef struct {
    uint16_t id;           // Transaction ID
    uint16_t flags;        // Flags (QR, Opcode, AA, TC, RD, RA, Z, RCODE)
    uint16_t qdcount;      // Question count
    uint16_t ancount;      // Answer count
    uint16_t nscount;      // Authority count
    uint16_t arcount;      // Additional count
    uint8_t data[];        // Questions and answers
} __attribute__((packed)) dns_header_t;

// DNS question structure
typedef struct {
    uint8_t* qname;        // Domain name (encoded)
    uint16_t qtype;        // Question type
    uint16_t qclass;       // Question class
} dns_question_t;

// DNS resource record structure
typedef struct {
    uint8_t* name;         // Domain name (encoded)
    uint16_t type;         // Record type
    uint16_t class;        // Record class
    uint32_t ttl;          // Time to live
    uint16_t rdlength;     // Data length
    uint8_t* rdata;        // Record data
} dns_rr_t;

// DNS record types
#define DNS_TYPE_A         1   // IPv4 address
#define DNS_TYPE_AAAA      28  // IPv6 address
#define DNS_TYPE_CNAME     5   // Canonical name
#define DNS_TYPE_MX        15  // Mail exchange
#define DNS_TYPE_TXT       16  // Text record

// DNS classes
#define DNS_CLASS_IN       1   // Internet

// DNS flags
#define DNS_FLAG_QR        0x8000  // Query/Response
#define DNS_FLAG_OPCODE    0x7800  // Opcode mask
#define DNS_FLAG_AA        0x0400  // Authoritative Answer
#define DNS_FLAG_TC        0x0200  // Truncated
#define DNS_FLAG_RD        0x0100  // Recursion Desired
#define DNS_FLAG_RA        0x0080  // Recursion Available
#define DNS_FLAG_RCODE     0x000F  // Response code mask

// DNS response codes
#define DNS_RCODE_NOERROR  0
#define DNS_RCODE_FORMERR  1
#define DNS_RCODE_SERVFAIL 2
#define DNS_RCODE_NXDOMAIN 3
#define DNS_RCODE_NOTIMP   4
#define DNS_RCODE_REFUSED  5

// DNS functions
error_code_t dns_init(void);
error_code_t dns_resolve(const char* hostname, uint32_t* ip_address);
error_code_t dns_resolve_ipv6(const char* hostname, uint8_t* ipv6_address);
error_code_t dns_set_nameserver(uint32_t nameserver_ip);

#endif // KERNEL_NET_DNS_H

