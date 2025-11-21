# TODO and Stub Fixes - Progress Report

## Status: In Progress

### ✅ Completed Fixes

1. **ARM64 Timer Scheduler Tick** (`kernel/hal/arm64/arm64_timer.c`)
   - ✅ Fixed: Added `scheduler_tick()` call in timer IRQ handler
   - Status: Complete

2. **ARM64 GIC IRQ Handler** (`kernel/hal/arm64/gic.c`)
   - ✅ Fixed: Added `irq_call_handlers()` call for registered IRQ handlers
   - Status: Complete (Note: `arm64_gic.c` has its own handler system)

3. **Time API Implementation** (`kernel/time.c`, `kernel/include/time.h`)
   - ✅ Created: `time_get_uptime_ms()` function using `timer_get_ms()`
   - Status: Complete

4. **Time Syscall** (`kernel/syscall/syscall.c`, `kernel/include/syscall/syscall.h`)
   - ✅ Added: `SYS_GET_UPTIME_MS` syscall (number 47)
   - ✅ Implemented: Syscall handler returns system uptime in milliseconds
   - Status: Complete

5. **Network Service Time TODOs** (`services/network/src/dns.rs`, `services/network/src/arp.rs`)
   - ✅ Fixed: DNS cache TTL checking with timestamp validation
   - ✅ Fixed: DNS cache timestamp setting using `sys_get_uptime_ms()`
   - ✅ Fixed: ARP cache timestamp setting using `sys_get_uptime_ms()`
   - ✅ Added: `sys_get_uptime_ms()` wrapper in `services/network/src/syscalls.rs`
   - Status: Complete

6. **Security Service Time TODO** (`services/security/src/capability.rs`)
   - ✅ Fixed: Capability timestamp setting using `sys_get_uptime_ms()`
   - ✅ Added: `sys_get_uptime_ms()` wrapper in `services/security/src/syscalls.rs`
   - Status: Complete

### 🔄 In Progress

3. **VFS Service TODOs** (`services/vfs/src/lib.rs`)
   - ⏳ Root filesystem mount
   - ⏳ Filesystem open/read/write operations
   - Status: Needs SFS integration

4. **Network Service TODOs** (`services/network/src/tcp.rs`)
   - ⏳ TCP packet building and sending
   - ⏳ TCP state machine
   - Status: Needs IP layer integration

### 📋 Remaining TODOs by Priority

#### High Priority (Critical Functionality)
- VFS root filesystem mount
- VFS file operations (open/read/write)
- TCP packet handling
- ARM64 exception handlers (sync, FIQ, SError)
- ARM64 device tree parsing

#### Medium Priority (Features)
- Audio driver TODOs (physical addresses, MMIO mapping)
- PCI driver TODOs (I/O port access)
- Compositor TODOs (IPC, framebuffer mapping)
- Window manager TODOs (IPC, service lookup)
- Editor TODOs (VFS integration, IPC)

#### Low Priority (Polish)
- Settings app TODOs (config file I/O)
- Login screen TODOs (IPC, authentication)
- Desktop TODOs (config file I/O)
- Test placeholder implementations

---

## Implementation Strategy

1. **Kernel Critical** ✅ (Done)
   - Timer and IRQ handlers

2. **Service Integration** (Next)
   - VFS filesystem operations
   - Network TCP/IP operations

3. **Driver Completion** (After services)
   - Audio driver MMIO/DMA
   - PCI I/O port access

4. **Application Integration** (Last)
   - IPC connections
   - VFS file operations
   - Config file handling

---

## Files Modified

**Kernel:**
- `kernel/hal/arm64/arm64_timer.c` - Added scheduler_tick()
- `kernel/hal/arm64/gic.c` - Added irq_call_handlers()
- `kernel/time.c` - Created time_get_uptime_ms() function
- `kernel/include/time.h` - Created time API header
- `kernel/syscall/syscall.c` - Added SYS_GET_UPTIME_MS handler
- `kernel/include/syscall/syscall.h` - Added SYS_GET_UPTIME_MS syscall number

**Services:**
- `services/network/src/syscalls.rs` - Added sys_get_uptime_ms() wrapper
- `services/network/src/dns.rs` - Fixed timestamp TODOs with TTL checking
- `services/network/src/arp.rs` - Fixed timestamp TODO
- `services/security/src/syscalls.rs` - Created syscalls module with sys_get_uptime_ms()
- `services/security/src/lib.rs` - Added syscalls module
- `services/security/src/capability.rs` - Fixed timestamp TODO

---

## Next Steps

1. Implement VFS root filesystem mount using SFS
2. Implement VFS file operations (open/read/write)
3. Implement TCP packet building and sending
4. Fix ARM64 exception handlers
5. Complete audio driver TODOs

