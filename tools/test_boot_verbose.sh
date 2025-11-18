#!/bin/bash
# Verbose boot test with all debug output
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
ISO="$PROJECT_ROOT/scarlett.iso"

if [ ! -f "$ISO" ]; then
    echo "[ERROR] ISO not found"
    exit 1
fi

echo "Testing Scarlett OS boot with full debug..."
echo ""

qemu-system-x86_64 \
    -cdrom "$ISO" \
    -m 512M \
    -serial file:serial_out.txt \
    -d int,cpu_reset,guest_errors \
    -D qemu_debug.txt \
    -display none \
    -no-reboot & 

QEMU_PID=$!
echo "QEMU PID: $QEMU_PID"
sleep 3

kill $QEMU_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true

echo "=== QEMU Debug Log ==="
if [ -f qemu_debug.txt ]; then
    tail -100 qemu_debug.txt
else
    echo "No debug log"
fi

echo ""
echo "=== Serial Output ==="
if [ -f serial_out.txt ]; then
    cat serial_out.txt
    if [ ! -s serial_out.txt ]; then
        echo "(Serial output is empty)"
    fi
else
    echo "No serial output file"
fi

