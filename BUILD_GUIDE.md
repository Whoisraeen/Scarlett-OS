# OS Build Guide

## Overview
This document provides step-by-step instructions to build and boot the OS, along with a comprehensive list of remaining work items.

## Prerequisites (WSL/Linux)

```bash
# Install required packages
sudo apt-get update
sudo apt-get install -y build-essential bison flex libgmp3-dev libmpc-dev \
    libmpfr-dev texinfo wget curl qemu-system-x86 xorriso mtools dosfstools \
    grub-pc-bin grub-efi-amd64-bin ovmf
```

## Part 1: Build x86_64-elf Cross-Compiler

The OS requires a freestanding cross-compiler to build properly. System GCC is not suitable for kernel development.

### Step 1: Download Source Code

```bash
cd ~
mkdir -p src
cd src

# Download binutils 2.41
wget https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz
tar xf binutils-2.41.tar.xz

# Download GCC 13.2.0
wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz
tar xf gcc-13.2.0.tar.xz

# Download GCC prerequisites
cd gcc-13.2.0
./contrib/download_prerequisites
cd ..
```

### Step 2: Build and Install Binutils

```bash
export TARGET=x86_64-elf
export PREFIX="$HOME/opt/cross"
export PATH="$PREFIX/bin:$PATH"

mkdir -p build-binutils
cd build-binutils

../binutils-2.41/configure \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror

make -j$(nproc)
make install

cd ..
```

### Step 3: Build and Install GCC

```bash
mkdir -p build-gcc
cd build-gcc

../gcc-13.2.0/configure \
    --target=$TARGET \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c \
    --without-headers

make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc
make install-gcc
make install-target-libgcc

cd ~
```

### Step 4: Add to PATH (Permanent)

```bash
# Add to ~/.bashrc
echo 'export PATH="$HOME/opt/cross/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Verify installation
x86_64-elf-gcc --version
x86_64-elf-ld --version
```

**Expected Time**: 30-60 minutes depending on CPU

## Part 2: Build the Kernel

### Navigate to Kernel Directory

```bash
cd /mnt/c/Users/woisr/Downloads/OS/kernel
```

### Clean Previous Build

```bash
wsl bash -c "cd /mnt/c/Users/woisr/Downloads/OS/kernel && make clean"
```

### Build Kernel

```bash
wsl bash -c "cd /mnt/c/Users/woisr/Downloads/OS/kernel && make"
```

### Current Build Status

**STATUS**: Compilation succeeds but linking FAILS

**Compilation**: All C and assembly files compile successfully with x86_64-elf-gcc

**Linking Errors**:
1. R_X86_64_32 relocation truncation in boot code
2. Undefined references to missing functions
3. Duplicate symbol definitions

## Part 3: Create Bootable Disk Image

**NOTE**: This step cannot be completed until linking errors are resolved.

```bash
# After kernel builds successfully, create disk image
make disk

# Or manually:
dd if=/dev/zero of=disk.img bs=1M count=256
sgdisk -n 1:2048:+100M -t 1:ef00 disk.img  # EFI partition
sgdisk -n 2 -t 2:8300 disk.img              # Root partition

# Format partitions
sudo losetup -fP disk.img
sudo mkfs.fat -F32 /dev/loop0p1
sudo mkfs.ext4 /dev/loop0p2

# Install Limine bootloader
sudo mount /dev/loop0p1 /mnt
sudo mkdir -p /mnt/EFI/BOOT
sudo cp kernel/kernel.elf /mnt/
sudo cp limine.cfg /mnt/
sudo cp limine-uefi-cd.bin /mnt/EFI/BOOT/BOOTX64.EFI
sudo umount /mnt
sudo losetup -d /dev/loop0
```

## Part 4: Boot with QEMU

```bash
# Boot with UEFI (OVMF)
qemu-system-x86_64 \
    -drive format=raw,file=disk.img \
    -bios /usr/share/ovmf/OVMF.fd \
    -m 2G \
    -cpu host \
    -enable-kvm \
    -serial stdio
```

---

## REMAINING WORK

## Critical Issues (Must Fix for Successful Build)

### Issue 1: R_X86_64_32 Relocation Truncation

**Location**: `kernel/hal/x86_64/multiboot2.S`

**Error**:
```
relocation truncated to fit: R_X86_64_32 against `.bss'
relocation truncated to fit: R_X86_64_32 against `.rodata'
```

