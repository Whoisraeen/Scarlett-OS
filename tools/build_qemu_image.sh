#!/bin/bash
# ScarlettOS QEMU Image Builder

set -e

echo "======================================"
echo "ScarlettOS QEMU Image Builder"
echo "======================================"
echo ""

# Configuration
IMAGE_NAME="scarlettos-qemu.img"
IMAGE_SIZE="2G"
MOUNT_POINT="/mnt/scarlettos-build"

echo "Creating disk image..."
qemu-img create -f qcow2 $IMAGE_NAME $IMAGE_SIZE

echo "Creating partitions..."
# Create partition table
cat > part.txt << EOF
label: gpt
size=512M, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B, name="EFI"
type=0FC63DAF-8483-4772-8E79-3D69D8477DE4, name="ROOT"
EOF

sfdisk $IMAGE_NAME < part.txt
rm part.txt

echo "Formatting partitions..."
# Mount image as loop device
sudo losetup -fP $IMAGE_NAME
LOOP_DEV=$(losetup -j $IMAGE_NAME | cut -d: -f1)

# Format partitions
sudo mkfs.fat -F32 ${LOOP_DEV}p1
sudo mkfs.ext4 ${LOOP_DEV}p2

echo "Installing system..."
sudo mkdir -p $MOUNT_POINT
sudo mount ${LOOP_DEV}p2 $MOUNT_POINT
sudo mkdir -p $MOUNT_POINT/boot
sudo mount ${LOOP_DEV}p1 $MOUNT_POINT/boot

# Install bootloader
sudo mkdir -p $MOUNT_POINT/boot/EFI/BOOT
sudo cp limine.cfg $MOUNT_POINT/boot/
sudo cp BOOTX64.EFI $MOUNT_POINT/boot/EFI/BOOT/

# Install kernel
sudo cp kernel.elf $MOUNT_POINT/boot/

# Install system files
sudo mkdir -p $MOUNT_POINT/{bin,lib,etc,usr,var,tmp,home}
sudo cp -r rootfs/* $MOUNT_POINT/ 2>/dev/null || true

# Unmount
sudo umount $MOUNT_POINT/boot
sudo umount $MOUNT_POINT
sudo rmdir $MOUNT_POINT
sudo losetup -d $LOOP_DEV

echo ""
echo "======================================"
echo "QEMU image created: $IMAGE_NAME"
echo "======================================"
echo ""
echo "To run:"
echo "  qemu-system-x86_64 -drive file=$IMAGE_NAME,format=qcow2 -m 2G"
echo ""
