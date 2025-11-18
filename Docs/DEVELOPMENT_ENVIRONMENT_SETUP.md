# Development Environment Setup Guide

## Overview

This guide provides step-by-step instructions for setting up a complete development environment for OS development, including cross-compilers, emulators, debuggers, and all necessary tools.

---

## System Requirements

### Recommended Development Machine

**Hardware:**
- CPU: Modern multi-core processor (8+ cores recommended)
- RAM: 32GB+ (16GB minimum)
- Storage: 500GB+ SSD
- Display: Dual monitors recommended (for debugging)

**Operating System:**
- **Primary:** Linux (Ubuntu 22.04 LTS, Fedora 38, or Arch Linux)
- **Alternative:** macOS (with some additional setup)
- **Not recommended:** Windows (use WSL2 if necessary)

---

## 1. Host Operating System Setup

### Option 1: Ubuntu/Debian-based Linux

```bash
# Update package list
sudo apt update && sudo apt upgrade -y

# Install essential build tools
sudo apt install -y \
    build-essential \
    git \
    curl \
    wget \
    vim \
    python3 \
    python3-pip

# Install libraries needed for building cross-compiler
sudo apt install -y \
    libgmp-dev \
    libmpfr-dev \
    libmpc-dev \
    libisl-dev \
    libcloog-isl-dev \
    texinfo \
    bison \
    flex
```

### Option 2: Fedora/RHEL-based Linux

```bash
# Update packages
sudo dnf update -y

# Install development tools
sudo dnf groupinstall -y "Development Tools"
sudo dnf install -y \
    git \
    curl \
    wget \
    vim \
    python3 \
    python3-pip

# Install cross-compiler dependencies
sudo dnf install -y \
    gmp-devel \
    mpfr-devel \
    libmpc-devel \
    isl-devel \
    texinfo \
    bison \
    flex
```

### Option 3: Arch Linux

```bash
# Update system
sudo pacman -Syu

# Install base development tools
sudo pacman -S --needed \
    base-devel \
    git \
    curl \
    wget \
    vim \
    python \
    python-pip

# Install cross-compiler dependencies
sudo pacman -S --needed \
    gmp \
    mpfr \
    libmpc \
    isl \
    texinfo
```

---

## 2. Cross-Compiler Toolchain

### 2.1 Environment Variables

Add to `~/.bashrc` or `~/.zshrc`:

```bash
# OS Development Environment
export PREFIX="$HOME/opt/cross"
export TARGET_X86=x86_64-elf
export TARGET_ARM=aarch64-elf
export TARGET_RISCV=riscv64-elf
export PATH="$PREFIX/bin:$PATH"
```

Apply changes:
```bash
source ~/.bashrc  # or ~/.zshrc
```

### 2.2 Build Binutils

**For x86_64:**

```bash
# Create directory structure
mkdir -p ~/osdev/toolchain
cd ~/osdev/toolchain

# Download binutils
wget https://ftp.gnu.org/gnu/binutils/binutils-2.41.tar.xz
tar xf binutils-2.41.tar.xz

# Create build directory
mkdir build-binutils-x86_64
cd build-binutils-x86_64

# Configure
../binutils-2.41/configure \
    --target=$TARGET_X86 \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror

# Build and install (this takes ~10 minutes)
make -j$(nproc)
make install

# Verify
$TARGET_X86-ld --version
```

**For ARM64:**

```bash
cd ~/osdev/toolchain
mkdir build-binutils-arm64
cd build-binutils-arm64

../binutils-2.41/configure \
    --target=$TARGET_ARM \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror

make -j$(nproc)
make install

# Verify
$TARGET_ARM-ld --version
```

**For RISC-V:**

```bash
cd ~/osdev/toolchain
mkdir build-binutils-riscv
cd build-binutils-riscv

../binutils-2.41/configure \
    --target=$TARGET_RISCV \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror

make -j$(nproc)
make install

# Verify
$TARGET_RISCV-ld --version
```

### 2.3 Build GCC

**For x86_64:**

