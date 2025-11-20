# Scarlett OS UEFI Boot Guide

This document details how to build, configure, and boot Scarlett OS using the Limine UEFI bootloader.

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Building the Kernel](#building-the-kernel)
3. [Bootloader Setup](#bootloader-setup)
4. [Creating Boot Disk](#creating-boot-disk)
5. [Booting with QEMU](#booting-with-qemu)
6. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Required Tools
- **WSL (Windows Subsystem for Linux)** with Ubuntu/Debian
- **GNU Make**
- **GCC cross-compiler** for x86_64-elf
- **NASM** assembler
- **QEMU** system emulator
- **OVMF** UEFI firmware
- **mtools** for FAT32 manipulation
- **sgdisk** for GPT partitioning

### Install Prerequisites (WSL/Ubuntu)
```bash
sudo apt update
sudo apt install build-essential nasm qemu-system-x86 ovmf mtools gdisk
```

---

## Building the Kernel

### 1. Build the Kernel
```bash
cd /mnt/c/Users/woisr/Downloads/OS
wsl make -C kernel
```

This produces `kernel/kernel.elf` (~818KB) with:
- Multiboot2 header at offset 0x1000 (4KB)
- Entry point at `multiboot2_entry`
- Supports 1024x768 framebuffer @ 32bpp

### 2. Verify Multiboot2 Header
```bash
wsl readelf -l kernel/kernel.elf | grep "LOAD"
wsl readelf -S kernel/kernel.elf | grep multiboot
```

The `.multiboot` section must be in the first LOAD segment and within the first 8-32KB of the file.

---

## Bootloader Setup

### 1. Download Limine Bootloader
```bash
wsl bash -c "mkdir -p /tmp/limine && cd /tmp/limine && \
  wget https://github.com/limine-bootloader/limine/raw/v8.x-binary/BOOTX64.EFI"
```

### 2. Create Limine Configuration
Create `build/limine.conf`:
```
timeout: 0
verbose: yes
serial: yes

/Scarlett OS
    protocol: multiboot2
    kernel_path: boot():/kernel.elf
```

**Important Configuration Notes:**
- `timeout: 0` - Boot immediately without menu
- `verbose: yes` - Enable detailed boot messages
- `serial: yes` - Output to serial console
- `/Scarlett OS` - Menu entry (note the `/` prefix for v8.x syntax)
- `protocol: multiboot2` - Must match kernel's Multiboot2 header
- `kernel_path: boot():/kernel.elf` - Kernel location on EFI partition

---

## Creating Boot Disk

### 1. Create GPT Disk Image
```bash
wsl bash -c "cd build && \
  # Create 512MB disk image
  dd if=/dev/zero of=disk.img bs=1M count=512 && \
  # Create GPT partition table
  sgdisk -n 1:2048:+127M -t 1:EF00 -c 1:'EFI System' disk.img"
```

### 2. Create FAT32 EFI Partition
```bash
wsl bash -c "cd build && \
  # Create FAT32 filesystem image
  dd if=/dev/zero of=part.img bs=512 count=260063 && \
  mkfs.fat -F 32 part.img && \
  # Create EFI directory structure
  MTOOLS_SKIP_CHECK=1 mmd -i part.img ::/EFI ::/EFI/BOOT && \
  # Copy bootloader
  MTOOLS_SKIP_CHECK=1 mcopy -i part.img /tmp/limine/BOOTX64.EFI ::/EFI/BOOT/ && \
  # Copy kernel
  MTOOLS_SKIP_CHECK=1 mcopy -i part.img ../kernel/kernel.elf ::/ && \
  # Copy Limine config
  MTOOLS_SKIP_CHECK=1 mcopy -i part.img limine.conf ::/ && \
  # Write partition to disk image
  dd if=part.img of=disk.img bs=512 seek=2048 conv=notrunc && \
  # Clean up
  rm -f part.img"
```

### 3. Verify Disk Structure
```bash
wsl bash -c "cd build && sgdisk -p disk.img"
wsl bash -c "cd build && MTOOLS_SKIP_CHECK=1 mdir -i disk.img@@1M ::/"
```

Expected structure:
```
/EFI/BOOT/BOOTX64.EFI  - Limine bootloader
/kernel.elf            - Scarlett OS kernel
/limine.conf           - Bootloader configuration
```

---

## Booting with QEMU

### Method 1: Quick Boot (Console Only)
```bash
wsl bash -c "cd build && \
  qemu-system-x86_64 \
    -bios /usr/share/OVMF/OVMF_CODE.fd \
    -drive file=disk.img,format=raw \
    -m 512M \
    -serial stdio"
```

### Method 2: Boot with GUI Window
```bash
wsl bash -c "cd build && \
  qemu-system-x86_64 \
    -bios /usr/share/OVMF/OVMF_CODE.fd \
    -drive file=disk.img,format=raw \
    -m 512M \
    -serial stdio \
    -display gtk"
```

### Method 3: Background Boot with Log
```bash
wsl bash -c "cd build && \
  timeout 60s qemu-system-x86_64 \
    -bios /usr/share/OVMF/OVMF_CODE.fd \
    -drive file=disk.img,format=raw \
    -m 512M \
    -serial stdio 2>&1 | tee boot.log"
```

### QEMU Parameters Explained
- `-bios /usr/share/OVMF/OVMF_CODE.fd` - Use OVMF UEFI firmware
- `-drive file=disk.img,format=raw` - Boot from our GPT disk image
- `-m 512M` - Allocate 512MB RAM
- `-serial stdio` - Redirect serial output to terminal
- `-display gtk` - Show graphical framebuffer window

---

## Complete Build & Boot Script

```bash
#!/bin/bash
# Complete Scarlett OS build and boot script

set -e  # Exit on error

echo "=== Building Kernel ==="
wsl make -C kernel

echo "=== Setting up Bootloader ==="
wsl bash -c "mkdir -p /tmp/limine"
wsl bash -c "cd /tmp/limine && [ ! -f BOOTX64.EFI ] && \
  wget -q https://github.com/limine-bootloader/limine/raw/v8.x-binary/BOOTX64.EFI || true"

echo "=== Creating Boot Disk ==="
wsl bash -c "cd build && \
  dd if=/dev/zero of=part.img bs=512 count=260063 2>/dev/null && \
  mkfs.fat -F 32 part.img >/dev/null 2>&1 && \
  MTOOLS_SKIP_CHECK=1 mmd -i part.img ::/EFI ::/EFI/BOOT 2>/dev/null && \
  MTOOLS_SKIP_CHECK=1 mcopy -i part.img /tmp/limine/BOOTX64.EFI ::/EFI/BOOT/ && \
  MTOOLS_SKIP_CHECK=1 mcopy -i part.img ../kernel/kernel.elf ::/ && \
  MTOOLS_SKIP_CHECK=1 mcopy -i part.img limine.conf ::/ && \
  dd if=part.img of=disk.img bs=512 seek=2048 conv=notrunc status=none && \
  rm -f part.img"

echo "=== Booting Scarlett OS ==="
wsl bash -c "cd build && \
  qemu-system-x86_64 \
    -bios /usr/share/OVMF/OVMF_CODE.fd \
    -drive file=disk.img,format=raw \
    -m 512M \
    -serial stdio"
```

---

## Boot Sequence

### 1. UEFI Firmware Boot
```
OVMF UEFI → GPT Partition Table → EFI System Partition
```

### 2. Bootloader Execution
```
/EFI/BOOT/BOOTX64.EFI (Limine) → Parse limine.conf → Load kernel.elf
```

### 3. Kernel Boot
```
Multiboot2 Entry → Parse boot info → Initialize subsystems → Desktop
```

### Expected Boot Output
```
====================================================
                  Scarlett OS
        A Modern Microkernel Operating System
====================================================
Version: 0.1.0 (Phase 1 - Development)
Architecture: x86_64
Build: Nov 19 2025 18:19:21
====================================================

[INFO] Verifying boot information...
[INFO] Framebuffer: 0x0000000080000000 (1024x768 @ 32 bpp)
[INFO] Initializing framebuffer...
[INFO] Boot splash screen initialized
...
[INFO] PCI enumeration complete: 6 device(s) found
[INFO] Ethernet NIC initialized: eth0
[INFO] Phase 3 initialization complete!
```

---

## Troubleshooting

### Issue: "PANIC: multiboot2: Invalid magic"
**Cause:** Multiboot2 header not in first 8-32KB of kernel ELF

**Solution:**
1. Check linker script (`kernel/kernel.ld`) has `.multiboot` section first
2. Verify section has "a" (allocatable) flag: `.section .multiboot, "a"`
3. Confirm with: `wsl readelf -l kernel/kernel.elf | head -20`

### Issue: "Failed to load kernel.elf"
**Cause:** File not on EFI partition or wrong path

**Solution:**
```bash
# Verify kernel is on partition
wsl bash -c "cd build && MTOOLS_SKIP_CHECK=1 mls -i disk.img@@1M ::/"
# Should show kernel.elf at root
```

### Issue: "No bootable device"
**Cause:** BOOTX64.EFI not in correct location

**Solution:**
```bash
# Must be at /EFI/BOOT/BOOTX64.EFI (case sensitive)
wsl bash -c "cd build && MTOOLS_SKIP_CHECK=1 mdir -i disk.img@@1M ::/EFI/BOOT"
```

### Issue: PCI devices enumerated incorrectly
**Cause:** Fixed in kernel/drivers/pci/pci.c

**Solution:** Kernel now correctly stops enumeration after bus 0 for single-bus systems

### Issue: System reboots/triple faults after boot
**Cause:** Exception during initialization

**Solution:**
- Check serial console output for exception messages
- Verify all subsystems initialized correctly
- Common culprit: userspace shell launch (currently disabled)

---

## Technical Details

### Multiboot2 Header Location
- **File Offset:** 0x1000 (4096 bytes)
- **Virtual Address:** 0x100000 (1MB)
- **Section:** `.multiboot` (RX, allocatable)
- **Segment:** First PT_LOAD segment

### Memory Map
- **Kernel Start:** 0x100000 (1MB)
- **Framebuffer:** 0x80000000 (2GB)
- **PHYS_MAP_BASE:** 0xFFFF800000000000
- **Total RAM:** 511 MB (505 MB usable)

### Initialized Subsystems
1. GDT (Global Descriptor Table)
2. IDT (Interrupt Descriptor Table)
3. PMM (Physical Memory Manager)
4. VMM (Virtual Memory Manager with 2MB huge pages)
5. Heap (16 MB kernel heap)
6. Scheduler (1 CPU, ticks disabled)
7. PCI (6 devices)
8. Network Stack (Ethernet, ARP, ICMP, TCP, Sockets)
9. Window Manager & Desktop Environment

---

## Development Workflow

### Rebuild Kernel Only
```bash
wsl make -C kernel
```

### Rebuild and Update Disk
```bash
wsl make -C kernel && \
wsl bash -c "cd build && \
  dd if=/dev/zero of=part.img bs=512 count=260063 2>/dev/null && \
  mkfs.fat -F 32 part.img >/dev/null 2>&1 && \
  MTOOLS_SKIP_CHECK=1 mmd -i part.img ::/EFI ::/EFI/BOOT 2>/dev/null && \
  MTOOLS_SKIP_CHECK=1 mcopy -i part.img /tmp/limine/BOOTX64.EFI ::/EFI/BOOT/ && \
  MTOOLS_SKIP_CHECK=1 mcopy -i part.img ../kernel/kernel.elf ::/ && \
  MTOOLS_SKIP_CHECK=1 mcopy -i part.img limine.conf ::/ && \
  dd if=part.img of=disk.img bs=512 seek=2048 conv=notrunc status=none && \
  rm -f part.img"
```

### Quick Test Boot
```bash
wsl bash -c "cd build && timeout 30s qemu-system-x86_64 \
  -bios /usr/share/OVMF/OVMF_CODE.fd \
  -drive file=disk.img,format=raw \
  -m 512M -serial stdio 2>&1 | head -100"
```

---

## Files Reference

### Key Files
- `kernel/kernel.elf` - Compiled kernel binary
- `kernel/kernel.ld` - Linker script (defines memory layout)
- `kernel/hal/x86_64/multiboot2.S` - Multiboot2 header assembly
- `build/limine.conf` - Bootloader configuration
- `build/disk.img` - GPT disk image
- `/tmp/limine/BOOTX64.EFI` - Limine UEFI bootloader

### Linker Script Highlights (`kernel/kernel.ld`)
```ld
OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(multiboot2_entry)

PHDRS {
    boot PT_LOAD FLAGS(5);  /* First LOAD segment */
}

SECTIONS {
    . = 0x100000;

    .multiboot ALIGN(8) : {
        KEEP(*(.multiboot))
    } :boot  /* Critical: in first LOAD segment */

    .text ALIGN(4096) : { ... }
    ...
}
```

---

## Additional Resources

- [Limine Bootloader Documentation](https://github.com/limine-bootloader/limine)
- [Multiboot2 Specification](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html)
- [UEFI Specification](https://uefi.org/specifications)
- [OSDev Wiki - Limine](https://wiki.osdev.org/Limine)

---

**Last Updated:** November 19, 2025
**Scarlett OS Version:** 0.1.0 (Phase 1 - Development)
