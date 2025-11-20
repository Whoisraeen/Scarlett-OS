#!/bin/bash
# Quick Boot Test with Configurable Timeout
# Usage: TIMEOUT=60 ./scripts/boot_test_quick.sh

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=== Quick OS Boot Test ===${NC}\n"

# Configuration
QEMU=qemu-system-x86_64
KERNEL_PATH=kernel/kernel.elf
ISO_PATH=build/scarlett.iso
MEMORY=512M
TIMEOUT=${TIMEOUT:-30}  # Default 30 seconds

# Check QEMU
if ! command -v $QEMU &> /dev/null; then
    echo -e "${RED}Error: QEMU not found.${NC}"
    exit 1
fi

# Determine boot method
if [ -f "$ISO_PATH" ]; then
    BOOT_METHOD="iso"
    BOOT_ARG="-cdrom $ISO_PATH"
    echo -e "${GREEN}Using ISO: $ISO_PATH${NC}"
elif [ -f "$KERNEL_PATH" ]; then
    BOOT_METHOD="kernel"
    BOOT_ARG="-kernel $KERNEL_PATH"
    echo -e "${GREEN}Using kernel: $KERNEL_PATH${NC}"
else
    echo -e "${RED}Error: No bootable image found${NC}"
    exit 1
fi

echo -e "${GREEN}Timeout: ${TIMEOUT} seconds${NC}\n"

# Run QEMU with timeout
timeout $TIMEOUT $QEMU \
    $BOOT_ARG \
    -m "$MEMORY" \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    2>&1 | tee boot_output.log || {
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 124 ]; then
        echo -e "\n${YELLOW}QEMU timed out after ${TIMEOUT} seconds${NC}"
        echo -e "${GREEN}Boot log saved to: boot_output.log${NC}"
        echo -e "${YELLOW}To see more output, increase timeout: TIMEOUT=60 ./scripts/boot_test_quick.sh${NC}"
    else
        echo -e "\n${RED}QEMU exited with code $EXIT_CODE${NC}"
        exit $EXIT_CODE
    fi
}

echo -e "\n${GREEN}=== Boot Test Complete ===${NC}"

