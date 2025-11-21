#!/bin/bash
# ScarlettOS Installation Script

set -e

echo "======================================"
echo "ScarlettOS Installation"
echo "======================================"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Error: This script must be run as root"
    exit 1
fi

# Configuration
INSTALL_DISK="/dev/sda"
MOUNT_POINT="/mnt/scarlettos"
BOOT_SIZE="512M"
ROOT_SIZE="remaining"

echo "WARNING: This will erase all data on $INSTALL_DISK"
read -p "Continue? (yes/no): " confirm
if [ "$confirm" != "yes" ]; then
    echo "Installation cancelled"
    exit 0
fi

echo ""
echo "Step 1: Partitioning disk..."
parted -s $INSTALL_DISK mklabel gpt
parted -s $INSTALL_DISK mkpart ESP fat32 1MiB $BOOT_SIZE
parted -s $INSTALL_DISK set 1 esp on
parted -s $INSTALL_DISK mkpart primary ext4 $BOOT_SIZE 100%

echo "Step 2: Formatting partitions..."
mkfs.fat -F32 ${INSTALL_DISK}1
mkfs.ext4 -F ${INSTALL_DISK}2

echo "Step 3: Mounting partitions..."
mkdir -p $MOUNT_POINT
mount ${INSTALL_DISK}2 $MOUNT_POINT
mkdir -p $MOUNT_POINT/boot
mount ${INSTALL_DISK}1 $MOUNT_POINT/boot

echo "Step 4: Installing bootloader..."
mkdir -p $MOUNT_POINT/boot/EFI/BOOT
cp limine.cfg $MOUNT_POINT/boot/
cp BOOTX64.EFI $MOUNT_POINT/boot/EFI/BOOT/

echo "Step 5: Installing kernel..."
cp kernel.elf $MOUNT_POINT/boot/

echo "Step 6: Installing system files..."
mkdir -p $MOUNT_POINT/{bin,lib,etc,usr,var,tmp,home}
cp -r rootfs/* $MOUNT_POINT/

echo "Step 7: Installing applications..."
cp -r apps/* $MOUNT_POINT/usr/bin/

echo "Step 8: Setting up configuration..."
cat > $MOUNT_POINT/etc/scarlettos.conf << EOF
# ScarlettOS Configuration
hostname=scarlettos
timezone=UTC
locale=en_US.UTF-8
EOF

echo "Step 9: Creating default user..."
mkdir -p $MOUNT_POINT/home/user
cat > $MOUNT_POINT/etc/passwd << EOF
root:x:0:0:root:/root:/bin/sh
user:x:1000:1000:User:/home/user:/bin/sh
EOF

echo "Step 10: Unmounting..."
umount $MOUNT_POINT/boot
umount $MOUNT_POINT
rmdir $MOUNT_POINT

echo ""
echo "======================================"
echo "Installation Complete!"
echo "======================================"
echo ""
echo "ScarlettOS has been installed to $INSTALL_DISK"
echo "You can now reboot your system."
echo ""
