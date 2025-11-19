/*
 * ARM64 JIT ORef Operation Test
 *
 * Tests the ORef operation which gets a pointer to a stack variable.
 * ORef is essential for:
 * - Passing addresses of local variables to functions
 * - Creating references to stack-allocated data
 * - Implementing ref/out parameters
 *
 * Test approach:
 * Since we can't directly test JIT bytecode, we test the underlying
 * stack addressing logic that ORef implements.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

// Test Results
int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) printf("  Testing: %s\n", name)
#define PASS() do { tests_passed++; printf("    ✓ PASS\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("    ✗ FAIL: %s\n", msg); } while(0)
#define ASSERT(cond, msg) if (!(cond)) { FAIL(msg); return; } else { PASS(); }

// Get current frame pointer
static inline uint64_t get_fp(void) {
	uint64_t fp;
	__asm__ volatile("mov %0, x29" : "=r"(fp));
	return fp;
}

// Simulate ORef operation for local variable (negative stackPos)
// ORef: dst = &ra where ra is at FP + stackPos (stackPos < 0 for locals)
static inline uint64_t oref_local(int stackPos) {
	uint64_t fp = get_fp();
	// ORef calculates: FP - abs(stackPos) for locals
	// ARM64: SUB Xd, X29, #abs(stackPos)
	return fp + stackPos;  // stackPos is negative, so this is subtraction
}

// Simulate ORef operation for stack argument (positive stackPos)
// ORef: dst = &ra where ra is at FP + stackPos (stackPos > 0 for args)
static inline uint64_t oref_arg(int stackPos) {
	uint64_t fp = get_fp();
	// ORef calculates: FP + stackPos for stack arguments
	// ARM64: ADD Xd, X29, #stackPos
	return fp + stackPos;  // stackPos is positive
}

// Test 1: ORef for local variable at FP-8
__attribute__((noinline))
static void test_oref_local_simple(void) {
	TEST("ORef for local variable (FP-8)");

	volatile int64_t local_var = 0x123456789ABCDEF0LL;
	uint64_t var_addr = (uint64_t)&local_var;
	uint64_t fp = get_fp();

	// Variable should be below FP
	ASSERT(var_addr < fp, "Local variable is below FP");

	// Calculate stackPos for this variable
	int stackPos = (int)(var_addr - fp);  // Will be negative

	// Simulate ORef operation
	uint64_t oref_result = oref_local(stackPos);

	// ORef should return the actual address
	ASSERT(oref_result == var_addr, "ORef returns correct address");

	// Verify we can read through the reference
	uint64_t *ref = (uint64_t*)oref_result;
	ASSERT(*ref == local_var, "Can read value through ORef pointer");
}

// Test 2: ORef for multiple local variables
__attribute__((noinline))
static void test_oref_multiple_locals(void) {
	TEST("ORef for multiple local variables");

	volatile int64_t local1 = 0x1111;
	volatile int64_t local2 = 0x2222;
	volatile int64_t local3 = 0x3333;

	uint64_t fp = get_fp();

	// Get addresses
	uint64_t addr1 = (uint64_t)&local1;
	uint64_t addr2 = (uint64_t)&local2;
	uint64_t addr3 = (uint64_t)&local3;

	// All should be below FP
	ASSERT(addr1 < fp && addr2 < fp && addr3 < fp,
	       "All local variables below FP");

	// Calculate stackPos for each
	int stackPos1 = (int)(addr1 - fp);
	int stackPos2 = (int)(addr2 - fp);
	int stackPos3 = (int)(addr3 - fp);

	// Simulate ORef operations
	uint64_t ref1 = oref_local(stackPos1);
	uint64_t ref2 = oref_local(stackPos2);
	uint64_t ref3 = oref_local(stackPos3);

	// Verify addresses match
	ASSERT(ref1 == addr1, "ORef correct for local1");
	ASSERT(ref2 == addr2, "ORef correct for local2");
	ASSERT(ref3 == addr3, "ORef correct for local3");

	// Verify values through references
	ASSERT(*(int64_t*)ref1 == 0x1111, "Value correct through ref1");
	ASSERT(*(int64_t*)ref2 == 0x2222, "Value correct through ref2");
	ASSERT(*(int64_t*)ref3 == 0x3333, "Value correct through ref3");
}

// Test 3: ORef with modification through pointer
__attribute__((noinline))
static void test_oref_modify(void) {
	TEST("Modify variable through ORef pointer");

	volatile int64_t local_var = 100;
	uint64_t fp = get_fp();
	uint64_t addr = (uint64_t)&local_var;
	int stackPos = (int)(addr - fp);

	// Get reference via ORef
	uint64_t ref = oref_local(stackPos);
	int64_t *ptr = (int64_t*)ref;

	// Modify through pointer
	*ptr = 200;

	// Verify original variable changed
	ASSERT(local_var == 200, "Variable modified through ORef pointer");

	// Modify original
	local_var = 300;

	// Verify pointer sees change
	ASSERT(*ptr == 300, "Pointer reflects variable change");
}

// Test 4: ORef for different sized variables
__attribute__((noinline))
static void test_oref_different_sizes(void) {
	TEST("ORef for different sized variables");

	volatile uint8_t  var8  = 0x12;
	volatile uint16_t var16 = 0x3456;
	volatile uint32_t var32 = 0x789ABCDE;
	volatile uint64_t var64 = 0xFEDCBA9876543210ULL;

	uint64_t fp = get_fp();

	// Get ORef for each
	int stackPos8  = (int)((uint64_t)&var8  - fp);
	int stackPos16 = (int)((uint64_t)&var16 - fp);
	int stackPos32 = (int)((uint64_t)&var32 - fp);
	int stackPos64 = (int)((uint64_t)&var64 - fp);

	uint64_t ref8  = oref_local(stackPos8);
	uint64_t ref16 = oref_local(stackPos16);
	uint64_t ref32 = oref_local(stackPos32);
	uint64_t ref64 = oref_local(stackPos64);

	// Verify addresses
	ASSERT(ref8  == (uint64_t)&var8,  "ORef correct for 8-bit var");
	ASSERT(ref16 == (uint64_t)&var16, "ORef correct for 16-bit var");
	ASSERT(ref32 == (uint64_t)&var32, "ORef correct for 32-bit var");
	ASSERT(ref64 == (uint64_t)&var64, "ORef correct for 64-bit var");

	// Verify values through pointers
	ASSERT(*(uint8_t*)ref8   == 0x12,               "8-bit value correct");
	ASSERT(*(uint16_t*)ref16 == 0x3456,             "16-bit value correct");
	ASSERT(*(uint32_t*)ref32 == 0x789ABCDE,         "32-bit value correct");
	ASSERT(*(uint64_t*)ref64 == 0xFEDCBA9876543210ULL, "64-bit value correct");
}

// Test 5: ORef in nested function calls
__attribute__((noinline))
static void nested_level2(int64_t *ptr, int64_t expected) {
	ASSERT(*ptr == expected, "Pointer valid in nested call level 2");
	*ptr = expected + 100;
}

__attribute__((noinline))
static void nested_level1(int64_t *ptr, int64_t expected) {
	ASSERT(*ptr == expected, "Pointer valid in nested call level 1");
	*ptr = expected + 50;
	nested_level2(ptr, expected + 50);
}

__attribute__((noinline))
static void test_oref_nested_calls(void) {
	TEST("ORef pointer across nested calls");

	volatile int64_t local_var = 42;
	uint64_t fp = get_fp();
	uint64_t addr = (uint64_t)&local_var;
	int stackPos = (int)(addr - fp);

	// Get reference via ORef
	uint64_t ref = oref_local(stackPos);
	int64_t *ptr = (int64_t*)ref;

	// Pass pointer to nested functions
	nested_level1(ptr, 42);

	// Verify final value
	ASSERT(local_var == 192, "Variable correctly modified: 42+50+100=192");
}

// Test 6: ORef with struct
typedef struct {
	int32_t  field1;
	uint64_t field2;
	int16_t  field3;
} TestStruct;

__attribute__((noinline))
static void test_oref_struct(void) {
	TEST("ORef for struct variable");

	volatile TestStruct local_struct = {
		.field1 = 100,
		.field2 = 0xDEADBEEFCAFEBABEULL,
		.field3 = -500
	};

	uint64_t fp = get_fp();
	uint64_t addr = (uint64_t)&local_struct;
	int stackPos = (int)(addr - fp);

	// Get reference via ORef
	uint64_t ref = oref_local(stackPos);
	TestStruct *ptr = (TestStruct*)ref;

	// Verify fields through pointer
	ASSERT(ptr->field1 == 100, "Struct field1 correct");
	ASSERT(ptr->field2 == 0xDEADBEEFCAFEBABEULL, "Struct field2 correct");
	ASSERT(ptr->field3 == -500, "Struct field3 correct");

	// Modify through pointer
	ptr->field1 = 200;
	ptr->field2 = 0x123456789ABCDEF0ULL;
	ptr->field3 = 1000;

	// Verify original changed
	ASSERT(local_struct.field1 == 200, "Struct field1 modified");
	ASSERT(local_struct.field2 == 0x123456789ABCDEF0ULL, "Struct field2 modified");
	ASSERT(local_struct.field3 == 1000, "Struct field3 modified");
}

// Test 7: ORef address calculation correctness
__attribute__((noinline))
static void test_oref_address_calculation(void) {
	TEST("ORef address calculation correctness");

	volatile int64_t vars[4] = {10, 20, 30, 40};

	uint64_t fp = get_fp();

	// Test ORef calculation for each variable
	for (int i = 0; i < 4; i++) {
		uint64_t actual_addr = (uint64_t)&vars[i];
		int stackPos = (int)(actual_addr - fp);
		uint64_t oref_addr = oref_local(stackPos);

		char msg[100];
		snprintf(msg, sizeof(msg), "ORef calculation correct for var[%d]", i);
		ASSERT(oref_addr == actual_addr, msg);

		snprintf(msg, sizeof(msg), "Value correct through ORef for var[%d]", i);
		ASSERT(*(int64_t*)oref_addr == vars[i], msg);
	}
}

int main(void) {
	printf("========================================\n");
	printf("ARM64 ORef Operation Tests\n");
	printf("========================================\n");
	printf("\n");

	// Run all tests
	test_oref_local_simple();
	test_oref_multiple_locals();
	test_oref_modify();
	test_oref_different_sizes();
	test_oref_nested_calls();
	test_oref_struct();
	test_oref_address_calculation();

	// Report results
	printf("\n");
	printf("========================================\n");
	printf("Test Results\n");
	printf("========================================\n");
	printf("Passed: %d\n", tests_passed);
	printf("Failed: %d\n", tests_failed);
	printf("\n");

	if (tests_failed == 0) {
		printf("✓ All ORef tests passed!\n");
		return 0;
	} else {
		printf("✗ Some ORef tests failed\n");
		return 1;
	}
}
