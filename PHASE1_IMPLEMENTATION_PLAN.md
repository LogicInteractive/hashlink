# Phase 1: Stack-Based ARM64 JIT - Implementation Plan

## Goal
Get a working ARM64 JIT proof-of-concept by storing all virtual registers on the stack frame. Simple but functional.

## Architecture

### Storage Model
```
Stack Frame Layout (X29 = Frame Pointer):
+------------------+ <- X29 + 8
| Saved LR (X30)   |
+------------------+ <- X29 (Frame Pointer)
| Saved FP (X29)   |
+------------------+ <- X29 - 8
| vreg[0]          | (8 bytes)
+------------------+ <- X29 - 16
| vreg[1]          | (8 bytes)
+------------------+
| vreg[2]          |
| ...              |
+------------------+
| vreg[n-1]        |
+------------------+ <- SP (Stack Pointer)
```

### Access Pattern
```c
// Every vreg access:
1. LOAD from [X29 + stackPos] into temp register (X10, X11, X12)
2. Perform operation on temp register
3. STORE result from temp register to [X29 + stackPos]
```

### Temporary Register Allocation
- **X10**: Primary temp for dst/result
- **X11**: Secondary temp for first operand
- **X12**: Tertiary temp for second operand
- **X9**: Reserved for function pointers, large immediates

## Implementation Steps

### Step 1: Setup (DONE)
- [x] Vreg initialization sets stack.id = -1 for all
- [x] stackPos calculated for each vreg
- [ ] Prologue stores arguments X0-X7 to stack
- [ ] Epilogue restores result from stack to X0

### Step 2: LOAD/STORE Macros
```c
#define LOAD_VREG(tmp_reg, vr) \
    if (vr) { \
        int pos = (vr)->stackPos; \
        if (pos >= -255 && pos <= 0) { \
            arm_ldur_imm(ctx, tmp_reg, X29, pos, 3); \
        } else { \
            arm_load_imm64(ctx, X9, pos); \
            arm_add_reg(ctx, X9, X29, X9, true); \
            arm_ldr_imm(ctx, tmp_reg, X9, 0, 3); \
        } \
    }

#define STORE_VREG(tmp_reg, vr) \
    if (vr) { \
        int pos = (vr)->stackPos; \
        if (pos >= -255 && pos <= 0) { \
            arm_stur_imm(ctx, tmp_reg, X29, pos, 3); \
        } else { \
            arm_load_imm64(ctx, X9, pos); \
            arm_add_reg(ctx, X9, X29, X9, true); \
            arm_str_imm(ctx, tmp_reg, X9, 0, 3); \
        } \
    }
```

### Step 3: Operation Patterns

#### Pattern A: Binary Operations (OAdd, OSub, OMul, etc)
```c
case OAdd:
    LOAD_VREG(X10, ra);  // Load first operand
    LOAD_VREG(X11, rb);  // Load second operand
    arm_add_reg(ctx, X10, X10, X11, true);  // Perform operation
    STORE_VREG(X10, dst);  // Store result
    break;
```

#### Pattern B: Unary Operations (ONeg, ONot, etc)
```c
case ONeg:
    LOAD_VREG(X10, ra);
    arm_neg(ctx, X10, X10, true);
    STORE_VREG(X10, dst);
    break;
```

#### Pattern C: Immediate Operations (OInt, OBool, etc)
```c
case OInt:
    arm_load_imm64(ctx, X10, m->code->ints[o->p2]);
    STORE_VREG(X10, dst);
    break;
```

#### Pattern D: Function Calls (OCall0-4, OCallN)
```c
case OCall1:
    // Load argument into X0
    LOAD_VREG(X0, R(o->p3));

    // Call function
    arm_load_imm64(ctx, X9, (uint64_t)m->functions_ptrs[o->p2]);
    arm_blr(ctx, X9);

    // Store result
    if (dst && dst->t->kind != HVOID) {
        STORE_VREG(X0, dst);
    }
    break;
```

