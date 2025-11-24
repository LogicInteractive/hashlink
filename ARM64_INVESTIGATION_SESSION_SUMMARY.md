# ARM64 JIT Investigation Session Summary

**Date:** 2025-11-24
**Branch:** `claude/investigate-arm-port-01BGZDyAMZGu2aGDAVSkLfXP`

---

## Investigation Results

### Areas Verified as Correct

After extensive code review, the following areas use proper 64-bit operations:

1. **LOAD_VREG/STORE_VREG macros** - Use `_size = 3` (64-bit) for:
   - HI32 type (explicitly set to 64-bit as fix)
   - All pointer types (HABSTRACT, HOBJ, etc.) via default case

2. **OCallN return values** - `STORE_VREG(X0, dst)` stores 64-bit return values correctly

3. **Native call arguments** - `LOAD_VREG((Arm64Reg)(X0 + i), arg)` loads 64-bit for function arguments

4. **Array access** (OGetArray/OSetArray) - Uses 64-bit operations for pointer types

5. **Type casting** (OSafeCast, OUnsafeCast, OToDyn) - Uses 64-bit copy

6. **This/field access** (OGetThis, OSetThis) - Uses 64-bit loads/stores

7. **Global access** (OGetGlobal, OSetGlobal) - Uses 64-bit operations

8. **Dynamic field access** (ODynGet/ODynSet) - Fixed to use `hl_dyn_geti64/seti64` for HI32

9. **ARM64 instruction encoders** - `arm_ldr_imm`, `arm_str_imm`, `arm_ldur_imm`, `arm_stur_imm` all correctly handle `size=3` for 64-bit operations

### Potential Issue: OField/OSetField Offset Calculation

**Current implementation:**
```c
// OField
int field_offset = o->p3 * HL_WSIZE;
arm_ldr_imm(ctx, X10, X10, field_offset / 8, 3);

// OSetField
int field_offset = o->p2 * HL_WSIZE;
arm_str_imm(ctx, X11, X10, field_offset / 8, 3);
```

**Concern:** This assumes `o->p2`/`o->p3` is a word offset that needs multiplication by `HL_WSIZE`. However:
- If bytecode stores **byte offsets**, this multiplication is wrong
- If bytecode stores **field indices**, we should use `rt->fields_indexes[idx]` instead

**Why x86 JIT doesn't have explicit OField/OSetField handlers:**
- After extensive search, no x86 case handlers found for OField/OSetField
- Either handled by a different mechanism, or falls through silently
- This needs verification on actual x86 execution

### Key Question: What Does o->p2/o->p3 Contain?

The bytecode format is unclear on what exactly these parameters contain:
- **Hypothesis A:** Word offset (byte_offset / HL_WSIZE) - current assumption
- **Hypothesis B:** Byte offset directly
- **Hypothesis C:** Field index requiring runtime lookup

Without bytecode format documentation or Haxe compiler source, this is hard to determine definitively.

---

## Crash Analysis

**Symptom:** `x0 = 0xf5c433ed` (should be `0x7ffff5c433ed`)

**Observation:** The truncated value is exactly the lower 32 bits of a valid pointer, suggesting:
1. A 32-bit load/store was used somewhere, OR
2. An explicit cast to 32-bit occurred, OR
3. Data was stored/loaded from wrong memory location

**Elimination:**
- vreg load/store: Uses 64-bit for all pointer types ✓
- Native call args/returns: Uses 64-bit ✓
- ODynGet/ODynSet: Fixed to use 64-bit for HI32 ✓

**Remaining suspects:**
1. Field offset mismatch in OField/OSetField
2. Some code path not yet identified
3. Interaction with bytecode that uses unexpected type representations

---

## Recommendations

### Immediate: Add Comprehensive Logging

```c
// In OField handler:
fprintf(stderr, "[OField] F%d: obj_ptr=%p p3=%d field_offset=%d loading from %p\n",
        f->findex, obj_ptr, o->p3, field_offset, obj_ptr + field_offset);

// In OSetField handler:
fprintf(stderr, "[OSetField] F%d: obj_ptr=%p p2=%d field_offset=%d storing %p\n",
        f->findex, obj_ptr, o->p2, field_offset, value);

// At crash point (before native call):
fprintf(stderr, "[PRE_CALL] hbset: X0=%p X1=%p X2=%p\n", x0, x1, x2);
```

### Medium-term: Verify Bytecode Format

1. Enhance `dump_bytecode.c` to show OField/OSetField parameters
2. Compare with runtime type information (`rt->fields_indexes`)
3. Determine if offset calculation is correct

### Long-term: Consider Alternative Fix

If field offsets in bytecode are pre-calculated based on T_SIZES at compile time, and we've changed T_SIZES[HI32] at runtime, this creates a fundamental mismatch that cannot be fixed by JIT changes alone.

**Options:**
1. Don't change T_SIZES[HI32] - find another way to handle pointer truncation
2. Recalculate field offsets at module load time
3. Generate new bytecode on ARM64 with correct type sizes

---

## Files Modified in This Investigation

| File | Changes | Purpose |
|------|---------|---------|
| src/jit.c | Fixed duplicate functions, ifdef guards | Compilation fixes |
| src/jit.c | ODynGet/ODynSet use 64-bit | Truncation fix |
| Various .md files | Documentation | Investigation notes |

---

## Conclusion

The ARM64 JIT infrastructure appears correct for 64-bit pointer handling. The persistent truncation at `hl_hbset_impl` suggests the issue is either:

1. **Field offset calculation mismatch** - Most likely if bytecode format differs from assumption
2. **Undiscovered code path** - Less likely given thorough review
3. **Fundamental bytecode/runtime incompatibility** - If T_SIZES changes break pre-calculated offsets

**Next step:** Add logging and test on ARM64 hardware to trace exact data flow leading to truncated pointer.

---

**Status:** Investigation complete, awaiting ARM64 hardware test with logging