**Root Cause**: Boot code uses 32-bit absolute relocations that cannot reach higher-half kernel address (0xFFFFFFFF80100000)

**Solutions** (Choose One):

1. **Use Position-Independent Code (PIC)**
   - Modify multiboot2.S to use RIP-relative addressing
   - Change all absolute memory references to PC-relative
   - Example: `mov rax, [rel variable]` instead of `mov rax, [variable]`

2. **Two-Stage Boot Process**
   - Keep boot code in lower-half (< 0x100000)
   - Add trampoline code to jump to higher-half kernel
   - Requires separate linker script sections

3. **Use -mcmodel=large Correctly**
   - Ensure all boot code uses 64-bit relocations
   - May require assembly code changes

**Recommended**: Option 1 (PIC) - Most compatible with Limine

### Issue 2: Missing Source Files

**Status**: These files are referenced in Makefile.arch but don't exist in the filesystem

#### Synchronization Primitives (kernel/sync/)

Missing files:
- `kernel/sync/spinlock.c`
- `kernel/sync/mutex.c`
- `kernel/sync/semaphore.c`
- `kernel/sync/rwlock.c`
- `kernel/sync/lockfree.c`

**Action Required**: Create these files with implementations or remove from Makefile.arch line 237-241

#### Security Subsystem (kernel/security/)

Missing file:
- `kernel/security/memory_protection.c`
- `kernel/security/capability.c`
- `kernel/security/rbac.c`
- `kernel/security/sandbox.c`
- `kernel/security/audit.c`

**Status**: Files exist but are referenced in build. Verify functionality.

#### Cryptography (kernel/crypto/)

Missing file:
- `kernel/crypto/crypto.c` (main crypto init/dispatch)

**Status**: Other crypto files exist (bn.c, ecc.c, sha256.c, aes256.c, etc.)

**Action Required**: Create crypto.c wrapper or remove reference

#### Desktop/Graphics (kernel/desktop/)

Missing file:
- `kernel/desktop/bootsplash.c`

**Action Required**: Create bootsplash implementation or remove from Makefile.arch line 271

#### Networking Stack (kernel/net/)

All network files exist and should work:
- network.c, ethernet.c, ip.c, udp.c, tcp.c, arp.c, icmp.c, dns.c, dhcp.c, ping.c, socket.c

**Action Required**: Verify all networking functions are properly implemented

### Issue 3: Duplicate Symbol Definitions

**Error**:
```
multiple definition of `ap_init'
multiple definition of `ap_initiate_startup'
```

**Location**: `kernel/hal/x86_64/ap_startup.c` and possibly `ap_startup.S`

**Root Cause**: ap_startup.o appears to be compiled twice or symbols defined in both .c and .S files

**Action Required**:
- Check if both ap_startup.c and ap_startup.S define the same symbols
- Remove duplicate definitions
- Verify Makefile.arch line 46 and line 60 don't duplicate files

### Issue 4: AHCI Driver Disabled

**Status**: AHCI driver is currently commented out in Makefile.arch line 216

**Reason**: Compilation errors (exact errors not documented)

**Location**: `kernel/drivers/ahci/ahci.c`

**Action Required**:
1. Uncomment line 216: `drivers/ahci/ahci.c`
2. Fix compilation errors
3. Verify AHCI header includes are correct
4. Test SATA disk detection

---

## Medium Priority Issues

### Issue 5: FAT32 Missing Include

**Location**: `kernel/fs/fat32_create.c:136`

**Error**: Include directive appears in middle of function

```c
#include "../include/drivers/rtc.h"  // Line 136 - WRONG LOCATION
```

**Action Required**: Move include to top of file (before line 10)

### Issue 6: Missing SFS Implementation

**Status**: Header exists but implementation is incomplete

**Files**:
- `kernel/include/fs/sfs.h` - Header exists
- `kernel/fs/sfs.c` - Stub implementation exists

**Action Required**:
- Complete SFS (Simple File System) implementation
- Or remove references if not needed

### Issue 7: Compositor IPC Headers

**Status**: New file created but not integrated

**File**: `libs/libgui/include/compositor_ipc.h`

**Action Required**:
- Integrate compositor IPC into libgui
- Update CMakeLists.txt if needed
- Add client implementation

---

## Low Priority Issues

### Issue 8: Cleanup Documentation Files

