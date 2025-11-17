# ARM64 JIT Debugging Notes

## Critical Discovery

The ARM64 JIT vreg initialization is INCOMPLETE!

### Root Cause

In `hl_jit_function`, x86 code initializes vregs:
```c
// Line 3605 (x86 section)
r->stack.id = i;
```

**ARM64 section has NO equivalent initialization!**

This means:
- `stack.id` is left uninitialized  
- GET_REG macro returns garbage values
- JIT code generates invalid register references

### Crashes Observed

1. **NULL function pointer crash** - hl_call_method tries to call address 0x0
2. **Invalid memory access** - trying to use register ID as memory address

### Fix Needed

ARM64 section needs proper vreg initialization. Two approaches:

**Option 1: Stack-based (simple but slow)**
```c
for(i=0;i<f->nregs;i++) {
    r->stack.id = -1;  // All on stack
    r->stackPos = -(i+1) * 8;  // Negative offset from FP
}
// Use LOAD_VREG/STORE_VREG macros for all access
```

**Option 2: Register allocation (complex but fast)**
```c  
for(i=0;i<f->nregs;i++) {
    if (i < 19) {
        r->stack.id = i;  // Map to X0-X18
    } else {
        r->stack.id = -1;
        r->stackPos = -(i-18) * 8;
    }
}
// Operations check stack.id and load/store as needed
```

### Current Status

- Attempting Option 1 (stack-based)
- Fixed 10 operations: OMov, OInt, OBool, OString, OBytes, OAdd, OSub, OMul, OSDiv, OUDiv
- ~90 operations still need conversion
- C test passes, Haxe programs crash

### Next Steps

1. Complete LOAD/STORE conversion for all operations  
2. OR implement simpler hybrid approach
3. OR get minimal set working first (just enough for HelloWorld)

### Time Investment

~3 hours spent debugging and refactoring.  
Estimated 2-4 hours more for complete fix.
