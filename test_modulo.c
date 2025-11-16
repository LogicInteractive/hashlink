/*
 * ARM64 Modulo Operations Test
 * Tests software implementation of modulo using: a % b = a - (a/b)*b
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>

// Test architecture
#if !defined(__aarch64__) && !defined(_M_ARM64)
#error "Must compile for ARM64!"
#endif

typedef int64_t (*test_func_t)(int64_t, int64_t);

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

// =====================================================================
// Test 1: Signed Modulo
// Implements: X0 = X0 % X1 using X0 - (X0/X1)*X1
// =====================================================================
uint32_t* gen_smod_test() {
	uint32_t *code = alloc_exec(4096);

	// X0 = dividend, X1 = divisor
	// X9 = temp

	// X9 = X0 / X1 (signed division)
	code[0] = 0x9ac10c09;  // SDIV X9, X0, X1

	// X9 = X9 * X1
	code[1] = 0x9b017d29;  // MUL X9, X9, X1

	// X0 = X0 - X9
	code[2] = 0xcb090000;  // SUB X0, X0, X9

	// RET
	code[3] = 0xd65f03c0;

	flush_cache(code, code + 4);
	return code;
}

// =====================================================================
// Test 2: Unsigned Modulo
// Implements: X0 = X0 % X1 using X0 - (X0/X1)*X1
// =====================================================================
uint32_t* gen_umod_test() {
	uint32_t *code = alloc_exec(4096);

	// X0 = dividend, X1 = divisor
	// X9 = temp

	// X9 = X0 / X1 (unsigned division)
	code[0] = 0x9ac10809;  // UDIV X9, X0, X1

	// X9 = X9 * X1
	code[1] = 0x9b017d29;  // MUL X9, X9, X1

	// X0 = X0 - X9
	code[2] = 0xcb090000;  // SUB X0, X0, X9

	// RET
	code[3] = 0xd65f03c0;

	flush_cache(code, code + 4);
	return code;
}

// =====================================================================
// Main Test
// =====================================================================
int main() {
	int tests_passed = 0;
	int tests_total = 0;

	printf("\n");
	printf("==============================================\n");
	printf("ARM64 Modulo Operations Tests\n");
	printf("==============================================\n\n");

	// Test 1: Signed Modulo
	printf("Test 1: Signed Modulo (SMOD)\n");
	printf("----------------------------------------------\n");
	uint32_t *smod_code = gen_smod_test();
	test_func_t smod_test = (test_func_t)smod_code;

	int64_t r1 = smod_test(17, 5);   // 17 % 5 = 2
	int64_t r2 = smod_test(100, 7);  // 100 % 7 = 2
	int64_t r3 = smod_test(50, 10);  // 50 % 10 = 0
	int64_t r4 = smod_test(-17, 5);  // -17 % 5 = -2 (implementation-defined, but this is typical)

	tests_total += 4;
	printf("  smod(17, 5) = %ld (expected: 2) %s\n", r1, r1 == 2 ? "✓" : "✗");
	printf("  smod(100, 7) = %ld (expected: 2) %s\n", r2, r2 == 2 ? "✓" : "✗");
	printf("  smod(50, 10) = %ld (expected: 0) %s\n", r3, r3 == 0 ? "✓" : "✗");
	printf("  smod(-17, 5) = %ld (expected: -2) %s\n", r4, r4 == -2 ? "✓" : "✗");

	if (r1 == 2) tests_passed++;
	if (r2 == 2) tests_passed++;
	if (r3 == 0) tests_passed++;
	if (r4 == -2) tests_passed++;

	munmap(smod_code, 4096);
	printf("\n");

	// Test 2: Unsigned Modulo
	printf("Test 2: Unsigned Modulo (UMOD)\n");
	printf("----------------------------------------------\n");
	uint32_t *umod_code = gen_umod_test();
	test_func_t umod_test = (test_func_t)umod_code;

	r1 = umod_test(17, 5);   // 17 % 5 = 2
	r2 = umod_test(100, 7);  // 100 % 7 = 2
	r3 = umod_test(50, 10);  // 50 % 10 = 0
	uint64_t r4u = (uint64_t)umod_test(255, 16);  // 255 % 16 = 15

	tests_total += 4;
	printf("  umod(17, 5) = %ld (expected: 2) %s\n", r1, r1 == 2 ? "✓" : "✗");
	printf("  umod(100, 7) = %ld (expected: 2) %s\n", r2, r2 == 2 ? "✓" : "✗");
	printf("  umod(50, 10) = %ld (expected: 0) %s\n", r3, r3 == 0 ? "✓" : "✗");
	printf("  umod(255, 16) = %lu (expected: 15) %s\n", r4u, r4u == 15 ? "✓" : "✗");

	if (r1 == 2) tests_passed++;
	if (r2 == 2) tests_passed++;
	if (r3 == 0) tests_passed++;
	if (r4u == 15) tests_passed++;

	munmap(umod_code, 4096);
	printf("\n");

	// Final Results
	printf("==============================================\n");
	printf("Results: %d/%d tests passed (%.1f%%)\n",
	       tests_passed, tests_total,
	       tests_total > 0 ? (100.0 * tests_passed / tests_total) : 0.0);
	printf("==============================================\n");

	if (tests_passed == tests_total) {
		printf("\n✓✓✓ ALL TESTS PASSED ✓✓✓\n");
		printf("ARM64 modulo operations are working correctly!\n\n");
		return 0;
	} else {
		printf("\n✗✗✗ SOME TESTS FAILED ✗✗✗\n\n");
		return 1;
	}
}
