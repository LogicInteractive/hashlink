# ARM64 JIT - Next Steps to Complete Phase 1

## Current Status (as of latest commit)
- **96% complete**
- JIT compiles successfully
- 4 operations fixed for GET_REG bug:
  - OStaticClosure ✓
  - OInstanceClosure ✓  
  - OVirtualClosure ✓
  - OCall ✓ (partial)
- Programs still crash with SIGNAL 11 during initialization

## Root Cause
~30 operations still use broken `GET_REG(vr)` pattern which treats vreg index as register number.
Must convert to `LOAD_VREG(reg, vr)` / `STORE_VREG(reg, vr)` pattern.

## Critical Operations Needing Fixes (Priority Order)

### Tier 1: Call Operations (CRITICAL - likely causing current crash)
1. **OCallMethod** (lines ~5011, 5024, 5051)
   - Fix: Load object into X10, args into X0-X7, store result from X0
2. **OCallThis** (lines ~5067, 5085, 5113)  
   - Fix: Load "this" into X0, args into X1-X7, store result from X0
3. **OCallClosure** (lines ~5137, 5144, 5165, 5174, 5191, 5218, 5239)
   - Fix: Handle both dynamic and regular closures with proper LOAD/STORE

### Tier 2: Dynamic Operations (HIGH - used by reflection/dynamic code)
4. **ODynGet** (lines ~5529, 5530)
   - Fix: Load object into X0, store result from X0
5. **ODynSet** (lines ~5616, 5617)
   - Fix: Load value into X0, object into X2

### Tier 3: Other Operations (MEDIUM)
6. ONew, OSwitch, OEnumAlloc
7. ONullCheck, OAssert  
8. OSetI8/I16, OGetI8/I16
9. OUnref, OSetref
10. ORethrow
11. Others (~15 more)

## Fix Pattern

### For argument loading:
```c
// OLD (WRONG):
Arm64Reg src = GET_REG(arg);
arm_mov_reg(ctx, X0, src, true);

// NEW (CORRECT):
LOAD_VREG(X0, arg);
```

### For result storing:
```c
// OLD (WRONG):
Arm64Reg rd = GET_REG(dst);
arm_mov_reg(ctx, rd, X0, true);

// NEW (CORRECT):
STORE_VREG(X0, dst);
```

## How to Complete

### Option A: Manual Fixes (Reliable but Slow)
1. For each operation, read the code
2. Identify all GET_REG uses
3. Replace with appropriate LOAD_VREG or STORE_VREG
4. Remove unnecessary register comparisons
5. Test compilation

### Option B: Python Script (Fast but Requires Care)
1. Create comprehensive script
2. Handle each operation's specific pattern
3. Test after each batch of fixes
4. Commit working state frequently

## Testing Strategy
1. Fix Tier 1 operations first
2. Compile and test after each operation
3. If program runs, move to Tier 2
4. If still crashes, use GDB to identify which operation is failing
5. Fix that operation specifically

## Estimated Work Remaining
- Tier 1: 3 operations, ~20 GET_REG uses → 1-2 hours
- Tier 2: 2 operations, ~4 GET_REG uses → 30 minutes  
- Tier 3: 10+ operations, ~15 GET_REG uses → 2-3 hours
- Total: 4-6 hours of careful systematic work

## Files to Modify
- `src/jit.c` - all fixes go here
- `ARM64_JIT_STATUS.md` - update progress
- Commit after each tier is complete

## Success Criteria
- `./hl test_empty.hl` runs without SIGNAL 11
- Simple Haxe programs execute correctly
- Phase 1 POC complete, ready for Phase 2 (register allocation)
