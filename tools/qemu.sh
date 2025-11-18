#!/bin/bash
# QEMU launch script for Scarlett OS

# Configuration
KERNEL="kernel/kernel.elf"
MEMORY="512M"
OVMF="/usr/share/OVMF/OVMF_CODE.fd"

# Check if OVMF exists in alternative locations
if [ ! -f "$OVMF" ]; then
    OVMF="/usr/share/edk2-ovmf/x64/OVMF_CODE.fd"
fi

if [ ! -f "$OVMF" ]; then
    OVMF="/usr/share/edk2/ovmf/OVMF_CODE.fd"
fi

if [ ! -f "$OVMF" ]; then
    echo "ERROR: OVMF firmware not found!"
    echo "Please install OVMF or update OVMF path in this script."
    exit 1
fi

# Check if kernel exists
if [ ! -f "$KERNEL" ]; then
    echo "ERROR: Kernel not found at $KERNEL"
    echo "Please build the kernel first."
    exit 1
fi

echo "Starting QEMU..."
echo "OVMF: $OVMF"
echo "Memory: $MEMORY"
echo ""

# For now, boot with multiboot2 (simpler for testing)
# TODO: Create proper UEFI bootloader
qemu-system-x86_64 \
    -kernel "$KERNEL" \
    -m "$MEMORY" \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -d int,cpu_reset,guest_errors \
    "$@"