```bash
cd ~/osdev/toolchain

# Download GCC
wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz
tar xf gcc-13.2.0.tar.xz

# Create build directory
mkdir build-gcc-x86_64
cd build-gcc-x86_64

# Configure
../gcc-13.2.0/configure \
    --target=$TARGET_X86 \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c,c++ \
    --without-headers

# Build and install (this takes ~30-60 minutes)
make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc
make install-gcc
make install-target-libgcc

# Verify
$TARGET_X86-gcc --version
```

**For ARM64:**

```bash
cd ~/osdev/toolchain
mkdir build-gcc-arm64
cd build-gcc-arm64

../gcc-13.2.0/configure \
    --target=$TARGET_ARM \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c,c++ \
    --without-headers

make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc
make install-gcc
make install-target-libgcc

# Verify
$TARGET_ARM-gcc --version
```

**For RISC-V:**

```bash
cd ~/osdev/toolchain
mkdir build-gcc-riscv
cd build-gcc-riscv

../gcc-13.2.0/configure \
    --target=$TARGET_RISCV \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c,c++ \
    --without-headers

make -j$(nproc) all-gcc
make -j$(nproc) all-target-libgcc
make install-gcc
make install-target-libgcc

# Verify
$TARGET_RISCV-gcc --version
```

### 2.4 Alternative: Pre-built Toolchains

If building from source fails, you can use pre-built toolchains:

```bash
# Install pre-built cross-compiler (Ubuntu)
sudo apt install gcc-x86-64-linux-gnu
sudo apt install gcc-aarch64-linux-gnu
sudo apt install gcc-riscv64-linux-gnu

# Note: These are for Linux target, not bare-metal (elf)
# For bare-metal, building from source is recommended
```

---

## 3. Assembly Tools

### NASM (x86/x86_64 assembler)

```bash
# Ubuntu/Debian
sudo apt install nasm

# Fedora
sudo dnf install nasm

# Arch
sudo pacman -S nasm

# Verify
nasm -version
```

### GAS (GNU Assembler - included with binutils)

Already installed with binutils.

---

## 4. Rust Toolchain

### 4.1 Install Rust

```bash
# Install rustup
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# Follow prompts, select default installation

# Add to PATH (usually done automatically)
source "$HOME/.cargo/env"

# Verify
rustc --version
cargo --version
```

### 4.2 Add Bare-Metal Targets

```bash
# x86_64 bare metal
rustup target add x86_64-unknown-none

# ARM64 bare metal
rustup target add aarch64-unknown-none

# RISC-V bare metal
rustup target add riscv64gc-unknown-none-elf

# Verify
rustup target list --installed
```

### 4.3 Install Rust Tools

```bash
# Rust formatter
rustup component add rustfmt

# Rust linter
rustup component add clippy

# Rust source (for IDE support)
rustup component add rust-src
```

---

## 5. Emulation & Virtualization

### 5.1 QEMU

**Ubuntu/Debian:**
```bash
sudo apt install -y \
    qemu-system-x86 \
    qemu-system-arm \
    qemu-system-misc \
    ovmf

# Verify
qemu-system-x86_64 --version
qemu-system-aarch64 --version
qemu-system-riscv64 --version
```

**Fedora:**
```bash
sudo dnf install -y \
    qemu-system-x86 \
    qemu-system-arm \
    qemu-system-riscv \
    edk2-ovmf

# Verify
qemu-system-x86_64 --version
```

**Arch:**
```bash
sudo pacman -S \
    qemu-system-x86 \
    qemu-system-arm \
    qemu-system-riscv \
    edk2-ovmf

# Verify
qemu-system-x86_64 --version
```

**OVMF (UEFI firmware for QEMU):**
- Ubuntu: `/usr/share/OVMF/OVMF_CODE.fd`
- Fedora: `/usr/share/edk2/ovmf/OVMF_CODE.fd`
- Arch: `/usr/share/edk2-ovmf/x64/OVMF_CODE.fd`

### 5.2 VirtualBox (Optional)

```bash
# Ubuntu/Debian
sudo apt install virtualbox

# Fedora
sudo dnf install VirtualBox

# Arch
sudo pacman -S virtualbox
```

### 5.3 KVM (Optional, for performance testing)

