/*
 * ARM64 Branch Instruction Test
 * Tests branch encodings and execution under QEMU
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

typedef uint64_t (*test_func_t)(uint64_t);

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
// Test 1: Unconditional Branch (B)
// Tests: if (x0 > 5) return 10; else return 20;
// =====================================================================
uint32_t* gen_branch_test() {
	uint32_t *code = alloc_exec(4096);
	// CMP X0, #5
	// Format: sf=1 11 100010 sh=0 imm12=5 Rn=0 Rd=31(XZR)
	// SUBS XZR, X0, #5
	code[0] = 0xf1001400;  // CMP X0, #5

	// B.LE skip  (branch to code[4] if X0 <= 5)
	// From code[1] to code[4] = 3 instructions forward
	code[1] = 0x5400006d;  // B.LE +3

	// MOV X0, #10 (when X0 > 5)
	code[2] = 0xd2800140;  // MOVZ X0, #10

	// B end (skip to code[5])
	code[3] = 0x14000002;  // B +2

	// skip: MOV X0, #20 (when X0 <= 5)
	code[4] = 0xd2800280;  // MOVZ X0, #20

	// end: RET
	code[5] = 0xd65f03c0;

	flush_cache(code, code + 6);
	return code;
}

// =====================================================================
// Test 2: Compare and Branch Zero (CBZ)
// Tests: if (x0 == 0) return 100; else return 200;
// =====================================================================
uint32_t* gen_cbz_test() {
	uint32_t *code = alloc_exec(4096);

	// CBZ X0, zero_case (if X0 == 0, jump to code[3])
	// imm19 = 2 (skip code[1] and code[2])
	code[0] = 0xb4000060;  // CBZ X0, +3 (to code[3])

	// MOV X0, #200 (non-zero case)
	code[1] = 0xd2801900;  // MOVZ X0, #200
	code[2] = 0x14000002;  // B +2 (to code[4])

	// zero_case: MOV X0, #100
	code[3] = 0xd2800c80;  // MOVZ X0, #100

	// RET
	code[4] = 0xd65f03c0;

	flush_cache(code, code + 5);
	return code;
}

// =====================================================================
// Test 3: Compare and Branch Non-Zero (CBNZ)
// Tests: if (x0 != 0) return x0 * 2; else return 999;
// =====================================================================
uint32_t* gen_cbnz_test() {
	uint32_t *code = alloc_exec(4096);

	// CBNZ X0, nonzero_case (if X0 != 0, jump to code[3])
	code[0] = 0xb5000060;  // CBNZ X0, +3 (to code[3])

	// MOV X0, #999 (zero case)
	code[1] = 0xd2807ce0;  // MOVZ X0, #999
	code[2] = 0x14000002;  // B +2 (to code[4])

	// nonzero_case: LSL X0, X0, #1 (multiply by 2)
	// LSL is alias for UBFM with immr=63, imms=62 (shift left by 1)
	// Format: sf=1 10 100110 N=1 immr=63 imms=62 Rn=0 Rd=0
	code[3] = 0xd37ff800;  // LSL X0, X0, #1

	// RET
	code[4] = 0xd65f03c0;

	flush_cache(code, code + 5);
	return code;
}

// =====================================================================
// Test 4: Conditional Branches (all conditions)
// Tests various comparison conditions
// =====================================================================
uint32_t* gen_cond_test(int test_type) {
	uint32_t *code = alloc_exec(4096);

	// CMP X0, #10
	code[0] = 0xf100281f;  // CMP X0, #10 (SUBS XZR, X0, #10)

	uint32_t cond_code;
	switch(test_type) {
		case 0: cond_code = 0x0; break;  // EQ
		case 1: cond_code = 0x1; break;  // NE
		case 2: cond_code = 0xB; break;  // LT
		case 3: cond_code = 0xA; break;  // GE
		case 4: cond_code = 0xD; break;  // LE
		case 5: cond_code = 0xC; break;  // GT
		default: cond_code = 0xE; break; // AL (always)
	}

	// B.cond +3 (branch to code[4] if condition true)
	code[1] = 0x54000000 | (3 << 5) | cond_code;

	// MOV X0, #0 (condition false)
	code[2] = 0xd2800000;
	code[3] = 0x14000002;  // B +2 (to code[5])

	// MOV X0, #1 (condition true)
	code[4] = 0xd2800020;

	// RET
	code[5] = 0xd65f03c0;

	flush_cache(code, code + 6);
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
	printf("ARM64 Branch Instruction Tests\n");
	printf("==============================================\n\n");

	// Test 1: Unconditional Branch
	printf("Test 1: B (Unconditional Branch)\n");
	printf("----------------------------------------------\n");
	uint32_t *branch_code = gen_branch_test();
	test_func_t branch_test = (test_func_t)branch_code;

	uint64_t r1 = branch_test(3);   // 3 <= 5, should return 20
	uint64_t r2 = branch_test(10);  // 10 > 5, should return 10

	tests_total += 2;
	printf("  branch_test(3) = %lu (expected: 20) %s\n", r1, r1 == 20 ? "✓" : "✗");
	printf("  branch_test(10) = %lu (expected: 10) %s\n", r2, r2 == 10 ? "✓" : "✗");
	if (r1 == 20) tests_passed++;
	if (r2 == 10) tests_passed++;
	munmap(branch_code, 4096);
	printf("\n");

	// Test 2: CBZ (Compare and Branch if Zero)
	printf("Test 2: CBZ (Compare and Branch if Zero)\n");
	printf("----------------------------------------------\n");
	uint32_t *cbz_code = gen_cbz_test();
	test_func_t cbz_test = (test_func_t)cbz_code;

	r1 = cbz_test(0);   // 0 == 0, should return 100
	r2 = cbz_test(5);   // 5 != 0, should return 200

	tests_total += 2;
	printf("  cbz_test(0) = %lu (expected: 100) %s\n", r1, r1 == 100 ? "✓" : "✗");
	printf("  cbz_test(5) = %lu (expected: 200) %s\n", r2, r2 == 200 ? "✓" : "✗");
	if (r1 == 100) tests_passed++;
	if (r2 == 200) tests_passed++;
	munmap(cbz_code, 4096);
	printf("\n");

	// Test 3: CBNZ (Compare and Branch if Non-Zero)
	printf("Test 3: CBNZ (Compare and Branch if Non-Zero)\n");
	printf("----------------------------------------------\n");
	uint32_t *cbnz_code = gen_cbnz_test();
	test_func_t cbnz_test = (test_func_t)cbnz_code;

	r1 = cbnz_test(0);   // 0 == 0, should return 999
	r2 = cbnz_test(50);  // 50 != 0, should return 100

	tests_total += 2;
	printf("  cbnz_test(0) = %lu (expected: 999) %s\n", r1, r1 == 999 ? "✓" : "✗");
	printf("  cbnz_test(50) = %lu (expected: 100) %s\n", r2, r2 == 100 ? "✓" : "✗");
	if (r1 == 999) tests_passed++;
	if (r2 == 100) tests_passed++;
	munmap(cbnz_code, 4096);
	printf("\n");

	// Test 4: Conditional Branches
	printf("Test 4: B.cond (Conditional Branches)\n");
	printf("----------------------------------------------\n");

	const char *cond_names[] = {"EQ", "NE", "LT", "GE", "LE", "GT"};
	uint64_t test_vals[] = {10, 10, 5, 15, 10, 15};
	uint64_t expected[] = {1, 0, 1, 1, 1, 1};

	for (int i = 0; i < 6; i++) {
		uint32_t *cond_code = gen_cond_test(i);
		test_func_t cond_test = (test_func_t)cond_code;

		// Debug: print first 3 instructions
		if (i == 1) {  // NE test
			printf("  DEBUG NE test code:\n");
			printf("    [0] = 0x%08x (CMP)\n", cond_code[0]);
			printf("    [1] = 0x%08x (B.NE)\n", cond_code[1]);
			printf("    [2] = 0x%08x (MOV #0)\n", cond_code[2]);
		}

		r1 = cond_test(test_vals[i]);
		tests_total++;
		printf("  B.%s with %lu: %lu (expected: %lu) %s\n",
		       cond_names[i], test_vals[i], r1, expected[i],
		       r1 == expected[i] ? "✓" : "✗");
		if (r1 == expected[i]) tests_passed++;

		munmap(cond_code, 4096);
	}
	printf("\n");

	// Final Results
	printf("==============================================\n");
	printf("Results: %d/%d tests passed (%.1f%%)\n",
	       tests_passed, tests_total,
	       tests_total > 0 ? (100.0 * tests_passed / tests_total) : 0.0);
	printf("==============================================\n");

	if (tests_passed == tests_total) {
		printf("\n✓✓✓ ALL TESTS PASSED ✓✓✓\n");
		printf("ARM64 branch instructions are working correctly!\n\n");
		return 0;
	} else {
		printf("\n✗✗✗ SOME TESTS FAILED ✗✗✗\n\n");
		return 1;
	}
}
