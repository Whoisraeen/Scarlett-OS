# Technical Architecture Specification

## Document Overview

This document provides detailed technical specifications for the operating system architecture, including microkernel design, IPC mechanisms, security model, and component interfaces.

---

## 1. Microkernel Architecture

### 1.1 Design Rationale

**Why Microkernel:**
- Fault isolation: Driver crashes don't crash kernel
- Security: Minimal trusted computing base (TCB)
- Modularity: Easy to add/remove/update components
- Flexibility: Services can be restarted without reboot
- Debugging: User-space debugging tools work for services

**Trade-offs:**
- Performance overhead from IPC
- More complex than monolithic design
- Requires careful IPC optimization

**Mitigation Strategies:**
- Zero-copy IPC where possible
- Shared memory for bulk data transfer
- Optimized message passing paths
- CPU cache-aware data structures

### 1.2 Kernel Space Components

#### 1.2.1 Address Space Manager

**Responsibilities:**
- Virtual address space creation/destruction
- Page table management
- Memory mapping/unmapping
- Page fault handling
- TLB management

**Key Data Structures:**
```c
typedef struct address_space {
    arch_page_table_t *page_table;  // Architecture-specific
    uint64_t asid;                   // Address Space ID
    list_head_t regions;             // Memory regions
    spinlock_t lock;
    uint64_t flags;
} address_space_t;

typedef struct memory_region {
    uint64_t start;
    uint64_t end;
    uint32_t permissions;  // R/W/X
    uint32_t type;         // Anonymous, File-backed, Shared
    void *backing_object;  // File or shared memory object
    list_head_t list;
} memory_region_t;
```

**Operations:**
- `as_create()` - Create new address space
- `as_destroy()` - Destroy address space
- `as_map()` - Map virtual to physical
- `as_unmap()` - Unmap region
- `as_protect()` - Change permissions
- `as_switch()` - Switch to different address space

#### 1.2.2 Thread Scheduler

**Design:** Priority-based preemptive scheduler with multiple scheduling classes

**Scheduling Classes:**
1. Real-time (priority 0-99)
2. Normal (priority 100-139)
3. Idle (priority 140)

**Key Data Structures:**
```c
typedef struct thread {
    uint64_t tid;
    uint32_t priority;
    uint32_t sched_class;
    cpu_context_t context;         // Register state
    void *kernel_stack;
    address_space_t *addr_space;
    uint32_t state;                // Running, Ready, Blocked
    list_head_t sched_list;
    uint64_t time_slice;
    uint64_t cpu_time;
    uint32_t cpu_affinity;
} thread_t;

typedef struct runqueue {
    spinlock_t lock;
    bitmap_t priority_bitmap;      // Quick priority lookup
    list_head_t queues[140];       // Per-priority queues
    thread_t *current;
    uint64_t nr_running;
} runqueue_t;
```

**Per-CPU Run Queues:**
- Each CPU has its own runqueue
- Load balancing across CPUs
- CPU affinity support
- Minimal lock contention

**Scheduling Algorithm:**
1. Pick highest priority non-empty queue
2. Round-robin within same priority
3. Preempt on timer tick if higher priority runnable
4. Voluntary yield on IPC wait

**Load Balancing:**
- Periodic balancing (every 100ms)
- On idle CPU, steal work from busiest CPU
- Respect CPU affinity
- Balance across NUMA nodes

#### 1.2.3 IPC Subsystem

**IPC Primitives:**
1. **Synchronous Send/Receive** (rendezvous)
2. **Asynchronous Notifications**
3. **Shared Memory**

**Message Structure:**
```c
typedef struct message {
    uint64_t sender_tid;
    uint64_t msg_id;
    uint32_t inline_size;         // Inline data size
    uint32_t num_caps;            // Number of capabilities
    uint64_t inline_data[8];      // 64 bytes inline
    capability_t caps[4];         // Transferred capabilities
    void *buffer;                 // Out-of-line data (optional)
    size_t buffer_size;
} message_t;

typedef struct capability {
    uint64_t cap_id;
    uint32_t type;                // Memory, Port, Thread, etc.
    uint32_t rights;              // Read, Write, Execute, etc.
    void *object_ptr;             // Kernel object reference
    uint64_t badge;               // Capability badge for identification
} capability_t;
```

