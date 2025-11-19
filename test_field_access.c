/*
 * ARM64 Field Access Test
 * Tests field access patterns using LDR/STR with immediate offsets
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

typedef uint64_t (*test_func_t)(void*, uint64_t);

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

// Simulated object structure
typedef struct {
	uint64_t field0;   // offset 0
	uint64_t field1;   // offset 8
	uint64_t field2;   // offset 16
	uint64_t field3;   // offset 24
	uint64_t field4;   // offset 32
} TestObject;

// =====================================================================
// Test 1: OField - Load field with immediate offset
// X0 = object pointer, returns object->field2 (offset 16)
// =====================================================================
uint32_t* gen_field_load_test() {
	uint32_t *code = alloc_exec(4096);

	// LDR X0, [X0, #16] - load from offset 16 (field2)
	// Format: size=11 111 0 01 imm12 Rn Rt
	// size=11 (64-bit), imm12=16/8=2, Rn=X0, Rt=X0
	code[0] = 0xf9400800;  // LDR X0, [X0, #16]

	// RET
	code[1] = 0xd65f03c0;

	flush_cache(code, code + 2);
	return code;
}

// =====================================================================
// Test 2: OSetField - Store field with immediate offset
// X0 = object pointer, X1 = value
// Stores X1 to object->field3 (offset 24)
// =====================================================================
uint32_t* gen_field_store_test() {
	uint32_t *code = alloc_exec(4096);

	// STR X1, [X0, #24] - store to offset 24 (field3)
	// Format: size=11 111 0 00 imm12 Rn Rt
	// size=11 (64-bit), imm12=24/8=3, Rn=X0, Rt=X1
	code[0] = 0xf9000c01;  // STR X1, [X0, #24]

	// MOV X0, #1 (return success)
	code[1] = 0xd2800020;

	// RET
	code[2] = 0xd65f03c0;

	flush_cache(code, code + 3);
	return code;
}

// =====================================================================
// Test 3: Load multiple fields
// Returns sum of field0 + field1 + field2
// =====================================================================
uint32_t* gen_multi_field_test() {
	uint32_t *code = alloc_exec(4096);

	// LDR X1, [X0, #0]  - load field0
	code[0] = 0xf9400001;

	// LDR X2, [X0, #8]  - load field1
	code[1] = 0xf9400402;

	// LDR X3, [X0, #16] - load field2
	code[2] = 0xf9400803;

	// ADD X0, X1, X2
	code[3] = 0x8b020020;

	// ADD X0, X0, X3
	code[4] = 0x8b030000;

	// RET
	code[5] = 0xd65f03c0;

	flush_cache(code, code + 6);
	return code;
}

// =====================================================================
// Test 4: String/bytes pointer loading (load 64-bit constant)
// Simulates loading a constant pointer
// =====================================================================
uint32_t* gen_const_load_test() {
	uint32_t *code = alloc_exec(4096);

	// MOVZ X0, #0x1234, LSL #0
	code[0] = 0xd2824680;  // MOVZ X0, #0x1234

	// MOVK X0, #0x5678, LSL #16
	code[1] = 0xf2aacf00;  // MOVK X0, #0x5678, LSL #16

	// MOVK X0, #0x9abc, LSL #32
	code[2] = 0xf2d35780;  // MOVK X0, #0x9abc, LSL #32

	// MOVK X0, #0xdef0, LSL #48
	code[3] = 0xf2fbde00;  // MOVK X0, #0xdef0, LSL #48

	// Result: 0xdef09abc56781234

	// RET
	code[4] = 0xd65f03c0;

	flush_cache(code, code + 5);
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
	printf("ARM64 Field Access Tests\n");
	printf("==============================================\n\n");

	// Test 1: Field Load
	printf("Test 1: OField - Load Field with Immediate Offset\n");
	printf("----------------------------------------------\n");
	TestObject obj1 = {100, 200, 300, 400, 500};
	uint32_t *field_load_code = gen_field_load_test();
	test_func_t field_load = (test_func_t)field_load_code;

	uint64_t r1 = field_load(&obj1, 0);  // Should load field2 = 300

	tests_total++;
	printf("  field_load(obj, 0) = %lu (expected: 300) %s\n", r1, r1 == 300 ? "✓" : "✗");
	if (r1 == 300) tests_passed++;
	munmap(field_load_code, 4096);
	printf("\n");

	// Test 2: Field Store
	printf("Test 2: OSetField - Store Field with Immediate Offset\n");
	printf("----------------------------------------------\n");
	TestObject obj2 = {100, 200, 300, 400, 500};
	uint32_t *field_store_code = gen_field_store_test();
	test_func_t field_store = (test_func_t)field_store_code;

	field_store(&obj2, 999);  // Should store 999 to field3

	tests_total++;
	printf("  After field_store(obj, 999): obj.field3 = %lu (expected: 999) %s\n",
	       obj2.field3, obj2.field3 == 999 ? "✓" : "✗");
	if (obj2.field3 == 999) tests_passed++;
	munmap(field_store_code, 4096);
	printf("\n");

	// Test 3: Multi-field Load
	printf("Test 3: Load Multiple Fields\n");
	printf("----------------------------------------------\n");
	TestObject obj3 = {10, 20, 30, 40, 50};
	uint32_t *multi_field_code = gen_multi_field_test();
	test_func_t multi_field = (test_func_t)multi_field_code;

	r1 = multi_field(&obj3, 0);  // Should return 10+20+30 = 60

	tests_total++;
	printf("  multi_field(obj) = %lu (expected: 60) %s\n", r1, r1 == 60 ? "✓" : "✗");
	if (r1 == 60) tests_passed++;
	munmap(multi_field_code, 4096);
	printf("\n");

	// Test 4: Constant Pointer Load (simulates OString/OBytes)
	printf("Test 4: Load 64-bit Constant (OString/OBytes pattern)\n");
	printf("----------------------------------------------\n");
	uint32_t *const_load_code = gen_const_load_test();
	test_func_t const_load = (test_func_t)const_load_code;

	r1 = const_load(NULL, 0);  // Should return 0xdef09abc56781234

	tests_total++;
	printf("  const_load() = 0x%016lx (expected: 0xdef09abc56781234) %s\n",
	       r1, r1 == 0xdef09abc56781234ULL ? "✓" : "✗");
	if (r1 == 0xdef09abc56781234ULL) tests_passed++;
	munmap(const_load_code, 4096);
	printf("\n");

	// Final Results
	printf("==============================================\n");
	printf("Results: %d/%d tests passed (%.1f%%)\n",
	       tests_passed, tests_total,
	       tests_total > 0 ? (100.0 * tests_passed / tests_total) : 0.0);
	printf("==============================================\n");

	if (tests_passed == tests_total) {
		printf("\n✓✓✓ ALL TESTS PASSED ✓✓✓\n");
		printf("ARM64 field access operations are working correctly!\n\n");
		return 0;
	} else {
		printf("\n✗✗✗ SOME TESTS FAILED ✗✗✗\n\n");
		return 1;
	}
}
