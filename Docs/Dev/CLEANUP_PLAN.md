# Kernel Driver Cleanup Plan

## Overview

After user-space drivers are tested and verified to work correctly, we need to remove the kernel-space device drivers, keeping only boot-critical drivers.

## Drivers to Remove

### ⚠️ Remove After Testing

1. **`kernel/drivers/ahci/`**
   - **Status:** Functionality migrated to `drivers/storage/ahci/`
   - **Action:** Delete directory after verification
   - **Dependencies:** Check `kernel/Makefile` and `kernel/core/main.c`

2. **`kernel/drivers/ethernet/`**
   - **Status:** Functionality migrated to `drivers/network/ethernet/`
   - **Action:** Delete directory after verification
   - **Dependencies:** Check `kernel/Makefile` and `kernel/core/main.c`

3. **`kernel/drivers/ata/`**
   - **Status:** To be migrated to `drivers/storage/ata/`
   - **Action:** Migrate first, then delete after verification
   - **Dependencies:** Check `kernel/Makefile` and `kernel/core/main.c`

4. **`kernel/drivers/virtio/`**
   - **Status:** To be migrated to `drivers/virtio/`
   - **Action:** Migrate first, then delete after verification
   - **Dependencies:** Check `kernel/Makefile` and `kernel/core/main.c`

## Drivers to Keep

### ✅ Keep (Boot-Critical)

1. **`kernel/drivers/pci/`**
   - **Reason:** Needed for device enumeration before user-space is ready
   - **Status:** Stays in kernel

2. **`kernel/drivers/graphics/framebuffer.c`**
   - **Reason:** Needed for early boot graphics output
   - **Status:** Stays in kernel

3. **`kernel/drivers/ps2/`**
   - **Reason:** Needed for early input before USB stack is available
   - **Status:** Stays in kernel

## Cleanup Steps

### Step 1: Verify User-Space Drivers Work

Before removing kernel drivers:
- [ ] Test AHCI driver read/write operations
- [ ] Test Ethernet driver send/receive operations
- [ ] Verify IPC communication works
- [ ] Verify MMIO mapping works
- [ ] Verify IRQ handling works
- [ ] Test end-to-end functionality

### Step 2: Update Kernel Makefile

**File:** `kernel/Makefile` or `kernel/Makefile.arch`

**Remove:**
```makefile
drivers/ahci/ahci.c \
drivers/ethernet/ethernet.c \
drivers/ata/ata.c \
drivers/virtio/virtio.c \
drivers/virtio/virtio_gpu.c \
```

**Keep:**
```makefile
drivers/pci/pci.c \
drivers/graphics/framebuffer.c \
drivers/ps2/keyboard.c \
drivers/ps2/mouse.c \
drivers/ps2/ps2.c \
```

### Step 3: Update Kernel Initialization

**File:** `kernel/core/main.c`

**Remove:**
```c
extern error_code_t ahci_init(void);
extern error_code_t ethernet_driver_init(void);
extern error_code_t ata_init(void);
```

**Keep:**
```c
extern error_code_t pci_init(void);
extern error_code_t framebuffer_init(framebuffer_info_t*);
extern error_code_t keyboard_init(void);
extern error_code_t mouse_init(void);
```

### Step 4: Remove Driver Directories

After Makefile and initialization updates:
```bash
rm -rf kernel/drivers/ahci/
rm -rf kernel/drivers/ethernet/
rm -rf kernel/drivers/ata/      # After migration
rm -rf kernel/drivers/virtio/   # After migration
```

### Step 5: Update Documentation

- Update `kernel/drivers/README.md` to reflect removed drivers
- Update `Docs/Dev/DRIVER_MIGRATION_STATUS.md` to mark cleanup complete
- Update `README.md` if needed

## Verification Checklist

After cleanup:
- [ ] Kernel builds successfully
- [ ] Kernel boots successfully
- [ ] User-space drivers are loaded
- [ ] Block device I/O works
- [ ] Network I/O works
- [ ] No references to removed drivers in codebase

## Rollback Plan

If issues are found after cleanup:
1. Restore driver directories from git
2. Restore Makefile changes
3. Restore initialization code
4. Investigate and fix issues
5. Retry cleanup after fixes

## Notes

- Cleanup should be done incrementally (one driver at a time)
- Test thoroughly after each removal
- Keep git history for easy rollback
- Document any issues found during cleanup

