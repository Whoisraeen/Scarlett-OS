//! TCP protocol implementation

use core::mem;

/// TCP header structure
#[repr(C, packed)]
pub struct TcpHeader {
    pub src_port: u16,
    pub dest_port: u16,
    pub seq_number: u32,
    pub ack_number: u32,
    pub data_offset: u8,      // Data offset (4 bits) + reserved (4 bits)
    pub flags: u8,
    pub window_size: u16,
    pub checksum: u16,
    pub urgent_ptr: u16,
    pub options: [u8; 0],      // Variable length options
}

/// TCP flags
pub const TCP_FLAG_FIN: u8 = 0x01;
pub const TCP_FLAG_SYN: u8 = 0x02;
pub const TCP_FLAG_RST: u8 = 0x04;
pub const TCP_FLAG_PSH: u8 = 0x08;
pub const TCP_FLAG_ACK: u8 = 0x10;
pub const TCP_FLAG_URG: u8 = 0x20;

/// TCP states
#[repr(u32)]
pub enum TcpState {
    Closed = 0,
    Listen = 1,
    SynSent = 2,
    SynReceived = 3,
    Established = 4,
    FinWait1 = 5,
    FinWait2 = 6,
    CloseWait = 7,
    Closing = 8,
    LastAck = 9,
    TimeWait = 10,
}

/// TCP connection
pub struct TcpConnection {
    pub local_ip: u32,
    pub remote_ip: u32,
    pub local_port: u16,
    pub remote_port: u16,
    pub state: TcpState,
    pub seq_num: u32,
    pub ack_num: u32,
    pub window_size: u32,
}

const MAX_TCP_CONNECTIONS: usize = 32;
static mut TCP_CONNECTIONS: [Option<TcpConnection>; MAX_TCP_CONNECTIONS] = [None; MAX_TCP_CONNECTIONS];
static mut NEXT_SEQ_NUM: u32 = 1;
static mut INITIALIZED: bool = false;

/// Initialize TCP
pub fn tcp_init() -> Result<(), ()> {
    unsafe {
        if INITIALIZED {
            return Ok(());
        }
        
        NEXT_SEQ_NUM = 1;
        INITIALIZED = true;
        
        Ok(())
    }
}

/// Create TCP connection
pub fn tcp_create_connection(local_ip: u32, local_port: u16, remote_ip: u32, remote_port: u16) -> Result<usize, ()> {
    unsafe {
        if !INITIALIZED {
            tcp_init()?;
        }
        
        // Find free slot
        for i in 0..MAX_TCP_CONNECTIONS {
            if TCP_CONNECTIONS[i].is_none() {
                TCP_CONNECTIONS[i] = Some(TcpConnection {
                    local_ip,
                    remote_ip,
                    local_port,
                    remote_port,
                    state: TcpState::Closed,
                    seq_num: NEXT_SEQ_NUM,
                    ack_num: 0,
                    window_size: 65535,
                });
                NEXT_SEQ_NUM += 1;
                return Ok(i);
            }
        }

        Err(())
    }
}

/// Initiate TCP connection (SYN)
pub fn tcp_connect(conn_id: usize) -> Result<(), ()> {
    unsafe {
        if conn_id >= MAX_TCP_CONNECTIONS {
            return Err(());
        }

        if let Some(ref mut conn) = TCP_CONNECTIONS[conn_id] {
            conn.state = TcpState::SynSent;
            // Send SYN packet
            let mut tcp_header = TcpHeader {
                src_port: conn.local_port,
                dest_port: conn.remote_port,
                seq_number: conn.seq_num,
                ack_number: 0,
                data_offset: 0x50, // 5 * 4 = 20 bytes header
                flags: TCP_FLAG_SYN,
                window_size: conn.window_size as u16,
                checksum: 0,
                urgent_ptr: 0,
            };
            // Calculate checksum and send via IP layer
            use crate::ip::ip_send;
            let header_bytes = unsafe {
                core::slice::from_raw_parts(&tcp_header as *const _ as *const u8, 20)
            };
            let _ = ip_send(conn.remote_ip, crate::ip::IP_PROTOCOL_TCP, header_bytes);
            Ok(())
        } else {
            Err(())
        }
    }
}

