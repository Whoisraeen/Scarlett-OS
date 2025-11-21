#!/bin/bash
# ScarlettOS VirtualBox Image Builder

set -e

echo "======================================"
echo "ScarlettOS VirtualBox Image Builder"
echo "======================================"
echo ""

# Configuration
VM_NAME="ScarlettOS"
IMAGE_NAME="scarlettos-vbox.vdi"
IMAGE_SIZE="8192"  # 8GB in MB
MEMORY="2048"      # 2GB RAM
CPUS="2"

echo "Creating VirtualBox VM..."
VBoxManage createvm --name "$VM_NAME" --ostype "Linux_64" --register

echo "Configuring VM..."
VBoxManage modifyvm "$VM_NAME" \
    --memory $MEMORY \
    --cpus $CPUS \
    --vram 128 \
    --boot1 disk \
    --boot2 dvd \
    --boot3 none \
    --boot4 none \
    --audio none \
    --usb on \
    --usbehci on

echo "Creating disk image..."
VBoxManage createmedium disk \
    --filename "$IMAGE_NAME" \
    --size $IMAGE_SIZE \
    --format VDI

echo "Attaching storage..."
VBoxManage storagectl "$VM_NAME" \
    --name "SATA Controller" \
    --add sata \
    --controller IntelAhci

VBoxManage storageattach "$VM_NAME" \
    --storagectl "SATA Controller" \
    --port 0 \
    --device 0 \
    --type hdd \
    --medium "$IMAGE_NAME"

echo "Installing system to disk..."
# Mount the VDI and install ScarlettOS
# This would require additional tools or a helper ISO

echo ""
echo "======================================"
echo "VirtualBox VM created: $VM_NAME"
echo "======================================"
echo ""
echo "To start the VM:"
echo "  VBoxManage startvm $VM_NAME"
echo ""
echo "To start with GUI:"
echo "  VBoxManage startvm $VM_NAME --type gui"
echo ""
