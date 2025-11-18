# Scarlett OS - Build Instructions

## Prerequisites

### Required Tools

1. **Cross-Compiler for x86_64**
   - `x86_64-elf-gcc` (version 13.2 or later)
   - `x86_64-elf-ld`
   - `x86_64-elf-objcopy`

2. **Assembler**
   - `nasm` (version 2.16 or later)

3. **Build Tools**
   - `make` (GNU Make)

4. **Testing**
   - `qemu-system-x86_64` (for emulation)
   - `gdb` (for debugging)

### Installation Guide

See `Docs/DEVELOPMENT_ENVIRONMENT_SETUP.md` for detailed installation instructions for your operating system.

#### Quick Setup (Ubuntu/Debian)

```bash
# Install build dependencies
sudo apt update
sudo apt install build-essential nasm qemu-system-x86 gdb \
                 libgmp-dev libmpfr-dev libmpc-dev texinfo

# Build cross-compiler (see full guide for details)
# Or use pre-built toolchain if available
```

#### Windows Users

We recommend using WSL2 (Windows Subsystem for Linux) for development:

1. Install WSL2 with Ubuntu
2. Follow Ubuntu installation steps above
3. Access the project from `/mnt/c/Users/...`

---

## Building the Kernel

### Option 1: Quick Build

```bash
# From project root
cd kernel
make
```

This will compile the kernel to `kernel/kernel.elf`.

### Option 2: Using Build Script

```bash
# From project root (Linux/WSL)
./tools/build.sh
```

### Build Targets

```bash
# Build kernel
make all

# Clean build artifacts
make clean

# Verbose build
make V=1
```

---

## Running in QEMU

### Option 1: Using Launch Script

```bash
# From project root (Linux/WSL)
./tools/qemu.sh
```

### Option 2: Manual QEMU Launch

```bash
qemu-system-x86_64 \
    -kernel kernel/kernel.elf \
    -m 512M \
    -serial stdio \
    -no-reboot \
    -no-shutdown
```

### QEMU Options Explained

- `-kernel kernel/kernel.elf` - Load kernel via multiboot2
- `-m 512M` - Allocate 512MB RAM
- `-serial stdio` - Redirect serial output to terminal
- `-no-reboot` - Don't reboot on triple fault
- `-no-shutdown` - Don't shutdown, just halt

---

## Debugging

### Option 1: Using Debug Script

```bash
# Terminal 1: Start QEMU with GDB support
./tools/debug.sh

# Terminal 2: Connect GDB
gdb kernel/kernel.elf -ex 'target remote localhost:1234'
```

### Option 2: Manual GDB Setup

```bash
# Terminal 1: Start QEMU with GDB stub
qemu-system-x86_64 \
    -kernel kernel/kernel.elf \
    -m 512M \
    -serial stdio \
    -s \
    -S

# Terminal 2: Launch GDB
gdb kernel/kernel.elf
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

### Useful GDB Commands

```gdb
# Set breakpoint
break kernel_main
break pmm_init

# Continue execution
continue

# Step through code
step
next

# Examine memory
x/10x $rsp
x/s 0xaddress

# Print variables
print boot_info
print *boot_info

# Backtrace
backtrace
bt

# Examine registers
info registers
```

---

## Build Configuration

### Compiler Flags

The kernel is built with the following flags:

```
-std=c11              # C11 standard
-ffreestanding        # Freestanding environment
-nostdlib             # No standard library
-fno-builtin          # No built-in functions
-fno-stack-protector  # No stack protection
-mno-red-zone         # No red zone (required for x86_64 kernel)
-mno-mmx              # No MMX
-mno-sse              # No SSE
-mno-sse2             # No SSE2
-mcmodel=large        # Large code model
-Wall -Wextra         # All warnings
-O2                   # Optimization level 2
-g                    # Debug symbols
```

### Linker Script

The kernel linker script (`kernel/kernel.ld`) places the kernel in the higher half:

- **Virtual Base:** `0xFFFFFFFF80000000`
- **Physical Base:** `0x100000` (1MB)

---

## Expected Output

When running successfully, you should see:

```
====================================================
                  Scarlett OS                       
        A Modern Microkernel Operating System      
