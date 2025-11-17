# ARM64 JIT Virtual Register System Analysis

## Problem Summary

The ARM64 JIT's virtual register (vreg) system is fundamentally incompatible with how operations are implemented. This causes all Haxe programs to crash.

## Root Cause

### x86 Approach (Working)
```c
// Line 3605: Initialize all vregs
r->stack.id = i;  // vreg index, NOT a physical register!
r->stackPos = -offset;  // Stack location

// Operations use sophisticated allocator:
preg *p = alloc_cpu(ctx, vreg, true);  // Allocates physical register OR loads from stack
op_mov(ctx, dst_preg, src_preg);  // Works with physical registers
```

x86 has:
- **Register allocator** (`alloc_cpu`, `alloc_fpu`, etc.)
- **Load/store helpers** (`load`, `store`, `copy`)
- **Physical register tracking** (`vreg->current` points to allocated preg)
- Operations work with `preg*`, not vreg directly

### ARM64 Current Approach (Broken)
```c
// Line 3605: Same initialization
r->stack.id = i;  // vreg index

// But operations use simplistic macro:
#define GET_REG(vr) ((vr)->stack.id >= 0 ? (vr)->stack.id : X0)

// Then use it as if it's a physical register:
Arm64Reg rd = GET_REG(dst);  // Returns vreg INDEX not physical register!
arm_mov_reg(ctx, rd, rn, true);  // Tries to use index as register number!
```

**The bug:** `stack.id` contains the vreg INDEX (0, 1, 2, ...) but ARM64 code treats it as a physical REGISTER NUMBER (X0, X1, X2, ...).

This "works" accidentally for vregs 0-18 because:
- vreg 0 → stack.id=0 → X0 (happens to be correct for first arg)
- vreg 1 → stack.id=1 → X1 (happens to be correct for second arg)
- etc.

But FAILS for:
- vreg 29 → X29 (Frame Pointer!)
- vreg 30 → X30 (Link Register!)
- vreg 31+ → Invalid registers
- Conflicts when vregs don't match function arguments

## Why Simple Fixes Don't Work

### Attempt 1: Map to X0-X18
```c
r->stack.id = i;  // vregs 0-18 → X0-X18
```
**Problem:** X0-X7 are argument/return registers (volatile), X8 is special, X9-X15 are temps. Values get clobbered across function calls!

### Attempt 2: Map to X19-X28 (callee-saved)
```c
r->stack.id = (i < 10) ? (X19 + i) : -1;
```
**Problems:**
- Only 10 registers available, programs often have 50+ vregs
- vregs 10+ get stack.id=-1 → GET_REG returns X0 → conflicts
- Arguments come in X0-X7, need to be moved to X19-X28
- Prologue/epilogue must save/restore X19-X28

### Attempt 3: All stack-based
```c
r->stack.id = -1;  // Force stack
#define LOAD_VREG(tmp, vr) arm_ldur(tmp, X29, vr->stackPos)
#define STORE_VREG(tmp, vr) arm_stur(tmp, X29, vr->stackPos)
```
**Problem:** Requires updating ALL ~100 operations to use LOAD/STORE instead of GET_REG. Doable but tedious (2-4 hours of work).

## The Right Solution

Implement proper register allocation like x86 has:

```c
// 1. Track which vregs are in registers
typedef struct {
    vreg *holds;  // Which vreg (if any) is in this register
    int locked;   // Reference count
} preg;

preg physical_regs[32];  // X0-X31

// 2. Allocate registers on demand
preg *alloc_reg_arm64(jit_ctx *ctx, vreg *r) {
    if (r->current) return r->current;  // Already allocated

    // Find free register or spill one
    preg *p = find_free_or_spill(ctx);

    // Load from stack if needed
    if (vreg_has_value_on_stack(r)) {
        arm_ldur(p->id, X29, r->stackPos);
    }

    r->current = p;
    p->holds = r;
    return p;
}

// 3. Operations use allocated registers
case OMov:
    preg *dst_reg = alloc_reg_arm64(ctx, dst);
    preg *src_reg = alloc_reg_arm64(ctx, src);
    arm_mov_reg(ctx, dst_reg->id, src_reg->id);
    break;
```

**Effort:** 1-2 days to implement properly
**Benefit:** ~90% of x86 performance

## Quick POC Path (What I Recommend)

For a working proof-of-concept in 2-3 hours:

1. **Implement stack-based system** (simple, guaranteed to work)
   - All vregs stored at [X29 + stackPos]
   - LOAD_VREG/STORE_VREG macros
   - Update all ~100 operations

2. **Performance:** ~60-70% of x86 (memory access overhead)

3. **Later upgrade** to full register allocator for production use

## Current Status

- **Identified root cause:** ✅ GET_REG macro fundamentally broken
- **Tested fixes:** ❌ Simple mappings don't work
- **Partial implementation:** Stack-based system 10% done
- **C test:** ✅ Still passes (doesn't use vregs)
- **Haxe programs:** ❌ All crash

## Next Steps

**Option A (Recommended):** Complete stack-based POC
- Time: 2-3 hours
- Risk: Low
- Performance: Adequate for testing

**Option B:** Implement full register allocator
- Time: 1-2 days
- Risk: Medium
- Performance: Excellent

## Files Modified

- `src/jit.c` line 3605: vreg initialization
- Various operation implementations using GET_REG

## References

- x86 register allocator: lines 1199-1350
- x86 operations using alloc_cpu: lines 3680+
- ARM64 operations using GET_REG: lines 4700-5900
