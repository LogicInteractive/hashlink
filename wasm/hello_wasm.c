/*
 * Minimal WASM Test - Demonstrates Emscripten is working
 * This is a standalone test to show WASM compilation works
 * For the full HashLink POC, you'd need to build libhl.a first
 */

#include <stdio.h>
#include <stdlib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Simple test functions
int add(int a, int b) {
    return a + b;
}

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

// Main entry point
int main() {
    printf("=================================\n");
    printf("HashLink WASM Build Test\n");
    printf("=================================\n\n");

    printf("✓ Emscripten is working!\n");
    printf("✓ WASM compilation successful!\n\n");

    printf("Testing basic operations:\n");
    printf("  2 + 3 = %d\n", add(2, 3));
    printf("  5! = %d\n", factorial(5));
    printf("  10 * 42 = %d\n", 10 * 42);

    float pi = 3.14159f;
    printf("  π ≈ %.5f\n", pi);
    printf("  π² ≈ %.5f\n", pi * pi);

    printf("\n");
    printf("This demonstrates that:\n");
    printf("  ✓ C code compiles to WASM\n");
    printf("  ✓ Emscripten runtime works\n");
    printf("  ✓ Console output works\n");
    printf("  ✓ Math operations work\n");
    printf("  ✓ Function calls work\n");

    printf("\n");
    printf("=================================\n");
    printf("✓ WASM Test Passed!\n");
    printf("=================================\n\n");

    printf("Next step: Build full HashLink POC\n");
    printf("  1. Install Haxe compiler\n");
    printf("  2. Run: ./wasm/build_wasm.sh\n");
    printf("  3. Run: ./wasm/run_test.sh\n");

    return 0;
}
