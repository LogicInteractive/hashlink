# ARM64 JIT vclosure Investigation and Debugging Report

## Problem Summary

The ARM64 JIT implementation crashes with a segmentation fault (SIGSEGV) when attempting to run even the simplest Haxe programs. The crash occurs at PC=0x0 (NULL pointer dereference) when the JIT-compiled entry point function is executed.

## Test Programs

We tested with increasingly simple programs:

### TestEmpty.hx
```haxe
class TestEmpty {
    static function main() {
        // Do nothing, just return
    }
}
```

### TestAbsolutelyEmpty.hx
```haxe
class TestAbsolutelyEmpty {
    static function main() {
        // Absolutely nothing - don't even call trace
    }
}
```

**Result**: Both crash with the same SIGSEGV at PC=0x0

## Debugging Process and Findings

### 1. Initial Investigation

**Symptom**: Program crashes immediately when calling the JIT-compiled entry point function

**Debug output revealed**:
```
[MAIN] Entry point setup: entrypoint=368, functions_indexes[entrypoint]=320
[MAIN] cl.t=0x55555559cb88, cl.fun=0x7ffff5c4e03c, cl.hasValue=0
[MAIN] m->jit_code=0x7ffff5c34000
[MAIN] cl.fun offset from JIT base = 0x1a03c
[hl_call_method] c=0x555555580330, cl->t=0x55555559cb88, cl->fun=0x7ffff5c4e03c, cl->hasValue=0, cl->value=(nil)
[hl_call_method] About to call static_call: cl->fun=0x7ffff5c4e03c, nargs=0
SIGNAL 11
```

**Key observations**:
- Entry point closure is set up correctly with valid function pointer
- `cl.fun` = 0x7ffff5c4e03c (valid JIT code address)
- `cl.hasValue` = 0 (correct for entry point)
- Crash happens INSIDE the JIT-compiled function, not during setup

### 2. GDB Analysis

```
Thread 1 "hl" received signal SIGSEGV, Segmentation fault.
0x0000000000000000 in ?? ()
#0  0x0000000000000000 in ?? ()
#1  0x00007ffff7f42f04 in hl_call_method () from ./libhl.so
#2  0x00007ffff7f4341c in hl_dyn_call () from ./libhl.so
#3  0x00007ffff7f43cbc in hl_dyn_call_safe () from ./libhl.so
#4  0x0000555555553ba0 in main ()

x9             0x0                 0
```

**Key finding**: Register X9 contains 0x0, indicating we loaded a NULL pointer and tried to BLR to it

### 3. Root Cause Analysis: vclosure Structure Offsets

The `vclosure` structure on HL_64 is:

```c
typedef struct _vclosure {
    hl_type *t;      // offset 0 (8 bytes)
    void *fun;       // offset 8 (8 bytes)
    int hasValue;    // offset 16 (4 bytes)
#ifdef HL_64
    int stackCount;  // offset 20 (4 bytes)
#endif
    void *value;     // offset 24 (8 bytes)
} vclosure;
```

**ARM64 LDR Instruction Scaling**:
- LDR with size=2 (32-bit): offset is scaled by 4 (offset * 4 = byte offset)
- LDR with size=3 (64-bit): offset is scaled by 8 (offset * 8 = byte offset)

**BUGS FOUND IN OCallClosure** (src/jit.c):

#### Bug 1: Loading closure->fun from wrong offset
```c
// WRONG (line 5226, 5248 - before fix):
arm_ldr_imm(ctx, X9, X11, 0, 3);  // Loads from offset 0*8 = 0 (closure->t, not closure->fun!)

// CORRECT (after fix):
arm_ldr_imm(ctx, X9, X11, 1, 3);  // Loads from offset 1*8 = 8 (closure->fun)
```

This caused the code to load the type pointer instead of the function pointer, resulting in jumping to an invalid address.

#### Bug 2: Loading closure->value from wrong offset
```c
// WRONG (line 5209 - before fix):
arm_ldr_imm(ctx, X0, X11, 1, 3);  // Loads from offset 1*8 = 8 (closure->fun, not closure->value!)

// CORRECT (after fix):
arm_ldr_imm(ctx, X0, X11, 3, 3);  // Loads from offset 3*8 = 24 (closure->value)
```

#### Bug 3: Loading closure->hasValue with wrong size and offset
```c
// WRONG (line 5187 - before fix):
arm_ldr_imm(ctx, X10, X11, 2, 3);  // 64-bit load from offset 2*8 = 16

// CORRECT (after fix):
arm_ldr_imm(ctx, X10, X11, 4, 2);  // 32-bit load from offset 4*4 = 16
```

While the byte offset was correct (16), using the wrong load size could cause issues with sign extension and register usage.

### 4. Additional Issues Fixed

#### main.c: Incomplete vclosure Initialization
The entry point vclosure was not fully initialized:

```c
// Before:
vclosure cl;
cl.t = ctx.code->functions[...].type;
cl.fun = ctx.m->functions_ptrs[...];
cl.hasValue = 0;
// Missing: stackCount and value fields!

// After:
vclosure cl;
cl.t = ctx.code->functions[...].type;
cl.fun = ctx.m->functions_ptrs[...];
cl.hasValue = 0;
#ifdef HL_64
cl.stackCount = 0;
#endif
cl.value = NULL;
```

## What We've Tried

