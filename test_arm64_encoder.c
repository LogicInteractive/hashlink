/*
 * ARM64 Instruction Encoder Test
 *
 * This program tests the ARM64 instruction encoders by generating
 * machine code and displaying it in hex format for verification.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Force ARM64 compilation mode for testing
#define HL_JIT_ARM64

// Minimal type definitions needed for the encoders
typedef enum {
	X0 = 0, X1 = 1, X2 = 2, X3 = 3, X4 = 4, X5 = 5, X6 = 6, X7 = 7,
	X8 = 8, X9 = 9, X10 = 10, X11 = 11, X12 = 12, X13 = 13, X14 = 14, X15 = 15,
	X16 = 16, X17 = 17, X18 = 18, X19 = 19, X20 = 20, X21 = 21, X22 = 22, X23 = 23,
	X24 = 24, X25 = 25, X26 = 26, X27 = 27, X28 = 28, X29 = 29, X30 = 30, XZR = 31
} Arm64Reg;

typedef enum {
	V0 = 0, V1 = 1, V2 = 2, V3 = 3, V4 = 4, V5 = 5, V6 = 6, V7 = 7
} Arm64FpReg;

typedef enum {
	COND_EQ = 0x0, COND_NE = 0x1, COND_LT = 0xB, COND_GE = 0xA, COND_AL = 0xE
} Arm64Condition;

typedef struct {
	union {
		unsigned char *b;
		unsigned int *w;
	} buf;
	unsigned char *startBuf;
} jit_ctx;

// Helper macros
#define B32(val) *ctx->buf.w++ = (unsigned int)(val)
#define ARM_BUF_POS() ((int)((unsigned char*)ctx->buf.w - ctx->startBuf))

static inline unsigned int arm_reg(int reg) {
	return (unsigned int)(reg & 0x1F);
}

static inline bool arm_fits_signed(int64_t val, int bits) {
	int64_t min = -(1LL << (bits - 1));
	int64_t max = (1LL << (bits - 1)) - 1;
	return val >= min && val <= max;
}

static inline bool arm_fits_unsigned(uint64_t val, int bits) {
	return val < (1ULL << bits);
}

// ARM64 Instruction Encoders (simplified versions from jit.c)

static void arm_add_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0 << 29) | (0x0B << 24) |
	                    (arm_reg(rm) << 16) | (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

static void arm_sub_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (2 << 29) | (0x0B << 24) |
	                    (arm_reg(rm) << 16) | (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

static void arm_mov_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x2A << 24) |
	                    (arm_reg(rm) << 16) | (31 << 5) | arm_reg(rd);
	B32(inst);
}

static void arm_movz(jit_ctx *ctx, Arm64Reg rd, unsigned int imm16, unsigned int shift, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x52 << 23) | (shift << 21) | (imm16 << 5) | arm_reg(rd);
	B32(inst);
}

static void arm_ldr_imm(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, unsigned int imm12, int size) {
	unsigned int inst = (size << 30) | (0x39 << 24) | (imm12 << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rt);
	B32(inst);
}

static void arm_str_imm(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, unsigned int imm12, int size) {
	unsigned int inst = (size << 30) | (0x39 << 24) | (0 << 22) | (imm12 << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rt);
	B32(inst);
}

static int arm_b(jit_ctx *ctx, int offset) {
	int pos = ARM_BUF_POS();
	if (offset == 0) {
		B32(0x14000000);
		return pos;
	}
	unsigned int imm26 = (offset >> 2) & 0x3FFFFFF;
	unsigned int inst = (0x05 << 26) | imm26;
	B32(inst);
	return pos;
}

static int arm_bl(jit_ctx *ctx, int offset) {
	int pos = ARM_BUF_POS();
	unsigned int imm26 = (offset >> 2) & 0x3FFFFFF;
	unsigned int inst = (0x25 << 26) | imm26;
	B32(inst);
	return pos;
}

static void arm_ret(jit_ctx *ctx, Arm64Reg rn) {
	unsigned int inst = (0xD65F << 16) | (arm_reg(rn) << 5);
	B32(inst);
}

static int arm_b_cond(jit_ctx *ctx, Arm64Condition cond, int offset) {
	int pos = ARM_BUF_POS();
	if (offset == 0) {
		B32(0x54000000 | cond);
		return pos;
	}
	unsigned int imm19 = (offset >> 2) & 0x7FFFF;
	unsigned int inst = (0x54 << 24) | (imm19 << 5) | cond;
	B32(inst);
	return pos;
}

// Test helper functions

void print_hex_bytes(unsigned char *buf, int len, const char *label) {
	printf("%-40s: ", label);
	for (int i = 0; i < len; i++) {
		printf("%02x ", buf[i]);
		if ((i + 1) % 4 == 0) printf(" ");
	}
	printf("\n");
}

void print_instructions(unsigned int *buf, int count, const char *label) {
	printf("\n%s:\n", label);
	printf("----------------------------------------\n");
	for (int i = 0; i < count; i++) {
		printf("  [%d] 0x%08x\n", i, buf[i]);
	}
}

int main() {
	printf("==============================================\n");
	printf("ARM64 Instruction Encoder Test\n");
	printf("==============================================\n\n");

	// Allocate buffer for generated code
	unsigned char buffer[1024];
	jit_ctx ctx;
	ctx.buf.b = buffer;
	ctx.startBuf = buffer;

	// Test 1: ADD (register) - 64-bit
	printf("Test 1: ADD X0, X1, X2 (add 64-bit registers)\n");
	memset(buffer, 0, sizeof(buffer));
	ctx.buf.b = buffer;
	arm_add_reg(&ctx, X0, X1, X2, true);
	print_hex_bytes(buffer, 4, "Expected: 0xeb020020 (little-endian)");
	printf("  Instruction: ADD X0, X1, X2\n\n");

	// Test 2: SUB (register) - 64-bit
	printf("Test 2: SUB X3, X4, X5 (subtract 64-bit registers)\n");
	memset(buffer, 0, sizeof(buffer));
	ctx.buf.b = buffer;
	arm_sub_reg(&ctx, X3, X4, X5, true);
	print_hex_bytes(buffer, 4, "Expected: 0xcb050083");
	printf("  Instruction: SUB X3, X4, X5\n\n");

	// Test 3: MOV (register) - 64-bit
	printf("Test 3: MOV X10, X11 (move register)\n");
	memset(buffer, 0, sizeof(buffer));
	ctx.buf.b = buffer;
	arm_mov_reg(&ctx, X10, X11, true);
	print_hex_bytes(buffer, 4, "Expected: 0xaa0b03ea");
	printf("  Instruction: MOV X10, X11 (ORR X10, XZR, X11)\n\n");

	// Test 4: MOVZ - Load immediate
	printf("Test 4: MOVZ X7, #0x1234 (load 16-bit immediate)\n");
	memset(buffer, 0, sizeof(buffer));
	ctx.buf.b = buffer;
	arm_movz(&ctx, X7, 0x1234, 0, true);
	print_hex_bytes(buffer, 4, "Expected: 0xd2824680");
	printf("  Instruction: MOVZ X7, #0x1234\n\n");

	// Test 5: LDR - Load from memory
	printf("Test 5: LDR X8, [X9, #16] (load from memory with offset)\n");
	memset(buffer, 0, sizeof(buffer));
	ctx.buf.b = buffer;
	arm_ldr_imm(&ctx, X8, X9, 2, 3);  // offset 16 = imm12*8, size=3 for 64-bit
	print_hex_bytes(buffer, 4, "Expected: 0xf9400928");
	printf("  Instruction: LDR X8, [X9, #16]\n\n");

	// Test 6: STR - Store to memory
	printf("Test 6: STR X12, [X13, #24] (store to memory with offset)\n");
	memset(buffer, 0, sizeof(buffer));
	ctx.buf.b = buffer;
	arm_str_imm(&ctx, X12, X13, 3, 3);  // offset 24 = imm12*8, size=3 for 64-bit
	print_hex_bytes(buffer, 4, "Expected: 0xf9000dac");
	printf("  Instruction: STR X12, [X13, #24]\n\n");

	// Test 7: B - Unconditional branch
	printf("Test 7: B +8 (branch forward 8 bytes = 2 instructions)\n");
	memset(buffer, 0, sizeof(buffer));
	ctx.buf.b = buffer;
	arm_b(&ctx, 8);
	print_hex_bytes(buffer, 4, "Expected: 0x14000002");
	printf("  Instruction: B +8 (PC + 8 bytes)\n\n");

	// Test 8: BL - Branch with link (function call)
	printf("Test 8: BL +16 (call function 16 bytes ahead)\n");
	memset(buffer, 0, sizeof(buffer));
	ctx.buf.b = buffer;
	arm_bl(&ctx, 16);
	print_hex_bytes(buffer, 4, "Expected: 0x94000004");
	printf("  Instruction: BL +16\n\n");

	// Test 9: RET - Return
	printf("Test 9: RET (return from function)\n");
	memset(buffer, 0, sizeof(buffer));
	ctx.buf.b = buffer;
	arm_ret(&ctx, X30);
	print_hex_bytes(buffer, 4, "Expected: 0xd65f03c0");
	printf("  Instruction: RET (return to X30/LR)\n\n");

	// Test 10: B.EQ - Conditional branch
	printf("Test 10: B.EQ +12 (branch if equal)\n");
	memset(buffer, 0, sizeof(buffer));
	ctx.buf.b = buffer;
	arm_b_cond(&ctx, COND_EQ, 12);
	print_hex_bytes(buffer, 4, "Expected: 0x54000060");
	printf("  Instruction: B.EQ +12\n\n");

	// Test 11: Simple function (prologue + body + epilogue)
	printf("Test 11: Simple function sequence\n");
	printf("----------------------------------------\n");
	memset(buffer, 0, sizeof(buffer));
	ctx.buf.b = buffer;

	// Simple function that adds two numbers:
	// add x0, x0, x1
	// ret
	arm_add_reg(&ctx, X0, X0, X1, true);
	arm_ret(&ctx, X30);

	int instr_count = (ctx.buf.b - buffer) / 4;
	print_instructions((unsigned int*)buffer, instr_count, "Function: add(x0, x1)");
	printf("\n");

	printf("==============================================\n");
	printf("Test complete! You can verify the hex output\n");
	printf("using an ARM64 disassembler or objdump.\n");
	printf("==============================================\n");

	return 0;
}
