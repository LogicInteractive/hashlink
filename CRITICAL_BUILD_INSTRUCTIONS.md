# CRITICAL: ARM64 Build Instructions

## The Problem

The Makefile only adds `-DHL_JIT_ARM64` when `$(ARCH) == aarch64` or `$(ARCH) == arm64`.

This means:
- **Building on x86_64:** `HL_JIT_ARM64` is NOT defined
- **Building on ARM64:** `HL_JIT_ARM64` IS defined

## Why This Matters

When `HL_JIT_ARM64` is not defined:
```c
// src/std/types.c:48-52
#if defined(HL_64) && defined(HL_JIT_ARM64)
	8, // I32 - CRITICAL FIX
#else
	4, // I32  ← WRONG! Uses 4 bytes instead of 8
#endif
```

**Result:** `libhl.so` has `T_SIZES[HI32] = 4` instead of 8!

This causes:
1. `hl_type_size(HI32)` returns **4** instead of 8
2. Vreg stack slots allocated as **4 bytes** instead of 8
3. 64-bit STORE_VREG writes **overflow into next vreg**
4. Memory corruption → pointer truncation → crash

## The Fix

**BUILD ON ARM64 HARDWARE ONLY!**

```bash
# On Raspberry Pi 5 (ARM64):
cd /path/to/hashlink
uname -m  # Should output: aarch64

make clean
make

# Verify HL_JIT_ARM64 was defined:
nm libhl.so | grep hl_type_size
# Then check T_SIZES in a debugger or with objdump
```

## Cross-Compilation Alternative

If you MUST cross-compile from x86_64, manually force the flag:

```bash
# WARNING: Untested!
make clean
make CFLAGS="$CFLAGS -DHL_JIT_ARM64"
```

But it's better to build natively on ARM64.

## Verification

After building on ARM64, verify `T_SIZES[HI32] == 8`:

```bash
# In GDB:
gdb ./hl
(gdb) break hl_type_size
(gdb) run /tmp/empty.hl
(gdb) print T_SIZES[3]
# Should show: 8 (not 4!)
```

Or check the logs - if you see `[HI32_FIX] F90 r1: size 4->8`, it means T_SIZES returned 4 (WRONG)!

## Summary

**The pointer truncation bug is likely caused by building on x86_64 instead of ARM64.**

When built on x86_64:
- HL_JIT_ARM64 not defined
- T_SIZES[HI32] = 4
- Vreg slots too small
- 64-bit stores overflow
- Corruption → truncation → crash

**Solution: Rebuild everything on ARM64 (Raspberry Pi 5).**
