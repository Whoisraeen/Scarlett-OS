//! Socket API Implementation
//!
//! Provides BSD-style socket interface for network applications

use crate::tcp;
use crate::udp;
use crate::ip;

/// Socket types
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u32)]
pub enum SocketType {
    Stream = 1,      // TCP
    Datagram = 2,    // UDP
    Raw = 3,         // Raw IP
}

/// Socket address families
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u16)]
pub enum AddressFamily {
    Inet = 2,        // IPv4
    Inet6 = 10,      // IPv6
}

/// Socket state
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SocketState {
    Closed,
    Bound,
    Listening,
    Connected,
    Error,
}

/// Socket address (IPv4)
#[repr(C)]
#[derive(Clone, Copy)]
pub struct SocketAddr {
    pub family: u16,
    pub port: u16,
    pub ip: u32,
    pub zero: [u8; 8],
}

impl SocketAddr {
    pub fn new(ip: u32, port: u16) -> Self {
        Self {
            family: AddressFamily::Inet as u16,
            port: port.to_be(),
            ip: ip.to_be(),
            zero: [0; 8],
        }
    }
}

/// Socket structure
pub struct Socket {
    pub socket_type: SocketType,
    pub state: SocketState,
    pub local_addr: SocketAddr,
    pub remote_addr: SocketAddr,
    pub protocol: u8,
    pub tcp_connection_id: Option<usize>,
    pub receive_buffer: [u8; 65536],
    pub receive_len: usize,
    pub send_buffer: [u8; 65536],
    pub send_len: usize,
}

impl Socket {
    pub fn new(socket_type: SocketType) -> Self {
        Self {
            socket_type,
            state: SocketState::Closed,
            local_addr: SocketAddr::new(0, 0),
            remote_addr: SocketAddr::new(0, 0),
            protocol: match socket_type {
                SocketType::Stream => ip::IP_PROTOCOL_TCP,
                SocketType::Datagram => ip::IP_PROTOCOL_UDP,
                SocketType::Raw => 0,
            },
            tcp_connection_id: None,
            receive_buffer: [0; 65536],
            receive_len: 0,
            send_buffer: [0; 65536],
            send_len: 0,
        }
    }
}

const MAX_SOCKETS: usize = 256;
static mut SOCKETS: [Option<Socket>; MAX_SOCKETS] = [None; MAX_SOCKETS];
static mut SOCKET_COUNT: usize = 0;

/// Create socket
pub fn socket_create(socket_type: SocketType) -> Result<usize, ()> {
    unsafe {
        // Find free socket slot
        for i in 0..MAX_SOCKETS {
            if SOCKETS[i].is_none() {
                SOCKETS[i] = Some(Socket::new(socket_type));
                SOCKET_COUNT += 1;
                return Ok(i);
            }
        }
    }

    Err(()) // No free sockets
}

/// Bind socket to address
pub fn socket_bind(socket_fd: usize, addr: SocketAddr) -> Result<(), ()> {
    unsafe {
        if socket_fd >= MAX_SOCKETS {
            return Err(());
        }

        if let Some(ref mut socket) = SOCKETS[socket_fd] {
            if socket.state != SocketState::Closed {
                return Err(()); // Already bound
            }

            socket.local_addr = addr;
            socket.state = SocketState::Bound;

            Ok(())
        } else {
            Err(())
        }
    }
}

/// Listen for connections (TCP only)
pub fn socket_listen(socket_fd: usize, backlog: u32) -> Result<(), ()> {
    unsafe {
        if socket_fd >= MAX_SOCKETS {
            return Err(());
        }

        if let Some(ref mut socket) = SOCKETS[socket_fd] {
            if socket.socket_type != SocketType::Stream {
                return Err(()); // Only TCP sockets can listen
            }

            if socket.state != SocketState::Bound {
                return Err(()); // Must be bound first
            }

            // Set up listen queue with backlog size
            // In full implementation, would allocate queue for pending connections
            // For now, just mark as listening
            let _ = backlog;

            socket.state = SocketState::Listening;

            Ok(())
        } else {
            Err(())
        }
    }
}

