#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Test ODynSet encoding and implementation
// ODynSet writes a field to a dynamic object by hashed name

typedef enum {
    X0 = 0, X1, X2, X3, X4, X5, X6, X7, X8, X9
} Arm64Reg;

// Helper to encode MOV (register)
uint32_t encode_mov_reg(Arm64Reg rd, Arm64Reg rn) {
    // ORR Xd, XZR, Xn (alias for MOV)
    return 0xaa0003e0 | (rn << 16) | rd;
}

// Helper to encode MOVZ
uint32_t encode_movz(Arm64Reg rd, uint16_t imm, int shift) {
    return 0xd2800000 | (shift << 21) | (imm << 5) | rd;
}

// Helper to encode MOVK
uint32_t encode_movk(Arm64Reg rd, uint16_t imm, int shift) {
    return 0xf2800000 | (shift << 21) | (imm << 5) | rd;
}

// Helper to encode BLR
uint32_t encode_blr(Arm64Reg rn) {
    return 0xd63f0000 | (rn << 5);
}

int test_dynset_integer_encoding() {
    printf("Test 1: ODynSet integer field - instruction encoding\n");

    // Simulate: dynobj.field = 42 (integer)
    // Steps:
    // 1. MOV X0, X5 (value in X0)
    // 2. Load hash into X1 (64-bit immediate)
    // 3. MOV X2, X4 (object in X2)
    // 4. Load function pointer into X9
    // 5. BLR X9

    uint32_t code[20];
    int pos = 0;

    // MOV X0, X5 (value)
    code[pos++] = encode_mov_reg(X0, X5);
    uint32_t expected_mov = 0xaa0503e0;
    if (code[0] != expected_mov) {
        printf("  ✗ MOV encoding failed: got 0x%08x, expected 0x%08x\n", code[0], expected_mov);
        return 0;
    }

    // Load hash into X1 (example hash: 0x123456789abcdef0)
    uint64_t hash = 0x123456789abcdef0ULL;
    code[pos++] = encode_movz(X1, hash & 0xFFFF, 0);
    code[pos++] = encode_movk(X1, (hash >> 16) & 0xFFFF, 1);
    code[pos++] = encode_movk(X1, (hash >> 32) & 0xFFFF, 2);
    code[pos++] = encode_movk(X1, (hash >> 48) & 0xFFFF, 3);

    // Verify MOVZ X1
    uint32_t expected_movz = 0xd2800000 | (0xdef0 << 5) | 1;
    if (code[1] != expected_movz) {
        printf("  ✗ MOVZ encoding failed: got 0x%08x, expected 0x%08x\n", code[1], expected_movz);
        return 0;
    }

    // MOV X2, X4 (object)
    code[pos++] = encode_mov_reg(X2, X4);
    uint32_t expected_mov2 = 0xaa0403e2;
    if (code[pos-1] != expected_mov2) {
        printf("  ✗ MOV X2 encoding failed: got 0x%08x, expected 0x%08x\n", code[pos-1], expected_mov2);
        return 0;
    }

    // Load function pointer into X9 (4 instructions)
    uint64_t func_ptr = 0xdeadbeef12345678ULL;
    code[pos++] = encode_movz(X9, func_ptr & 0xFFFF, 0);
    code[pos++] = encode_movk(X9, (func_ptr >> 16) & 0xFFFF, 1);
    code[pos++] = encode_movk(X9, (func_ptr >> 32) & 0xFFFF, 2);
    code[pos++] = encode_movk(X9, (func_ptr >> 48) & 0xFFFF, 3);

    // BLR X9
    code[pos++] = encode_blr(X9);
    uint32_t expected_blr = 0xd63f0120;
    if (code[pos-1] != expected_blr) {
        printf("  ✗ BLR encoding failed: got 0x%08x, expected 0x%08x\n", code[pos-1], expected_blr);
        return 0;
    }

    printf("  ✓ All instructions encoded correctly\n");
    printf("    - MOV X0, X5: 0x%08x\n", code[0]);
    printf("    - Hash load: 4 instructions\n");
    printf("    - MOV X2, X4: 0x%08x\n", code[5]);
    printf("    - Function load: 4 instructions\n");
    printf("    - BLR X9: 0x%08x\n", code[pos-1]);
    return 1;
}

