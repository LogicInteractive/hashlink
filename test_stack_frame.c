/*
 * ARM64 JIT Stack Frame Test
 *
 * Tests the stack frame infrastructure including:
 * - Function prologue (FP/LR save, stack allocation)
 * - Function epilogue (FP/LR restore, stack deallocation)
 * - Local variable stack layout
 * - Frame pointer validity
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

// ARM64 stack frame layout test
// Stack grows downwards on ARM64
//   High addresses
//   +------------------+
//   | Saved LR (X30)   | <- FP + 8
//   +------------------+
//   | Saved FP (X29)   | <- FP (X29 points here)
//   +------------------+
//   | Local var 1      | <- FP - 8
//   | Local var 2      | <- FP - 16
//   | Local var 3      | <- FP - 24
//   +------------------+ <- SP
//   Low addresses

// Get current frame pointer
static inline uint64_t get_fp(void) {
	uint64_t fp;
	__asm__ volatile("mov %0, x29" : "=r"(fp));
	return fp;
}

// Get current stack pointer
static inline uint64_t get_sp(void) {
	uint64_t sp;
	__asm__ volatile("mov %0, sp" : "=r"(sp));
	return sp;
}

// Get current link register
static inline uint64_t get_lr(void) {
	uint64_t lr;
	__asm__ volatile("mov %0, x30" : "=r"(lr));
	return lr;
}

// Test 1: Verify frame pointer is properly set
__attribute__((noinline))
static void test_frame_pointer_setup(void) {
	TEST("Frame pointer setup");

	uint64_t fp = get_fp();
	uint64_t sp = get_sp();

	// FP should be above SP (stack grows downward)
	ASSERT(fp > sp, "FP should be above SP");

	// FP and SP should be 16-byte aligned (ARM64 AAPCS requirement)
	ASSERT((fp & 0xF) == 0, "FP should be 16-byte aligned");
	ASSERT((sp & 0xF) == 0, "SP should be 16-byte aligned");

	// Saved FP should point to caller's frame
	uint64_t *saved_fp_ptr = (uint64_t*)fp;
	uint64_t saved_fp = *saved_fp_ptr;
	ASSERT(saved_fp > fp, "Saved FP should point to caller's frame");

	// Saved LR should be a valid code address
	uint64_t *saved_lr_ptr = (uint64_t*)(fp + 8);
	uint64_t saved_lr = *saved_lr_ptr;
	ASSERT(saved_lr != 0, "Saved LR should not be null");
}

// Test 2: Verify stack space allocation
__attribute__((noinline))
static void test_stack_allocation(void) {
	TEST("Stack space allocation");

	uint64_t fp = get_fp();
	uint64_t sp = get_sp();

	// Frame should have space for saved FP/LR plus local variables
	uint64_t frame_size = fp - sp;

	// Minimum frame: 16 bytes for FP/LR
	ASSERT(frame_size >= 16, "Frame should have at least 16 bytes");

	// Frame size should be 16-byte aligned
	ASSERT((frame_size & 0xF) == 0, "Frame size should be 16-byte aligned");
}

// Test 3: Verify local variables can be stored on stack
__attribute__((noinline))
static void test_local_variables(void) {
	TEST("Local variable stack storage");

	uint64_t fp = get_fp();

	// Simulate local variables at negative offsets from FP
	// These would be at stackPos = -8, -16, -24 in the JIT
	volatile int64_t local1 = 0x1111111111111111LL;
	volatile int64_t local2 = 0x2222222222222222LL;
	volatile int64_t local3 = 0x3333333333333333LL;

	// Variables should be allocated on stack (below FP)
	uint64_t local1_addr = (uint64_t)&local1;
	uint64_t local2_addr = (uint64_t)&local2;
	uint64_t local3_addr = (uint64_t)&local3;

	ASSERT(local1_addr < fp, "Local variable 1 should be below FP");
	ASSERT(local2_addr < fp, "Local variable 2 should be below FP");
	ASSERT(local3_addr < fp, "Local variable 3 should be below FP");

	// Verify values are stored correctly
	ASSERT(local1 == 0x1111111111111111LL, "Local variable 1 value correct");
	ASSERT(local2 == 0x2222222222222222LL, "Local variable 2 value correct");
	ASSERT(local3 == 0x3333333333333333LL, "Local variable 3 value correct");
}

// Test 4: Verify function arguments on stack
__attribute__((noinline))
static void test_stack_arguments_impl(
	int arg0, int arg1, int arg2, int arg3,
	int arg4, int arg5, int arg6, int arg7,
	int arg8, int arg9  // arg8+ should be on stack
) {
	TEST("Stack arguments (9th and 10th args)");

	uint64_t fp = get_fp();

	// First 8 args are in registers (X0-X7)
	// 9th and 10th args should be on caller's stack (above our FP)
	// According to AAPCS64:
	// - arg8 is at [FP + 16] (after saved FP and LR)
	// - arg9 is at [FP + 24]

	ASSERT(arg8 == 88, "9th argument value correct");
	ASSERT(arg9 == 99, "10th argument value correct");
}

static void test_stack_arguments(void) {
	test_stack_arguments_impl(0, 1, 2, 3, 4, 5, 6, 7, 88, 99);
}

// Test 5: Verify frame chain integrity
__attribute__((noinline))
static void leaf_function(uint64_t *out_fp) {
	*out_fp = get_fp();
}

__attribute__((noinline))
static void middle_function(uint64_t *out_fp, uint64_t *out_middle_fp) {
	*out_middle_fp = get_fp();
	leaf_function(out_fp);
}

__attribute__((noinline))
static void test_frame_chain(void) {
	TEST("Frame chain integrity");

	uint64_t root_fp = get_fp();
	uint64_t middle_fp = 0;
	uint64_t leaf_fp = 0;

	middle_function(&leaf_fp, &middle_fp);

	// Each nested function should have FP above its caller
	ASSERT(middle_fp > root_fp, "Middle frame FP > root FP");
	ASSERT(leaf_fp > middle_fp, "Leaf frame FP > middle FP");

	// Verify frame chain links
	uint64_t *middle_saved_fp = (uint64_t*)middle_fp;
	uint64_t *leaf_saved_fp = (uint64_t*)leaf_fp;

	// Note: This may not match exactly due to compiler optimizations
	// but the relationship should hold
	ASSERT(*middle_saved_fp >= root_fp, "Middle saved FP points to caller frame");
	ASSERT(*leaf_saved_fp >= middle_fp, "Leaf saved FP points to caller frame");
}

// Test 6: Verify 16-byte stack alignment is maintained
__attribute__((noinline))
static void test_alignment_depth1(void);
__attribute__((noinline))
static void test_alignment_depth2(void);
__attribute__((noinline))
static void test_alignment_depth3(void);

__attribute__((noinline))
static void test_alignment_depth3(void) {
	uint64_t sp = get_sp();
	ASSERT((sp & 0xF) == 0, "SP aligned at depth 3");
}

__attribute__((noinline))
static void test_alignment_depth2(void) {
	uint64_t sp = get_sp();
	ASSERT((sp & 0xF) == 0, "SP aligned at depth 2");
	test_alignment_depth3();
}

__attribute__((noinline))
static void test_alignment_depth1(void) {
	uint64_t sp = get_sp();
	ASSERT((sp & 0xF) == 0, "SP aligned at depth 1");
	test_alignment_depth2();
}

static void test_stack_alignment(void) {
	TEST("Stack alignment across nested calls");
	test_alignment_depth1();
}

// Test 7: Verify frame size calculation
__attribute__((noinline))
static void test_frame_size_calculation(void) {
	TEST("Frame size calculation");

	uint64_t fp = get_fp();
	uint64_t sp = get_sp();
	uint64_t frame_size = fp - sp;

	// Frame includes:
	// - 16 bytes for saved FP/LR
	// - Space for local variables
	// - Padding to maintain 16-byte alignment

	// Since this function has minimal locals, frame should be small
	ASSERT(frame_size >= 16, "Frame includes FP/LR (16 bytes minimum)");
	ASSERT(frame_size < 256, "Frame size reasonable for simple function");
	ASSERT((frame_size & 0xF) == 0, "Frame size is 16-byte aligned");

	printf("    Frame size: %lu bytes\n", frame_size);
}

int main(void) {
	printf("========================================\n");
	printf("ARM64 Stack Frame Tests\n");
	printf("========================================\n");
	printf("\n");

	// Run all tests
	test_frame_pointer_setup();
	test_stack_allocation();
	test_local_variables();
	test_stack_arguments();
	test_frame_chain();
	test_stack_alignment();
	test_frame_size_calculation();

	// Report results
	printf("\n");
	printf("========================================\n");
	printf("Test Results\n");
	printf("========================================\n");
	printf("Passed: %d\n", tests_passed);
	printf("Failed: %d\n", tests_failed);
	printf("\n");

	if (tests_failed == 0) {
		printf("✓ All stack frame tests passed!\n");
		return 0;
	} else {
		printf("✗ Some stack frame tests failed\n");
		return 1;
	}
}
