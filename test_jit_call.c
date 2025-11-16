/*
 * JIT-to-JIT Function Call Test
 * Tests calling between JIT-generated functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>

#if !defined(__aarch64__) && !defined(_M_ARM64)
#error "Must compile for ARM64!"
#endif

void* alloc_exec(size_t size) {
	void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
	                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (mem == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	return mem;
}

void flush_cache(void *start, void *end) {
	__builtin___clear_cache((char*)start, (char*)end);
}

int main() {
	printf("Testing ARM64 JIT-to-JIT function calls...\n\n");

	uint32_t *code = alloc_exec(4096);

	// ============================================
	// CALLEE function (at offset 0x100 = 64 instructions)
	// Add X0 and X1, return result
	// ============================================
	uint32_t *callee = code + 64;  // Put callee at offset 256 bytes

	callee[0] = 0x8b010000;  // ADD X0, X0, X1
	callee[1] = 0xd65f03c0;  // RET

	// ============================================
	// CALLER function (at offset 0)
	// Sets up arguments and calls callee
	// ============================================

	// MOV X0, #10
	code[0] = 0xd2800140;  // MOVZ X0, #10

	// MOV X1, #20
	code[1] = 0xd2800281;  // MOVZ X1, #20

	// BL to callee (offset = 64 - 2 = 62 instructions)
	// BL instruction format: 100101 imm26
	// offset is in instructions, not bytes
	code[2] = 0x9400003e;  // BL +62

	// Result now in X0, just return it
	code[3] = 0xd65f03c0;  // RET

	flush_cache(code, code + 70);

	// Call the JIT code
	printf("Calling JIT code...\n");
	uint64_t (*test_func)(void) = (void*)code;
	uint64_t result = test_func();

	printf("Result: %lu (expected: 30)\n", result);

	munmap(code, 4096);

	if (result == 30) {
		printf("\n✓✓✓ TEST PASSED - JIT-to-JIT calls work!\n");
		return 0;
	} else {
		printf("\n✗✗✗ TEST FAILED\n");
		return 1;
	}
}