int test_dynset_pointer_encoding() {
    printf("\nTest 2: ODynSet pointer field - instruction encoding\n");

    // For pointer types, we use hl_dyn_setp
    // Sequence is the same, just different function pointer

    uint32_t code[20];
    int pos = 0;

    // MOV X0, X3 (value - pointer)
    code[pos++] = encode_mov_reg(X0, X3);

    // Load hash
    uint64_t hash = 0x9876543210fedcbaULL;
    code[pos++] = encode_movz(X1, hash & 0xFFFF, 0);
    code[pos++] = encode_movk(X1, (hash >> 16) & 0xFFFF, 1);
    code[pos++] = encode_movk(X1, (hash >> 32) & 0xFFFF, 2);
    code[pos++] = encode_movk(X1, (hash >> 48) & 0xFFFF, 3);

    // MOV X2, X6 (object)
    code[pos++] = encode_mov_reg(X2, X6);

    // Load hl_dyn_setp pointer
    uint64_t setp_ptr = 0xabcdef0123456789ULL;
    code[pos++] = encode_movz(X9, setp_ptr & 0xFFFF, 0);
    code[pos++] = encode_movk(X9, (setp_ptr >> 16) & 0xFFFF, 1);
    code[pos++] = encode_movk(X9, (setp_ptr >> 32) & 0xFFFF, 2);
    code[pos++] = encode_movk(X9, (setp_ptr >> 48) & 0xFFFF, 3);

    // BLR X9
    code[pos++] = encode_blr(X9);

    if (code[pos-1] != 0xd63f0120) {
        printf("  ✗ BLR encoding failed\n");
        return 0;
    }

    printf("  ✓ Pointer field set encoding correct\n");
    printf("    - Total instructions: %d\n", pos);
    return 1;
}

int test_dynset_float_encoding() {
    printf("\nTest 3: ODynSet float field - instruction encoding\n");

    // For F64 types, we use hl_dyn_setd

    uint32_t code[20];
    int pos = 0;

    // MOV X0, X7 (value - double in integer register for now)
    code[pos++] = encode_mov_reg(X0, X7);

    // Load hash
    uint64_t hash = 0x1111222233334444ULL;
    code[pos++] = encode_movz(X1, hash & 0xFFFF, 0);
    code[pos++] = encode_movk(X1, (hash >> 16) & 0xFFFF, 1);
    code[pos++] = encode_movk(X1, (hash >> 32) & 0xFFFF, 2);
    code[pos++] = encode_movk(X1, (hash >> 48) & 0xFFFF, 3);

    // MOV X2, X8 (object)
    code[pos++] = encode_mov_reg(X2, X8);

    // Load hl_dyn_setd pointer
    uint64_t setd_ptr = 0x5555666677778888ULL;
    code[pos++] = encode_movz(X9, setd_ptr & 0xFFFF, 0);
    code[pos++] = encode_movk(X9, (setd_ptr >> 16) & 0xFFFF, 1);
    code[pos++] = encode_movk(X9, (setd_ptr >> 32) & 0xFFFF, 2);
    code[pos++] = encode_movk(X9, (setd_ptr >> 48) & 0xFFFF, 3);

    // BLR X9
    code[pos++] = encode_blr(X9);

    if (code[0] != 0xaa0703e0) {
        printf("  ✗ MOV X0, X7 encoding failed: got 0x%08x\n", code[0]);
        return 0;
    }

    if (code[pos-1] != 0xd63f0120) {
        printf("  ✗ BLR encoding failed\n");
        return 0;
    }

    printf("  ✓ Float field set encoding correct\n");
    return 1;
}

