# Migration Guide: Kernel to User-Space

**Date:** 2025-01-27  
**Status:** In Progress

---

## Overview

This guide documents the migration of remaining kernel-space logic to user-space services, specifically filesystem drivers and the TCP/IP stack.

---

## Filesystem Driver Migration

### Current State

**Location:** `kernel/fs/fat32.c`  
**Status:** Kernel-space implementation

### Target State

**Location:** `drivers/storage/fat32/` (Rust)  
**Status:** User-space driver

### Migration Steps

1. **Create Rust Driver Structure**
   ```rust
   drivers/storage/fat32/
   ├── Cargo.toml
   ├── src/
   │   ├── main.rs
   │   ├── lib.rs
   │   ├── fat32.rs
   │   └── block.rs
   ```

2. **Migrate Data Structures**
   - `fat32_boot_sector_t` → Rust struct
   - `fat32_fs_t` → Rust struct
   - `fat32_file_t` → Rust struct

3. **Migrate Functions**
   - `fat32_init()` → `fat32::init()`
   - `fat32_mount()` → `fat32::mount()`
   - `fat32_open()` → `fat32::open()`
   - `fat32_read()` → `fat32::read()`
   - `fat32_write()` → `fat32::write()`
   - `fat32_close()` → `fat32::close()`

4. **Block Device Interface**
   - Create IPC interface to block device driver
   - Use syscalls for block device access
   - Handle block device capabilities

5. **VFS Integration**
   - Register filesystem with VFS service
   - Implement VFS filesystem operations trait
   - Handle mount/unmount requests

### Implementation Notes

- **Block Device Access:** Use IPC to communicate with block device driver
- **Memory Management:** Use user-space allocators
- **Error Handling:** Convert C error codes to Rust Result types
- **Thread Safety:** Use Rust's ownership system for safety

---

## TCP/IP Stack Migration

### Current State

**Location:** `kernel/net/`  
**Status:** Kernel-space implementation

### Target State

**Location:** `services/network/src/` (Rust)  
**Status:** User-space service

### Migration Steps

1. **Migrate Core Protocols**
   - `kernel/net/ip.c` → `services/network/src/ip.rs`
   - `kernel/net/tcp.c` → `services/network/src/tcp.rs`
   - `kernel/net/udp.c` → `services/network/src/udp.rs`
   - `kernel/net/ethernet.c` → `services/network/src/ethernet.rs`

2. **Migrate Data Structures**
   - `ip_header_t` → Rust struct
   - `tcp_header_t` → Rust struct
   - `tcp_connection_t` → Rust struct
   - `udp_header_t` → Rust struct

3. **Migrate Functions**
   - `ip_send()` → `ip::send()`
   - `ip_receive()` → `ip::receive()`
   - `tcp_connect()` → `tcp::connect()`
   - `tcp_listen()` → `tcp::listen()`
   - `tcp_accept()` → `tcp::accept()`
   - `tcp_send()` → `tcp::send()`
   - `tcp_receive()` → `tcp::receive()`

4. **Network Device Integration**
   - Use IPC to communicate with network drivers
   - Handle network device capabilities
   - Process packets from drivers

5. **Socket API**
   - Implement socket creation
   - Handle bind, listen, accept, connect
   - Implement send/receive operations

### Implementation Notes

- **Packet Buffers:** Use DMA-allocated buffers for network packets
- **Interrupt Handling:** Register IRQ handlers for network devices
- **Thread Safety:** Use Rust's concurrency primitives
- **Memory Management:** Use user-space allocators for packet buffers

---

## Migration Checklist

### Filesystem Drivers

- [ ] Create FAT32 driver structure
- [ ] Migrate FAT32 data structures
- [ ] Migrate FAT32 functions
- [ ] Implement block device IPC interface
- [ ] Integrate with VFS service
- [ ] Test file operations
- [ ] Migrate other filesystems (ext2, etc.)

### TCP/IP Stack

- [ ] Migrate IP protocol
- [ ] Migrate TCP protocol
- [ ] Migrate UDP protocol
- [ ] Migrate Ethernet protocol
- [ ] Implement socket API
- [ ] Integrate with network service
- [ ] Test network operations
- [ ] Test TCP/UDP connections

---

## Testing Strategy

### Unit Tests

1. **Filesystem Tests**
   - Test FAT32 mount/unmount
   - Test file open/read/write/close
   - Test directory operations
   - Test error handling

2. **Network Tests**
   - Test IP packet send/receive
   - Test TCP connection establishment
   - Test TCP data transfer
   - Test UDP send/receive

### Integration Tests

1. **Service Communication**
   - Test VFS ↔ Filesystem driver communication
   - Test Network ↔ Network driver communication
   - Test capability enforcement

2. **End-to-End Tests**
   - Test file operations through VFS
   - Test network operations through socket API
   - Test service restart and recovery

### QEMU Tests

1. **Boot Tests**
   - Test kernel boot
   - Test service startup
   - Test service registration

2. **Functional Tests**
   - Test file system operations
   - Test network operations
   - Test IPC communication

---

## Performance Considerations

### Filesystem

- **Caching:** Implement filesystem cache in user-space
- **Buffering:** Use buffered I/O for better performance
- **Async Operations:** Consider async I/O for better concurrency

### Network

- **Zero-Copy:** Use zero-copy techniques for packet handling
- **Batching:** Batch multiple packets for better throughput
- **Polling:** Consider polling for high-performance scenarios

---

## Security Considerations

### Capabilities

- **Filesystem Access:** Require capabilities for filesystem operations
- **Network Access:** Require capabilities for network operations
- **Device Access:** Require capabilities for device access

### Validation

- **Input Validation:** Validate all inputs from user-space
- **Bounds Checking:** Check all buffer bounds
- **Resource Limits:** Enforce resource limits

---

## Next Steps

1. **Start with FAT32 Driver**
   - Create basic structure
   - Migrate data structures
   - Migrate core functions
   - Test with VFS service

2. **Migrate TCP/IP Stack**
   - Start with IP protocol
   - Add TCP protocol
   - Add UDP protocol
   - Test with network service

3. **Complete Testing**
   - Unit tests
   - Integration tests
   - QEMU tests

---

*Last Updated: 2025-01-27*

