/*
 * ARM64 JIT Runtime Verification Test (Simplified for QEMU)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>

// Minimal type definitions
typedef enum {
	X0 = 0, X1 = 1, X30 = 30, XZR = 31
} Arm64Reg;

typedef struct {
	union {
		unsigned char *b;
		unsigned int *w;
	} buf;
	unsigned char *startBuf;
} jit_ctx;

#define B32(val) *ctx->buf.w++ = (unsigned int)(val)

static inline unsigned int arm_reg(int reg) {
	return (unsigned int)(reg & 0x1F);
}

// ARM64 Instruction Encoders
static void arm_add_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0 << 29) | (0x0B << 24) |
	                    (arm_reg(rm) << 16) | (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

static void arm_ret(jit_ctx *ctx, Arm64Reg rn) {
	unsigned int inst = (0xD65F << 16) | (arm_reg(rn) << 5);
	B32(inst);
}

typedef uint64_t (*jit_func_t)(uint64_t, uint64_t);

int main() {
	printf("\n");
	printf("==============================================\n");
	printf("ARM64 JIT Verification Test\n");
	printf("==============================================\n\n");

	// Detect architecture
#if defined(__aarch64__) || defined(_M_ARM64)
	printf("✓ Compiled for ARM64/AArch64\n\n");
#else
	printf("✗ NOT compiled for ARM64!\n");
	printf("ERROR: Must compile with aarch64-linux-gnu-gcc\n\n");
	return 1;
#endif

	// Allocate executable memory
	printf("Step 1: Allocating executable memory...\n");
	void *mem = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
	                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mem == MAP_FAILED) {
		printf("✗ mmap() failed\n");
		perror("  Error");
		return 1;
	}
	printf("✓ Allocated at: %p\n\n", mem);

	// Generate code
	printf("Step 2: Generating ARM64 code...\n");
	jit_ctx ctx;
	ctx.buf.b = (unsigned char*)mem;
	ctx.startBuf = (unsigned char*)mem;

	// Generate: add x0, x0, x1; ret
	arm_add_reg(&ctx, X0, X0, X1, true);
	arm_ret(&ctx, X30);

	uint32_t *code = (uint32_t*)mem;
	printf("  Instruction 1: 0x%08x (ADD X0, X0, X1)\n", code[0]);
	printf("  Instruction 2: 0x%08x (RET)\n", code[1]);
	printf("✓ Code generated\n\n");

	// Flush instruction cache
	printf("Step 3: Flushing instruction cache...\n");
	__builtin___clear_cache((char*)mem, (char*)ctx.buf.b);
	printf("✓ Cache flushed\n\n");

	// Execute generated code
	printf("Step 4: Executing generated code...\n");
	jit_func_t add_func = (jit_func_t)mem;

	printf("  Calling add_func(5, 3)...\n");
	uint64_t result1 = add_func(5, 3);
	printf("  Result: %lu %s\n", result1, result1 == 8 ? "✓" : "✗");

	printf("  Calling add_func(100, 200)...\n");
	uint64_t result2 = add_func(100, 200);
	printf("  Result: %lu %s\n", result2, result2 == 300 ? "✓" : "✗");

	printf("  Calling add_func(0xFFFFFFFF, 1)...\n");
	uint64_t result3 = add_func(0xFFFFFFFF, 1);
	printf("  Result: 0x%lx %s\n", result3, result3 == 0x100000000 ? "✓" : "✗");

	// Cleanup
	munmap(mem, 4096);

	// Final result
	printf("\n==============================================\n");
	if (result1 == 8 && result2 == 300 && result3 == 0x100000000) {
		printf("✓✓✓ ALL TESTS PASSED ✓✓✓\n");
		printf("ARM64 JIT is working correctly!\n");
		printf("==============================================\n\n");
		return 0;
	} else {
		printf("✗✗✗ TESTS FAILED ✗✗✗\n");
		printf("==============================================\n\n");
		return 1;
	}
}