1. ✅ **Fixed MOVZ/MOVK encoding** (previous commit) - Instructions now generate correctly
2. ✅ **Fixed vclosure field offsets** - All three offset bugs corrected
3. ✅ **Verified JIT code generation** - MOVZ/MOVK generate valid ARM64 instructions
4. ✅ **Verified entry point setup** - Closure is initialized correctly
5. ✅ **Added instruction cache flush** - Cache coherency handled properly
6. ✅ **Verified call patching** - BL instructions are patched with correct addresses
7. ✅ **Tested with minimal programs** - Even empty main() still crashes

## Current Status

**STILL CRASHING**: Despite fixing the vclosure offset bugs, programs still crash with SIGSEGV at PC=0x0

This indicates there are **additional issues** beyond the vclosure offset problems.

## Next Steps for Debugging

### 1. Identify Which BLR is Failing

The crash happens inside the entry point function at offset 0x1a03c. We need to:

1. Extract and disassemble the JIT code:
   ```bash
   # The JIT code is dumped to /tmp/jit_code.bin during compilation
   objdump -D -b binary -maarch64 -Mno-aliases /tmp/jit_code.bin > jit_disasm.txt
   # Look at offset 0x1a03c
   ```

2. Find the specific BLR instruction that's jumping to NULL

3. Trace back what operation generated that BLR

### 2. Check Global Function Pointer Initialization

The crash might be related to how HFUN globals are initialized:

```c
// In module.c:
for(i=0;i<m->code->nglobals;i++) {
    hl_type *t = m->code->globals[i];
    if( t->kind == HFUN )
        *(void**)(m->globals_data + m->globals_indexes[i]) = null_function;
    // ...
}
```

**Hypothesis**: An OGetGlobal might be loading a global function that's still set to `null_function`, but `null_function` itself might not be properly accessible from JIT code.

**Test**: Add debug output to see if any OGetGlobal operations are loading function pointers

### 3. Verify Function Call Operations

Check all operations that can result in indirect calls:

- **OCall**: Direct function calls (uses BL, should be patched)
- **OCallClosure**: Closure calls (uses BLR with loaded function pointer) - **FIXED**
- **OCallMethod**: Virtual method calls (uses BLR)
- **OCallThis**: Method calls on 'this' (uses BLR)
- **OGetGlobal + OCall**: Loading and calling global functions

**Action**: Add runtime checks before each BLR to log the function pointer being called:

```c
// Before BLR X9, add:
// MOV X0, X9
// Load address of printf format
// BL printf
// Reload X9
// BLR X9
```

### 4. Check for Missing Operations

The entry point function must call some initialization code. Possible missing implementations:

- Exception handler setup
- Stack frame initialization
- Module initialization calls
- Runtime type checks

**Action**: Review the bytecode for the entry point function:

```bash
# Decompile the .hl file
hl --dump /tmp/TestAbsolutelyEmpty.hl > bytecode.txt
# Look at function 368 (the entry point)
```

### 5. Compare with Working x86-64 JIT

The x86-64 JIT works correctly. Compare implementations:

1. How does x86 OCallClosure load closure fields?
2. How does x86 handle function pointer calls?
3. Are there any ARM64-specific issues with calling convention?

### 6. Minimal JIT Test

Create a minimal test that doesn't use the full HashLink runtime:

```c
// Test calling a simple JIT-compiled function directly
void test_simple_jit() {
    // Generate: MOV X0, #42; RET
    // Call it and verify it returns 42
}
```

This can verify that basic JIT code generation and calling works.

## Likely Culprits

Based on the evidence, the most likely remaining issues are:

1. **OGetGlobal loading NULL function pointers**
   - Even empty programs need some globals
   - Probability: **HIGH**

2. **Missing operation implementations**
   - Some operations might not be implemented for ARM64
   - The _jit_error prints message but might not be seen
   - Probability: **MEDIUM**

3. **Calling convention mismatch**
   - ARM64 has different calling conventions than x86
   - Function prologue/epilogue might be wrong
   - Probability: **LOW** (prologue looks correct)

4. **Stack alignment issues**
   - ARM64 requires 16-byte stack alignment
   - Incorrect alignment can cause crashes
   - Probability: **MEDIUM**

## Files Modified

- **src/jit.c**: Fixed vclosure offset bugs in OCallClosure (lines 5199, 5211, 5228, 5251)
- **src/main.c**: Added complete vclosure initialization (lines 298-301)
- **src/module.c**: Removed debug output

## References

- vclosure structure definition: src/hl.h (around line 650)
- OCallClosure implementation: src/jit.c (lines 5147-5258)
- ARM64 LDR instruction: ARM Architecture Reference Manual (C6.2.119)
- hl_call_method: src/std/fun.c (line 118)

## Conclusion

We've made significant progress in identifying and fixing the vclosure offset bugs, which were causing the JIT code to load incorrect pointers from closure structures. However, the program still crashes, indicating there are additional issues in the ARM64 JIT implementation that need to be addressed.

The next steps should focus on:
1. Identifying exactly which operation is generating the failing BLR
2. Checking if function pointers from globals are properly initialized
3. Verifying all operations that can result in indirect calls are implemented correctly

---
**Date**: 2025-11-17
**Status**: vclosure offsets fixed, but runtime crash persists
**Branch**: claude/arm-port-vi-01W7cnxC7ajBnBH9UTafTUX5-01P5UrNX1XKXvnZ15A28PbET
**Commit**: 87f051b
