# ARM64 JIT Debugging Notes (Updated 2025-11-17)

## Timeline of Discoveries and Fixes

### Phase 1: Initial Implementation
- Implemented 50+ ARM64 JIT operations
- Created stack-based vreg system with LOAD_VREG/STORE_VREG macros
- All operations converted to use stack instead of registers

### Phase 2: GET_REG Bug Discovery (Resolved)
- **Problem:** Some operations still used GET_REG(vr) which treated vreg index as register number
- **Solution:** Converted all 38 operations to use LOAD_VREG/STORE_VREG
- **Status:** ✅ FIXED

### Phase 3: MOVZ/MOVK Encoding Bugs (Resolved)
- **Problem:** arm_movz used 0x52 instead of 0xA5, arm_movk used 0x72 instead of 0xE5
- **Root Cause:** Wrong opc field (bits 30-29) in instruction encoding
- **Impact:** All 64-bit immediate loads were generating garbage instructions (STP, STR, LDR)
- **Fix:** Changed encodings to correct values
- **Status:** ✅ FIXED (Commit 9dfede4)

### Phase 4: vclosure Offset Bugs (Resolved)
- **Problem:** OCallClosure was loading closure fields from wrong offsets
- **Root Cause:** Misunderstanding of ARM64 LDR scaled addressing and vclosure structure layout

**Bug 1: closure->fun offset**
```c
// WRONG:
arm_ldr_imm(ctx, X9, X11, 0, 3);  // Loads t (offset 0), not fun!

// CORRECT:
arm_ldr_imm(ctx, X9, X11, 1, 3);  // Loads fun (offset 8 = 1*8)
```

**Bug 2: closure->value offset**
```c
// WRONG:
arm_ldr_imm(ctx, X0, X11, 1, 3);  // Loads fun (offset 8), not value!

// CORRECT:
arm_ldr_imm(ctx, X0, X11, 3, 3);  // Loads value (offset 24 = 3*8)
```

**Bug 3: closure->hasValue size**
```c
// SUBOPTIMAL:
arm_ldr_imm(ctx, X10, X11, 2, 3);  // 64-bit load, works but wrong

// CORRECT:
arm_ldr_imm(ctx, X10, X11, 4, 2);  // 32-bit load (offset 16 = 4*4)
```

**vclosure Structure Reference:**
```c
typedef struct _vclosure {
    hl_type *t;      // offset 0 (8 bytes)
    void *fun;       // offset 8 (8 bytes) <- scaled offset 1 for 64-bit
    int hasValue;    // offset 16 (4 bytes) <- scaled offset 4 for 32-bit
#ifdef HL_64
    int stackCount;  // offset 20 (4 bytes)
#endif
    void *value;     // offset 24 (8 bytes) <- scaled offset 3 for 64-bit
} vclosure;
```

- **Status:** ✅ FIXED (Commit 87f051b)

### Phase 5: Current Issue - NULL Function Pointer Crash (ACTIVE)

**Problem:**
- All programs crash with SIGSEGV at PC=0x0
- Crash occurs inside JIT-compiled entry point function
- Even absolutely empty Haxe programs crash
- Entry point closure is set up correctly with valid function pointer
- Crash happens when JIT code tries to BLR to address 0x0

**Evidence:**
```
GDB trace shows:
- X9 register = 0x0
- PC = 0x0
- Crash in: #0 0x0000000000000000
- Called from: #1 hl_call_method
```

**What We Know (Updated 2025-11-18):**
1. ✅ Entry point function address is valid (0x7ffff5c4e03c)
2. ✅ Entry point closure is properly initialized
3. ✅ Prologue correctly sets X29=SP (verified instruction: 0x910003FD)
4. ✅ Arguments saved from X0-X7 to stack correctly
5. ✅ Call patching works (verified BL instructions patched with correct offsets)
6. ✅ OCallClosure closures do NOT have NULL fun pointers (NULL checks never trigger)
7. ✅ No HFUN globals loaded (OGetGlobal only loads HOBJ/HABSTRACT types)
8. ✅ All native function pointers valid at compile time
9. ✅ Crash occurs after "OCall0 JIT function 240: will be patched"
10. ❌ Inside JIT code, a BLR X9 executes with X9=0

**What We Don't Know:**
1. ❓ Which exact operation/instruction generates the failing BLR
2. ❓ Why a vreg value loaded from memory is NULL at runtime
3. ❓ Is it inside function 240 or during the call to it?
4. ❓ Is there memory corruption or uninitialized data?
5. ❓ Is the stack memory correctly allocated and accessible?

## Current Debugging Approach

### Step 1: Identify the Failing Operation
Need to determine which JIT operation generates the BLR that jumps to NULL.

**Method A: GDB with Assembly Stepping**
```bash
gdb ./hl
(gdb) run /tmp/TestEmpty.hl
# Let it reach crash
(gdb) x/20i $pc-40  # Disassemble before crash
# Find the BLR instruction
# Trace back through the code to see what loads X9
```

