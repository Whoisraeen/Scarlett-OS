#!/bin/bash
# Build and create Limine bootable ISO

set -e

cd "$(dirname "$0")/.."

echo "[BUILD] Building kernel with Limine support..."
cd kernel
make clean
make
cd ..

echo "[ISO] Creating bootable ISO with Limine..."

# Copy kernel to ISO directory
cp kernel/kernel.elf build/iso/kernel.elf

# Create ISO
xorriso -as mkisofs -b limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    --efi-boot limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image --protective-msdos-label \
    build/iso -o build/scarlett.iso 2>&1 | tail -3

echo ""
echo "Build complete! Boot ISO: build/scarlett.iso"
echo ""
echo "To boot in QEMU:"
echo "  qemu-system-x86_64 -cdrom build/scarlett.iso -m 512M -serial stdio"
echo ""
