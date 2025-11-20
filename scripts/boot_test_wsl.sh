#!/bin/bash
# WSL2 Boot Test Script
# Simple boot test that runs for a few seconds and shows output

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=== OS Boot Test (WSL2) ===${NC}\n"

# Configuration
QEMU=qemu-system-x86_64
KERNEL_PATH=kernel/kernel.elf
MEMORY=512M
TIMEOUT=${TIMEOUT:-30}  # Default 30 seconds, can be overridden

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
echo -e "${GREEN}Starting QEMU (will run for ${TIMEOUT} seconds)...${NC}\n"
echo -e "${YELLOW}Boot output:${NC}\n"

# Run QEMU with timeout and capture output
timeout $TIMEOUT $QEMU \
    -kernel "$KERNEL_PATH" \
    -m "$MEMORY" \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -d guest_errors \
    2>&1 | tee boot_output.log || {
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 124 ]; then
        echo -e "\n${YELLOW}QEMU timed out after ${TIMEOUT} seconds (this is normal for boot tests)${NC}"
    else
        echo -e "\n${RED}QEMU exited with code $EXIT_CODE${NC}"
    fi
}

echo -e "\n${GREEN}=== Boot Test Complete ===${NC}"
echo -e "${YELLOW}If you saw kernel output above, boot was successful!${NC}"
echo -e "${GREEN}Full boot log saved to: boot_output.log${NC}"

