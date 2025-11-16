# ARM64 JIT Testing Guide

This document describes how to test the ARM64 JIT implementation to verify it's working correctly and actually running ARM64 code (not x86).

## Test Programs

### 1. `test_arm64_encoder.c` - Static Encoder Tests
Tests individual ARM64 instruction encoders by generating instructions and verifying their hex encodings.

**Purpose**: Verify instruction encoders produce correct machine code
**Status**: ✅ All 11 tests passing

**Build & Run**:
```bash
gcc -I src -o test_arm64_encoder test_arm64_encoder.c
./test_arm64_encoder
```

### 2. `test_arm64_jit.c` - JIT Runtime Execution Test
Tests JIT-generated ARM64 code by actually executing it under QEMU ARM64 emulation.

**Purpose**: Verify generated code executes correctly
**Status**: ✅ All tests passing

**Build & Run**:
```bash
# Cross-compile for ARM64
aarch64-linux-gnu-gcc -static -o test_arm64_jit test_arm64_jit.c

# Run under QEMU ARM64 emulator
qemu-aarch64-static test_arm64_jit
```

**Expected Output**:
```
==============================================
ARM64 JIT Verification Test
==============================================

✓ Compiled for ARM64/AArch64

Step 1: Allocating executable memory...
✓ Allocated at: 0x400000812000

Step 2: Generating ARM64 code...
  Instruction 1: 0x8b010000 (ADD X0, X0, X1)
  Instruction 2: 0xd65f03c0 (RET)
✓ Code generated

Step 3: Flushing instruction cache...
✓ Cache flushed

Step 4: Executing generated code...
  Calling add_func(5, 3)...
  Result: 8 ✓
  Calling add_func(100, 200)...
  Result: 300 ✓
  Calling add_func(0xFFFFFFFF, 1)...
  Result: 0x100000000 ✓

==============================================
✓✓✓ ALL TESTS PASSED ✓✓✓
ARM64 JIT is working correctly!
==============================================
```

### 3. `test_proof_arm64.c` - Proof of ARM64 Execution
Proves beyond doubt that code is running on ARM64, not x86, by using ARM64-specific instructions and system registers.

**Purpose**: Absolute verification that we're on ARM64
**Status**: ✅ All tests passing

**Build & Run**:
```bash
# Cross-compile for ARM64
aarch64-linux-gnu-gcc -static -o test_proof_arm64 test_proof_arm64.c

# Run under QEMU ARM64 emulator
qemu-aarch64-static test_proof_arm64
```

**What it tests**:
1. ✅ Compile-time architecture detection (`__aarch64__`)
2. ✅ ARM64-specific MADD instruction (doesn't exist on x86)
3. ✅ ARM64 system registers (MIDR_EL1)
4. ✅ 64-bit pointer size verification

**Expected Output**:
```
==============================================
PROOF OF ARM64 EXECUTION
==============================================

Test 1: Compile-time Architecture
✓ __aarch64__ is defined
✓ This binary was compiled for ARM64

Test 2: ARM64-specific Instructions
  MADD: 100 + (5 * 3) = 115
✓ ARM64 MADD instruction executed correctly

Test 3: ARM64 System Registers
  MIDR_EL1: 0x00000000000f0510
  Implementer: 0x00 (QEMU)
✓ Successfully read ARM64 system registers

Test 4: ARM64 Pointer Size
✓ 64-bit pointers confirmed

==============================================
✓✓✓ PROOF COMPLETE ✓✓✓
==============================================

CONCLUSION: We are DEFINITELY running on ARM64!
This would be impossible on x86/x86-64.
```

## Verifying Binary Architecture

To confirm a binary is actually ARM64:

```bash
file test_arm64_jit
# Output: ELF 64-bit LSB executable, ARM aarch64, version 1 (GNU/Linux), statically linked
```

## Disassembling Generated Instructions

To verify instruction encodings:

```bash
# Disassemble ADD instruction
printf '\x00\x00\x01\x8b' | aarch64-linux-gnu-objdump -D -b binary -m aarch64 /dev/stdin
# Output:    0:	8b010000 	add	x0, x0, x1

# Disassemble RET instruction
printf '\xc0\x03\x5f\xd6' | aarch64-linux-gnu-objdump -D -b binary -m aarch64 /dev/stdin
# Output:    0:	d65f03c0 	ret
```

## Prerequisites

**On Ubuntu/Debian**:
```bash
sudo apt-get install -y qemu-user-static gcc-aarch64-linux-gnu libc6-dev-arm64-cross
```

**On macOS** (via Homebrew):
```bash
brew install qemu
brew tap messense/macos-cross-toolchains
brew install aarch64-unknown-linux-gnu
```

## Quick Test All

Run all tests in sequence:

```bash
#!/bin/bash
set -e

echo "Building and testing ARM64 JIT..."

# Test 1: Encoder tests
echo "==> Test 1: Encoders"
gcc -I src -o test_arm64_encoder test_arm64_encoder.c
./test_arm64_encoder

# Test 2: JIT runtime
echo ""
echo "==> Test 2: JIT Runtime"
aarch64-linux-gnu-gcc -static -o test_arm64_jit test_arm64_jit.c
qemu-aarch64-static test_arm64_jit

# Test 3: Proof of ARM64
echo ""
echo "==> Test 3: Proof of ARM64"
aarch64-linux-gnu-gcc -static -o test_proof_arm64 test_proof_arm64.c
qemu-aarch64-static test_proof_arm64

echo ""
echo "✅ All ARM64 JIT tests passed!"
```

## Troubleshooting

### Segmentation Fault in QEMU

If you get a segfault when running under QEMU:
- Ensure you're using `-static` linking
- Check that QEMU ARM64 is installed correctly
- Try running with `qemu-aarch64-static -strace` for debugging

### Wrong Architecture Detected

If tests show x86 instead of ARM64:
- Verify you're using `aarch64-linux-gnu-gcc`, not regular `gcc`
- Check with `file <binary>` that it's ARM64
- Ensure you're running with `qemu-aarch64-static`, not regular execution

### Permission Denied for Executable Memory

If mmap fails with permission denied:
- This is expected on some systems with strict W^X policies
- The tests handle this gracefully with error messages

## Test Results Summary

| Test | Purpose | Status | Time |
|------|---------|--------|------|
| test_arm64_encoder | Verify instruction encoding | ✅ PASS | <1s |
| test_arm64_jit | Verify runtime execution | ✅ PASS | <1s |
| test_proof_arm64 | Prove ARM64 (not x86) | ✅ PASS | <1s |

## Next Steps

After all basic tests pass:
1. Test Phase 2 instruction encoders (multiply, divide, bitwise)
2. Test Phase 3 bytecode-to-native mapping
3. Create HashLink bytecode test programs
4. Test with real Haxe-compiled programs

---

**Last Updated**: 2025-11-16
**ARM64 JIT Status**: Phase 3 - Minimal Implementation
**Overall Progress**: ~35% (Phases 0-2 complete, Phase 3 in progress)
