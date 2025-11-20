#!/bin/bash
# WSL2 Boot Test using GRUB
# Creates a minimal bootable disk with GRUB

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}=== OS Boot Test (GRUB) ===${NC}\n"

# Configuration
KERNEL_PATH=kernel/kernel.elf
BUILD_DIR=build
DISK_IMG=$BUILD_DIR/boot_test.img
GRUB_DIR=$BUILD_DIR/grub

# Check dependencies
if ! command -v grub-mkrescue &> /dev/null && ! command -v grub2-mkrescue &> /dev/null; then
    echo -e "${YELLOW}GRUB not found. Trying direct QEMU boot...${NC}"
    # Fall back to direct boot
    qemu-system-x86_64 \
        -kernel "$KERNEL_PATH" \
        -m 512M \
        -serial stdio \
        -no-reboot \
        -no-shutdown \
        -append "console=ttyS0" || true
    exit 0
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

echo -e "${GREEN}Creating bootable disk image...${NC}"

# Create directories
mkdir -p $BUILD_DIR
mkdir -p $GRUB_DIR/boot/grub

# Copy kernel
cp "$KERNEL_PATH" $GRUB_DIR/boot/

# Create GRUB config
cat > $GRUB_DIR/boot/grub/grub.cfg <<EOF
set timeout=0
set default=0

menuentry "Scarlett OS" {
    multiboot2 /boot/kernel.elf
    boot
}
EOF

# Create ISO
echo -e "${GREEN}Creating ISO...${NC}"
GRUB_CMD=grub-mkrescue
if ! command -v $GRUB_CMD &> /dev/null; then
    GRUB_CMD=grub2-mkrescue
fi

$GRUB_CMD -o $DISK_IMG $GRUB_DIR 2>/dev/null || {
    echo -e "${YELLOW}GRUB ISO creation failed. Trying direct boot...${NC}"
    qemu-system-x86_64 \
        -kernel "$KERNEL_PATH" \
        -m 512M \
        -serial stdio \
        -no-reboot \
        -no-shutdown || true
    exit 0
}

echo -e "${GREEN}Booting from ISO...${NC}"
echo -e "${YELLOW}Press Ctrl+C to exit${NC}\n"

# Configuration
TIMEOUT=${TIMEOUT:-30}  # Default 30 seconds, can be overridden

# Boot from ISO with timeout to allow tests to complete
timeout $TIMEOUT qemu-system-x86_64 \
    -cdrom $DISK_IMG \
    -m 512M \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    2>&1 || {
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 124 ]; then
        echo -e "\n${YELLOW}QEMU timed out after ${TIMEOUT} seconds (this is normal for boot tests)${NC}"
        echo -e "${GREEN}If you saw kernel output above, boot was successful!${NC}"
    else
        echo -e "\n${RED}QEMU exited with code $EXIT_CODE${NC}"
        exit $EXIT_CODE
    fi
}

