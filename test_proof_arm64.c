/*
 * Proof of ARM64 Execution Test
 *
 * This test uses ARM64-specific instructions that don't exist on x86.
 * If this runs successfully, we're DEFINITELY on ARM64, not x86.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main() {
	printf("\n");
	printf("==============================================\n");
	printf("PROOF OF ARM64 EXECUTION\n");
	printf("==============================================\n\n");

	// Test 1: Compile-time architecture check
	printf("Test 1: Compile-time Architecture\n");
	printf("----------------------------------------------\n");
#if defined(__aarch64__) || defined(_M_ARM64)
	printf("✓ __aarch64__ is defined\n");
	printf("✓ This binary was compiled for ARM64\n\n");
#elif defined(__x86_64__)
	printf("✗ __x86_64__ is defined\n");
	printf("✗ This binary was compiled for x86-64\n");
	printf("ERROR: Wrong architecture!\n\n");
	return 1;
#else
	printf("? Unknown architecture\n\n");
	return 1;
#endif

	// Test 2: ARM64-specific inline assembly
	printf("Test 2: ARM64-specific Instructions\n");
	printf("----------------------------------------------\n");
	printf("Executing ARM64 inline assembly...\n");

	// Use MADD instruction - ARM64-specific multiply-add
	// x86 doesn't have this instruction
	uint64_t a = 5;
	uint64_t b = 3;
	uint64_t c = 100;
	uint64_t result;

	__asm__ volatile(
		"madd %0, %1, %2, %3\n"  // result = c + (a * b)
		: "=r"(result)
		: "r"(a), "r"(b), "r"(c)
	);

	printf("  MADD: %lu + (%lu * %lu) = %lu\n", c, a, b, result);
	printf("  Expected: %lu\n", c + (a * b));

	if (result == c + (a * b)) {
		printf("✓ ARM64 MADD instruction executed correctly\n\n");
	} else {
		printf("✗ MADD instruction gave wrong result\n\n");
		return 1;
	}

	// Test 3: Check CPU features via system registers
	printf("Test 3: ARM64 System Registers\n");
	printf("----------------------------------------------\n");

	// Read MIDR_EL1 (Main ID Register) - ARM64-specific
	uint64_t midr;
	__asm__ volatile("mrs %0, midr_el1" : "=r"(midr));
	printf("  MIDR_EL1: 0x%016lx\n", midr);

	// Decode some fields
	unsigned int implementer = (midr >> 24) & 0xFF;
	unsigned int variant = (midr >> 20) & 0xF;
	unsigned int architecture = (midr >> 16) & 0xF;
	unsigned int partnum = (midr >> 4) & 0xFFF;
	unsigned int revision = midr & 0xF;

	printf("  Implementer: 0x%02x ", implementer);
	switch (implementer) {
		case 0x41: printf("(ARM Limited)\n"); break;
		case 0x51: printf("(Qualcomm)\n"); break;
		case 0x00: printf("(QEMU)\n"); break;
		default: printf("(Unknown)\n"); break;
	}
	printf("  Part Number: 0x%03x\n", partnum);
	printf("  Variant: 0x%x, Revision: 0x%x\n", variant, revision);

	printf("✓ Successfully read ARM64 system registers\n\n");

	// Test 4: Pointer authentication (if available)
	printf("Test 4: ARM64 Pointer Size\n");
	printf("----------------------------------------------\n");
	printf("  sizeof(void*) = %zu bytes\n", sizeof(void*));
	printf("  sizeof(long) = %zu bytes\n", sizeof(long));

	if (sizeof(void*) == 8 && sizeof(long) == 8) {
		printf("✓ 64-bit pointers confirmed\n\n");
	} else {
		printf("✗ Unexpected pointer size\n\n");
		return 1;
	}

	// Final verdict
	printf("==============================================\n");
	printf("✓✓✓ PROOF COMPLETE ✓✓✓\n");
	printf("==============================================\n");
	printf("\n");
	printf("This program:\n");
	printf("  1. Was compiled for ARM64/AArch64\n");
	printf("  2. Executed ARM64-specific instructions (MADD)\n");
	printf("  3. Read ARM64 system registers (MIDR_EL1)\n");
	printf("  4. All tests passed\n");
	printf("\n");
	printf("CONCLUSION: We are DEFINITELY running on ARM64!\n");
	printf("This would be impossible on x86/x86-64.\n");
	printf("\n");

	return 0;
}
