# Kernel-Space Drivers

## Overview

This directory contains drivers that must remain in kernel space because they are **boot-critical** and needed before user-space services are initialized.

## Drivers That Stay in Kernel

According to the microkernel architecture in `Docs/Dev/OS_DEVELOPMENT_PLAN.md`, only minimal boot-critical drivers should be in kernel space:

### ✅ Boot-Critical Drivers (Stay in Kernel)

1. **`pci/`** - PCI bus enumeration
   - **Reason:** Needed for device discovery before user-space is ready
   - **Status:** Stays in kernel

2. **`graphics/framebuffer.c`** - Framebuffer driver
   - **Reason:** Needed for early boot graphics output
   - **Status:** Stays in kernel

3. **`ps2/`** - PS/2 keyboard and mouse drivers
   - **Reason:** Needed for early input before USB stack is available
   - **Status:** Stays in kernel

### ⚠️ Drivers To Be Migrated to User-Space

These drivers are currently in kernel space but should be migrated to user-space (`drivers/` in Rust):

1. **`ahci/`** - AHCI storage driver
   - **Target:** `drivers/storage/ahci/` (Rust)
   - **Status:** Migration pending

2. **`ata/`** - ATA storage driver
   - **Target:** `drivers/storage/ata/` (Rust)
   - **Status:** Migration pending

3. **`ethernet/`** - Ethernet NIC driver
   - **Target:** `drivers/network/ethernet/` (Rust)
   - **Status:** Migration pending

4. **`virtio/`** - VirtIO drivers
   - **Target:** `drivers/virtio/` (Rust)
   - **Status:** Migration pending

## Migration Status

See `Docs/Dev/DRIVER_MIGRATION_STATUS.md` for detailed migration tracking.

## Architecture Compliance

The development plan requires:
- **User-Space Components (Ring 3):** Device drivers (Rust)
- **Kernel-Space Components (Ring 0):** Only basic hardware abstraction

This directory should eventually contain only the boot-critical drivers listed above.

