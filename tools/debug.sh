#!/bin/bash
# Debug script for Scarlett OS with GDB

KERNEL="kernel/kernel.elf"
MEMORY="512M"

# Check if kernel exists
if [ ! -f "$KERNEL" ]; then
    echo "ERROR: Kernel not found at $KERNEL"
    echo "Please build the kernel first."
    exit 1
fi

echo "Starting QEMU with GDB support..."
echo "Connect with: gdb $KERNEL -ex 'target remote localhost:1234'"
echo ""

# Start QEMU with GDB support, waiting for connection
qemu-system-x86_64 \
    -kernel "$KERNEL" \
    -m "$MEMORY" \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -s \
    -S \
    "$@"

