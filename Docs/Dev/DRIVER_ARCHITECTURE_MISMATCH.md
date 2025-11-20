# Driver Architecture Mismatch Analysis

## Current State vs. Development Plan

### Development Plan Requirements

According to `OS_DEVELOPMENT_PLAN.md`:

**User-Space Components (Ring 3):**
- Device drivers (Rust) - *Can map MMIO directly for performance*
- **Sandbox Guarantees:** Drivers run in sandboxed processes with explicit capabilities; no ambient kernel privileges.

**Kernel-Space Components (Ring 0):**
- Address space management
- Thread scheduling and synchronization
- IPC primitives
- Interrupt handling
- Basic hardware abstraction
- *Performance Fast-Paths:* Graphics command submission, Audio buffer mapping

**Driver Framework (Rust):**
- Standard driver interface
- Bus drivers (PCI, USB, I2C, SPI, etc.) - **Should be in user-space**
- Device class abstractions
- DMA API
- Interrupt handling API

### Current Implementation

**Kernel-Space Drivers (C) - `kernel/drivers/`:**
- `pci/` - PCI bus driver (C)
- `ahci/` - AHCI storage driver (C)
- `ata/` - ATA storage driver (C)
- `ethernet/` - Ethernet NIC driver (C)
- `ps2/` - PS/2 keyboard/mouse drivers (C)
- `graphics/framebuffer.c` - Framebuffer driver (C)
- `virtio/` - VirtIO drivers (C)

**User-Space Drivers (Rust) - `drivers/`:**
- `input/keyboard/` - Keyboard driver stub (Rust)
- `input/mouse/` - Mouse driver stub (Rust)
- `storage/ahci/` - AHCI driver stub (Rust)
- `storage/ata/` - ATA driver stub (Rust)
- `storage/fat32/` - FAT32 filesystem (Rust)
- `serial/` - Serial driver stub (Rust)

## Problem

**Mismatch:** Most functional drivers are currently in kernel space (C), but the development plan requires them to be in user-space (Rust) for:
1. **Security:** Fault isolation (driver crash ≠ kernel crash)
2. **Sandboxing:** Explicit capabilities, no ambient privileges
3. **Microkernel Architecture:** Minimal TCB (Trusted Computing Base)

## Migration Strategy

### Phase 1: Keep Critical Boot Drivers in Kernel (Temporary)
Some drivers may need to remain in kernel space temporarily for early boot:
- **PCI Bus Driver** - Needed for device enumeration before user-space is ready
- **Framebuffer Driver** - Needed for early graphics output
- **PS/2 Drivers** - Needed for early input (before USB stack)

**Rationale:** These are required before user-space services are initialized.

### Phase 2: Migrate Device Drivers to User-Space
Move device drivers to user-space (Rust) in `drivers/`:
- **Storage Drivers** (AHCI, ATA, NVMe) → `drivers/storage/`
- **Network Drivers** (Ethernet, Wi-Fi) → `drivers/network/`
- **Input Drivers** (USB keyboard/mouse) → `drivers/input/`
- **Graphics Drivers** (GPU) → `drivers/graphics/`

### Phase 3: Refactor Bus Drivers
Consider moving bus drivers to user-space where possible:
- **PCI Driver** - Could be a user-space service that communicates with kernel via syscalls
- **USB Stack** - Should definitely be user-space
- **I2C/SPI** - User-space drivers

### Recommended Structure

```
kernel/
├── drivers/              # Minimal boot-time drivers only
│   ├── pci/             # PCI enumeration (may stay in kernel)
│   └── graphics/        # Basic framebuffer (may stay in kernel)
│
drivers/                  # User-space drivers (Rust)
├── bus/
│   ├── pci/             # PCI bus driver (user-space)
│   └── usb/             # USB stack (XHCI, etc.)
├── storage/
│   ├── ahci/            # AHCI driver (user-space)
│   ├── ata/             # ATA driver (user-space)
│   └── nvme/            # NVMe driver (user-space)
├── network/
│   ├── ethernet/        # Ethernet driver (user-space)
│   └── wifi/            # Wi-Fi driver (user-space)
├── input/
│   ├── keyboard/        # Keyboard driver (user-space)
│   └── mouse/           # Mouse driver (user-space)
└── graphics/
    └── gpu/             # GPU drivers (user-space)
```

## Action Items

1. ✅ **Immediate:** Document which drivers must stay in kernel (boot-critical) - **COMPLETE**
2. ✅ **Short-term:** Create user-space driver framework with IPC interfaces - **COMPLETE**
3. ⏳ **Medium-term:** Migrate device drivers to user-space one by one - **IN PROGRESS**
4. ⏳ **Long-term:** Refactor bus drivers to user-space where possible - **PENDING**

## Progress Update

### Completed
- ✅ Created driver framework library (`drivers/framework/`)
- ✅ Documented boot-critical drivers that stay in kernel
- ✅ Created user-space Ethernet driver structure
- ✅ Updated development plan and README

### In Progress
- ⏳ Implementing user-space drivers (AHCI, Ethernet, etc.)
- ⏳ Creating driver IPC protocol
- ⏳ Updating device manager to load user-space drivers

### Next Steps
1. Implement full user-space AHCI driver
2. Implement full user-space Ethernet driver
3. Create driver loading mechanism in device manager
4. Test user-space drivers
5. Remove kernel-space device drivers (keep only boot-critical)

## Notes

- The framebuffer driver might need to stay in kernel for early boot graphics
- PCI enumeration might need kernel support, but device drivers using PCI should be user-space
- Performance fast-paths (graphics, audio) can still map MMIO directly from user-space
- All drivers should communicate via IPC with the device manager service