/// Send data on TCP connection
pub fn tcp_send(conn_id: usize, data: &[u8]) -> Result<(), ()> {
    unsafe {
        if conn_id >= MAX_TCP_CONNECTIONS {
            return Err(());
        }

        if let Some(ref conn) = TCP_CONNECTIONS[conn_id] {
            if conn.state != TcpState::Established {
                return Err(());
            }

            // Build and send TCP segment
            let mut tcp_header = TcpHeader {
                src_port: conn.local_port,
                dest_port: conn.remote_port,
                seq_number: conn.seq_num,
                ack_number: conn.ack_num,
                data_offset: 0x50,
                flags: TCP_FLAG_ACK | TCP_FLAG_PSH,
                window_size: conn.window_size as u16,
                checksum: 0,
                urgent_ptr: 0,
            };
            // Build packet: header + data
            let mut packet = [0u8; 1500];
            unsafe {
                core::ptr::copy_nonoverlapping(&tcp_header as *const _ as *const u8, packet.as_mut_ptr(), 20);
            }
            let data_len = data.len().min(1480);
            packet[20..20+data_len].copy_from_slice(&data[0..data_len]);
            // Send via IP layer
            use crate::ip::ip_send;
            let _ = ip_send(conn.remote_ip, crate::ip::IP_PROTOCOL_TCP, &packet[0..20+data_len]);
            Ok(())
        } else {
            Err(())
        }
    }
}

/// Receive data from TCP connection
pub fn tcp_receive(conn_id: usize, buffer: &mut [u8]) -> Result<usize, ()> {
    unsafe {
        if conn_id >= MAX_TCP_CONNECTIONS {
            return Err(());
        }

        if let Some(ref conn) = TCP_CONNECTIONS[conn_id] {
            if conn.state != TcpState::Established {
                return Err(());
            }

            // Retrieve data from receive buffer
            // For now, return 0 (receive buffer not fully implemented)
            // Full implementation would:
            // 1. Check receive buffer for data
            // 2. Copy data to buffer
            // 3. Update ack_num
            let _ = buffer;
            Ok(0)
        } else {
            Err(())
        }
    }
}

/// Close TCP connection
pub fn tcp_close(conn_id: usize) -> Result<(), ()> {
    unsafe {
        if conn_id >= MAX_TCP_CONNECTIONS {
            return Err(());
        }

        if let Some(ref mut conn) = TCP_CONNECTIONS[conn_id] {
            conn.state = TcpState::FinWait1;
            // Send FIN packet
            let mut tcp_header = TcpHeader {
                src_port: conn.local_port,
                dest_port: conn.remote_port,
                seq_number: conn.seq_num,
                ack_number: conn.ack_num,
                data_offset: 0x50,
                flags: TCP_FLAG_FIN | TCP_FLAG_ACK,
                window_size: conn.window_size as u16,
                checksum: 0,
                urgent_ptr: 0,
            };
            use crate::ip::ip_send;
            let header_bytes = unsafe {
                core::slice::from_raw_parts(&tcp_header as *const _ as *const u8, 20)
            };
            let _ = ip_send(conn.remote_ip, crate::ip::IP_PROTOCOL_TCP, header_bytes);
            TCP_CONNECTIONS[conn_id] = None;
            Ok(())
        } else {
            Err(())
        }
    }
}

