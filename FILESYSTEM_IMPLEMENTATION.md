# Filesystem Implementation Plan

**Date:** November 18, 2025  
**Status:** Ready to Implement

---

## 🎯 Goal: Basic Filesystem (VFS + FAT32)

### Why FAT32?
- **Simple:** Well-documented, straightforward structure
- **Compatible:** Can create FAT32 images on host system
- **Sufficient:** Good enough for Phase 4, can add better FS later

---

## 📋 Implementation Plan

### Step 1: VFS (Virtual File System) Layer (2-3 weeks)

**Purpose:** Abstract filesystem operations so we can support multiple filesystems later.

**Files to Create:**
- `kernel/include/fs/vfs.h` - VFS interface
- `kernel/fs/vfs.c` - VFS implementation
- `kernel/include/fs/file.h` - File descriptor management
- `kernel/fs/file.c` - File operations

**Key Features:**
- File descriptor management
- Inode abstraction
- Directory operations
- Path resolution
- Mount point management

### Step 2: Block Device Layer (1-2 weeks)

**Purpose:** Abstract storage device access.

**Files to Create:**
- `kernel/include/fs/block.h` - Block device interface
- `kernel/fs/block.c` - Block device implementation

**Key Features:**
- Block read/write operations
- Device registration
- Buffer cache (optional, for performance)

### Step 3: FAT32 Implementation (3-4 weeks)

**Purpose:** Actual filesystem that can read/write files.

**Files to Create:**
- `kernel/include/fs/fat32.h` - FAT32 structures
- `kernel/fs/fat32.c` - FAT32 implementation

**Key Features:**
- FAT32 superblock reading
- Directory traversal
- File reading
- File writing
- Directory creation
- File creation/deletion

### Step 4: Storage Driver (2-3 weeks)

**Purpose:** Actually talk to disk hardware.

**Files to Create:**
- `kernel/include/drivers/ata.h` - ATA interface
- `kernel/drivers/ata.c` - ATA driver
- `kernel/include/drivers/ahci.h` - AHCI interface (for SATA)
- `kernel/drivers/ahci.c` - AHCI driver

**Key Features:**
- ATA/IDE support
- AHCI (SATA) support
- Disk detection
- Sector read/write

---

## 🚀 Starting Implementation

Let me start with the VFS layer, then block devices, then FAT32.

