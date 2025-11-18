#!/bin/bash
# Test boot with VGA window visible
cd "$(dirname "$0")/.."

echo "Booting Scarlett OS - Check VGA window for output!"
echo "Look for: SCP64 on screen"
echo "Press Ctrl+C to exit"
echo ""

qemu-system-x86_64 \
    -cdrom scarlett.iso \
    -m 512M \
    -serial stdio \
    -no-reboot

