#!/bin/bash
# Simple QEMU test using multiboot

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
KERNEL="$PROJECT_ROOT/kernel/kernel.elf"

if [ ! -f "$KERNEL" ]; then
    echo "[ERROR] Kernel not found: $KERNEL"
    exit 1
fi

echo "[*] Testing Scarlett OS in QEMU..."
echo "[*] Serial output will appear below"
echo "[*] Press Ctrl+C to exit"
echo "=========================================="

# Try with multiboot using -initrd trick
qemu-system-x86_64 \
    -kernel "$KERNEL" \
    -m 512M \
    -serial stdio \
    -no-reboot \
    -no-shutdown \
    -d int,cpu_reset \
    -D /tmp/qemu_debug.log \
    || echo "[ERROR] QEMU failed - check /tmp/qemu_debug.log"

