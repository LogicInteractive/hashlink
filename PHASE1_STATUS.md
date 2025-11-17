# Phase 1 Implementation Status

## What's Done ✅

### 1. Analysis & Planning
- [x] Root cause identified and documented (VREG_SYSTEM_ANALYSIS.md)
- [x] Implementation plan created (PHASE1_IMPLEMENTATION_PLAN.md)
- [x] Macros designed and added to src/jit.c

### 2. Infrastructure Added
```c
// Lines inserted before ARM64 operations (~line 4135)
#define LOAD_VREG(tmp_reg, vr) - Loads vreg from stack into temp register
#define STORE_VREG(tmp_reg, vr) - Stores temp register to vreg on stack
#define GET_REG(vr) - Compatibility macro (returns X10)
```

**Location:** src/jit.c, inserted before `case OIncr:`

**How it works:**
- All vregs stored at `[X29 + stackPos]` (negative offsets from frame pointer)
- Operations load into X10/X11/X12, compute, then store back
- LDUR for offsets -255 to 0, calculated addressing for larger offsets

## What's Left TODO 🔨

### 3. Update ~100 Operations

**Pattern for binary operations (30 ops):**
```c
// OLD:
case OAdd:
    Arm64Reg rd = GET_REG(dst);
    Arm64Reg rn = GET_REG(ra);
    Arm64Reg rm = GET_REG(rb);
    arm_add_reg(ctx, rd, rn, rm, true);
    break;

// NEW:
case OAdd:
    LOAD_VREG(X10, ra);
    LOAD_VREG(X11, rb);
    arm_add_reg(ctx, X10, X10, X11, true);
    STORE_VREG(X10, dst);
    break;
```

**Operations needing this pattern:**
- Arithmetic: OAdd, OSub, OMul, OSDiv, OUDiv, OSMod, OUMod
- Bitwise: OAnd, OOr, OXor, OShl, OSShr, OUShr
- Comparisons: (in jump operations)

**Pattern for unary operations (10 ops):**
```c
case ONeg:
    LOAD_VREG(X10, ra);
    arm_neg(ctx, X10, X10, true);
    STORE_VREG(X10, dst);
    break;
```

**Pattern for immediate loads (5 ops):**
```c
case OInt:
    arm_load_imm64(ctx, X10, m->code->ints[o->p2]);
    STORE_VREG(X10, dst);
    break;
```

**Pattern for function calls (10 ops):**
```c
case OCall1:
    LOAD_VREG(X0, R(o->p3));  // Load arg into X0
    arm_load_imm64(ctx, X9, (uint64_t)m->functions_ptrs[o->p2]);
    arm_blr(ctx, X9);
    if (dst && dst->t->kind != HVOID) {
        STORE_VREG(X0, dst);  // Store result
    }
    break;
```

**Pattern for memory access (15 ops):**
```c
case OField:
    LOAD_VREG(X10, ra);  // Load object pointer
    // Calculate field offset
    arm_ldr_imm(ctx, X10, X10, field_offset, 3);
    STORE_VREG(X10, dst);
    break;
```

**Pattern for jumps (20 ops):**
```c
case OJNotZero:
    LOAD_VREG(X10, dst);
    int jump = arm_do_cbnz(ctx, X10, true);
    register_jump(ctx, jump, target);
    break;
```

### 4. Special Cases Need Manual Attention

**OCallMethod, OCallThis, OCallClosure:**
- Complex argument marshalling
- Multiple vregs accessed
- Need careful temp register management

**OSwitch:**
- Table-based dispatch
- Multiple vreg accesses

**Closures (OStaticClosure, OInstanceClosure, OVirtualClosure):**
- Pointer patching
- Already complex, needs careful update

**Exception Handling (OTrap, OEndTrap):**
- Currently no-ops
- May need special handling

### 5. Additional Required Changes

**Function Prologue:**
Need to add code to store arguments X0-X7 into vreg stack:
```c
// After arm_prologue(ctx, framesize);
for (i = 0; i < nargs && i < 8; i++) {
    vreg *r = R(i);
    STORE_VREG(X0 + i, r);  // Store argument to stack
}
```

**Function Epilogue (ORet):**
Need to load return value from stack:
```c
case ORet:
    if (dst) {
        LOAD_VREG(X0, dst);  // Load return value
    }
    arm_ret(ctx);
    break;
```

## Estimation

**Operations to update:** ~100
**Average time per operation:** 1-2 minutes
**Total time:** 2-3 hours of systematic work

**Breakdown:**
- 30 binary ops × 1 min = 30 min
- 10 unary ops × 1 min = 10 min
- 5 immediate ops × 1 min = 5 min
- 20 jump ops × 2 min = 40 min
- 10 call ops × 3 min = 30 min
- 15 memory ops × 3 min = 45 min
- 10 special cases × 10 min = 100 min
- **Total: ~4 hours**

## How to Complete

### Automated Approach (Recommended)
Create Python/sed scripts for each pattern:
```python
# fix_binary_ops.py
operations = ['OAdd', 'OSub', 'OMul', ...]
for op in operations:
    # Find operation
    # Replace with LOAD/STORE pattern
    # Write back
```

### Manual Approach
1. Open src/jit.c
2. Search for each `case O...:`
3. Find `GET_REG` usage
4. Replace with LOAD/STORE pattern
5. Test compilation after each batch

### Testing Strategy
After each batch of fixes:
```bash
make clean
CFLAGS="-O0 -g -DHL_64 -DHL_JIT_ARM64" make
./hl test_empty.hl
```

## Current Compilation Status

**Expected:** Will NOT compile yet
**Reason:** GET_REG still used in ~100 places
**Solution:** Complete operation updates above

## Next Steps

1. **Option A:** I continue and complete all updates (4 hours)
2. **Option B:** You/team completes using this guide (4 hours)
3. **Option C:** Create automated scripts first (1 hour + 1 hour execution)

**Recommendation:** Option C - write scripts to automate the tedious parts.

## Files Modified

- `src/jit.c` - Macros added at line ~4135
- NOT YET: Individual operations (awaiting updates)

## Success Criteria

Once complete:
- [x] Macros defined
- [ ] All operations updated
- [ ] Compilation succeeds
- [ ] test_empty.hl runs
- [ ] TestInt.hx works
- [ ] Hello World works

---

**Status:** Infrastructure ready, operations awaiting systematic updates
**Time to completion:** 3-4 hours of focused work
**Risk:** Low - pattern is proven, just needs application
**Reward:** Working ARM64 JIT POC!
