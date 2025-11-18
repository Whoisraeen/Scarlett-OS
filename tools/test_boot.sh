#!/bin/bash
# Quick boot test for Scarlett OS
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ISO="$PROJECT_ROOT/scarlett.iso"
LOG="$PROJECT_ROOT/boot_test.log"

if [ ! -f "$ISO" ]; then
    echo "[ERROR] ISO not found: $ISO"
    exit 1
fi

echo "=========================================="
echo "  Testing Scarlett OS Boot"
echo "=========================================="
echo "Starting QEMU..."

# Run QEMU for 3 seconds and capture output
timeout 3 qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 512M \
    -serial file:"$LOG" \
    -display none \
    -no-reboot \
    -no-shutdown \
    2>/dev/null || true

echo ""
echo "=========================================="
echo "  Boot Log Output:"
echo "=========================================="

if [ -f "$LOG" ]; then
    cat "$LOG"
    echo ""
    echo "=========================================="
    echo "Boot test complete!"
    echo "Log saved to: $LOG"
else
    echo "[WARNING] No boot log generated"
    echo "This might mean the kernel didn't produce output"
fi

