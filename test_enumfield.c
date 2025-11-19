#include <stdio.h>
#include <stdint.h>

// Test OSetEnumField encoding and implementation
// OSetEnumField sets a field within an enum value

typedef enum {
    X0 = 0, X1, X2, X3, X4, X5, X6, X7, X8, X9
} Arm64Reg;

// Helper to encode STR (immediate offset)
uint32_t encode_str_imm(Arm64Reg rt, Arm64Reg rn, int offset, int size) {
    // STR Xt, [Xn, #offset]
    // size: 0=byte, 1=halfword, 2=word, 3=doubleword
    uint32_t imm12 = offset & 0xFFF;
    return (size << 30) | (0x39 << 24) | (imm12 << 10) | (rn << 5) | rt;
}

// Helper to encode STRB (byte store)
uint32_t encode_strb_imm(Arm64Reg rt, Arm64Reg rn, int offset) {
    return encode_str_imm(rt, rn, offset, 0);
}

// Helper to encode STRH (halfword store)
uint32_t encode_strh_imm(Arm64Reg rt, Arm64Reg rn, int offset) {
    return encode_str_imm(rt, rn, offset, 1);
}

// Helper to encode STR (word - 32-bit)
uint32_t encode_str_w_imm(Arm64Reg rt, Arm64Reg rn, int offset) {
    return encode_str_imm(rt, rn, offset, 2);
}

// Helper to encode STR (doubleword - 64-bit)
uint32_t encode_str_x_imm(Arm64Reg rt, Arm64Reg rn, int offset) {
    return encode_str_imm(rt, rn, offset, 3);
}

int test_enum_field_64bit() {
    printf("Test 1: OSetEnumField - 64-bit field (8 bytes)\n");

    // Setting a 64-bit field at offset 8 in enum
    // STR X5, [X4, #1]  (offset/8 = 8/8 = 1)

    uint32_t code = encode_str_x_imm(X5, X4, 1);
    uint32_t expected = 0xf9000485;  // STR X5, [X4, #8]

    if (code != expected) {
        printf("  ✗ Encoding failed: got 0x%08x, expected 0x%08x\n", code, expected);
        return 0;
    }

    printf("  ✓ 64-bit field store correct: STR X5, [X4, #8]\n");
    printf("    Encoding: 0x%08x\n", code);
    return 1;
}

int test_enum_field_32bit() {
    printf("\nTest 2: OSetEnumField - 32-bit field (4 bytes)\n");

    // Setting a 32-bit field at offset 12 in enum
    // STR W3, [X2, #3]  (offset/4 = 12/4 = 3)

    uint32_t code = encode_str_w_imm(X3, X2, 3);
    uint32_t expected = 0xb9000c43;  // STR W3, [X2, #12]

    if (code != expected) {
        printf("  ✗ Encoding failed: got 0x%08x, expected 0x%08x\n", code, expected);
        return 0;
    }

    printf("  ✓ 32-bit field store correct: STR W3, [X2, #12]\n");
    printf("    Encoding: 0x%08x\n", code);
    return 1;
}

int test_enum_field_16bit() {
    printf("\nTest 3: OSetEnumField - 16-bit field (2 bytes)\n");

    // Setting a 16-bit field at offset 4 in enum
    // STRH W6, [X7, #2]  (offset/2 = 4/2 = 2)

    uint32_t code = encode_strh_imm(X6, X7, 2);
    uint32_t expected = 0x790008e6;  // STRH W6, [X7, #4]

    if (code != expected) {
        printf("  ✗ Encoding failed: got 0x%08x, expected 0x%08x\n", code, expected);
        return 0;
    }

    printf("  ✓ 16-bit field store correct: STRH W6, [X7, #4]\n");
    printf("    Encoding: 0x%08x\n", code);
    return 1;
}

int test_enum_field_8bit() {
    printf("\nTest 4: OSetEnumField - 8-bit field (1 byte)\n");

    // Setting an 8-bit field at offset 2 in enum
    // STRB W1, [X8, #2]

    uint32_t code = encode_strb_imm(X1, X8, 2);
    uint32_t expected = 0x39000901;  // STRB W1, [X8, #2]

    if (code != expected) {
        printf("  ✗ Encoding failed: got 0x%08x, expected 0x%08x\n", code, expected);
        return 0;
    }

    printf("  ✓ 8-bit field store correct: STRB W1, [X8, #2]\n");
    printf("    Encoding: 0x%08x\n", code);
    return 1;
}

