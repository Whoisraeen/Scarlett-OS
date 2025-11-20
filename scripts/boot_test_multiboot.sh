#!/bin/bash
# WSL2 Boot Test with Multiboot2
# Uses QEMU's multiboot2 support

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=== OS Boot Test (Multiboot2) ===${NC}\n"

# Configuration
QEMU=qemu-system-x86_64
KERNEL_PATH=kernel/kernel.elf
MEMORY=512M

# Check QEMU
if ! command -v $QEMU &> /dev/null; then
    echo -e "${RED}Error: QEMU not found.${NC}"
    echo "Install with: sudo apt install qemu-system-x86"
    exit 1
fi

# Check kernel
if [ ! -f "$KERNEL_PATH" ]; then
    echo -e "${YELLOW}Kernel not found. Building...${NC}"
    cd kernel
    make clean
    make
    cd ..
fi

if [ ! -f "$KERNEL_PATH" ]; then
    echo -e "${RED}Error: Kernel build failed${NC}"
    exit 1
fi

echo -e "${GREEN}Kernel: $KERNEL_PATH${NC}"
echo -e "${GREEN}Starting QEMU with multiboot2...${NC}\n"
echo -e "${YELLOW}Press Ctrl+C to exit${NC}\n"

# Boot with multiboot2 (QEMU supports this directly)
$QEMU \
    -kernel "$KERNEL_PATH" \
    -m "$MEMORY" \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -machine type=q35 \
    -cpu qemu64

