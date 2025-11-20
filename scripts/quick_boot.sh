#!/bin/bash
# Quick Boot Test - Minimal setup

KERNEL=kernel/kernel.elf
QEMU=qemu-system-x86_64

# Build kernel if needed
if [ ! -f "$KERNEL" ]; then
    echo "Building kernel..."
    cd kernel && make && cd ..
fi

# Boot
echo "Booting kernel..."
$QEMU -kernel "$KERNEL" -m 512M -serial stdio -no-reboot -no-shutdown

