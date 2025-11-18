#!/bin/bash
# Create a bootable disk image for Scarlett OS

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
KERNEL="$PROJECT_ROOT/kernel/kernel.elf"
OUTPUT="$PROJECT_ROOT/scarlett_boot.img"
MOUNT_POINT="/tmp/scarlett_mount"

echo "[*] Creating bootable disk image..."

# Check if kernel exists
if [ ! -f "$KERNEL" ]; then
    echo "[ERROR] Kernel not found: $KERNEL"
    echo "[ERROR] Run 'make' in the kernel directory first"
    exit 1
fi

# Create a 10MB disk image
echo "[*] Creating 10MB disk image..."
dd if=/dev/zero of="$OUTPUT" bs=1M count=10 status=progress

# Create partition table and format
echo "[*] Creating partition..."
parted -s "$OUTPUT" mklabel msdos
parted -s "$OUTPUT" mkpart primary ext2 1MiB 100%
parted -s "$OUTPUT" set 1 boot on

# Set up loop device
echo "[*] Setting up loop device..."
LOOP_DEV=$(sudo losetup --find --show --partscan "$OUTPUT")
PART_DEV="${LOOP_DEV}p1"

# Wait for partition device
sleep 1

# Format partition
echo "[*] Formatting partition..."
sudo mkfs.ext2 -F "$PART_DEV"

# Mount partition
echo "[*] Mounting partition..."
sudo mkdir -p "$MOUNT_POINT"
sudo mount "$PART_DEV" "$MOUNT_POINT"

# Create boot directory structure
echo "[*] Creating boot directories..."
sudo mkdir -p "$MOUNT_POINT/boot/grub"

# Copy kernel
echo "[*] Copying kernel..."
sudo cp "$KERNEL" "$MOUNT_POINT/boot/scarlett.elf"

# Create GRUB config
echo "[*] Creating GRUB configuration..."
cat << 'EOF' | sudo tee "$MOUNT_POINT/boot/grub/grub.cfg" > /dev/null
set timeout=0
set default=0

menuentry "Scarlett OS" {
    multiboot2 /boot/scarlett.elf
    boot
}
EOF

# Install GRUB
echo "[*] Installing GRUB bootloader..."
sudo grub-install --target=i386-pc --boot-directory="$MOUNT_POINT/boot" "$LOOP_DEV"

# Cleanup
echo "[*] Cleaning up..."
sudo umount "$MOUNT_POINT"
sudo rmdir "$MOUNT_POINT"
sudo losetup -d "$LOOP_DEV"

echo "[SUCCESS] Boot disk created: $OUTPUT"
echo ""
echo "To test, run:"
echo "  qemu-system-x86_64 -drive format=raw,file=$OUTPUT -m 512M -serial stdio"

