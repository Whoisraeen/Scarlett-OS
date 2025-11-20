# Multi-Architecture Build System

The Scarlett OS build system supports building the kernel for multiple architectures: x86_64, ARM64 (AArch64), and RISC-V 64-bit.

## Architecture Selection

The build system uses the `ARCH` variable to select the target architecture:

```bash
# Build for x86_64 (default)
make ARCH=x86_64 all

# Build for ARM64
make ARCH=arm64 all

# Build for RISC-V 64
make ARCH=riscv64 all
```

If `ARCH` is not specified, it defaults to `x86_64`.

## Build Targets

### Single Architecture Build

```bash
# Build everything for current architecture
make ARCH=x86_64 all

# Build only the kernel
make ARCH=x86_64 kernel

# Build only libraries
make ARCH=x86_64 libs

# Build only services
make ARCH=arm64 services

# Build only drivers
make ARCH=riscv64 drivers
```

### Multi-Architecture Build

```bash
# Build kernel for all architectures
make all-archs

# Or build each architecture separately
make ARCH=x86_64 kernel
make ARCH=arm64 kernel
make ARCH=riscv64 kernel
```

## Architecture-Specific Configuration

### x86_64

- **Toolchain**: `x86_64-elf-gcc`, `x86_64-elf-ld`, `x86_64-elf-as`
- **Rust Target**: `x86_64-unknown-none`
- **QEMU**: `qemu-system-x86_64`
- **Linker Script**: `kernel/kernel.ld`
- **Bootloader**: UEFI (Limine)

### ARM64 (AArch64)

- **Toolchain**: `aarch64-elf-gcc`, `aarch64-elf-ld`, `aarch64-elf-as`
- **Rust Target**: `aarch64-unknown-none`
- **QEMU**: `qemu-system-aarch64`
- **Linker Script**: `kernel/kernel.ld.arm64`
- **Bootloader**: UEFI (Limine) - TODO

### RISC-V 64

- **Toolchain**: `riscv64-elf-gcc`, `riscv64-elf-ld`, `riscv64-elf-as`
- **Rust Target**: `riscv64gc-unknown-none-elf`
- **QEMU**: `qemu-system-riscv64`
- **Linker Script**: `kernel/kernel.ld.riscv64`
- **Bootloader**: TODO

## Required Tools

### Cross-Compilers

Install cross-compilers for each architecture:

```bash
# Ubuntu/Debian
sudo apt install gcc-x86-64-elf gcc-aarch64-elf gcc-riscv64-elf

# Arch Linux
sudo pacman -S x86_64-elf-gcc aarch64-elf-gcc riscv64-elf-gcc

# macOS (via Homebrew)
brew install x86_64-elf-gcc
brew install aarch64-elf-gcc
brew install riscv64-elf-gcc
```

### Rust Targets

Install Rust targets for cross-compilation:

```bash
rustup target add x86_64-unknown-none
rustup target add aarch64-unknown-none
rustup target add riscv64gc-unknown-none-elf
```

### QEMU

Install QEMU for all architectures:

```bash
# Ubuntu/Debian
sudo apt install qemu-system-x86 qemu-system-arm qemu-system-riscv

# Arch Linux
sudo pacman -S qemu-system-x86 qemu-system-aarch64 qemu-system-riscv

# macOS
brew install qemu
```

## Build System Structure

```
kernel/
├── Makefile              # Main kernel Makefile
├── Makefile.arch         # Architecture-specific configuration
├── kernel.ld             # x86_64 linker script
├── kernel.ld.arm64      # ARM64 linker script
├── kernel.ld.riscv64    # RISC-V linker script
└── hal/
    ├── x86_64/          # x86_64 HAL implementation
    ├── arm64/           # ARM64 HAL implementation
    └── riscv/           # RISC-V HAL implementation
```

## Running in QEMU

### x86_64

```bash
make ARCH=x86_64 run
```

### ARM64

```bash
make ARCH=arm64 run
```

### RISC-V 64

```bash
make ARCH=riscv64 run
```

## Debugging

Start QEMU with GDB support:

```bash
make ARCH=x86_64 debug
```

Then connect with GDB:

```bash
gdb kernel/kernel-x86_64.elf
(gdb) target remote localhost:1234
```

## Verification

Verify that all required tools are installed:

```bash
# Check for current architecture
make ARCH=x86_64 verify

# Check for all architectures
make ARCH=x86_64 verify
make ARCH=arm64 verify
make ARCH=riscv64 verify
```

## Output Files

The build system generates architecture-specific output files:

- `kernel/kernel-x86_64.elf` - x86_64 kernel binary
- `kernel/kernel-arm64.elf` - ARM64 kernel binary
- `kernel/kernel-riscv64.elf` - RISC-V 64 kernel binary
- `build/kernel.elf` - Symlink to current architecture's kernel

## Clean Builds

```bash
# Clean current architecture
make ARCH=x86_64 clean

# Clean all architectures
make clean-all
```

## Architecture-Specific Features

### x86_64

- Full feature support
- UEFI bootloader
- Multiboot2 support
- APIC support
- SSE/SSE2 for floating point (in specific files)

### ARM64

- Basic HAL implementation
- GIC (Generic Interrupt Controller) support (TODO)
- UEFI bootloader (TODO)

### RISC-V 64

- Basic HAL implementation
- PLIC (Platform-Level Interrupt Controller) support (TODO)
- Bootloader (TODO)

## Troubleshooting

### Missing Cross-Compiler

If you get errors about missing cross-compilers:

```bash
# Check if compiler exists
which x86_64-elf-gcc

# Install if missing (see Required Tools section)
```

### Wrong Architecture

If you get linker errors, make sure you're using the correct architecture:

```bash
# Check current architecture
echo $ARCH

# Set explicitly
export ARCH=x86_64
make clean
make all
```

### Rust Target Not Installed

If Rust builds fail:

```bash
# List installed targets
rustup target list

# Install missing target
rustup target add <target-name>
```

## Future Enhancements

- [ ] ARM64 bootloader support
- [ ] RISC-V bootloader support
- [ ] Architecture-specific optimization flags
- [ ] CI/CD integration for multi-arch builds
- [ ] Automated testing for all architectures