```bash
# Ubuntu/Debian
sudo apt install qemu-kvm libvirt-daemon-system

# Fedora
sudo dnf install @virtualization

# Arch
sudo pacman -S qemu libvirt virt-manager

# Add user to kvm group
sudo usermod -aG kvm $USER

# Reboot or re-login
```

---

## 6. Debugging Tools

### 6.1 GDB

```bash
# Ubuntu/Debian
sudo apt install gdb

# Fedora
sudo dnf install gdb

# Arch
sudo pacman -S gdb

# Verify
gdb --version
```

**GDB Configuration for OS debugging (`~/.gdbinit`):**

```gdb
# Enable history
set history save on
set history size 10000
set history filename ~/.gdb_history

# Pretty printing
set print pretty on
set print array on
set print array-indexes on

# Disable confirmation
set confirm off

# Architecture
set architecture i386:x86-64

# Connect to QEMU
define qemu
    target remote localhost:1234
end

# Print page table entry
define pte
    x/1gx $arg0
end
```

### 6.2 LLDB (Alternative to GDB)

```bash
# Ubuntu/Debian
sudo apt install lldb

# Fedora
sudo dnf install lldb

# Arch
sudo pacman -S lldb
```

---

## 7. Build System Tools

### 7.1 Make

Already installed with build-essential.

```bash
# Verify
make --version
```

### 7.2 CMake

```bash
# Ubuntu/Debian
sudo apt install cmake

# Fedora
sudo dnf install cmake

# Arch
sudo pacman -S cmake

# Verify
cmake --version
```

### 7.3 Ninja (Optional, faster than Make)

```bash
# Ubuntu/Debian
sudo apt install ninja-build

# Fedora
sudo dnf install ninja-build

# Arch
sudo pacman -S ninja

# Verify
ninja --version
```

---

## 8. Development Tools

### 8.1 Text Editors / IDEs

**VS Code (Recommended):**
```bash
# Download from https://code.visualstudio.com/
# Or use snap on Ubuntu
sudo snap install code --classic

# Install extensions:
# - C/C++ (Microsoft)
# - rust-analyzer
# - x86 and x86_64 Assembly
# - Remote - SSH (for remote development)
```

**CLion (JetBrains):**
- Download from https://www.jetbrains.com/clion/
- Excellent for C/C++ development
- Paid (free for students/open source)

**Vim/Neovim:**
```bash
# Ubuntu/Debian
sudo apt install neovim

# Fedora
sudo dnf install neovim

# Arch
sudo pacman -S neovim

# Install language servers:
# - clangd for C/C++
# - rust-analyzer for Rust
```

### 8.2 Version Control

**Git:**
Already installed.

**Git Configuration:**
```bash
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
git config --global core.editor "vim"
git config --global init.defaultBranch main
```

---

## 9. Static Analysis & Code Quality

### 9.1 Clang Tools

```bash
# Ubuntu/Debian
sudo apt install \
    clang \
    clang-format \
    clang-tidy \
    clangd

# Fedora
sudo dnf install \
    clang \
    clang-tools-extra

# Arch
sudo pacman -S \
    clang \
    clang-tools-extra
```

### 9.2 Linters

**.clang-format (C/C++ formatting):**
```yaml
# .clang-format
---
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
PointerAlignment: Left
AllowShortFunctionsOnASingleLine: Empty
```

**.clang-tidy (C/C++ linting):**
```yaml
# .clang-tidy
---
Checks: '-*,clang-analyzer-*,bugprone-*,performance-*,readability-*'
```

### 9.3 Valgrind (for user-space code)

```bash
# Ubuntu/Debian
sudo apt install valgrind

# Fedora
sudo dnf install valgrind

# Arch
sudo pacman -S valgrind
```

---

## 10. Documentation Tools

### 10.1 Doxygen

```bash
# Ubuntu/Debian
sudo apt install doxygen graphviz

# Fedora
sudo dnf install doxygen graphviz

# Arch
sudo pacman -S doxygen graphviz
```

**Doxyfile configuration:**
```bash
# Generate default config
doxygen -g Doxyfile

# Edit Doxyfile:
# PROJECT_NAME = "OS Project"
# OUTPUT_DIRECTORY = docs/api
# RECURSIVE = YES
# EXTRACT_ALL = YES
# GENERATE_HTML = YES
```

