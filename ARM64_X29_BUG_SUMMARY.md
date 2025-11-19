# ARM64 JIT Critical Bug: Frame Pointer Corruption

## Problem Summary

**Severity:** CRITICAL - Causes segmentation faults in all HashLink programs on ARM64
**Root Cause:** Native C functions corrupt X29 (frame pointer) during execution
**Impact:** Any JIT-compiled HashLink code that calls native functions will crash

## Technical Details

### The Bug

When HashLink JIT-compiled ARM64 code calls native C functions via `BLR` (Branch with Link Register), the frame pointer register **X29 gets corrupted** (set to invalid values like `0x1`). After the native call returns, subsequent stack access using X29 causes **SIGSEGV** (segmentation fault).

### Why This Happens

1. **Our JIT Convention:** In our ARM64 JIT implementation, `X29 = SP` (frame pointer equals stack pointer)
   - Set in prologue: `arm_add_imm(ctx, X29, XZR, 0, true)` → `ADD X29, SP, #0`
   - All stack access uses `[X29, offset]` pattern

2. **Native Function Calls:** Some native C functions violate the ARM64 AAPCS (calling convention)
   - X29 is supposed to be **callee-saved** (preserved across calls)
   - But certain functions corrupt it anyway (possibly due to compiler optimizations or non-standard ABI)

3. **Post-Call Crash:** After BLR returns, X29 has wrong value:
   ```
   X29 = 0x1  (corrupted)
   SP  = 0x7fffffffeee0  (correct)

   STUR X0, [X29, #-48]  ← CRASH! Tries to write to 0x1 - 48 = invalid address
   ```

## Code Architecture

### JIT Function Structure

```c
int hl_jit_function(jit_ctx *ctx, hl_module *m, hl_function *f) {
    // 1. Setup
    ctx->totalRegsSize = <calculate stack size>;

    // 2. Generate prologue
    arm_prologue(ctx, ctx->totalRegsSize + 16);
    // Emits:
    //   STP X29, X30, [SP, #-framesize]!  // Save FP/LR, adjust SP
    //   ADD X29, SP, #0                    // X29 = SP

    // 3. Generate function body with operations
    for (each opcode) {
        switch (opcode) {
            case OCallN:  // Call with N arguments
            case OCall0:  // Call with 0 arguments
            case OCall1:  // Call with 1 argument
            // ... etc
        }
    }

    // 4. Generate epilogue
    arm_epilogue(ctx, ctx->totalRegsSize + 16);
    // Emits:
    //   LDP X29, X30, [SP], #framesize  // Restore FP/LR, adjust SP
    //   RET                              // Return to caller
}
```

### Where Native Calls Happen

Native functions are called in multiple places:

1. **OCall Handlers** (OCall0, OCall1, OCall2, OCall3, OCall4, OCallN)
   - Lines: 4464, 4525, 4574, 4625, 4678, 5328
   - Pattern:
     ```c
     if (isNative) {
         void *fptr = ctx->m->functions_ptrs[o->p2];
         arm_load_imm64(ctx, X9, (uint64_t)fptr);
         arm_blr(ctx, X9);  // ← X29 gets corrupted here!
     }
     ```

2. **OCallMethod, OCallThis, OCallClosure**
   - Lines: 5487, 5540, 5586, 5647, 5659
   - Dynamic method dispatch to native functions

3. **Helper Functions** (Error handlers)
   - Lines: 3319, 3352, 3370
   - Call jit_fail, jit_null_fail for runtime errors
   - These have their own prologues but still need the fix

## The Fix

### Simple Solution

After EVERY `arm_blr(ctx, X9)` that might call a native function, restore X29:

```c
arm_blr(ctx, X9);
arm_add_imm(ctx, X29, XZR, 0, true);  // Restore: X29 = SP + 0
```

This works because:
- In our ABI, X29 always equals SP (no dynamic stack allocation)
- After a call, SP is unchanged (callee preserves it)
- So we can safely restore X29 from SP

### Implementation Status

**Attempted Fix:**
- Added X29 restore after all BLR calls in OCall sections using Python script
- Source code has 20+ fixes applied
- But generated JIT code does NOT include the restore instruction

**Current Issue:**
- The crashing BLR doesn't have the X29 restore in generated code
- This suggests either:
  1. The fix isn't being compiled correctly
  2. The crashing BLR is from a code path we haven't fixed yet
  3. There's a caching or build issue

## Difficulty Assessment

**Is it difficult to fix?**

**No, conceptually simple:**
1. Find all `arm_blr(ctx, X9)` calls
2. Add `arm_add_imm(ctx, X29, XZR, 0, true)` after each one
3. Done!

**Challenges:**
- **Coverage:** Must find ALL BLR call sites (currently ~20+ locations)
- **Testing:** Must verify the fix actually makes it into generated code
- **Verification:** Need to confirm generated ARM64 machine code includes `ADD X29, SP, #0` (opcode 0x910003FD)

## Next Steps

1. **Verify current fixes are compiled:**
   ```bash
   make clean && make
   objdump -d hl | grep -A 5 "blr.*x9"
   ```

2. **Add X29 restore to remaining locations:**
   - Helper functions (lines 3319, 3352, 3370)
   - OCallMethod/This/Closure (lines 5487, 5540, 5586, 5647, 5659)
   - Any other BLR sites discovered

3. **Verify fix in runtime:**
   - Run under GDB: `gdb ./hl test_empty.hl`
   - Disassemble at crash: `x/20i $pc-32`
   - Verify pattern: `blr x9` → `add x29, sp, #0` → `stur ...`

4. **Alternative approach** (if systematic fix fails):
   - Create wrapper: `static void call_native_and_fix_x29(ctx, fptr)`
   - Use wrapper everywhere instead of direct BLR

## Files Modified

- `src/jit.c` - Main JIT compiler (20+ BLR call sites)
- `src/std/types.c` - (Debug output, can be reverted)
- `src/main.c` - (Debug output, can be reverted)

## Testing

**Test case:** `./hl test_empty.hl`
**Expected:** Program runs without segfault
**Actual:** SIGSEGV at first STUR after native call
**X29 value:** 0x1 (should be ~0x7fffffffeee0, same as SP)

## References

- ARM64 AAPCS (calling convention): X29 is callee-saved
- HashLink JIT architecture: Stack-based virtual register system
- Prologue generation: `arm_prologue()` at line 7350
- Epilogue generation: `arm_epilogue()` at line 7401