**IPC Operations:**
- `ipc_send(target_tid, message)` - Synchronous send
- `ipc_receive(message)` - Blocking receive
- `ipc_call(target_tid, request, reply)` - Send + Receive
- `ipc_notify(target_tid, notification)` - Async signal
- `ipc_wait_notification()` - Wait for notification

**IPC Optimization:**
- Short messages use inline data (64 bytes)
- Larger messages use shared memory + handle passing
- Fast path for common cases
- Kernel buffers to avoid double-copy

**Zero-Copy IPC:**
For large data transfers:
1. Sender maps pages into shared region
2. IPC transfers page ownership via capability
3. Receiver accesses pages directly
4. No data copying required

#### 1.2.4 Interrupt Handling

**Interrupt Flow:**
```
Hardware Interrupt
    ↓
CPU vectors to IDT entry (assembly)
    ↓
Save CPU context
    ↓
Kernel interrupt handler
    ↓
Acknowledge interrupt
    ↓
Send notification to user-space driver
    ↓
Restore context / Schedule if needed
    ↓
Return from interrupt
```

**Interrupt Controller Abstraction:**
```c
typedef struct irq_controller_ops {
    void (*init)(void);
    void (*enable_irq)(uint32_t irq);
    void (*disable_irq)(uint32_t irq);
    void (*ack_irq)(uint32_t irq);
    void (*eoi_irq)(uint32_t irq);
    void (*set_affinity)(uint32_t irq, uint32_t cpu);
} irq_controller_ops_t;
```

**User-Space IRQ Handling:**
- Driver registers for IRQ via system call
- Kernel sends IPC notification on interrupt
- Driver handles in user space
- Driver signals completion
- Minimal time in interrupt context

#### 1.2.5 System Call Interface

**System Call Mechanism:**
- x86_64: `syscall` instruction
- ARM64: `svc` instruction
- RISC-V: `ecall` instruction

**System Call Categories:**
1. Thread management: `thread_create`, `thread_exit`, `thread_yield`
2. Address space: `as_create`, `as_map`, `as_unmap`
3. IPC: `ipc_send`, `ipc_receive`, `ipc_call`
4. Capability: `cap_create`, `cap_destroy`, `cap_delegate`
5. Interrupt: `irq_register`, `irq_ack`
6. Debug: `debug_print`, `debug_break`

**System Call Convention (x86_64):**
- Syscall number in `rax`
- Arguments in `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`
- Return value in `rax`
- `syscall` instruction enters kernel
- `sysret` returns to user space

**System Call Validation:**
- Validate all user pointers
- Check capability rights
- Enforce security policies
- Rate limiting for DOS prevention

### 1.3 User-Space Services

#### 1.3.1 Service Architecture

**Core Services:**
```
Device Manager
    ├─ PCI Bus Driver
    ├─ USB Bus Driver
    └─ Device Tree Parser

Virtual File System Server
    ├─ Native FS Driver
    ├─ FAT32 Driver
    └─ VFS Cache

Network Server
    ├─ TCP/IP Stack
    ├─ Socket API
    └─ Network Drivers

Security Server
    ├─ Capability Manager
    ├─ Authentication
    └─ Policy Enforcement

Display Server
    ├─ Compositor
    ├─ Window Manager
    └─ Input Manager
```

**Service Communication:**
- Services communicate via IPC
- Well-defined interfaces
- Capability-based access control
- Service discovery via name server

#### 1.3.2 Driver Framework

**Driver Lifecycle:**
1. Device enumeration (by bus driver)
2. Driver loading (by device manager)
3. Driver initialization
4. Register interrupt handlers
5. Create device nodes
6. Normal operation
7. Cleanup and unload

