/**
 * @file tcp.h
 * @brief TCP protocol definitions
 */

#ifndef KERNEL_NET_TCP_H
#define KERNEL_NET_TCP_H

#include "../types.h"
#include "../errors.h"

// TCP flags
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

// TCP states
typedef enum {
    TCP_STATE_CLOSED,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECEIVED,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_CLOSING,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT
} tcp_state_t;

// TCP connection structure
typedef struct {
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    tcp_state_t state;
    uint32_t seq_num;      // Sequence number
    uint32_t ack_num;      // Acknowledgment number
    uint32_t window_size;  // Receive window size
    void* receive_buffer;  // Receive buffer
    size_t receive_buffer_size;
    size_t receive_buffer_pos;
    void* user_data;
} tcp_connection_t;

// TCP header structure
typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_number;
    uint32_t ack_number;
    uint8_t data_offset;   // Data offset (4 bits) + reserved (4 bits)
    uint8_t flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
    uint8_t options[];
} __attribute__((packed)) tcp_header_t;

// TCP functions
error_code_t tcp_init(void);
tcp_connection_t* tcp_create_connection(uint32_t local_ip, uint16_t local_port, uint32_t remote_ip, uint16_t remote_port);
error_code_t tcp_send(tcp_connection_t* conn, void* data, size_t len);
error_code_t tcp_receive(tcp_connection_t* conn, void* buffer, size_t* len);
error_code_t tcp_handle_packet(void* buffer, size_t len, uint32_t src_ip);
error_code_t tcp_close(tcp_connection_t* conn);
error_code_t tcp_send_packet(tcp_connection_t* conn, uint8_t flags, void* data, size_t data_len);

#endif // KERNEL_NET_TCP_H