/// Send TCP data
pub fn tcp_send(conn_idx: usize, data: &[u8]) -> Result<(), ()> {
    unsafe {
        if conn_idx >= MAX_TCP_CONNECTIONS {
            return Err(());
        }
        
        if let Some(ref mut conn) = TCP_CONNECTIONS[conn_idx] {
            // Build TCP packet
            let mut tcp_header = TcpHeader {
                src_port: conn.local_port,
                dest_port: conn.remote_port,
                seq_number: conn.seq_num,
                ack_number: conn.ack_num,
                data_offset: 0x50,
                flags: TCP_FLAG_ACK | TCP_FLAG_PSH,
                window_size: conn.window_size as u16,
                checksum: 0,
                urgent_ptr: 0,
            };
            // Build packet
            let mut packet = [0u8; 1500];
            unsafe {
                core::ptr::copy_nonoverlapping(&tcp_header as *const _ as *const u8, packet.as_mut_ptr(), 20);
            }
            let data_len = data.len().min(1480);
            packet[20..20+data_len].copy_from_slice(&data[0..data_len]);
            // Send via IP layer
            use crate::ip::ip_send;
            let _ = ip_send(conn.remote_ip, crate::ip::IP_PROTOCOL_TCP, &packet[0..20+data_len]);
            Ok(())
        } else {
            Err(())
        }
    }
}

/// Receive TCP data
pub fn tcp_receive(conn_idx: usize, buffer: &mut [u8]) -> Result<usize, ()> {
    unsafe {
        if conn_idx >= MAX_TCP_CONNECTIONS {
            return Err(());
        }
        
        if let Some(_conn) = &TCP_CONNECTIONS[conn_idx] {
            // Receive from IP layer
            use crate::ip::ip_receive;
            let mut ip_buffer = [0u8; 1500];
            match ip_receive(&mut ip_buffer) {
                Ok((len, _src_ip, protocol)) => {
                    if protocol == crate::ip::IP_PROTOCOL_TCP && len >= 20 {
                        // Parse TCP header
                        let tcp_header = unsafe {
                            &*(ip_buffer.as_ptr() as *const TcpHeader)
                        };
                        // Copy data to buffer
                        let data_len = (len - 20).min(buffer.len());
                        buffer[0..data_len].copy_from_slice(&ip_buffer[20..20+data_len]);
                        Ok(data_len)
                    } else {
                        Ok(0)
                    }
                }
                Err(_) => Ok(0)
            }
        } else {
            Err(())
        }
    }
}

/// Handle TCP packet
pub fn tcp_handle_packet(buffer: &[u8], src_ip: u32) -> Result<(), ()> {
    // Parse TCP header
    if buffer.len() < 20 {
        return Err(());
    }
    let tcp_header = unsafe {
        &*(buffer.as_ptr() as *const TcpHeader)
    };
    
    // Find or create connection
    let mut conn_idx = None;
    unsafe {
        for i in 0..MAX_TCP_CONNECTIONS {
            if let Some(ref conn) = TCP_CONNECTIONS[i] {
                if conn.local_port == tcp_header.dest_port && 
                   conn.remote_ip == src_ip &&
                   conn.remote_port == tcp_header.src_port {
                    conn_idx = Some(i);
                    break;
                }
            }
        }
    }
    
    // Update connection state
    if let Some(idx) = conn_idx {
        unsafe {
            if let Some(ref mut conn) = TCP_CONNECTIONS[idx] {
                // Handle TCP state machine
                match conn.state {
                    TcpState::SynSent => {
                        if (tcp_header.flags & TCP_FLAG_SYN) != 0 && (tcp_header.flags & TCP_FLAG_ACK) != 0 {
                            conn.state = TcpState::Established;
                            conn.ack_num = tcp_header.seq_number + 1;
                        }
                    }
                    TcpState::Established => {
                        if (tcp_header.flags & TCP_FLAG_FIN) != 0 {
                            conn.state = TcpState::CloseWait;
                        } else if (tcp_header.flags & TCP_FLAG_ACK) != 0 {
                            conn.ack_num = tcp_header.seq_number;
                        }
                    }
                    _ => {}
                }
            }
        }
    }
    
    Ok(())
}