**Driver Interface (Rust):**
```rust
pub trait Driver {
    fn probe(&mut self, device: &Device) -> Result<()>;
    fn init(&mut self) -> Result<()>;
    fn remove(&mut self) -> Result<()>;
    fn suspend(&mut self) -> Result<()>;
    fn resume(&mut self) -> Result<()>;
}

pub struct Device {
    pub id: DeviceId,
    pub resources: Vec<Resource>,
    pub properties: HashMap<String, Value>,
}

pub enum Resource {
    Irq(u32),
    IoPort(u16, u16),
    Memory(PhysAddr, usize),
    Dma(DmaChannel),
}
```

**Driver Categories:**
- Bus drivers (enumerate devices on bus)
- Class drivers (common interface for device class)
- Function drivers (specific device)
- Filter drivers (intercept requests)

---

## 2. Memory Management

### 2.1 Physical Memory Management

**Boot-Time Memory Map:**
1. Firmware provides memory map (E820 or DT)
2. Kernel reserves its own regions
3. Remaining memory added to allocator

**Buddy Allocator:**
- Manages physical page frames
- Orders from 2^0 to 2^10 pages (4KB to 4MB)
- Fast allocation/deallocation
- Coalescing of adjacent free blocks

**Data Structures:**
```c
#define MAX_ORDER 11

typedef struct page_frame {
    uint64_t flags;
    uint32_t ref_count;
    list_head_t list;
    union {
        struct {
            uint32_t order;
            void *private;
        } buddy;
        struct {
            void *slab;
        } slab;
    };
} page_frame_t;

typedef struct zone {
    page_frame_t *pages;
    uint64_t start_pfn;
    uint64_t end_pfn;
    list_head_t free_lists[MAX_ORDER];
    spinlock_t lock;
} zone_t;
```

**NUMA Awareness:**
- Allocate from local node when possible
- Fall back to remote nodes under pressure
- Track per-node statistics

### 2.2 Virtual Memory Management

**Page Table Structure (x86_64):**
- 4-level paging (PML4, PDP, PD, PT)
- 48-bit virtual address space
- 4KB page size (2MB/1GB huge pages supported)

**Memory Region Types:**
1. **Anonymous:** Regular heap/stack memory
2. **File-backed:** Memory-mapped files
3. **Shared:** Shared between processes
4. **Device:** MMIO regions
5. **Reserved:** Kernel-reserved regions

**Page Fault Handling:**
```
Page Fault Exception
    ↓
Determine fault address (CR2 on x86)
    ↓
Check if address is valid (in a memory region)
    ↓
Check permissions (R/W/X)
    ↓
Handle specific case:
    - Copy-on-Write → Allocate new page, copy data
    - Lazy allocation → Allocate page, zero it
    - File-backed → Read from file
    - Invalid access → Send SIGSEGV equivalent
```

**Copy-on-Write (CoW):**
- Used for `fork()` equivalent
- Pages marked read-only
- Write fault triggers copy
- Reference counted pages

### 2.3 Kernel Memory Allocation

**Slab Allocator:**
- Cache for commonly used kernel objects
- Reduces allocation overhead
- Better cache locality
- Per-CPU caches to reduce contention

**Slab Caches:**
```c
typedef struct slab_cache {
    const char *name;
    size_t object_size;
    size_t align;
    void (*ctor)(void *);         // Constructor
    void (*dtor)(void *);         // Destructor
    list_head_t slabs_full;
    list_head_t slabs_partial;
    list_head_t slabs_empty;
    spinlock_t lock;
    per_cpu_t *cpu_caches;
} slab_cache_t;
```

**Common Caches:**
- thread_cache
- address_space_cache
- page_table_cache
- ipc_message_cache

**General Kernel Allocator:**
- For variable-sized allocations
- Built on top of buddy allocator
- Similar to `kmalloc` in Linux

---

## 3. Security Architecture

### 3.1 Capability System

