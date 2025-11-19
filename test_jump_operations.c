/*
 * ARM64 Jump Operations Test
 * Tests all jump patterns used by HashLink bytecode operations
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

typedef uint64_t (*test_func_t)(uint64_t, uint64_t);

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
// Test 1: OJAlways - Unconditional Jump
// Returns: always 100
// =====================================================================
uint32_t* gen_ojalways_test() {
	uint32_t *code = alloc_exec(4096);

	// MOV X0, #50 (this should be skipped)
	code[0] = 0xd2800640;  // MOVZ X0, #50

	// B +2 (unconditional jump to code[3])
	code[1] = 0x14000002;  // B +2

	// MOV X0, #999 (should be skipped)
	code[2] = 0xd2807ce0;  // MOVZ X0, #999

	// MOV X0, #100
	code[3] = 0xd2800c80;  // MOVZ X0, #100

	// RET
	code[4] = 0xd65f03c0;

	flush_cache(code, code + 5);
	return code;
}

// =====================================================================
// Test 2: OJTrue - Jump if non-zero
// if (x0 != 0) return 200; else return 300;
// =====================================================================
uint32_t* gen_ojtrue_test() {
	uint32_t *code = alloc_exec(4096);

	// CBNZ X0, +3 (jump to code[3] if X0 != 0)
	code[0] = 0xb5000060;  // CBNZ X0, +3

	// MOV X0, #300 (X0 was 0)
	code[1] = 0xd2802580;  // MOVZ X0, #300
	code[2] = 0x14000002;  // B +2 (to code[4])

	// MOV X0, #200 (X0 was non-zero)
	code[3] = 0xd2801900;  // MOVZ X0, #200

	// RET
	code[4] = 0xd65f03c0;

	flush_cache(code, code + 5);
	return code;
}

// =====================================================================
// Test 3: OJFalse - Jump if zero
// if (x0 == 0) return 400; else return 500;
// =====================================================================
uint32_t* gen_ojfalse_test() {
	uint32_t *code = alloc_exec(4096);

	// CBZ X0, +3 (jump to code[3] if X0 == 0)
	code[0] = 0xb4000060;  // CBZ X0, +3

	// MOV X0, #500 (X0 was non-zero)
	code[1] = 0xd2803e80;  // MOVZ X0, #500
	code[2] = 0x14000002;  // B +2 (to code[4])

	// MOV X0, #400 (X0 was zero)
	code[3] = 0xd2803200;  // MOVZ X0, #400

	// RET
	code[4] = 0xd65f03c0;

	flush_cache(code, code + 5);
	return code;
}

// =====================================================================
// Test 4: OJEq - Jump if equal
// if (x0 == x1) return 1; else return 0;
// =====================================================================
uint32_t* gen_ojeq_test() {
	uint32_t *code = alloc_exec(4096);

	// CMP X0, X1 (SUBS XZR, X0, X1)
	code[0] = 0xeb01001f;  // SUBS XZR, X0, X1

	// B.EQ +3 (jump to code[4] if equal)
	code[1] = 0x54000060;  // B.EQ +3

	// MOV X0, #0 (not equal)
	code[2] = 0xd2800000;  // MOVZ X0, #0
	code[3] = 0x14000002;  // B +2

	// MOV X0, #1 (equal)
	code[4] = 0xd2800020;  // MOVZ X0, #1

	// RET
	code[5] = 0xd65f03c0;

	flush_cache(code, code + 6);
	return code;
}

// =====================================================================
// Test 5: OJNotEq - Jump if not equal
// if (x0 != x1) return 1; else return 0;
// =====================================================================
uint32_t* gen_ojnoteq_test() {
	uint32_t *code = alloc_exec(4096);

	// CMP X0, X1 (SUBS XZR, X0, X1)
	code[0] = 0xeb01001f;  // SUBS XZR, X0, X1

	// B.NE +3 (jump to code[4] if not equal)
	code[1] = 0x54000061;  // B.NE +3

	// MOV X0, #0 (equal)
	code[2] = 0xd2800000;  // MOVZ X0, #0
	code[3] = 0x14000002;  // B +2

	// MOV X0, #1 (not equal)
	code[4] = 0xd2800020;  // MOVZ X0, #1

	// RET
	code[5] = 0xd65f03c0;

	flush_cache(code, code + 6);
	return code;
}

// =====================================================================
// Test 6: OJSLt - Jump if signed less than
// if (x0 < x1) return 1; else return 0;
// =====================================================================
uint32_t* gen_ojslt_test() {
	uint32_t *code = alloc_exec(4096);

	// CMP X0, X1
	code[0] = 0xeb01001f;  // SUBS XZR, X0, X1

	// B.LT +3 (jump if X0 < X1, signed)
	code[1] = 0x5400006b;  // B.LT +3

	// MOV X0, #0 (not less than)
	code[2] = 0xd2800000;
	code[3] = 0x14000002;  // B +2

	// MOV X0, #1 (less than)
	code[4] = 0xd2800020;

	// RET
	code[5] = 0xd65f03c0;

	flush_cache(code, code + 6);
	return code;
}

// =====================================================================
// Test 7: OJSGte - Jump if signed greater or equal
// if (x0 >= x1) return 1; else return 0;
// =====================================================================
uint32_t* gen_ojsgte_test() {
	uint32_t *code = alloc_exec(4096);

	// CMP X0, X1
	code[0] = 0xeb01001f;

	// B.GE +3
	code[1] = 0x5400006a;  // B.GE +3

	// MOV X0, #0
	code[2] = 0xd2800000;
	code[3] = 0x14000002;

	// MOV X0, #1
	code[4] = 0xd2800020;

	// RET
	code[5] = 0xd65f03c0;

	flush_cache(code, code + 6);
	return code;
}

// =====================================================================
// Test 8: OJULt - Jump if unsigned less than
// if (x0 < x1, unsigned) return 1; else return 0;
// =====================================================================
uint32_t* gen_ojult_test() {
	uint32_t *code = alloc_exec(4096);

	// CMP X0, X1
	code[0] = 0xeb01001f;

	// B.LO +3 (LO = unsigned less than)
	code[1] = 0x54000063;  // B.LO +3

	// MOV X0, #0
	code[2] = 0xd2800000;
	code[3] = 0x14000002;

	// MOV X0, #1
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
	printf("ARM64 Jump Operations Tests\n");
	printf("==============================================\n\n");

	// Test 1: OJAlways
	printf("Test 1: OJAlways (Unconditional Jump)\n");
	printf("----------------------------------------------\n");
	uint32_t *always_code = gen_ojalways_test();
	test_func_t always_test = (test_func_t)always_code;

	uint64_t r1 = always_test(0, 0);
	tests_total++;
	printf("  always_test() = %lu (expected: 100) %s\n", r1, r1 == 100 ? "✓" : "✗");
	if (r1 == 100) tests_passed++;
	munmap(always_code, 4096);
	printf("\n");

	// Test 2: OJTrue
	printf("Test 2: OJTrue (Jump if Non-Zero)\n");
	printf("----------------------------------------------\n");
	uint32_t *true_code = gen_ojtrue_test();
	test_func_t true_test = (test_func_t)true_code;

	r1 = true_test(0, 0);   // 0 -> false branch -> 300
	uint64_t r2 = true_test(5, 0);   // 5 -> true branch -> 200

	tests_total += 2;
	printf("  true_test(0) = %lu (expected: 300) %s\n", r1, r1 == 300 ? "✓" : "✗");
	printf("  true_test(5) = %lu (expected: 200) %s\n", r2, r2 == 200 ? "✓" : "✗");
	if (r1 == 300) tests_passed++;
	if (r2 == 200) tests_passed++;
	munmap(true_code, 4096);
	printf("\n");

	// Test 3: OJFalse
	printf("Test 3: OJFalse (Jump if Zero)\n");
	printf("----------------------------------------------\n");
	uint32_t *false_code = gen_ojfalse_test();
	test_func_t false_test = (test_func_t)false_code;

	r1 = false_test(0, 0);   // 0 -> true (zero) branch -> 400
	r2 = false_test(5, 0);   // 5 -> false (non-zero) branch -> 500

	tests_total += 2;
	printf("  false_test(0) = %lu (expected: 400) %s\n", r1, r1 == 400 ? "✓" : "✗");
	printf("  false_test(5) = %lu (expected: 500) %s\n", r2, r2 == 500 ? "✓" : "✗");
	if (r1 == 400) tests_passed++;
	if (r2 == 500) tests_passed++;
	munmap(false_code, 4096);
	printf("\n");

	// Test 4: OJEq
	printf("Test 4: OJEq (Jump if Equal)\n");
	printf("----------------------------------------------\n");
	uint32_t *eq_code = gen_ojeq_test();
	test_func_t eq_test = (test_func_t)eq_code;

	r1 = eq_test(10, 10);  // equal -> 1
	r2 = eq_test(10, 5);   // not equal -> 0

	tests_total += 2;
	printf("  eq_test(10, 10) = %lu (expected: 1) %s\n", r1, r1 == 1 ? "✓" : "✗");
	printf("  eq_test(10, 5) = %lu (expected: 0) %s\n", r2, r2 == 0 ? "✓" : "✗");
	if (r1 == 1) tests_passed++;
	if (r2 == 0) tests_passed++;
	munmap(eq_code, 4096);
	printf("\n");

	// Test 5: OJNotEq
	printf("Test 5: OJNotEq (Jump if Not Equal)\n");
	printf("----------------------------------------------\n");
	uint32_t *noteq_code = gen_ojnoteq_test();
	test_func_t noteq_test = (test_func_t)noteq_code;

	r1 = noteq_test(10, 10);  // equal -> 0
	r2 = noteq_test(10, 5);   // not equal -> 1

	tests_total += 2;
	printf("  noteq_test(10, 10) = %lu (expected: 0) %s\n", r1, r1 == 0 ? "✓" : "✗");
	printf("  noteq_test(10, 5) = %lu (expected: 1) %s\n", r2, r2 == 1 ? "✓" : "✗");
	if (r1 == 0) tests_passed++;
	if (r2 == 1) tests_passed++;
	munmap(noteq_code, 4096);
	printf("\n");

	// Test 6: OJSLt
	printf("Test 6: OJSLt (Jump if Signed Less Than)\n");
	printf("----------------------------------------------\n");
	uint32_t *slt_code = gen_ojslt_test();
	test_func_t slt_test = (test_func_t)slt_code;

	r1 = slt_test(5, 10);   // 5 < 10 -> 1
	r2 = slt_test(10, 5);   // 10 < 5 -> 0
	uint64_t r3 = slt_test(10, 10);  // 10 < 10 -> 0

	tests_total += 3;
	printf("  slt_test(5, 10) = %lu (expected: 1) %s\n", r1, r1 == 1 ? "✓" : "✗");
	printf("  slt_test(10, 5) = %lu (expected: 0) %s\n", r2, r2 == 0 ? "✓" : "✗");
	printf("  slt_test(10, 10) = %lu (expected: 0) %s\n", r3, r3 == 0 ? "✓" : "✗");
	if (r1 == 1) tests_passed++;
	if (r2 == 0) tests_passed++;
	if (r3 == 0) tests_passed++;
	munmap(slt_code, 4096);
	printf("\n");

	// Test 7: OJSGte
	printf("Test 7: OJSGte (Jump if Signed Greater or Equal)\n");
	printf("----------------------------------------------\n");
	uint32_t *sgte_code = gen_ojsgte_test();
	test_func_t sgte_test = (test_func_t)sgte_code;

	r1 = sgte_test(10, 5);   // 10 >= 5 -> 1
	r2 = sgte_test(5, 10);   // 5 >= 10 -> 0
	r3 = sgte_test(10, 10);  // 10 >= 10 -> 1

	tests_total += 3;
	printf("  sgte_test(10, 5) = %lu (expected: 1) %s\n", r1, r1 == 1 ? "✓" : "✗");
	printf("  sgte_test(5, 10) = %lu (expected: 0) %s\n", r2, r2 == 0 ? "✓" : "✗");
	printf("  sgte_test(10, 10) = %lu (expected: 1) %s\n", r3, r3 == 1 ? "✓" : "✗");
	if (r1 == 1) tests_passed++;
	if (r2 == 0) tests_passed++;
	if (r3 == 1) tests_passed++;
	munmap(sgte_code, 4096);
	printf("\n");

	// Test 8: OJULt
	printf("Test 8: OJULt (Jump if Unsigned Less Than)\n");
	printf("----------------------------------------------\n");
	uint32_t *ult_code = gen_ojult_test();
	test_func_t ult_test = (test_func_t)ult_code;

	r1 = ult_test(5, 10);   // 5 < 10 -> 1
	r2 = ult_test(10, 5);   // 10 < 5 -> 0
	// Test with unsigned values (0xFFFFFFFFFFFFFFFF > 10 when unsigned)
	r3 = ult_test(0xFFFFFFFFFFFFFFFFULL, 10);  // max_uint < 10 -> 0 (unsigned)

	tests_total += 3;
	printf("  ult_test(5, 10) = %lu (expected: 1) %s\n", r1, r1 == 1 ? "✓" : "✗");
	printf("  ult_test(10, 5) = %lu (expected: 0) %s\n", r2, r2 == 0 ? "✓" : "✗");
	printf("  ult_test(max_uint, 10) = %lu (expected: 0) %s\n", r3, r3 == 0 ? "✓" : "✗");
	if (r1 == 1) tests_passed++;
	if (r2 == 0) tests_passed++;
	if (r3 == 0) tests_passed++;
	munmap(ult_code, 4096);
	printf("\n");

	// Final Results
	printf("==============================================\n");
	printf("Results: %d/%d tests passed (%.1f%%)\n",
	       tests_passed, tests_total,
	       tests_total > 0 ? (100.0 * tests_passed / tests_total) : 0.0);
	printf("==============================================\n");

	if (tests_passed == tests_total) {
		printf("\n✓✓✓ ALL TESTS PASSED ✓✓✓\n");
		printf("ARM64 jump operations are working correctly!\n\n");
		return 0;
	} else {
		printf("\n✗✗✗ SOME TESTS FAILED ✗✗✗\n\n");
		return 1;
	}
}
