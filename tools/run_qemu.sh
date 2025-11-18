#!/bin/bash
# Run Scarlett OS in QEMU
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ISO="$PROJECT_ROOT/scarlett.iso"

if [ ! -f "$ISO" ]; then
    echo "[ERROR] ISO not found: $ISO"
    echo "Run: ./tools/create_iso.sh"
    exit 1
fi

echo "=========================================="
echo "  Booting Scarlett OS in QEMU"
echo "=========================================="
echo "Serial output will appear below."
echo "Press Ctrl+A then X to exit QEMU"
echo "=========================================="
echo ""

qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 512M \
    -serial stdio \
    -no-reboot \
    -no-shutdown