**Capability Structure:**
```c
typedef struct capability {
    uint64_t cap_id;              // Unique capability ID
    uint32_t object_type;         // Thread, Memory, Port, etc.
    uint32_t rights;              // Bit flags: R, W, X, Grant, etc.
    uint64_t badge;               // User-defined badge
    void *object;                 // Pointer to kernel object
    list_head_t list;
    atomic_t ref_count;
} capability_t;

typedef struct cap_space {
    capability_t **caps;          // Array of capability pointers
    size_t size;                  // Number of slots
    bitmap_t *used;               // Bitmap of used slots
    spinlock_t lock;
} cap_space_t;
```

**Capability Rights:**
- `CAP_READ` - Read access
- `CAP_WRITE` - Write access
- `CAP_EXECUTE` - Execute access
- `CAP_GRANT` - Can delegate to others
- `CAP_REVOKE` - Can revoke delegated caps
- `CAP_DELETE` - Can delete object

**Capability Operations:**
- `cap_create(object, rights)` - Create new capability
- `cap_copy(cap_id)` - Duplicate capability
- `cap_derive(cap_id, new_rights)` - Create derived cap with subset of rights
- `cap_grant(target, cap_id)` - Transfer capability to another process
- `cap_revoke(cap_id)` - Revoke capability
- `cap_delete(cap_id)` - Delete capability

**Capability-Based IPC:**
- IPC messages can include capabilities
- Receiver gets capability to sender's resources
- Used for resource sharing (memory, ports, etc.)
- Enables safe delegation

### 3.2 Traditional Security (ACL)

**User/Group Model:**
```c
typedef struct user {
    uint32_t uid;
    uint32_t primary_gid;
    uint32_t *supp_gids;
    size_t num_supp_gids;
    char *username;
    char *home_dir;
    uint64_t capabilities;        // System capabilities
} user_t;

typedef struct group {
    uint32_t gid;
    char *groupname;
    list_head_t members;
} group_t;
```

**File System ACLs:**
```c
typedef struct acl_entry {
    uint32_t type;                // User, Group, Other
    uint32_t id;                  // UID or GID
    uint32_t permissions;         // R, W, X
} acl_entry_t;

typedef struct acl {
    acl_entry_t *entries;
    size_t num_entries;
} acl_t;
```

**Process Credentials:**
```c
typedef struct credentials {
    uid_t uid;                    // User ID
    gid_t gid;                    // Primary group ID
    uid_t euid;                   // Effective user ID
    gid_t egid;                   // Effective group ID
    cap_space_t *cap_space;       // Capability space
    acl_t *acl;                   // ACL
} cred_t;
```

### 3.3 Security Policies

**Mandatory Access Control (MAC):**
- Enforce system-wide security policies
- Cannot be overridden by users
- Label-based access control
- Similar to SELinux concepts

**Sandboxing:**
```rust
pub struct Sandbox {
    allowed_syscalls: Vec<SyscallId>,
    max_memory: usize,
    max_cpu_time: Duration,
    allowed_files: Vec<PathBuf>,
    network_access: NetworkPolicy,
}

pub enum NetworkPolicy {
    None,
    LocalhostOnly,
    RestrictedPorts(Vec<u16>),
    Full,
}
```

**Isolation Mechanisms:**
- Namespace isolation (PID, network, mount, etc.)
- Resource quotas (memory, CPU, file descriptors)
- Seccomp-style syscall filtering
- Filesystem views (chroot-like)

---

## 4. File System Architecture

### 4.1 Virtual File System (VFS)

**VFS Layer:**
- Abstract interface for file systems
- Uniform API for applications
- Caching layer
- Path resolution

**Key VFS Objects:**
```c
typedef struct vfs_inode {
    uint64_t ino;
    uint32_t mode;                // Type and permissions
    uint32_t uid;
    uint32_t gid;
    uint64_t size;
    uint64_t atime, mtime, ctime;
    uint32_t nlinks;
    void *fs_private;             // FS-specific data
    struct vfs_inode_ops *ops;
} vfs_inode_t;

typedef struct vfs_dentry {
    char *name;
    vfs_inode_t *inode;
    struct vfs_dentry *parent;
    list_head_t children;
    list_head_t siblings;
    uint32_t ref_count;
} vfs_dentry_t;

typedef struct vfs_file {
    vfs_dentry_t *dentry;
    uint64_t position;
    uint32_t flags;               // O_RDONLY, O_WRONLY, etc.
    void *private;
    struct vfs_file_ops *ops;
} vfs_file_t;
```