/// Connect socket to remote address
pub fn socket_connect(socket_fd: usize, addr: SocketAddr) -> Result<(), ()> {
    unsafe {
        if socket_fd >= MAX_SOCKETS {
            return Err(());
        }

        if let Some(ref mut socket) = SOCKETS[socket_fd] {
            socket.remote_addr = addr;

            match socket.socket_type {
                SocketType::Stream => {
                    // TCP connect
                    let local_ip = u32::from_be(socket.local_addr.ip);
                    let local_port = u16::from_be(socket.local_addr.port);
                    let remote_ip = u32::from_be(addr.ip);
                    let remote_port = u16::from_be(addr.port);

                    let conn_id = tcp::tcp_create_connection(
                        local_ip,
                        local_port,
                        remote_ip,
                        remote_port,
                    )?;

                    socket.tcp_connection_id = Some(conn_id);

                    // Initiate TCP handshake
                    tcp::tcp_connect(conn_id)?;

                    socket.state = SocketState::Connected;

                    Ok(())
                }
                SocketType::Datagram => {
                    // UDP "connect" just sets remote address
                    socket.state = SocketState::Connected;
                    Ok(())
                }
                SocketType::Raw => {
                    socket.state = SocketState::Connected;
                    Ok(())
                }
            }
        } else {
            Err(())
        }
    }
}

/// Accept incoming connection (TCP only)
pub fn socket_accept(socket_fd: usize) -> Result<(usize, SocketAddr), ()> {
    unsafe {
        if socket_fd >= MAX_SOCKETS {
            return Err(());
        }

        if let Some(ref socket) = SOCKETS[socket_fd] {
            if socket.socket_type != SocketType::Stream {
                return Err(());
            }

            if socket.state != SocketState::Listening {
                return Err(());
            }

            // Check listen queue for pending connections
            // In full implementation, would check queue and return next connection
            // For now, return error (no pending connections)
            // Full implementation would:
            // 1. Check if queue has pending connections
            // 2. Create new socket for accepted connection
            // 3. Return new socket FD
            Err(())
        } else {
            Err(())
        }
    }
}

/// Send data on socket
pub fn socket_send(socket_fd: usize, data: &[u8], flags: u32) -> Result<usize, ()> {
    unsafe {
        if socket_fd >= MAX_SOCKETS {
            return Err(());
        }

        if let Some(ref mut socket) = SOCKETS[socket_fd] {
            let _ = flags; // TODO: Handle flags

            match socket.socket_type {
                SocketType::Stream => {
                    // TCP send
                    if let Some(conn_id) = socket.tcp_connection_id {
                        tcp::tcp_send(conn_id, data)?;
                        Ok(data.len())
                    } else {
                        Err(())
                    }
                }
                SocketType::Datagram => {
                    // UDP send
                    let remote_ip = u32::from_be(socket.remote_addr.ip);
                    let remote_port = u16::from_be(socket.remote_addr.port);
                    let local_port = u16::from_be(socket.local_addr.port);

                    udp::udp_send(remote_ip, remote_port, local_port, data)?;
                    Ok(data.len())
                }
                SocketType::Raw => {
                    // Raw IP send
                    let remote_ip = u32::from_be(socket.remote_addr.ip);
                    ip::ip_send(remote_ip, socket.protocol, data)?;
                    Ok(data.len())
                }
            }
        } else {
            Err(())
        }
    }
}