### 10.2 Markdown Preview

**VS Code:** Built-in markdown preview

**Command-line:**
```bash
# Install grip (GitHub-flavored markdown renderer)
pip3 install grip

# Preview markdown
grip README.md
```

---

## 11. Testing & CI Tools

### 11.1 Docker (for CI/CD)

```bash
# Ubuntu/Debian
sudo apt install docker.io
sudo systemctl start docker
sudo systemctl enable docker
sudo usermod -aG docker $USER

# Fedora
sudo dnf install docker
sudo systemctl start docker
sudo systemctl enable docker
sudo usermod -aG docker $USER

# Arch
sudo pacman -S docker
sudo systemctl start docker
sudo systemctl enable docker
sudo usermod -aG docker $USER

# Reboot or re-login for group changes
```

### 11.2 GitHub Actions / GitLab CI

Configuration files will be in project repository.

---

## 12. Additional Utilities

### 12.1 Hex Editor

```bash
# hexdump (built-in)
hexdump -C file.bin

# xxd (part of vim)
xxd file.bin

# Install hexedit
sudo apt install hexedit  # Ubuntu
sudo dnf install hexedit  # Fedora
sudo pacman -S hexedit    # Arch
```

### 12.2 Disk Image Tools

```bash
# mtools (for FAT filesystems)
sudo apt install mtools     # Ubuntu
sudo dnf install mtools     # Fedora
sudo pacman -S mtools       # Arch

# Create disk image
dd if=/dev/zero of=disk.img bs=1M count=512

# Format as FAT32
mkfs.vfat -F 32 disk.img

# Mount disk image
mkdir -p mnt
sudo mount -o loop disk.img mnt

# Copy files
sudo cp kernel.elf mnt/

# Unmount
sudo umount mnt
```

### 12.3 Serial Console Tools

```bash
# minicom
sudo apt install minicom    # Ubuntu
sudo dnf install minicom    # Fedora
sudo pacman -S minicom      # Arch

# screen (alternative)
# Already installed on most systems

# Usage with QEMU serial output:
screen /dev/pts/X  # where X is the pts number
```

---

## 13. Testing Environment Setup

### 13.1 Create Test Script

**tools/qemu-x86_64.sh:**
```bash
#!/bin/bash

KERNEL="build/kernel.elf"
OVMF_CODE="/usr/share/OVMF/OVMF_CODE.fd"  # Adjust path
DISK="disk.img"

qemu-system-x86_64 \
    -bios "$OVMF_CODE" \
    -drive file="$DISK",format=raw \
    -m 512M \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -s \
    -S  # Remove this for normal boot (this waits for GDB)
```

Make executable:
```bash
chmod +x tools/qemu-x86_64.sh
```

### 13.2 Create GDB Script

**tools/gdb-qemu.sh:**
```bash
#!/bin/bash

gdb -ex "target remote localhost:1234" \
    -ex "symbol-file build/kernel.elf" \
    -ex "break kernel_main" \
    -ex "continue"
```

Make executable:
```bash
chmod +x tools/gdb-qemu.sh
```

---

## 14. Verify Installation

### 14.1 Verification Script

Create `tools/verify-env.sh`:

