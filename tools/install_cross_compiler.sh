#!/bin/bash
# Script to build and install x86_64-elf cross-compiler
# This will take 30-60 minutes depending on your system

set -e

# Configuration
PREFIX="$HOME/opt/cross"
TARGET=x86_64-elf
BINUTILS_VERSION=2.41
GCC_VERSION=13.2.0

# Create directories
mkdir -p ~/src
mkdir -p $PREFIX

echo "==========================================="
echo "Installing x86_64-elf Cross-Compiler"
echo "==========================================="
echo "This will install to: $PREFIX"
echo "Binutils version: $BINUTILS_VERSION"
echo "GCC version: $GCC_VERSION"
echo ""
echo "This will take 30-60 minutes..."
echo "==========================================="
echo ""

# Install dependencies
echo "[1/6] Installing dependencies..."
sudo apt update
sudo apt install -y build-essential bison flex libgmp3-dev libmpc-dev \
    libmpfr-dev texinfo libncurses-dev

# Download binutils
echo "[2/6] Downloading binutils $BINUTILS_VERSION..."
cd ~/src
if [ ! -f "binutils-$BINUTILS_VERSION.tar.xz" ]; then
    wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz
fi
if [ ! -d "binutils-$BINUTILS_VERSION" ]; then
    tar xf binutils-$BINUTILS_VERSION.tar.xz
fi

# Download GCC
echo "[3/6] Downloading GCC $GCC_VERSION..."
if [ ! -f "gcc-$GCC_VERSION.tar.xz" ]; then
    wget https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz
fi
if [ ! -d "gcc-$GCC_VERSION" ]; then
    tar xf gcc-$GCC_VERSION.tar.xz
fi

# Build binutils
echo "[4/6] Building binutils (10-15 minutes)..."
mkdir -p build-binutils
cd build-binutils
../binutils-$BINUTILS_VERSION/configure --target=$TARGET --prefix="$PREFIX" \
    --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
make install

# Build GCC
echo "[5/6] Building GCC (20-40 minutes)..."
cd ~/src
mkdir -p build-gcc
cd build-gcc
../gcc-$GCC_VERSION/configure --target=$TARGET --prefix="$PREFIX" \
    --disable-nls --enable-languages=c,c++ --without-headers
make all-gcc -j$(nproc)
make all-target-libgcc -j$(nproc)
make install-gcc
make install-target-libgcc

# Update PATH
echo "[6/6] Updating PATH..."
export PATH="$PREFIX/bin:$PATH"

# Add to .bashrc if not already there
if ! grep -q "$PREFIX/bin" ~/.bashrc; then
    echo "" >> ~/.bashrc
    echo "# x86_64-elf cross-compiler" >> ~/.bashrc
    echo "export PATH=\"$PREFIX/bin:\$PATH\"" >> ~/.bashrc
    echo "Added $PREFIX/bin to PATH in ~/.bashrc"
fi

echo ""
echo "==========================================="
echo "Installation complete!"
echo "==========================================="
echo ""
echo "Installed tools:"
$PREFIX/bin/$TARGET-gcc --version | head -1
$PREFIX/bin/$TARGET-ld --version | head -1
echo ""
echo "To use the cross-compiler, run:"
echo "  source ~/.bashrc"
echo "Or start a new terminal session."
echo ""
echo "Verify installation with:"
echo "  which x86_64-elf-gcc"
echo "  x86_64-elf-gcc --version"
echo ""
