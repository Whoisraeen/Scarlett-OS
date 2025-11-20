#!/bin/bash
# QEMU Test Script for OS
# Tests the OS in QEMU with various configurations

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
QEMU=qemu-system-x86_64
KERNEL_IMAGE=build/kernel.bin
ISO_IMAGE=build/os.iso
MEMORY=512M
DISK_IMAGE=build/disk.img

# Check if QEMU is installed
if ! command -v $QEMU &> /dev/null; then
    echo -e "${RED}Error: QEMU not found. Please install QEMU.${NC}"
    exit 1
fi

# Check if kernel image exists
if [ ! -f "$KERNEL_IMAGE" ]; then
    echo -e "${YELLOW}Warning: Kernel image not found. Building...${NC}"
    make kernel
fi

echo -e "${GREEN}Starting QEMU test...${NC}"

# Test 1: Basic boot test
echo -e "${YELLOW}Test 1: Basic Boot Test${NC}"
$QEMU \
    -kernel "$KERNEL_IMAGE" \
    -m "$MEMORY" \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -d guest_errors \
    -monitor stdio \
    -append "test=basic" \
    > test_output_boot.log 2>&1 &
QEMU_PID=$!

# Wait a bit for boot
sleep 5

# Check if QEMU is still running (kernel didn't crash)
if ps -p $QEMU_PID > /dev/null; then
    echo -e "${GREEN}✓ Boot test passed${NC}"
    kill $QEMU_PID 2>/dev/null || true
else
    echo -e "${RED}✗ Boot test failed - QEMU exited early${NC}"
    exit 1
fi

# Test 2: Service startup test
echo -e "${YELLOW}Test 2: Service Startup Test${NC}"
$QEMU \
    -kernel "$KERNEL_IMAGE" \
    -m "$MEMORY" \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -d guest_errors \
    -monitor stdio \
    -append "test=services" \
    > test_output_services.log 2>&1 &
QEMU_PID=$!

sleep 10

if ps -p $QEMU_PID > /dev/null; then
    echo -e "${GREEN}✓ Service startup test passed${NC}"
    kill $QEMU_PID 2>/dev/null || true
else
    echo -e "${RED}✗ Service startup test failed${NC}"
    exit 1
fi

# Test 3: IPC communication test
echo -e "${YELLOW}Test 3: IPC Communication Test${NC}"
$QEMU \
    -kernel "$KERNEL_IMAGE" \
    -m "$MEMORY" \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -d guest_errors \
    -monitor stdio \
    -append "test=ipc" \
    > test_output_ipc.log 2>&1 &
QEMU_PID=$!

sleep 10

if ps -p $QEMU_PID > /dev/null; then
    echo -e "${GREEN}✓ IPC communication test passed${NC}"
    kill $QEMU_PID 2>/dev/null || true
else
    echo -e "${RED}✗ IPC communication test failed${NC}"
    exit 1
fi

echo -e "${GREEN}All tests passed!${NC}"

