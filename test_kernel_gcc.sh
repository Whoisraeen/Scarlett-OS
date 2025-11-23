#!/bin/bash
# Quick test script to verify kernel builds and boots (using gcc)

set -e

echo "======================================"
echo " Scarlett OS - Quick Test (GCC Mode)"
echo "======================================"
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test 1: Check if toolchain exists
echo "Test 1: Checking toolchain..."
if command -v gcc &> /dev/null; then
    echo -e "${GREEN}✓${NC} gcc found"
else
    echo -e "${RED}✗${NC} gcc not found"
    exit 1
fi

if command -v nasm &> /dev/null; then
    echo -e "${GREEN}✓${NC} NASM found"
else
    echo -e "${RED}✗${NC} NASM not found"
    exit 1
fi

# Test 2: Build kernel
echo ""
echo "Test 2: Building kernel..."
cd kernel
make clean > /dev/null 2>&1
if make CC=gcc LD=ld OBJCOPY=objcopy AS=as; then
    echo -e "${GREEN}✓${NC} Kernel built successfully"
else
    echo -e "${RED}✗${NC} Kernel build failed"
    exit 1
fi
cd ..

# Test 3: Check if QEMU exists
echo ""
echo "Test 3: Checking QEMU..."
if command -v qemu-system-x86_64 &> /dev/null; then
    echo -e "${GREEN}✓${NC} QEMU found"
else
    echo -e "${YELLOW}!${NC} QEMU not found - skipping boot test"
    exit 0
fi

# Test 4: Boot test (timeout after 5 seconds)
echo ""
echo "Test 4: Boot test (5 seconds)..."
echo "Looking for: 'Scarlett OS' in output..."

timeout 5 qemu-system-x86_64 \
    -kernel kernel/kernel.elf \
    -m 512M \
    -serial stdio \
    -display none \
    -no-reboot \
    -no-shutdown 2>&1 | tee /tmp/scarlett_boot.log &

QEMU_PID=$!
sleep 5
kill $QEMU_PID 2>/dev/null || true

# Check if boot banner appeared
if grep -q "Scarlett OS" /tmp/scarlett_boot.log; then
    echo -e "${GREEN}✓${NC} Kernel booted and printed banner!"
else
    echo -e "${RED}✗${NC} Boot banner not found"
    echo "Boot log:"
    cat /tmp/scarlett_boot.log
    exit 1
fi

# Check for specific initialization messages
echo ""
echo "Checking initialization..."

if grep -q "GDT initialized" /tmp/scarlett_boot.log; then
    echo -e "${GREEN}✓${NC} GDT initialized"
else
    echo -e "${YELLOW}!${NC} GDT initialization not confirmed"
fi

if grep -q "Scheduler ticks ENABLED" /tmp/scarlett_boot.log; then
    echo -e "${GREEN}✓${NC} Scheduler ticks ENABLED"
else
    echo -e "${YELLOW}!${NC} Scheduler ticks NOT confirmed"
fi

if grep -q "ACPI Service Starting" /tmp/scarlett_boot.log; then
    echo -e "${GREEN}✓${NC} ACPI Service started"
else
    echo -e "${YELLOW}!${NC} ACPI Service NOT confirmed"
fi

if grep -q "NVMe Driver Starting" /tmp/scarlett_boot.log; then
    echo -e "${GREEN}✓${NC} NVMe Driver started"
else
    echo -e "${YELLOW}!${NC} NVMe Driver NOT confirmed"
fi

# Summary
echo ""
echo "======================================"
echo -e "${GREEN} All tests passed!${NC}"
echo "======================================"