/// Receive data from socket
pub fn socket_recv(socket_fd: usize, buffer: &mut [u8], flags: u32) -> Result<usize, ()> {
    unsafe {
        if socket_fd >= MAX_SOCKETS {
            return Err(());
        }

        if let Some(ref mut socket) = SOCKETS[socket_fd] {
            let _ = flags; // TODO: Handle flags

            match socket.socket_type {
                SocketType::Stream => {
                    // TCP receive
                    if let Some(conn_id) = socket.tcp_connection_id {
                        tcp::tcp_receive(conn_id, buffer)
                    } else {
                        Err(())
                    }
                }
                SocketType::Datagram => {
                    // UDP receive
                    udp::udp_receive(buffer).map(|(len, _, _, _)| len)
                }
                SocketType::Raw => {
                    // Raw IP receive
                    ip::ip_receive(buffer).map(|(len, _, _)| len)
                }
            }
        } else {
            Err(())
        }
    }
}

/// Send data to specific address (UDP)
pub fn socket_sendto(socket_fd: usize, data: &[u8], addr: SocketAddr, flags: u32) -> Result<usize, ()> {
    unsafe {
        if socket_fd >= MAX_SOCKETS {
            return Err(());
        }

        if let Some(ref socket) = SOCKETS[socket_fd] {
            let _ = flags;

            if socket.socket_type != SocketType::Datagram {
                return Err(());
            }

            let remote_ip = u32::from_be(addr.ip);
            let remote_port = u16::from_be(addr.port);
            let local_port = u16::from_be(socket.local_addr.port);

            udp::udp_send(remote_ip, remote_port, local_port, data)?;
            Ok(data.len())
        } else {
            Err(())
        }
    }
}

/// Receive data with source address (UDP)
pub fn socket_recvfrom(socket_fd: usize, buffer: &mut [u8], flags: u32) -> Result<(usize, SocketAddr), ()> {
    unsafe {
        if socket_fd >= MAX_SOCKETS {
            return Err(());
        }

        if let Some(ref socket) = SOCKETS[socket_fd] {
            let _ = flags;

            if socket.socket_type != SocketType::Datagram {
                return Err(());
            }

            let (len, src_ip, src_port, _) = udp::udp_receive(buffer)?;
            let addr = SocketAddr::new(src_ip, src_port);

            Ok((len, addr))
        } else {
            Err(())
        }
    }
}

/// Close socket
pub fn socket_close(socket_fd: usize) -> Result<(), ()> {
    unsafe {
        if socket_fd >= MAX_SOCKETS {
            return Err(());
        }

        if let Some(ref mut socket) = SOCKETS[socket_fd] {
            // Close TCP connection if present
            if let Some(conn_id) = socket.tcp_connection_id {
                tcp::tcp_close(conn_id)?;
            }

            SOCKETS[socket_fd] = None;
            SOCKET_COUNT -= 1;

            Ok(())
        } else {
            Err(())
        }
    }
}

/// Set socket option
pub fn socket_setsockopt(socket_fd: usize, level: u32, optname: u32, optval: &[u8]) -> Result<(), ()> {
    unsafe {
        if socket_fd >= MAX_SOCKETS {
            return Err(());
        }

        if SOCKETS[socket_fd].is_some() {
            // Implement socket options
            // Common options: SO_REUSEADDR, SO_KEEPALIVE, TCP_NODELAY, etc.
            // For now, just acknowledge (options not fully implemented)
            let _ = (level, optname, optval);
            Ok(())
        } else {
            Err(())
        }
    }
}

/// Get socket option
pub fn socket_getsockopt(socket_fd: usize, level: u32, optname: u32, optval: &mut [u8]) -> Result<usize, ()> {
    unsafe {
        if socket_fd >= MAX_SOCKETS {
            return Err(());
        }

        if SOCKETS[socket_fd].is_some() {
            // Implement socket options
            // Common options: SO_REUSEADDR, SO_KEEPALIVE, TCP_NODELAY, etc.
            // For now, return 0 (options not fully implemented)
            let _ = (level, optname, optval);
            Ok(0)
        } else {
            Err(())
        }
    }
}