**VFS Operations:**
```c
struct vfs_inode_ops {
    int (*lookup)(vfs_inode_t *dir, const char *name, vfs_inode_t **result);
    int (*create)(vfs_inode_t *dir, const char *name, uint32_t mode);
    int (*mkdir)(vfs_inode_t *dir, const char *name, uint32_t mode);
    int (*unlink)(vfs_inode_t *dir, const char *name);
    int (*rmdir)(vfs_inode_t *dir, const char *name);
    int (*rename)(vfs_inode_t *old_dir, const char *old_name,
                  vfs_inode_t *new_dir, const char *new_name);
};

struct vfs_file_ops {
    ssize_t (*read)(vfs_file_t *file, void *buf, size_t count);
    ssize_t (*write)(vfs_file_t *file, const void *buf, size_t count);
    int (*ioctl)(vfs_file_t *file, uint32_t cmd, void *arg);
    int (*mmap)(vfs_file_t *file, vm_region_t *region);
    int (*flush)(vfs_file_t *file);
    int (*close)(vfs_file_t *file);
};
```

### 4.2 Native File System

**Design Goals:**
- Modern features (CoW, snapshots, compression)
- Crash consistency (journaling or log-structured)
- Good performance
- Scalability

**Potential Design (B-tree based):**
```
Superblock
    ├─ Root B-tree
    │   ├─ Inode tree
    │   ├─ Extent tree
    │   ├─ Directory tree
    │   └─ Metadata tree
    ├─ Free space tree
    └─ Journal / Log
```

**Key Features:**
- Copy-on-Write for data and metadata
- Extent-based allocation (not block lists)
- Online defragmentation
- Snapshots (cheap clones of tree)
- Optional compression (LZ4, ZSTD)
- Optional encryption (per-file or per-volume)
- Checksums for data integrity

---

## 5. Network Architecture

### 5.1 Network Stack Design

**Layer Architecture:**
```
┌─────────────────────────────┐
│    Application Layer         │
│    (Socket API)              │
├─────────────────────────────┤
│    Transport Layer           │
│    (TCP, UDP, SCTP)          │
├─────────────────────────────┤
│    Network Layer             │
│    (IPv4, IPv6, ICMP)        │
├─────────────────────────────┤
│    Link Layer                │
│    (Ethernet, WiFi)          │
├─────────────────────────────┤
│    Network Drivers           │
└─────────────────────────────┘
```

**Socket Abstraction:**
```rust
pub struct Socket {
    domain: AddressFamily,       // AF_INET, AF_INET6
    sock_type: SocketType,       // SOCK_STREAM, SOCK_DGRAM
    protocol: Protocol,          // TCP, UDP, etc.
    state: SocketState,
    local_addr: SocketAddr,
    remote_addr: Option<SocketAddr>,
    send_queue: PacketQueue,
    recv_queue: PacketQueue,
    options: SocketOptions,
}
```

### 5.2 Protocol Implementation

**TCP State Machine:**
- CLOSED, LISTEN, SYN_SENT, SYN_RECEIVED
- ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2
- CLOSE_WAIT, CLOSING, LAST_ACK, TIME_WAIT

**TCP Features:**
- Reliable delivery with retransmission
- Flow control (sliding window)
- Congestion control (Cubic or BBR)
- Fast retransmit and recovery
- Selective acknowledgments (SACK)
- TCP timestamps

---

## 6. GUI Architecture

### 6.1 Integrated Graphics Design

**Component Stack:**
```
┌───────────────────────────────┐
│   Desktop Applications        │
├───────────────────────────────┤
│   Widget Toolkit (C++)        │
├───────────────────────────────┤
│   Window Manager              │
├───────────────────────────────┤
│   Compositor                  │
├───────────────────────────────┤
│   Display Server Core         │
├───────────────────────────────┤
│   Graphics API (2D/3D)        │
├───────────────────────────────┤
│   GPU Driver                  │
└───────────────────────────────┘
```

