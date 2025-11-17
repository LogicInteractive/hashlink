# ARM64 JIT - Next Steps (Updated 2025-11-17)

## Current Status

### ✅ Completed
- All GET_REG bugs fixed (38 operations converted to LOAD_VREG/STORE_VREG)
- MOVZ/MOVK instruction encoding bugs fixed
- vclosure structure offset bugs fixed:
  - closure->fun: offset 0 → 1 (8 bytes)
  - closure->value: offset 1 → 3 (24 bytes)
  - closure->hasValue: 64-bit load → 32-bit load with correct offset
- vclosure initialization in main.c completed
- Instruction cache flush implemented
- Build system works correctly

### ❌ Current Issue

**Programs crash with SIGSEGV at PC=0x0**
- Entry point function is called with valid pointer
- Crash occurs INSIDE JIT-compiled code
- Even absolutely empty Haxe programs crash
- Indicates issue in runtime initialization, not test code

## Detailed Investigation

See **ARM64_VCLOSURE_INVESTIGATION.md** for comprehensive analysis including:
- Complete problem summary and symptoms
- Root cause analysis of vclosure bugs
- GDB debugging traces
- vclosure structure layout reference
- All fixes applied so far
- Detailed next steps with code examples

## Immediate Next Steps (Priority Order)

### 1. Identify the Failing BLR Instruction (Highest Priority)

**Goal:** Find exactly which operation generates the BLR that jumps to NULL

**Approach A: Disassemble JIT Code**
```bash
# JIT code may be dumped during compilation (if debug enabled)
objdump -D -b binary -maarch64 /tmp/jit_code.bin > jit_disasm.txt
# Look at entry point offset (e.g., 0x1a03c)
# Find the BLR instruction that would jump to 0x0
```

**Approach B: Add Runtime Tracing**
```c
// In src/jit.c, before each arm_blr call, add:
// Save X9
// Load printf format and X9 value
// Call printf
// Restore X9
// Do the actual BLR
```

**Approach C: GDB Stepping**
```bash
gdb ./hl
(gdb) run /tmp/TestEmpty.hl
(gdb) break hl_call_method
(gdb) stepi  # Step through assembly until crash
(gdb) disassemble $pc-32,$pc+32
```

### 2. Check Global Function Pointer Initialization (High Priority)

**Hypothesis:** OGetGlobal operations may be loading NULL function pointers

**Test:**
```c
// Add debug output in OGetGlobal operation (src/jit.c)
case OGetGlobal:
    int gindex = o->p2;
    void *gptr = (void*)(m->globals_data + m->globals_indexes[gindex]);
    void *gvalue = *(void**)gptr;

    // DEBUG:
    printf("[OGetGlobal] Loading global %d: gptr=%p, value=%p, kind=%d\n",
           gindex, gptr, gvalue, m->code->globals[gindex]->kind);

    // If kind==HFUN and value==null_function, verify null_function is valid
    if (m->code->globals[gindex]->kind == HFUN) {
        printf("[OGetGlobal] HFUN global: null_function=%p\n", null_function);
    }

    arm_load_imm64(ctx, X10, (uint64_t)gptr);
    arm_ldr_imm(ctx, X10, X10, 0, 3);
    STORE_VREG(X10, dst);
    break;
```

### 3. Verify Function Call Operations (Medium Priority)

Check all operations that can generate BLR:

**OCall** (Direct function calls)
- Should use BL (patched), not BLR
- Verify patching is working correctly

**OCallMethod** (Virtual method calls)
- Loads method pointer from vtable
- Calls via BLR
- Verify vtable access is correct

**OCallThis** (Method calls on 'this')
- Similar to OCallMethod
- Verify 'this' pointer handling

**OCallClosure** (Already fixed)
- We fixed the offset bugs
- But verify the fix is actually being used

### 4. Test with Minimal Program (Medium Priority)

Create a minimal test that bypasses HashLink runtime:

```c
// test_minimal_jit.c
#include "hl.h"

int main() {
    // Manually create minimal JIT code
    // Just: MOV X0, #42; RET
    unsigned char code[8];
    // ... encode instructions ...

    // Call it
    int (*func)() = (int(*)())code;
    int result = func();
    printf("Result: %d (expected 42)\n", result);
    return 0;
}
```

This verifies basic JIT execution works outside the full runtime.

### 5. Compare with x86-64 Implementation (Low Priority)

The x86-64 JIT works. Compare implementations:

**Key Questions:**
- How does x86 OCallClosure load closure fields?
- How does x86 handle function pointer calls?
- Are there any special cases for initialization?
- Does x86 have any workarounds we're missing?

**Files to compare:**
- `src/jit.c` - x86 vs ARM64 sections
- Look for `#ifdef HL_JIT_X86` vs `#ifdef HL_JIT_ARM64`

## Likely Root Causes (Ranked by Probability)

### 1. OGetGlobal Loading NULL (Probability: HIGH)
Even empty programs need some globals for runtime initialization. If OGetGlobal loads a function pointer that's NULL or points to `null_function`, and `null_function` isn't properly accessible from JIT code, we'd get a NULL pointer crash.

**Test:** Add extensive logging to OGetGlobal

### 2. Missing Operation Implementation (Probability: MEDIUM)
The _jit_error function should print a message, but we might not see it. Some operations might call _jit_error (unimplemented) but execution continues, loading NULL.

**Test:** Review all operations that might be hit during initialization, verify they're implemented

### 3. Function Prologue/Epilogue Issue (Probability: MEDIUM)
Arguments might not be properly passed/received, or return values might be mishandled.

**Test:** Create minimal function that just returns a constant, verify it works

### 4. Stack Alignment Issue (Probability: LOW)
ARM64 requires 16-byte stack alignment. If our prologue doesn't maintain this, function calls could fail.

**Test:** Verify arm_prologue maintains proper alignment

### 5. Calling Convention Mismatch (Probability: LOW)
If we're not following ARM64 calling conventions correctly, native function calls would fail.

**Test:** Verify argument passing matches ARM64 ABI

## Testing Strategy

1. **Add debug output to identify failing operation**
   - Don't guess, identify the exact BLR that fails

2. **Fix that specific operation**
   - Once identified, fix should be straightforward

3. **Test again**
   - If different operation fails, repeat

4. **Iterate until program runs**
   - There may be multiple issues

## Time Estimates

- **Best Case:** 2-4 hours (single issue, easy to identify)
- **Likely Case:** 8-12 hours (multiple issues, requires systematic debugging)
- **Worst Case:** 20+ hours (fundamental architecture issue)

## Success Criteria

- [ ] `./hl /tmp/TestEmpty.hl` runs without crash
- [ ] Simple "Hello World" program works
- [ ] TestInt.hx executes correctly
- [ ] Ready to move to Phase 2 (optimizations)

## Resources

- **ARM64_VCLOSURE_INVESTIGATION.md** - Comprehensive investigation report
- **ARM64_JIT_STATUS.md** - Current status summary
- **DEBUGGING_NOTES.md** - Historical debugging notes
- **src/jit.c** - All ARM64 JIT implementation
- **src/std/fun.c** - hl_call_method, hl_dyn_call implementations
- **ARM Architecture Reference Manual** - For instruction encoding

---

**Last Updated:** 2025-11-17
**Current Blocker:** SIGSEGV at PC=0x0 in JIT-compiled entry point
**Next Action:** Identify which BLR instruction is jumping to NULL
