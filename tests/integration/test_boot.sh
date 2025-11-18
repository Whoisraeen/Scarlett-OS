#!/bin/bash
# Integration boot test - verify OS boots and initializes correctly

set -e

echo "======================================================"
echo "        Scarlett OS Integration Boot Test"
echo "======================================================"
echo ""

# Configuration
ISO_PATH="../build/scarlett.iso"
BOOT_OUTPUT="boot_output.txt"
TIMEOUT=10

# Check if ISO exists
if [ ! -f "$ISO_PATH" ]; then
    echo "ERROR: ISO not found at $ISO_PATH"
    echo "Please run 'make iso' first"
    exit 1
fi

echo "[1/4] Starting QEMU..."
timeout $TIMEOUT qemu-system-x86_64 \
    -cdrom "$ISO_PATH" \
    -m 512M \
    -serial stdio \
    -display none \
    -no-reboot \
    > "$BOOT_OUTPUT" 2>&1 || true

echo "[2/4] Analyzing boot output..."

# Check for successful initialization messages
TESTS_PASSED=0
TESTS_FAILED=0

check_message() {
    local message="$1"
    local description="$2"

    if grep -q "$message" "$BOOT_OUTPUT"; then
        echo "  ✓ $description"
        ((TESTS_PASSED++))
        return 0
    else
        echo "  ✗ $description"
        ((TESTS_FAILED++))
        return 1
    fi
}

echo "[3/4] Verifying initialization..."

check_message "Scarlett OS" "Boot banner displayed"
check_message "GDT initialized successfully" "GDT initialization"
check_message "IDT initialized successfully" "IDT initialization"
check_message "PMM initialized" "Physical memory manager"
check_message "VMM initialized" "Virtual memory manager" || true  # May not be integrated yet
check_message "Heap initialized" "Kernel heap allocator" || true  # May not be integrated yet
check_message "Scheduler initialized" "Thread scheduler" || true  # Phase 2
check_message "IPC system initialized" "IPC system" || true  # Phase 2
check_message "System calls initialized" "System calls" || true  # Phase 2

# Check for errors/panics
if grep -qi "panic\|error\|fault" "$BOOT_OUTPUT"; then
    echo "  ⚠ Warning: Errors detected in boot output"
    echo ""
    echo "Errors found:"
    grep -i "panic\|error\|fault" "$BOOT_OUTPUT" || true
fi

echo ""
echo "[4/4] Test Summary"
echo "  Tests passed: $TESTS_PASSED"
echo "  Tests failed: $TESTS_FAILED"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo "✓ Integration test PASSED"
    echo ""
    exit 0
else
    echo "✗ Integration test FAILED ($TESTS_FAILED checks failed)"
    echo ""
    echo "Full boot output:"
    cat "$BOOT_OUTPUT"
    echo ""
    exit 1
fi