**Display Server:**
- Manages display outputs
- Routes input events
- Provides graphics APIs to applications
- Integrated with OS (not a separate process)

**Compositor:**
- Composites window surfaces
- GPU-accelerated rendering
- Effects (shadows, transparency, animations)
- V-sync and presentation

### 6.2 Graphics API

**2D API:**
```cpp
class GraphicsContext {
public:
    void drawLine(Point p1, Point p2, Color c);
    void drawRect(Rect r, Color c);
    void fillRect(Rect r, Color c);
    void drawText(Point p, const String& text, Font f);
    void drawImage(Point p, const Image& img);
    void setClipRect(Rect r);
    void setTransform(Matrix3x3 m);
};
```

**3D API (Vulkan-like):**
- Command buffers
- Pipeline state objects
- Descriptor sets
- Memory management
- Synchronization primitives

---

## 7. Cross-Platform HAL

### 7.1 HAL Interface

**Core HAL Functions:**
```c
// CPU
void hal_cpu_init(void);
void hal_cpu_idle(void);
void hal_cpu_halt(void);

// Memory
void hal_mm_init(void);
paddr_t hal_mm_virt_to_phys(vaddr_t vaddr);
void hal_mm_flush_tlb(vaddr_t vaddr);
void hal_mm_switch_address_space(page_table_t *pt);

// Interrupts
void hal_irq_init(void);
void hal_irq_enable(uint32_t irq);
void hal_irq_disable(uint32_t irq);

// Context switching
void hal_context_switch(thread_t *prev, thread_t *next);
void hal_context_init(thread_t *thread, void (*entry)(void*), void *arg);

// Timers
void hal_timer_init(uint32_t freq_hz);
uint64_t hal_timer_get_ticks(void);

// Atomic operations
uint64_t hal_atomic_add(atomic_t *var, uint64_t val);
uint64_t hal_atomic_cas(atomic_t *var, uint64_t old_val, uint64_t new_val);
```

### 7.2 Architecture-Specific Implementation

**Example: x86_64 HAL**
- Paging: 4-level page tables
- Interrupts: IDT, APIC
- Timers: HPET, TSC
- Context: x86 register state

**Example: ARM64 HAL**
- Paging: 4-level page tables (different format)
- Interrupts: GIC
- Timers: Generic timer
- Context: ARM register state

---

## 8. Performance Considerations

### 8.1 IPC Optimization

**Fast Path:**
- Inline small messages (< 64 bytes)
- Direct copy to receiver buffer
- Minimal state changes
- No memory allocation

**Zero-Copy:**
- Capability-based page transfer
- Shared memory regions
- DMA where applicable

### 8.2 Scheduler Optimization

**Cache Awareness:**
- Keep threads on same CPU (cache affinity)
- Per-CPU runqueues (reduce lock contention)
- Batch operations

**Low Latency:**
- Real-time priorities preempt immediately
- Minimal interrupt disable time
- Fast context switch paths

### 8.3 Memory Optimization

**Huge Pages:**
- 2MB/1GB pages for large allocations
- Reduces TLB pressure
- Transparent huge pages

**NUMA:**
- Allocate memory local to CPU
- Migrate pages under some conditions
- Track per-node statistics

---

## Appendix: Data Structure Sizes

**Estimated Memory Footprint:**
- `thread_t`: ~512 bytes
- `address_space_t`: ~256 bytes
- `capability_t`: ~64 bytes
- `vfs_inode_t`: ~256 bytes
- `page_frame_t`: ~64 bytes (per 4KB page)

**Example System:**
- 1000 threads: ~512 KB
- 100 address spaces: ~25 KB
- 10000 capabilities: ~640 KB
- 1GB RAM (256K pages): ~16 MB for page frames

---

*Document Version: 1.0*
*Last Updated: 2025-11-17*
