#!/bin/bash
# Run ARM64 kernel in QEMU

set -e

KERNEL_ELF="kernel/kernel-arm64.elf"
QEMU_CMD="qemu-system-aarch64"

# Check if kernel exists
if [ ! -f "$KERNEL_ELF" ]; then
    echo "Error: Kernel not found at $KERNEL_ELF"
    echo "Build it first with: make ARCH=arm64 kernel"
    exit 1
fi

# Check if QEMU is available
if ! command -v $QEMU_CMD &> /dev/null; then
    echo "Error: $QEMU_CMD not found"
    echo "Install QEMU: sudo apt install qemu-system-arm"
    exit 1
fi

echo "Starting ARM64 kernel in QEMU..."
echo "Kernel: $KERNEL_ELF"
echo ""
echo "Press Ctrl+A then X to exit QEMU"
echo ""

# Run QEMU with virt machine (ARM64)
$QEMU_CMD \
    -M virt \
    -cpu cortex-a72 \
    -smp 4 \
    -m 512M \
    -kernel "$KERNEL_ELF" \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -nographic

