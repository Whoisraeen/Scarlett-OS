#!/bin/bash
# ScarlettOS Test Runner
# Runs all tests and reports results

set -e

echo "======================================"
echo "ScarlettOS Test Suite"
echo "======================================"
echo ""

TOTAL_PASSED=0
TOTAL_FAILED=0
TOTAL_TESTS=0

# Function to run a test
run_test() {
    local test_name=$1
    local test_binary=$2
    
    echo "Running: $test_name"
    if [ -f "$test_binary" ]; then
        if $test_binary; then
            echo "  ✓ PASSED"
            TOTAL_PASSED=$((TOTAL_PASSED + 1))
        else
            echo "  ✗ FAILED"
            TOTAL_FAILED=$((TOTAL_FAILED + 1))
        fi
    else
        echo "  ⊘ SKIPPED (binary not found)"
    fi
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo ""
}

# Unit Tests
echo "=== Unit Tests ==="
run_test "Memory Management" "tests/unit/test_memory"
run_test "Scheduler" "tests/unit/test_scheduler"
echo ""

# Integration Tests
echo "=== Integration Tests ==="
run_test "IPC" "tests/integration/test_ipc"
run_test "File System" "tests/integration/test_filesystem"
run_test "Network" "tests/integration/test_network"
echo ""

# System Tests
echo "=== System Tests ==="
run_test "Boot Test" "tests/system/test_boot"
run_test "GUI Test" "tests/system/test_gui"
echo ""

# Summary
echo "======================================"
echo "Test Summary"
echo "======================================"
echo "Total Tests: $TOTAL_TESTS"
echo "Passed:      $TOTAL_PASSED ✓"
echo "Failed:      $TOTAL_FAILED ✗"
echo "======================================"

if [ $TOTAL_FAILED -eq 0 ]; then
    echo "All tests passed!"
    exit 0
else
    echo "$TOTAL_FAILED test(s) failed!"
    exit 1
fi