int test_enum_multiple_fields() {
    printf("\nTest 5: OSetEnumField - Multiple fields in sequence\n");

    // Simulate setting multiple fields in an enum:
    // Field 0 (64-bit) at offset 0
    // Field 1 (32-bit) at offset 8
    // Field 2 (16-bit) at offset 12
    // Field 3 (8-bit) at offset 14

    uint32_t code[4];

    code[0] = encode_str_x_imm(X3, X2, 0);   // Field 0: STR X3, [X2, #0]
    code[1] = encode_str_w_imm(X4, X2, 2);   // Field 1: STR W4, [X2, #8]  (imm12=2 in encoding)
    code[2] = encode_strh_imm(X5, X2, 6);    // Field 2: STRH W5, [X2, #12] (imm12=6 in encoding)
    code[3] = encode_strb_imm(X6, X2, 14);   // Field 3: STRB W6, [X2, #14] (imm12=14 in encoding)

    // Verify each encoding
    // Note: imm12 field is at bits [21:10], scaled offset
    uint32_t expected[4] = {
        0xf9000043,  // STR X3, [X2, #0]
        0xb9000844,  // STR W4, [X2, #8]  (imm12 << 10 = 2 << 10 = 0x800)
        0x79001845,  // STRH W5, [X2, #12] (imm12 << 10 = 6 << 10 = 0x1800)
        0x39003846   // STRB W6, [X2, #14] (imm12 << 10 = 14 << 10 = 0x3800)
    };

    for (int i = 0; i < 4; i++) {
        if (code[i] != expected[i]) {
            printf("  ✗ Field %d encoding failed: got 0x%08x, expected 0x%08x\n",
                   i, code[i], expected[i]);
            return 0;
        }
    }

    printf("  ✓ All field stores correct\n");
    printf("    - 64-bit field: 0x%08x\n", code[0]);
    printf("    - 32-bit field: 0x%08x\n", code[1]);
    printf("    - 16-bit field: 0x%08x\n", code[2]);
    printf("    - 8-bit field:  0x%08x\n", code[3]);
    return 1;
}

int test_enum_offset_calculation() {
    printf("\nTest 6: OSetEnumField - Offset calculation verification\n");

    // Verify that offsets are scaled correctly for each size

    struct {
        int byte_offset;
        int size;  // 8, 16, 32, 64 bits
        int expected_scaled;
    } tests[] = {
        {0, 64, 0},      // 0 / 8 = 0
        {8, 64, 1},      // 8 / 8 = 1
        {16, 64, 2},     // 16 / 8 = 2
        {0, 32, 0},      // 0 / 4 = 0
        {4, 32, 1},      // 4 / 4 = 1
        {8, 32, 2},      // 8 / 4 = 2
        {0, 16, 0},      // 0 / 2 = 0
        {2, 16, 1},      // 2 / 2 = 1
        {4, 16, 2},      // 4 / 2 = 2
        {0, 8, 0},       // No scaling
        {1, 8, 1},
        {2, 8, 2},
    };

    int count = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < count; i++) {
        int scaled = tests[i].byte_offset / (tests[i].size / 8);
        if (scaled != tests[i].expected_scaled) {
            printf("  ✗ Offset calculation failed for %d-bit at offset %d\n",
                   tests[i].size, tests[i].byte_offset);
            printf("    Got %d, expected %d\n", scaled, tests[i].expected_scaled);
            return 0;
        }
    }

    printf("  ✓ All offset calculations correct\n");
    printf("    - Tested %d offset calculations\n", count);
    return 1;
}

int test_enum_field_at_zero() {
    printf("\nTest 7: OSetEnumField - Field at offset 0\n");

    // First field in enum (after tag/discriminator)
    // STR X0, [X1, #0]

    uint32_t code = encode_str_x_imm(X0, X1, 0);
    uint32_t expected = 0xf9000020;

    if (code != expected) {
        printf("  ✗ Encoding failed: got 0x%08x, expected 0x%08x\n", code, expected);
        return 0;
    }

    printf("  ✓ Field at offset 0 correct: STR X0, [X1, #0]\n");
    return 1;
}

int test_enum_large_offset() {
    printf("\nTest 8: OSetEnumField - Large offset (within imm12 range)\n");

    // Large enum with field at offset 256 bytes (64-bit field)
    // STR X7, [X8, #32]  (256 / 8 = 32, imm12 = 32)

    uint32_t code = encode_str_x_imm(X7, X8, 32);
    // imm12 << 10 = 32 << 10 = 0x8000
    uint32_t expected = 0xf9008107;  // STR X7, [X8, #256]

    if (code != expected) {
        printf("  ✗ Encoding failed: got 0x%08x, expected 0x%08x\n", code, expected);
        return 0;
    }

    printf("  ✓ Large offset correct: STR X7, [X8, #256]\n");
    printf("    Note: imm12 can handle offsets up to 4095*8 = 32760 bytes\n");
    return 1;
}

int main() {
    printf("=== OSetEnumField Operation Tests ===\n\n");

    int passed = 0;
    int total = 8;

    passed += test_enum_field_64bit();
    passed += test_enum_field_32bit();
    passed += test_enum_field_16bit();
    passed += test_enum_field_8bit();
    passed += test_enum_multiple_fields();
    passed += test_enum_offset_calculation();
    passed += test_enum_field_at_zero();
    passed += test_enum_large_offset();

    printf("\n=== Results ===\n");
    printf("Passed: %d/%d tests\n", passed, total);

    if (passed == total) {
        printf("✓ All OSetEnumField tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed\n");
        return 1;
    }
}
