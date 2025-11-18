#!/bin/bash
# Build script for Scarlett OS

set -e

echo "===================================="
echo " Building Scarlett OS"
echo "===================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check for required tools
echo "Checking build environment..."

check_tool() {
    if ! command -v $1 &> /dev/null; then
        echo -e "${RED}ERROR: $1 not found${NC}"
        echo "Please install $1 and try again."
        exit 1
    else
        echo -e "${GREEN}✓${NC} $1 found"
    fi
}

check_tool x86_64-elf-gcc
check_tool x86_64-elf-ld
check_tool nasm

echo ""

# Build kernel
echo "Building kernel..."
cd kernel
make clean
make -j$(nproc)
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓${NC} Kernel built successfully"
else
    echo -e "${RED}✗${NC} Kernel build failed"
    exit 1
fi
cd ..

echo ""
echo -e "${GREEN}Build complete!${NC}"
echo ""
echo "Kernel binary: kernel/kernel.elf"

