# Driver Migration Status

## Overview

This document tracks the migration of drivers from kernel-space (C) to user-space (Rust) according to the microkernel architecture defined in `OS_DEVELOPMENT_PLAN.md`.

## Migration Strategy

### Phase 1: Boot-Critical Drivers (Stay in Kernel)

These drivers must remain in kernel space temporarily because they are needed before user-space services are initialized:

- **`kernel/drivers/pci/`** - PCI bus enumeration (needed for device discovery)
- **`kernel/drivers/graphics/framebuffer.c`** - Early boot graphics output
- **`kernel/drivers/ps2/`** - PS/2 keyboard/mouse (early input before USB)

**Rationale:** These are required during kernel initialization before user-space is ready.

### Phase 2: User-Space Drivers (Rust)

All device drivers should be migrated to user-space in `drivers/`:

#### Storage Drivers
- [x] **Framework created** - `drivers/framework/`
- [x] **AHCI driver** - `drivers/storage/ahci/` (structure complete, hardware implementation pending)
- [ ] **ATA driver** - `drivers/storage/ata/` (migrate from `kernel/drivers/ata/`)
- [ ] **NVMe driver** - `drivers/storage/nvme/` (new)

#### Network Drivers
- [x] **Ethernet driver** - `drivers/network/ethernet/` (structure complete, hardware implementation pending)
- [ ] **Wi-Fi driver** - `drivers/network/wifi/` (new)

#### Input Drivers
- [ ] **USB Keyboard** - `drivers/input/keyboard/` (enhance existing stub)
- [ ] **USB Mouse** - `drivers/input/mouse/` (enhance existing stub)
- [x] **PS/2 drivers** - Stay in kernel (boot-critical)

#### Graphics Drivers
- [x] **Framebuffer** - Stay in kernel (boot-critical)
- [ ] **GPU drivers** - `drivers/graphics/gpu/` (new)

#### Bus Drivers
- [x] **PCI enumeration** - Stay in kernel (boot-critical)
- [ ] **USB stack (XHCI)** - `drivers/bus/usb/` (new, user-space)

## Current Status

### Kernel-Space Drivers (C)
- ✅ PCI bus driver (`kernel/drivers/pci/`) - **Stays in kernel**
- ✅ Framebuffer driver (`kernel/drivers/graphics/framebuffer.c`) - **Stays in kernel**
- ✅ PS/2 drivers (`kernel/drivers/ps2/`) - **Stays in kernel**
- ⚠️ AHCI driver (`kernel/drivers/ahci/`) - **To be migrated**
- ⚠️ ATA driver (`kernel/drivers/ata/`) - **To be migrated**
- ⚠️ Ethernet driver (`kernel/drivers/ethernet/`) - **To be migrated**
- ⚠️ VirtIO drivers (`kernel/drivers/virtio/`) - **To be migrated**

### User-Space Drivers (Rust)
- ✅ Driver framework (`drivers/framework/`) - **Complete**
- ✅ AHCI driver (`drivers/storage/ahci/`) - **Structure complete, hardware implementation pending**
- ⚠️ ATA driver (`drivers/storage/ata/`) - **Stub exists, needs implementation**
- ✅ Ethernet driver (`drivers/network/ethernet/`) - **Structure complete, hardware implementation pending**
- ⚠️ Input drivers (`drivers/input/`) - **Stubs exist, need implementation**
- ✅ Driver loading system (`services/device_manager/src/driver.rs`) - **Complete**

## Migration Steps

1. ✅ Create driver framework library (`drivers/framework/`)
2. ✅ Implement user-space AHCI driver (structure complete)
3. ✅ Implement user-space Ethernet driver (structure complete)
4. ✅ Create user-space driver IPC protocol
5. ✅ Update device manager to load user-space drivers
6. ⏳ Complete hardware-specific implementation
7. ⏳ Test user-space drivers
8. ⏳ Remove kernel-space device drivers (keep only boot-critical)

## Notes

- User-space drivers can map MMIO directly via `SYS_MMIO_MAP` syscall
- User-space drivers can register IRQ handlers via `SYS_IRQ_REGISTER` syscall
- User-space drivers communicate with device manager via IPC
- Performance fast-paths (graphics, audio) can still map MMIO directly from user-space