====================================================
Version: 0.1.0 (Phase 1 - Development)
Architecture: x86_64
Build: Nov 18 2025 12:34:56
====================================================

[INFO] Verifying boot information...
[INFO] Boot info verified successfully
[INFO] Kernel loaded at: 0xFFFFFFFF80100000 - 0xFFFFFFFF8012ABCD
[INFO] Kernel size: 172 KB
[INFO] BSS section: 0xFFFFFFFF80120000 - 0xFFFFFFFF80125000
[INFO] Bootloader: Scarlett UEFI Bootloader v1.0
[INFO] Framebuffer: 0x00000000FD000000 (1024x768 @ 32 bpp)

Memory Map (X regions):
  Base               Length             Pages        Type
  ---------------------------------------------------------------
  0x0000000000000000 0x000000000009F000 159          Available
  ...
  ---------------------------------------------------------------
  Total Memory:   512 MB
  Usable Memory:  480 MB

[INFO] Initializing GDT...
[INFO] GDT initialized successfully
[INFO] Initializing IDT...
[INFO] IDT initialized successfully
[INFO] Initializing Physical Memory Manager...
[INFO] PMM initialized: 512 MB total, 480 MB free, 32 MB used
[INFO] TODO: Initialize VMM
[INFO] TODO: Initialize heap

[INFO] Kernel initialization complete!
[INFO] System is now idle.
```

---

## Troubleshooting

### Problem: Cross-compiler not found

**Solution:**
```bash
# Check if cross-compiler is in PATH
which x86_64-elf-gcc

# If not found, add to PATH
export PATH="$HOME/opt/cross/bin:$PATH"

# Or install pre-built toolchain
```

### Problem: NASM not found

**Solution:**
```bash
# Ubuntu/Debian
sudo apt install nasm

# Fedora
sudo dnf install nasm

# Check version
nasm -version
```

### Problem: QEMU not found

**Solution:**
```bash
# Ubuntu/Debian
sudo apt install qemu-system-x86

# Fedora
sudo dnf install qemu-system-x86

# Check installation
qemu-system-x86_64 --version
```

### Problem: Build fails with "undefined reference"

**Solution:**
- Make sure all source files are listed in `kernel/Makefile`
- Check that function declarations match definitions
- Verify linker script is correct

### Problem: QEMU boots but no output

**Solution:**
- Make sure serial console is working: check `-serial stdio` flag
- Verify `serial_init()` is called early in kernel
- Try adding debug output before any initialization

### Problem: Kernel crashes/triple faults

**Solution:**
- Use GDB to find crash location
- Check exception handler output
- Verify stack is properly set up
- Check page tables are valid
- Enable QEMU debug output: `-d int,cpu_reset,guest_errors`

---

## Clean Build

If you encounter issues, try a clean build:

```bash
# Clean all build artifacts
make clean

# Clean kernel only
cd kernel && make clean

# Rebuild
make all
```

---

## Testing Checklist

Before committing changes, verify:

- [ ] Code compiles without warnings
- [ ] Kernel boots in QEMU
- [ ] Serial output is visible
- [ ] No triple faults or crashes
- [ ] Memory manager works (check output)
- [ ] Exception handling works (uncomment test in main.c)

---

## Performance Tips

### Faster Builds

```bash
# Use parallel compilation
make -j$(nproc)

# Or specify number of jobs
make -j4
```

### Faster QEMU

```bash
# Enable KVM acceleration (Linux only)
qemu-system-x86_64 \
    -enable-kvm \
    -cpu host \
    ...other flags...
```

---

## Additional Resources

- [OSDev Wiki](https://wiki.osdev.org/)
- [Intel SDM (Software Developer Manual)](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [x86_64 Assembly Reference](https://www.felixcloutier.com/x86/)
- [GDB Documentation](https://www.gnu.org/software/gdb/documentation/)

---

## Next Steps

After successfully building and running:

1. Read `PHASE1_STATUS.md` for implementation details
2. Review `Docs/TECHNICAL_ARCHITECTURE.md` for architecture overview
3. See `Docs/OS_DEVELOPMENT_PLAN.md` for roadmap
4. Begin Phase 2 development

---

**Note:** These instructions are for Phase 1. Full UEFI boot support will be added in Phase 2.

**Last Updated:** November 18, 2025