**Method B: Add Runtime Tracing**
Before each `arm_blr(ctx, X9)` in src/jit.c, add:
```c
// Save registers
arm_stp(ctx, X0, X1, SP, -2, true, true);  // Pre-index
arm_stp(ctx, X2, X3, SP, -2, true, true);

// Print X9 value
static char fmt[] = "[BLR] About to call X9=%p\n";
arm_load_imm64(ctx, X0, (uint64_t)fmt);
arm_mov_reg(ctx, X1, X9, true);
arm_load_imm64(ctx, X8, (uint64_t)&printf);
arm_blr(ctx, X8);

// Restore registers
arm_ldp(ctx, X2, X3, SP, 2, true, true);  // Post-index
arm_ldp(ctx, X0, X1, SP, 2, true, true);

// Original BLR
arm_blr(ctx, X9);
```

### Step 2: Check OGetGlobal
If OGetGlobal is loading NULL function pointers:

```c
case OGetGlobal:
    int gindex = o->p2;
    hl_type *gtype = m->code->globals[gindex];
    void *gaddr = (void*)(m->globals_data + m->globals_indexes[gindex]);

    // At JIT compile time, print what's there
    printf("[OGetGlobal COMPILE] global=%d, type.kind=%d, addr=%p\n",
           gindex, gtype->kind, gaddr);

    if (gtype->kind == HFUN) {
        void *current_value = *(void**)gaddr;
        printf("[OGetGlobal COMPILE] HFUN value=%p, null_function=%p\n",
               current_value, null_function);
    }

    // Generate code to load global
    arm_load_imm64(ctx, X10, (uint64_t)gaddr);
    arm_ldr_imm(ctx, X10, X10, 0, 3);
    STORE_VREG(X10, dst);
    break;
```

### Step 3: Verify Function Prologue
The entry point must receive its arguments correctly:

```c
// In hl_jit_function, ARM64 section:
// After arm_prologue
#ifdef HL_JIT_ARM64
arm_prologue(ctx, ctx->totalRegsSize + 16);

// Store incoming arguments to stack
for(i=0;i<nargs;i++) {
    vreg *r = R(i);
    if (i < 8) {
        // Arg in X0-X7
        STORE_VREG(X0 + i, r);
    } else {
        // Arg on stack - need to load and store
        // TODO: Handle stack args
    }
}
#endif
```

## Hypotheses (Ranked by Likelihood)

### 1. OGetGlobal loads NULL (80% likely)
Empty programs still need runtime initialization. If globals aren't initialized before JIT code accesses them, we get NULL.

**Test:** Add compile-time and runtime logging to OGetGlobal

### 2. OCall uses BLR instead of BL (60% likely)
If OCall generates BLR instead of patched BL, and the function pointer isn't initialized, we'd call NULL.

**Test:** Check OCall implementation, verify it uses call patching

### 3. Missing operation implementation (40% likely)
An operation used during initialization might call _jit_error, which sets PC to 0.

**Test:** Search for _jit_error calls, add visible logging

### 4. Function argument passing broken (30% likely)
If arguments aren't properly passed to functions, function pointers received as arguments might be NULL.

**Test:** Verify argument storage in prologue

### 5. Calling convention violation (20% likely)
If we don't preserve registers correctly across calls, X9 could be corrupted.

**Test:** Verify all BLR calls preserve required registers

## Tools and Resources

### GDB Commands
```bash
# Run until crash
gdb ./hl
(gdb) run /tmp/TestEmpty.hl
(gdb) bt  # Backtrace
(gdb) info registers  # See all registers
(gdb) x/20i $pc-40  # Disassemble around crash
(gdb) x/10gx $sp  # Examine stack
```

### Disassemble JIT Code
```bash
# If JIT code is dumped to file:
objdump -D -b binary -maarch64 -Mno-aliases /tmp/jit_code.bin > jit_asm.txt

# Look at specific offset:
objdump -D -b binary -maarch64 --start-address=0x1a03c /tmp/jit_code.bin
```

### ARM64 Reference
- **LDR encoding**: ARM ARM C6.2.119
- **BLR encoding**: ARM ARM C6.2.33
- **Calling convention**: ARM ARM Procedure Call Standard

## Next Session Plan

1. Add BLR tracing to every arm_blr call
2. Rebuild and run
3. Identify which BLR fails
4. Examine that operation
5. Fix the bug
6. Test again
7. Repeat until working

## Files Modified So Far

- **src/jit.c**:
  - MOVZ/MOVK encoding fixes
  - vclosure offset fixes in OCallClosure
  - All GET_REG operations converted

- **src/main.c**:
  - vclosure initialization completed
  - Use-after-free fix

- **src/module.c**:
  - Debug output cleaned up

## Success Metrics

- [ ] Identify failing BLR
- [ ] Fix the operation generating it
- [ ] Program executes without crash
- [ ] TestEmpty.hl completes successfully
- [ ] Simple Haxe programs run

---

**Last Updated:** 2025-11-17
**Current Status:** vclosure bugs fixed, but NULL pointer crash persists
**Next Action:** Add BLR tracing or use GDB to identify failing operation
**Time Invested:** ~6 hours debugging vclosure issues
**Estimated Time to Fix:** 2-8 hours depending on issue complexity
