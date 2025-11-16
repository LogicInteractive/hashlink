/*
 * ARM64 Memory Operations Test
 * Tests LDR/STR with register offsets
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>

// Test architecture
#if !defined(__aarch64__) && !defined(_M_ARM64)
#error "Must compile for ARM64!"
#endif

typedef uint64_t (*test_func_t)(uint64_t*, uint64_t);

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
// Test 1: LDR with Register Offset
// Load from array[x1]: x0 = *(array + x1*8)
// =====================================================================
uint32_t* gen_ldr_reg_test() {
	uint32_t *code = alloc_exec(4096);

	// X0 = array pointer (input)
	// X1 = index (input)

	// LSL X1, X1, #3 (multiply index by 8 for 64-bit elements)
	code[0] = 0xd37df021;  // LSL X1, X1, #3

	// LDR X0, [X0, X1] (load from array[index])
	// Format: size=11 111 0 00 1 Rm option S 10 Rn Rt
	// size=11 (64-bit), Rm=X1, option=011, S=0, Rn=X0, Rt=X0
	code[1] = 0xf8616800;  // LDR X0, [X0, X1]

	// RET
	code[2] = 0xd65f03c0;

	flush_cache(code, code + 3);
	return code;
}

// =====================================================================
// Test 2: STR with Register Offset
// Store to array[x1]: *(array + x1*8) = 999
// =====================================================================
uint32_t* gen_str_reg_test() {
	uint32_t *code = alloc_exec(4096);

	// X0 = array pointer (input)
	// X1 = index (input)

	// MOV X2, #999
	code[0] = 0xd2807ce2;  // MOVZ X2, #999

	// LSL X1, X1, #3 (multiply index by 8)
	code[1] = 0xd37df021;  // LSL X1, X1, #3

	// STR X2, [X0, X1] (store to array[index])
	// Format: size=11 111 0 00 0 Rm option S 10 Rn Rt
	// size=11 (64-bit), Rm=X1, option=011, S=0, Rn=X0, Rt=X2
	code[2] = 0xf8216802;  // STR X2, [X0, X1]

	// MOV X0, #1 (return success)
	code[3] = 0xd2800020;

	// RET
	code[4] = 0xd65f03c0;

	flush_cache(code, code + 5);
	return code;
}

// =====================================================================
// Test 3: LDR byte with register offset (LDRB)
// =====================================================================
uint32_t* gen_ldrb_reg_test() {
	uint32_t *code = alloc_exec(4096);

	// X0 = array pointer
	// X1 = byte index

	// LDRB W0, [X0, X1] (load byte)
	// Format: size=00 111 0 00 1 Rm option S 10 Rn Rt
	// size=00 (8-bit), Rm=X1, option=011, S=0, Rn=X0, Rt=W0
	code[0] = 0x38616800;  // LDRB W0, [X0, X1]

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
	printf("ARM64 Memory Operations Tests\n");
	printf("==============================================\n\n");

	// Test 1: LDR with Register Offset
	printf("Test 1: LDR X0, [X0, X1] (64-bit load)\n");
	printf("----------------------------------------------\n");
	uint64_t test_array[] = {100, 200, 300, 400, 500};
	uint32_t *ldr_code = gen_ldr_reg_test();
	test_func_t ldr_test = (test_func_t)ldr_code;

	uint64_t r1 = ldr_test(test_array, 0);  // array[0] = 100
	uint64_t r2 = ldr_test(test_array, 2);  // array[2] = 300
	uint64_t r3 = ldr_test(test_array, 4);  // array[4] = 500

	tests_total += 3;
	printf("  ldr_test(array, 0) = %lu (expected: 100) %s\n", r1, r1 == 100 ? "✓" : "✗");
	printf("  ldr_test(array, 2) = %lu (expected: 300) %s\n", r2, r2 == 300 ? "✓" : "✗");
	printf("  ldr_test(array, 4) = %lu (expected: 500) %s\n", r3, r3 == 500 ? "✓" : "✗");
	if (r1 == 100) tests_passed++;
	if (r2 == 300) tests_passed++;
	if (r3 == 500) tests_passed++;
	munmap(ldr_code, 4096);
	printf("\n");

	// Test 2: STR with Register Offset
	printf("Test 2: STR X2, [X0, X1] (64-bit store)\n");
	printf("----------------------------------------------\n");
	uint64_t store_array[] = {0, 0, 0, 0, 0};
	uint32_t *str_code = gen_str_reg_test();
	test_func_t str_test = (test_func_t)str_code;

	str_test(store_array, 1);  // store 999 to array[1]
	str_test(store_array, 3);  // store 999 to array[3]

	tests_total += 2;
	printf("  After str_test(array, 1): array[1] = %lu (expected: 999) %s\n",
	       store_array[1], store_array[1] == 999 ? "✓" : "✗");
	printf("  After str_test(array, 3): array[3] = %lu (expected: 999) %s\n",
	       store_array[3], store_array[3] == 999 ? "✓" : "✗");
	if (store_array[1] == 999) tests_passed++;
	if (store_array[3] == 999) tests_passed++;
	munmap(str_code, 4096);
	printf("\n");

	// Test 3: LDRB (byte load)
	printf("Test 3: LDRB W0, [X0, X1] (8-bit load)\n");
	printf("----------------------------------------------\n");
	uint8_t byte_array[] = {0x11, 0x22, 0x33, 0x44, 0x55};
	uint32_t *ldrb_code = gen_ldrb_reg_test();
	test_func_t ldrb_test = (test_func_t)ldrb_code;

	r1 = ldrb_test((uint64_t*)byte_array, 0);  // byte[0] = 0x11
	r2 = ldrb_test((uint64_t*)byte_array, 2);  // byte[2] = 0x33
	r3 = ldrb_test((uint64_t*)byte_array, 4);  // byte[4] = 0x55

	tests_total += 3;
	printf("  ldrb_test(array, 0) = 0x%02lx (expected: 0x11) %s\n", r1, r1 == 0x11 ? "✓" : "✗");
	printf("  ldrb_test(array, 2) = 0x%02lx (expected: 0x33) %s\n", r2, r2 == 0x33 ? "✓" : "✗");
	printf("  ldrb_test(array, 4) = 0x%02lx (expected: 0x55) %s\n", r3, r3 == 0x55 ? "✓" : "✗");
	if (r1 == 0x11) tests_passed++;
	if (r2 == 0x33) tests_passed++;
	if (r3 == 0x55) tests_passed++;
	munmap(ldrb_code, 4096);
	printf("\n");

	// Final Results
	printf("==============================================\n");
	printf("Results: %d/%d tests passed (%.1f%%)\n",
	       tests_passed, tests_total,
	       tests_total > 0 ? (100.0 * tests_passed / tests_total) : 0.0);
	printf("==============================================\n");

	if (tests_passed == tests_total) {
		printf("\n✓✓✓ ALL TESTS PASSED ✓✓✓\n");
		printf("ARM64 memory operations are working correctly!\n\n");
		return 0;
	} else {
		printf("\n✗✗✗ SOME TESTS FAILED ✗✗✗\n\n");
		return 1;
	}
}