```bash
#!/bin/bash

echo "Verifying development environment..."

# Cross-compilers
echo -n "x86_64-elf-gcc: "
command -v x86_64-elf-gcc >/dev/null && echo "OK" || echo "MISSING"

echo -n "aarch64-elf-gcc: "
command -v aarch64-elf-gcc >/dev/null && echo "OK" || echo "MISSING"

echo -n "riscv64-elf-gcc: "
command -v riscv64-elf-gcc >/dev/null && echo "OK" || echo "MISSING"

# Assemblers
echo -n "nasm: "
command -v nasm >/dev/null && echo "OK" || echo "MISSING"

# Rust
echo -n "rustc: "
command -v rustc >/dev/null && echo "OK" || echo "MISSING"

# QEMU
echo -n "qemu-system-x86_64: "
command -v qemu-system-x86_64 >/dev/null && echo "OK" || echo "MISSING"

echo -n "qemu-system-aarch64: "
command -v qemu-system-aarch64 >/dev/null && echo "OK" || echo "MISSING"

echo -n "qemu-system-riscv64: "
command -v qemu-system-riscv64 >/dev/null && echo "OK" || echo "MISSING"

# Debugger
echo -n "gdb: "
command -v gdb >/dev/null && echo "OK" || echo "MISSING"

# Build tools
echo -n "make: "
command -v make >/dev/null && echo "OK" || echo "MISSING"

echo -n "cmake: "
command -v cmake >/dev/null && echo "OK" || echo "MISSING"

# Clang tools
echo -n "clang-format: "
command -v clang-format >/dev/null && echo "OK" || echo "MISSING"

# Documentation
echo -n "doxygen: "
command -v doxygen >/dev/null && echo "OK" || echo "MISSING"

echo ""
echo "Verification complete!"
```

Run verification:
```bash
chmod +x tools/verify-env.sh
./tools/verify-env.sh
```

---

## 15. Troubleshooting

### Issue: Cross-compiler build fails

**Solution:**
- Ensure all dependencies installed
- Check binutils built successfully first
- Use `--disable-werror` flag
- Try a different GCC version

### Issue: QEMU doesn't boot

**Solution:**
- Verify OVMF path is correct
- Check disk image is valid
- Try with `-serial stdio` to see boot messages
- Use `-d int,cpu_reset` for debug output

### Issue: GDB can't connect to QEMU

**Solution:**
- Ensure QEMU started with `-s` flag
- Check port 1234 not in use: `netstat -an | grep 1234`
- Try `target remote localhost:1234` in GDB

### Issue: Permission denied for /dev/kvm

**Solution:**
```bash
sudo usermod -aG kvm $USER
# Log out and log back in
```

---

## 16. Quick Start Test

### Create Hello World Kernel

**test/hello.c:**
```c
void kernel_main(void) {
    const char *msg = "Hello, OS Development!";
    volatile unsigned short *vga = (volatile unsigned short*)0xB8000;

    for (int i = 0; msg[i] != '\0'; i++) {
        vga[i] = (unsigned short)msg[i] | 0x0F00;
    }

    while (1) {
        asm("hlt");
    }
}
```

**test/linker.ld:**
```ld
ENTRY(kernel_main)

SECTIONS {
    . = 0x100000;

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
    }
}
```

**Compile and test:**
```bash
# Compile
x86_64-elf-gcc -ffreestanding -c test/hello.c -o test/hello.o

# Link
x86_64-elf-ld -T test/linker.ld test/hello.o -o test/kernel.elf

# Create disk image (multiboot2 loader needed for this simple test)
# Or use with custom bootloader

# Test in QEMU (with multiboot)
qemu-system-x86_64 -kernel test/kernel.elf
```

---

## Appendix A: Recommended VS Code Extensions

- **C/C++** (ms-vscode.cpptools)
- **rust-analyzer** (rust-lang.rust-analyzer)
- **x86 and x86_64 Assembly** (13xforever.language-x86-64-assembly)
- **Hex Editor** (ms-vscode.hexeditor)
- **GitLens** (eamodio.gitlens)
- **Better Comments** (aaron-bond.better-comments)
- **Markdown All in One** (yzhang.markdown-all-in-one)
- **Todo Tree** (gruntfuggly.todo-tree)

---

## Appendix B: Useful Resources

**Documentation:**
- [OSDev Wiki](https://wiki.osdev.org/)
- [Intel Software Developer Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [ARM Architecture Reference Manual](https://developer.arm.com/documentation/)
- [RISC-V Specifications](https://riscv.org/technical/specifications/)
- [UEFI Specification](https://uefi.org/specifications)

**Forums:**
- [OSDev Forums](https://forum.osdev.org/)
- [r/osdev](https://reddit.com/r/osdev)

**Books:**
- "Operating Systems: Three Easy Pieces"
- "Modern Operating Systems" by Tanenbaum
- "Operating System Concepts" by Silberschatz

---

*Document Version: 1.0*
*Last Updated: 2025-11-17*
