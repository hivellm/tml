#!/bin/bash
# Integration test for --out-dir parameter

set -e  # Exit on error

TML_BIN="${TML_BIN:-./build/debug/bin/tml.exe}"
TEST_FILE="packages/compiler/tests/cli/test_out_dir_fixture.tml"
TEST_DIR="test_output_dir"

echo "=== Testing --out-dir parameter ==="

# Clean up previous test artifacts
rm -rf "$TEST_DIR"

# Test 1: Build executable with custom output directory
echo "Test 1: Build executable with --out-dir"
$TML_BIN build "$TEST_FILE" --out-dir="$TEST_DIR"
if [ -f "$TEST_DIR/test_out_dir.exe" ] || [ -f "$TEST_DIR/test_out_dir" ]; then
    echo "✓ Executable created in custom directory"
else
    echo "✗ Executable NOT found in custom directory"
    exit 1
fi

# Clean up
rm -rf "$TEST_DIR"

# Test 2: Build static library with custom output directory
echo "Test 2: Build static library with --out-dir"
$TML_BIN build "$TEST_FILE" --crate-type=lib --out-dir="$TEST_DIR"
if [ -f "$TEST_DIR/test_out_dir.lib" ] || [ -f "$TEST_DIR/libtest_out_dir.a" ]; then
    echo "✓ Static library created in custom directory"
else
    echo "✗ Static library NOT found in custom directory"
    exit 1
fi

# Clean up
rm -rf "$TEST_DIR"

# Test 3: Build library with header in custom directory
echo "Test 3: Build library with header using --out-dir"
$TML_BIN build "$TEST_FILE" --crate-type=lib --emit-header --out-dir="$TEST_DIR"
if [ -f "$TEST_DIR/test_out_dir.h" ]; then
    echo "✓ Header file created in custom directory"
else
    echo "✗ Header file NOT found in custom directory"
    exit 1
fi

if [ -f "$TEST_DIR/test_out_dir.lib" ] || [ -f "$TEST_DIR/libtest_out_dir.a" ]; then
    echo "✓ Library created alongside header in custom directory"
else
    echo "✗ Library NOT found in custom directory"
    exit 1
fi

# Clean up
rm -rf "$TEST_DIR"

echo ""
echo "=== All tests passed ✓ ==="
