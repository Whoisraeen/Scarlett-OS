#!/bin/bash
# Create bootable ISO for Scarlett OS
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
KERNEL="$PROJECT_ROOT/kernel/kernel.elf"
OUTPUT_ISO="$PROJECT_ROOT/scarlett.iso"
ISO_DIR="$PROJECT_ROOT/isofiles"

echo "[*] Creating bootable ISO for Scarlett OS..."

# Check if kernel exists
if [ ! -f "$KERNEL" ]; then
    echo "[ERROR] Kernel not found: $KERNEL"
    echo "Run: cd kernel && make"
    exit 1
fi

# Clean and create ISO directory structure
rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR/boot/grub"

# Copy kernel
echo "[*] Copying kernel..."
cp "$KERNEL" "$ISO_DIR/boot/scarlett.elf"

# Create GRUB config
echo "[*] Creating GRUB configuration..."
cat > "$ISO_DIR/boot/grub/grub.cfg" << 'EOF'
set timeout=0
set default=0

menuentry "Scarlett OS - Phase 1" {
    multiboot2 /boot/scarlett.elf
    boot
}
EOF

# Create bootable ISO
echo "[*] Creating ISO image..."
grub-mkrescue -o "$OUTPUT_ISO" "$ISO_DIR" 2>/dev/null

# Clean up
rm -rf "$ISO_DIR"

# Check result
if [ -f "$OUTPUT_ISO" ]; then
    echo "[SUCCESS] Bootable ISO created: $OUTPUT_ISO"
    echo ""
    SIZE=$(du -h "$OUTPUT_ISO" | cut -f1)
    echo "ISO Size: $SIZE"
    echo ""
    echo "To test, run:"
    echo "  ./tools/run_qemu.sh"
    echo ""
    echo "Or manually:"
    echo "  qemu-system-x86_64 -cdrom $OUTPUT_ISO -m 512M -serial stdio"
else
    echo "[ERROR] Failed to create ISO"
    exit 1
fi

