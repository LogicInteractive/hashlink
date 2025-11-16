#!/bin/bash
# ARM64 JIT Test Runner
# Compiles and runs all ARM64 JIT tests under QEMU

set -e

echo "========================================"
echo "ARM64 JIT Test Suite Runner"
echo "========================================"
echo ""

COMPILER="aarch64-linux-gnu-gcc"
QEMU="qemu-aarch64-static"
TOTAL_PASSED=0
TOTAL_TESTS=0

# Check if cross-compiler is available
if ! command -v $COMPILER &> /dev/null; then
    echo "Error: $COMPILER not found"
    echo "Install with: sudo apt-get install gcc-aarch64-linux-gnu"
    exit 1
fi

# Check if QEMU is available
if ! command -v $QEMU &> /dev/null; then
    echo "Error: $QEMU not found"
    echo "Install with: sudo apt-get install qemu-user-static"
    exit 1
fi

echo "Compiler: $COMPILER"
echo "Emulator: $QEMU"
echo ""

# Function to run a test
run_test() {
    local test_name=$1
    local test_file="${test_name}.c"
    local test_bin="./${test_name}"
    
    if [ ! -f "$test_file" ]; then
        echo "Warning: $test_file not found, skipping"
        return
    fi
    
    echo "Building $test_name..."
    if ! $COMPILER -o $test_bin $test_file -static 2>&1 | grep -v "warning:"; then
        echo "  ✗ Build failed"
        return
    fi
    
    echo "Running $test_name under QEMU..."
    if $QEMU $test_bin; then
        TOTAL_PASSED=$((TOTAL_PASSED + 1))
    else
        echo "  ✗ Test failed"
    fi
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    # Clean up binary
    rm -f $test_bin
    echo ""
}

# Run all ARM64 JIT tests
echo "========================================"
echo "Running ARM64 JIT Tests"
echo "========================================"
echo ""

# Phase 1 tests
run_test "test_arm64_basic"
run_test "test_shift_encoders"

# Phase 2 tests  
run_test "test_jump_operations"

# Phase 3 tests
run_test "test_memory_ops"
run_test "test_modulo"
run_test "test_field_access"
run_test "test_conversions"

# Latest tests (exceptions and conversions)
run_test "test_exceptions"

echo "========================================"
echo "Final Results"
echo "========================================"
echo "Test suites passed: $TOTAL_PASSED/$TOTAL_TESTS"

if [ $TOTAL_PASSED -eq $TOTAL_TESTS ]; then
    echo "✓ All tests passed!"
    exit 0
else
    echo "✗ Some tests failed"
    exit 1
fi
