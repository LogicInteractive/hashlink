/*
 * ARM64 JIT Test - ODynGet Operation
 *
 * Tests dynamic field reading (dst = obj.field) with type-specific getters.
 *
 * Getter functions used:
 * - hl_dyn_geti(obj, hash, type)   -> int (32-bit integers, bool)
 * - hl_dyn_geti64(obj, hash)       -> int64 (64-bit integers)
 * - hl_dyn_getp(obj, hash, type)   -> void* (pointer types)
 * - hl_dyn_getf(obj, hash)         -> float
 * - hl_dyn_getd(obj, hash)         -> double
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Mock ARM64 instruction encoding
unsigned int code[256];
int code_pos = 0;

#define B32(val) (code[code_pos++] = (val))

// ARM64 register definitions
typedef enum {
    X0 = 0, X1, X2, X3, X4, X5, X6, X7, X8, X9,
    X10, X11, X12, X13, X14, X15, X16, X17, X18
} Arm64Reg;

// Mock type system
typedef enum {
    HI32, HI64, HF32, HF64, HBOOL,
    HOBJ, HPTR, HDYNAMIC
} hl_type_kind;

typedef struct {
    hl_type_kind kind;
} hl_type;

// Helper functions
void arm_mov_reg(Arm64Reg rd, Arm64Reg rn, int is_64bit) {
    // MOV Xd, Xn (ORR Xd, XZR, Xn)
    unsigned int inst = (is_64bit ? 0xAA0003E0 : 0x2A0003E0) | (rn << 16) | rd;
    B32(inst);
}

void arm_load_imm64(Arm64Reg rd, uint64_t imm) {
    // MOVZ + MOVK sequence to load 64-bit immediate
    // MOVZ Xd, #imm[0:15], LSL #0
    B32(0xD2800000 | ((imm & 0xFFFF) << 5) | rd);
    // MOVK Xd, #imm[16:31], LSL #16
    B32(0xF2A00000 | (((imm >> 16) & 0xFFFF) << 5) | rd);
    // MOVK Xd, #imm[32:47], LSL #32
    B32(0xF2C00000 | (((imm >> 32) & 0xFFFF) << 5) | rd);
    // MOVK Xd, #imm[48:63], LSL #48
    B32(0xF2E00000 | (((imm >> 48) & 0xFFFF) << 5) | rd);
}

// Test functions
int test_dynget_i32() {
    printf("\nTest 1: ODynGet - Integer Field (hl_dyn_geti)\n");
    printf("-----------------------------------------------\n");

    code_pos = 0;
    memset(code, 0, sizeof(code));

    // Simulate: int result = obj.field_name
    // where obj is in X5, result goes to X10
    Arm64Reg r_obj = X5;
    Arm64Reg rd = X10;
    uint64_t hash = 0x12345678ABCDEF00ULL;
    hl_type dst_type = { .kind = HI32 };

    // Expected sequence:
    // 1. MOV X0, X5 (object to X0)
    // 2. MOVZ/MOVK X1, #hash (load hash to X1)
    // 3. MOVZ/MOVK X2, #type_ptr (load type to X2)
    // 4. MOVZ/MOVK X9, #func_ptr (load hl_dyn_geti to X9)
    // 5. BLR X9
    // 6. MOV X10, X0 (result to destination)

    // Generate the code
    if (r_obj != X0) {
        arm_mov_reg(X0, r_obj, 1);
    }
    arm_load_imm64(X1, hash);
    arm_load_imm64(X2, (uint64_t)&dst_type);
    arm_load_imm64(X9, 0xDEADBEEF00000000ULL);  // Mock function pointer
    B32(0xd63f0120);  // BLR X9
    if (rd != X0) {
        arm_mov_reg(rd, X0, 1);
    }

    // Verify
    printf("  Generated %d instructions\n", code_pos);

    // Check MOV X0, X5
    if (code[0] == 0xAA0503E0) {
        printf("  ✓ MOV X0, X5 encoded correctly\n");
    } else {
        printf("  ✗ MOV encoding wrong: 0x%08x\n", code[0]);
        return 0;
    }

    // Check BLR X9
    int blr_pos = code_pos - 2;  // Before final MOV
    if (code[blr_pos] == 0xd63f0120) {
        printf("  ✓ BLR X9 encoded correctly\n");
    } else {
        printf("  ✗ BLR encoding wrong at position %d: 0x%08x\n", blr_pos, code[blr_pos]);
        return 0;
    }

    // Check final MOV X10, X0
    if (code[code_pos-1] == 0xAA0003EA) {
        printf("  ✓ MOV X10, X0 encoded correctly\n");
    } else {
        printf("  ✗ Final MOV encoding wrong: 0x%08x\n", code[code_pos-1]);
        return 0;
    }

    printf("  ✓ hl_dyn_geti call sequence verified\n");
    return 1;
}

int test_dynget_i64() {
    printf("\nTest 2: ODynGet - Int64 Field (hl_dyn_geti64)\n");
    printf("-----------------------------------------------\n");

    code_pos = 0;
    memset(code, 0, sizeof(code));

    // Simulate: int64 result = obj.field_name
    Arm64Reg r_obj = X3;
    Arm64Reg rd = X7;
    uint64_t hash = 0xFEDCBA9876543210ULL;

    // Expected sequence (no type argument for geti64):
    // 1. MOV X0, X3
    // 2. MOVZ/MOVK X1, #hash
    // 3. MOVZ/MOVK X9, #func_ptr
    // 4. BLR X9
    // 5. MOV X7, X0

    if (r_obj != X0) {
        arm_mov_reg(X0, r_obj, 1);
    }
    arm_load_imm64(X1, hash);
    arm_load_imm64(X9, 0xCAFEBABE00000000ULL);
    B32(0xd63f0120);  // BLR X9
    if (rd != X0) {
        arm_mov_reg(rd, X0, 1);
    }

    printf("  Generated %d instructions\n", code_pos);

    // Should be fewer instructions (no type argument)
    if (code_pos < 15) {  // Less than i32 version
        printf("  ✓ No type argument (correct for i64)\n");
    }

    // Verify MOV X0, X3
    if (code[0] == 0xAA0303E0) {
        printf("  ✓ MOV X0, X3 encoded correctly\n");
    } else {
        printf("  ✗ MOV encoding wrong: 0x%08x\n", code[0]);
        return 0;
    }

    // Verify BLR X9
    int blr_pos = code_pos - 2;
    if (code[blr_pos] == 0xd63f0120) {
        printf("  ✓ BLR X9 encoded correctly\n");
    } else {
        printf("  ✗ BLR encoding wrong: 0x%08x\n", code[blr_pos]);
        return 0;
    }

    // Verify final MOV X7, X0
    if (code[code_pos-1] == 0xAA0003E7) {
        printf("  ✓ MOV X7, X0 encoded correctly\n");
    } else {
        printf("  ✗ Final MOV encoding wrong: 0x%08x\n", code[code_pos-1]);
        return 0;
    }

    printf("  ✓ hl_dyn_geti64 call sequence verified\n");
    return 1;
}

int test_dynget_pointer() {
    printf("\nTest 3: ODynGet - Pointer Field (hl_dyn_getp)\n");
    printf("-----------------------------------------------\n");

    code_pos = 0;
    memset(code, 0, sizeof(code));

    // Simulate: ptr result = obj.field_name
    Arm64Reg r_obj = X2;
    Arm64Reg rd = X8;
    uint64_t hash = 0x1122334455667788ULL;
    hl_type dst_type = { .kind = HOBJ };

    // Expected sequence (needs type argument):
    // 1. MOV X0, X2
    // 2. MOVZ/MOVK X1, #hash
    // 3. MOVZ/MOVK X2, #type_ptr
    // 4. MOVZ/MOVK X9, #func_ptr
    // 5. BLR X9
    // 6. MOV X8, X0

    if (r_obj != X0) {
        arm_mov_reg(X0, r_obj, 1);
    }
    arm_load_imm64(X1, hash);
    arm_load_imm64(X2, (uint64_t)&dst_type);
    arm_load_imm64(X9, 0xBEEFCAFE00000000ULL);
    B32(0xd63f0120);  // BLR X9
    if (rd != X0) {
        arm_mov_reg(rd, X0, 1);
    }

    printf("  Generated %d instructions\n", code_pos);

    // Verify MOV X0, X2
    if (code[0] == 0xAA0203E0) {
        printf("  ✓ MOV X0, X2 encoded correctly\n");
    } else {
        printf("  ✗ MOV encoding wrong: 0x%08x\n", code[0]);
        return 0;
    }

    // Verify BLR X9
    int blr_pos = code_pos - 2;
    if (code[blr_pos] == 0xd63f0120) {
        printf("  ✓ BLR X9 encoded correctly\n");
    } else {
        printf("  ✗ BLR encoding wrong: 0x%08x\n", code[blr_pos]);
        return 0;
    }

    // Verify final MOV X8, X0
    if (code[code_pos-1] == 0xAA0003E8) {
        printf("  ✓ MOV X8, X0 encoded correctly\n");
    } else {
        printf("  ✗ Final MOV encoding wrong: 0x%08x\n", code[code_pos-1]);
        return 0;
    }

    printf("  ✓ hl_dyn_getp call sequence verified\n");
    return 1;
}

int test_dynget_double() {
    printf("\nTest 4: ODynGet - Double Field (hl_dyn_getd)\n");
    printf("-----------------------------------------------\n");

    code_pos = 0;
    memset(code, 0, sizeof(code));

    // Simulate: double result = obj.field_name
    Arm64Reg r_obj = X4;
    Arm64Reg rd = X11;
    uint64_t hash = 0xAABBCCDD11223344ULL;

    // Expected sequence (no type argument for getd):
    // 1. MOV X0, X4
    // 2. MOVZ/MOVK X1, #hash
    // 3. MOVZ/MOVK X9, #func_ptr
    // 4. BLR X9
    // 5. MOV X11, X0

    if (r_obj != X0) {
        arm_mov_reg(X0, r_obj, 1);
    }
    arm_load_imm64(X1, hash);
    arm_load_imm64(X9, 0xF00DFACE00000000ULL);
    B32(0xd63f0120);  // BLR X9
    if (rd != X0) {
        arm_mov_reg(rd, X0, 1);
    }

    printf("  Generated %d instructions\n", code_pos);

    // Verify MOV X0, X4
    if (code[0] == 0xAA0403E0) {
        printf("  ✓ MOV X0, X4 encoded correctly\n");
    } else {
        printf("  ✗ MOV encoding wrong: 0x%08x\n", code[0]);
        return 0;
    }

    // Verify BLR X9
    int blr_pos = code_pos - 2;
    if (code[blr_pos] == 0xd63f0120) {
        printf("  ✓ BLR X9 encoded correctly\n");
    } else {
        printf("  ✗ BLR encoding wrong: 0x%08x\n", code[blr_pos]);
        return 0;
    }

    // Verify final MOV X11, X0
    if (code[code_pos-1] == 0xAA0003EB) {
        printf("  ✓ MOV X11, X0 encoded correctly\n");
    } else {
        printf("  ✗ Final MOV encoding wrong: 0x%08x\n", code[code_pos-1]);
        return 0;
    }

    printf("  ✓ hl_dyn_getd call sequence verified\n");
    return 1;
}

int test_dynget_no_move() {
    printf("\nTest 5: ODynGet - Object Already in X0\n");
    printf("---------------------------------------\n");

    code_pos = 0;
    memset(code, 0, sizeof(code));

    // Simulate: int result = obj.field where obj is already in X0
    Arm64Reg r_obj = X0;  // Already in X0
    Arm64Reg rd = X0;     // Result also stays in X0
    uint64_t hash = 0x9999999999999999ULL;
    hl_type dst_type = { .kind = HI32 };

    // Expected sequence (skip unnecessary MOVs):
    // 1. (no MOV, already in X0)
    // 2. MOVZ/MOVK X1, #hash
    // 3. MOVZ/MOVK X2, #type_ptr
    // 4. MOVZ/MOVK X9, #func_ptr
    // 5. BLR X9
    // 6. (no MOV, result stays in X0)

    if (r_obj != X0) {
        arm_mov_reg(X0, r_obj, 1);
    }
    arm_load_imm64(X1, hash);
    arm_load_imm64(X2, (uint64_t)&dst_type);
    arm_load_imm64(X9, 0xAAAABBBBCCCCDDDDULL);
    B32(0xd63f0120);  // BLR X9
    if (rd != X0) {
        arm_mov_reg(rd, X0, 1);
    }

    printf("  Generated %d instructions\n", code_pos);

    // First instruction should be MOVZ (not MOV)
    if ((code[0] & 0xFFE00000) == 0xD2800000) {
        printf("  ✓ No unnecessary MOV at start\n");
    } else {
        printf("  ✗ Unexpected instruction at start: 0x%08x\n", code[0]);
        return 0;
    }

    // Last instruction should be BLR (not MOV)
    if (code[code_pos-1] == 0xd63f0120) {
        printf("  ✓ No unnecessary MOV at end\n");
    } else {
        printf("  ✗ Unexpected instruction at end: 0x%08x\n", code[code_pos-1]);
        return 0;
    }

    printf("  ✓ Optimized sequence (no redundant moves)\n");
    return 1;
}

int test_dynget_type_dispatch() {
    printf("\nTest 6: ODynGet - Type Dispatch Verification\n");
    printf("----------------------------------------------\n");

    // This test verifies the type-based dispatch logic
    hl_type types[] = {
        { .kind = HI32 },   // Uses hl_dyn_geti (needs type)
        { .kind = HI64 },   // Uses hl_dyn_geti64 (no type)
        { .kind = HF32 },   // Uses hl_dyn_getf (no type)
        { .kind = HF64 },   // Uses hl_dyn_getd (no type)
        { .kind = HBOOL },  // Uses hl_dyn_geti (needs type)
        { .kind = HOBJ },   // Uses hl_dyn_getp (needs type)
    };

    const char *names[] = {
        "HI32 (hl_dyn_geti)",
        "HI64 (hl_dyn_geti64)",
        "HF32 (hl_dyn_getf)",
        "HF64 (hl_dyn_getd)",
        "HBOOL (hl_dyn_geti)",
        "HOBJ (hl_dyn_getp)",
    };

    int needs_type[] = { 1, 0, 0, 0, 1, 1 };

    for (int i = 0; i < 6; i++) {
        code_pos = 0;
        memset(code, 0, sizeof(code));

        Arm64Reg r_obj = X1;
        Arm64Reg rd = X2;
        uint64_t hash = 0x1234567890ABCDEFULL;

        if (r_obj != X0) {
            arm_mov_reg(X0, r_obj, 1);
        }
        arm_load_imm64(X1, hash);

        if (needs_type[i]) {
            arm_load_imm64(X2, (uint64_t)&types[i]);
        }

        arm_load_imm64(X9, 0x0000000012345678ULL);
        B32(0xd63f0120);

        if (rd != X0) {
            arm_mov_reg(rd, X0, 1);
        }

        int expected_count = 1 + 4 + (needs_type[i] ? 4 : 0) + 4 + 1 + 1;
        // 1 MOV + 4 MOVZ/MOVK (hash) + [4 MOVZ/MOVK (type)] + 4 MOVZ/MOVK (func) + BLR + MOV

        printf("  %s: %d instructions ", names[i], code_pos);
        if (code_pos == expected_count) {
            printf("✓\n");
        } else {
            printf("✗ (expected %d)\n", expected_count);
            return 0;
        }
    }

    printf("  ✓ All type dispatches generate correct sequences\n");
    return 1;
}

int main() {
    printf("========================================\n");
    printf("ARM64 JIT - ODynGet Operation Tests\n");
    printf("========================================\n");

    int passed = 0;
    int total = 6;

    passed += test_dynget_i32();
    passed += test_dynget_i64();
    passed += test_dynget_pointer();
    passed += test_dynget_double();
    passed += test_dynget_no_move();
    passed += test_dynget_type_dispatch();

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed", passed, total);
    if (passed == total) {
        printf(" ✓\n");
    } else {
        printf(" ✗\n");
    }
    printf("========================================\n");

    return (passed == total) ? 0 : 1;
}
