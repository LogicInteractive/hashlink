/*
 * ARM64 Type Conversion Test
 * Tests sign-extension and type conversions
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

typedef int64_t (*test_func_t)(int32_t);

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
// Test 1: Sign-extend 32-bit to 64-bit (SXTW)
// =====================================================================
uint32_t* gen_sxtw_test() {
	uint32_t *code = alloc_exec(4096);

	// SXTW X0, W0 - sign extend W0 to X0
	// Format: 1 00 10011 01 0 00000 011111 Rn Rd
	// inst = 0x93407C00 | (Rn << 5) | Rd
	code[0] = 0x93407c00;  // SXTW X0, W0

	// RET
	code[1] = 0xd65f03c0;

	flush_cache(code, code + 2);
	return code;
}

// =====================================================================
// Test 2: Zero-extend (UXTW) - using MOV
// =====================================================================
uint32_t* gen_uxtw_test() {
	uint32_t *code = alloc_exec(4096);

	// MOV W0, W0 - clears upper 32 bits (zero-extend)
	code[0] = 0x2a0003e0;  // MOV W0, W0

	// RET
	code[1] = 0xd65f03c0;

	flush_cache(code, code + 2);
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
	printf("ARM64 Type Conversion Tests\n");
	printf("==============================================\n\n");

	// Test 1: Sign-extend (SXTW)
	printf("Test 1: Sign-Extend 32-bit to 64-bit (SXTW)\n");
	printf("----------------------------------------------\n");
	uint32_t *sxtw_code = gen_sxtw_test();
	test_func_t sxtw_test = (test_func_t)sxtw_code;

	// Test positive value
	int64_t r1 = sxtw_test(0x12345678);  // Positive: stays positive
	tests_total++;
	printf("  sxtw(0x12345678) = 0x%016lx (expected: 0x0000000012345678) %s\n",
	       (uint64_t)r1, (uint64_t)r1 == 0x0000000012345678ULL ? "✓" : "✗");
	if ((uint64_t)r1 == 0x0000000012345678ULL) tests_passed++;

	// Test negative value (sign bit set)
	int64_t r2 = sxtw_test((int32_t)0x80000000);  // -2147483648
	tests_total++;
	printf("  sxtw(0x80000000) = 0x%016lx (expected: 0xffffffff80000000) %s\n",
	       (uint64_t)r2, (uint64_t)r2 == 0xffffffff80000000ULL ? "✓" : "✗");
	if ((uint64_t)r2 == 0xffffffff80000000ULL) tests_passed++;

	// Test -1
	int64_t r3 = sxtw_test((int32_t)0xFFFFFFFF);  // -1
	tests_total++;
	printf("  sxtw(0xFFFFFFFF) = 0x%016lx (expected: 0xffffffffffffffff) %s\n",
	       (uint64_t)r3, (uint64_t)r3 == 0xffffffffffffffffULL ? "✓" : "✗");
	if ((uint64_t)r3 == 0xffffffffffffffffULL) tests_passed++;

	munmap(sxtw_code, 4096);
	printf("\n");

	// Test 2: Zero-extend
	printf("Test 2: Zero-Extend (MOV W-register)\n");
	printf("----------------------------------------------\n");
	uint32_t *uxtw_code = gen_uxtw_test();
	test_func_t uxtw_test = (test_func_t)uxtw_code;

	// When we pass 0xFFFFFFFFFFFFFFFF, MOV W0,W0 should zero upper bits
	// But the function receives it as int32_t parameter, so already truncated
	r1 = uxtw_test(0xFFFFFFFF);
	tests_total++;
	printf("  uxtw(0xFFFFFFFF) = 0x%016lx (expected: 0x00000000ffffffff) %s\n",
	       (uint64_t)r1, (uint64_t)r1 == 0x00000000ffffffffULL ? "✓" : "✗");
	if ((uint64_t)r1 == 0x00000000ffffffffULL) tests_passed++;

	munmap(uxtw_code, 4096);
	printf("\n");

	// Final Results
	printf("==============================================\n");
	printf("Results: %d/%d tests passed (%.1f%%)\n",
	       tests_passed, tests_total,
	       tests_total > 0 ? (100.0 * tests_passed / tests_total) : 0.0);
	printf("==============================================\n");

	if (tests_passed == tests_total) {
		printf("\n✓✓✓ ALL TESTS PASSED ✓✓✓\n");
		printf("ARM64 type conversion operations are working correctly!\n\n");
		return 0;
	} else {
		printf("\n✗✗✗ SOME TESTS FAILED ✗✗✗\n\n");
		return 1;
	}
}