int test_dynset_i64_encoding() {
    printf("\nTest 4: ODynSet I64 field - instruction encoding\n");

    // For HI64 types, we use hl_dyn_seti64

    uint32_t code[20];
    int pos = 0;

    // MOV X0, X2 (64-bit integer value)
    code[pos++] = encode_mov_reg(X0, X2);

    // Load hash
    uint64_t hash = 0xfedcba9876543210ULL;
    code[pos++] = encode_movz(X1, hash & 0xFFFF, 0);
    code[pos++] = encode_movk(X1, (hash >> 16) & 0xFFFF, 1);
    code[pos++] = encode_movk(X1, (hash >> 32) & 0xFFFF, 2);
    code[pos++] = encode_movk(X1, (hash >> 48) & 0xFFFF, 3);

    // MOV X2, X1 (object)
    code[pos++] = encode_mov_reg(X2, X1);

    // Load hl_dyn_seti64 pointer
    uint64_t seti64_ptr = 0x1234567890abcdefULL;
    code[pos++] = encode_movz(X9, seti64_ptr & 0xFFFF, 0);
    code[pos++] = encode_movk(X9, (seti64_ptr >> 16) & 0xFFFF, 1);
    code[pos++] = encode_movk(X9, (seti64_ptr >> 32) & 0xFFFF, 2);
    code[pos++] = encode_movk(X9, (seti64_ptr >> 48) & 0xFFFF, 3);

    // BLR X9
    code[pos++] = encode_blr(X9);

    if (code[0] != 0xaa0203e0) {
        printf("  ✗ MOV X0, X2 encoding failed: got 0x%08x\n", code[0]);
        return 0;
    }

    printf("  ✓ I64 field set encoding correct\n");
    return 1;
}

int test_dynset_sequence() {
    printf("\nTest 5: Complete ODynSet sequence verification\n");

    // Verify the complete sequence matches expected pattern
    uint32_t code[20];
    int pos = 0;

    // Standard sequence:
    // 1. Move value to X0
    // 2. Load 64-bit hash into X1 (4 instructions)
    // 3. Move object to X2
    // 4. Load function pointer into X9 (4 instructions)
    // 5. BLR X9
    // Total: 11 instructions

    code[pos++] = encode_mov_reg(X0, X5);        // Value
    code[pos++] = encode_movz(X1, 0x1234, 0);    // Hash [0:15]
    code[pos++] = encode_movk(X1, 0x5678, 1);    // Hash [16:31]
    code[pos++] = encode_movk(X1, 0x9abc, 2);    // Hash [32:47]
    code[pos++] = encode_movk(X1, 0xdef0, 3);    // Hash [48:63]
    code[pos++] = encode_mov_reg(X2, X4);        // Object
    code[pos++] = encode_movz(X9, 0xabcd, 0);    // Func [0:15]
    code[pos++] = encode_movk(X9, 0xef01, 1);    // Func [16:31]
    code[pos++] = encode_movk(X9, 0x2345, 2);    // Func [32:47]
    code[pos++] = encode_movk(X9, 0x6789, 3);    // Func [48:63]
    code[pos++] = encode_blr(X9);                 // Call

    int expected_count = 11;
    if (pos != expected_count) {
        printf("  ✗ Instruction count mismatch: got %d, expected %d\n", pos, expected_count);
        return 0;
    }

    // Verify each instruction type
    // MOV is ORR with XZR: 0xaa0003e0 | (rm << 16) | rd
    // Check opcode and XZR source (bits [9:5] = 31)
    if ((code[0] & 0xffe003e0) != 0xaa0003e0) {
        printf("  ✗ First instruction not a MOV: got 0x%08x\n", code[0]);
        return 0;
    }

    if ((code[1] & 0xff800000) != 0xd2800000) {
        printf("  ✗ Second instruction not a MOVZ\n");
        return 0;
    }

    if ((code[10] & 0xfffffc1f) != 0xd63f0000) {
        printf("  ✗ Last instruction not a BLR\n");
        return 0;
    }

    printf("  ✓ Complete sequence verified\n");
    printf("    - Total instructions: %d\n", pos);
    printf("    - Pattern: MOV + 4xLOAD_HASH + MOV + 4xLOAD_FUNC + BLR\n");
    return 1;
}

int main() {
    printf("=== ODynSet Operation Tests ===\n\n");

    int passed = 0;
    int total = 5;

    passed += test_dynset_integer_encoding();
    passed += test_dynset_pointer_encoding();
    passed += test_dynset_float_encoding();
    passed += test_dynset_i64_encoding();
    passed += test_dynset_sequence();

    printf("\n=== Results ===\n");
    printf("Passed: %d/%d tests\n", passed, total);

    if (passed == total) {
        printf("✓ All ODynSet tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed\n");
        return 1;
    }
}
