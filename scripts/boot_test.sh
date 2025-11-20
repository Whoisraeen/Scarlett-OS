#!/bin/bash
# Simple Boot Test Script
# Tests kernel boot in QEMU

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=== OS Boot Test ===${NC}\n"

# Configuration
QEMU=qemu-system-x86_64
KERNEL_PATH=build/kernel.elf
KERNEL_DIR=kernel
MEMORY=512M

# Check QEMU
if ! command -v $QEMU &> /dev/null; then
    echo -e "${RED}Error: QEMU not found. Please install QEMU.${NC}"
    echo "Install with: sudo apt install qemu-system-x86"
    exit 1
fi

# Build kernel if needed
if [ ! -f "$KERNEL_PATH" ]; then
    echo -e "${YELLOW}Kernel not found. Building...${NC}"
    cd "$KERNEL_DIR"
    make clean
    make
    cd ..
    cp "$KERNEL_DIR/kernel.elf" "$KERNEL_PATH" 2>/dev/null || cp "$KERNEL_DIR/kernel.elf" build/ 2>/dev/null || true
fi

# Check if kernel exists
if [ ! -f "$KERNEL_PATH" ] && [ -f "$KERNEL_DIR/kernel.elf" ]; then
    KERNEL_PATH="$KERNEL_DIR/kernel.elf"
fi

if [ ! -f "$KERNEL_PATH" ]; then
    echo -e "${RED}Error: Kernel not found at $KERNEL_PATH${NC}"
    echo "Please build the kernel first: cd kernel && make"
    exit 1
fi

echo -e "${GREEN}Kernel found: $KERNEL_PATH${NC}"
echo -e "${GREEN}Starting QEMU...${NC}\n"
echo -e "${YELLOW}Press Ctrl+A then X to exit QEMU${NC}\n"

# Boot with QEMU using multiboot2
$QEMU \
    -kernel "$KERNEL_PATH" \
    -m "$MEMORY" \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -d guest_errors