#### Pattern E: Conditional Jumps
```c
case OJNotZero:
    LOAD_VREG(X10, dst);
    // CBNZ X10, target
    int jump = arm_do_cbnz(ctx, X10, true);
    register_jump(ctx, jump, (opCount + 1) + o->p2);
    break;
```

### Step 4: Operations to Update (~100 total)

**Core (10 ops):**
- OMov, OUnsafeCast
- OInt, OBool, ONull
- OString, OBytes
- OGetGlobal, OSetGlobal
- ORet

**Arithmetic (14 ops):**
- OAdd, OSub, OMul, OSDiv, OUDiv, OSMod, OUMod
- ONeg, ONot, OIncr, ODecr
- OShl, OSShr, OUShr
- OAnd, OOr, OXor

**Comparisons & Jumps (20 ops):**
- OJAlways, OJNotZero, OJZero, OJNull, OJNotNull
- OJEq, OJNotEq, OJSLt, OJSGte, OJSLte, OJSGt
- OJULt, OJUGte, OJNotLt, OJNotGte
- OLabel, OSwitch

**Function Calls (10 ops):**
- OCall0, OCall1, OCall2, OCall3, OCall4, OCallN
- OCallMethod, OCallThis, OCallClosure
- ORet

**Memory Access (15 ops):**
- OGetI8, OGetI16, OGetI32, OGetMem
- OSetI8, OSetI16, OSetI32, OSetMem
- OField, OSetField, OGetThis, OSetThis
- OArraySize, OType, OGetType

**Objects & Arrays (15 ops):**
- ONew, ONewArray
- ODynGet, ODynSet
- OArrayRead, OArrayWrite
- OEnum, OEnumAlloc, OEnumIndex, OEnumField
- OSafeCast, OToVirtual, OToInt, OToSFloat, OToUFloat, OToDyn

**Closures & Exceptions (10 ops):**
- OStaticClosure, OInstanceClosure, OVirtualClosure
- OTrap, OEndTrap, OThrow, ORethrow
- ONullCheck, ORefOffset

**Misc (6 ops):**
- OFloat, OString, OBytes
- OLabel, ONop, ORef

## Testing Strategy

1. **Minimal test**: Empty main() - proves initialization works
2. **Arithmetic test**: x + y - proves operations work
3. **Hello World**: trace("Hello") - proves strings and calls work
4. **Fibonacci**: Recursive function - proves full system works

## Performance Expectations

- **Memory accesses**: Every vreg access = 1 load + 1 store = 2 mem ops
- **Typical operation**: 4-6 memory accesses (load operands, store result)
- **Estimated speed**: 60-70% of optimized x86 JIT
- **Still much faster than**: Bytecode interpreter (~10x faster)

## Phase 2 Preview

After Phase 1 works, Phase 2 will add register allocation:
- Use X19-X28 (10 callee-saved registers) for hot vregs
- Track liveness analysis to minimize spills
- Implement x86-style alloc_reg/free_reg
- Expected performance: 90-100% of x86

## Files to Modify

- `src/jit.c`: All operation implementations (lines ~4700-5900)
- Estimated changes: ~500-800 lines modified
- Time estimate: 2-3 hours systematic work

## Success Criteria

- [x] C test program passes
- [ ] test_empty.hl runs without crash
- [ ] TestInt.hx (arithmetic) works
- [ ] Hello World compiles and runs
- [ ] Fibonacci (recursive) works correctly

## Documentation

- [x] VREG_SYSTEM_ANALYSIS.md - Root cause analysis
- [x] PHASE1_IMPLEMENTATION_PLAN.md - This document
- [ ] PHASE1_RESULTS.md - Test results and performance data
- [ ] PHASE2_PLAN.md - Detailed register allocator design

---
**Status**: Ready to implement
**Estimated time**: 2-3 hours
**Risk**: Low (straightforward if tedious)
**Reward**: Working ARM64 JIT POC!