**Status**: Many old dev documentation files marked for deletion in git

**Files** (All in Docs/Dev/):
- ALL_TODOS.md
- BUILD_SYSTEM.md
- CLEANUP_PLAN.md
- COMPLETE_WORK_SUMMARY.md
- COMPLETION_SUMMARY.md
- COMPLIANCE_*.md (multiple files)
- DRIVER_*.md (multiple files)
- FINAL_*.md (multiple files)
- HARDWARE_IMPLEMENTATION_STATUS.md
- IMPLEMENTATION_*.md (multiple files)
- LOW_PRIORITY_WORK_COMPLETE.md
- PRIORITY_TASKS.md
- PRODUCTION_READINESS_ANALYSIS.md
- REMAINING_*.md (multiple files)
- SERVICE_INTEGRATION_COMPLETE.md

**Action Required**:
```bash
git rm Docs/Dev/*.md
git commit -m "Remove outdated development documentation"
```

### Issue 9: Untracked Files

**New Files**:
- `all_todos.txt`
- `kernel/build_log.txt`
- `kernel/fs/sfs.c`
- `kernel/include/fs/sfs.h`
- `libs/libgui/include/compositor_ipc.h`
- `libs/libgui/src/compositor_client.c`

**Action Required**: Review and either add to git or add to .gitignore

---

## Testing Checklist (After Build Succeeds)

- [ ] Kernel boots with Limine
- [ ] Serial output works
- [ ] VGA/framebuffer display works
- [ ] Memory management initializes
- [ ] Scheduler starts
- [ ] ATA disk detection works
- [ ] AHCI disk detection works (after re-enabling)
- [ ] FAT32 filesystem mounts
- [ ] ext4 filesystem mounts
- [ ] Network card detection
- [ ] Keyboard input works
- [ ] Mouse input works
- [ ] GUI compositor starts
- [ ] Desktop application launches
- [ ] File manager works
- [ ] Terminal emulator works

---

## Build Command Reference

### Quick Build Commands

```bash
# Full clean build
cd /mnt/c/Users/woisr/Downloads/OS/kernel
make clean && make

# Build with verbose output
make V=1

# Build for specific architecture
make ARCH=x86_64

# Create disk image (after kernel builds)
make disk

# Clean everything including disk image
make distclean
```

### Troubleshooting Commands

```bash
# Check if cross-compiler is in PATH
which x86_64-elf-gcc

# Verify cross-compiler works
x86_64-elf-gcc --version

# Check for undefined symbols in object files
x86_64-elf-nm hal/x86_64/multiboot2.o | grep " U "

# Dump relocations
x86_64-elf-objdump -r hal/x86_64/multiboot2.o

# Check linker script
cat kernel.ld | grep -A 5 "SECTIONS"

# View compilation command for specific file
make V=1 | grep "multiboot2"
```

### Git Commands

```bash
# Check current status
git status

# Commit changes
git add .
git commit -m "Description of changes"

# View recent commits
git log --oneline -10

# Discard changes to a file
git checkout -- path/to/file

# Create new branch for experimental changes
git checkout -b fix-boot-code
```

---

## Next Steps (Prioritized)

1. **[CRITICAL]** Fix R_X86_64_32 relocation errors in multiboot2.S
   - This blocks the entire build
   - Must be fixed before kernel can link

2. **[CRITICAL]** Add or remove missing source files from Makefile.arch
   - sync/*.c files
   - crypto/crypto.c
   - desktop/bootsplash.c

3. **[CRITICAL]** Fix duplicate symbol definitions for ap_init/ap_initiate_startup

4. **[HIGH]** Re-enable and fix AHCI driver

5. **[MEDIUM]** Move RTC include to proper location in fat32_create.c

6. **[MEDIUM]** Complete SFS implementation or remove

7. **[LOW]** Clean up old documentation files

8. **[LOW]** Review and commit untracked files

---

## Resources

- [OSDev Wiki](https://wiki.osdev.org/)
- [x86_64 ABI Documentation](https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf)
- [Limine Bootloader Docs](https://github.com/limine-bootloader/limine)
- [GCC Cross-Compiler Tutorial](https://wiki.osdev.org/GCC_Cross-Compiler)
- [Higher Half Kernel Guide](https://wiki.osdev.org/Higher_Half_x86_Bare_Bones)

---

Last Updated: 2025-11-24
