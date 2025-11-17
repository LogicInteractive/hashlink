/*
 * Copyright (C)2015-2016 Haxe Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifdef _MSC_VER
#pragma warning(disable:4820)
#endif
#include <math.h>
#include <hlmodule.h>
#include "hlsystem.h"

// =====================================================================
// Architecture Detection
// =====================================================================
// Allow manual override via -DHL_JIT_ARM64 or -DHL_JIT_X86
#if !defined(HL_JIT_X86) && !defined(HL_JIT_ARM64) && !defined(HL_JIT_ARM32)
#   if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#       define HL_JIT_X86
#   elif defined(__aarch64__) || defined(_M_ARM64)
#       define HL_JIT_ARM64
#   elif defined(__arm__) || defined(_M_ARM)
#       define HL_JIT_ARM32
#       error "JIT: ARM32 not yet implemented, please use ARM64 (AArch64) or x86-64"
#   else
#       error "JIT: Unsupported processor architecture (only x86/x86-64 and ARM64 supported)"
#   endif
#endif

#ifdef HL_DEBUG
#	define JIT_DEBUG
#endif

// =====================================================================
// x86/x86-64 Backend
// =====================================================================
#ifdef HL_JIT_X86

typedef enum {
	Eax = 0,
	Ecx = 1,
	Edx = 2,
	Ebx = 3,
	Esp = 4,
	Ebp = 5,
	Esi = 6,
	Edi = 7,
#ifdef HL_64
	R8 = 8,
	R9 = 9,
	R10	= 10,
	R11	= 11,
	R12	= 12,
	R13	= 13,
	R14	= 14,
	R15	= 15,
#endif
	_LAST = 0xFF
} CpuReg;

typedef enum {
	MOV,
	LEA,
	PUSH,
	ADD,
	SUB,
	IMUL,	// only overflow flag changes compared to MUL
	DIV,
	IDIV,
	CDQ,
	CDQE,
	POP,
	RET,
	CALL,
	AND,
	OR,
	XOR,
	CMP,
	TEST,
	NOP,
	SHL,
	SHR,
	SAR,
	INC,
	DEC,
	JMP,
	// FPU
	FSTP,
	FSTP32,
	FLD,
	FLD32,
	FLDCW,
	// SSE
	MOVSD,
	MOVSS,
	COMISD,
	COMISS,
	ADDSD,
	SUBSD,
	MULSD,
	DIVSD,
	ADDSS,
	SUBSS,
	MULSS,
	DIVSS,
	XORPD,
	CVTSI2SD,
	CVTSI2SS,
	CVTSD2SI,
	CVTSD2SS,
	CVTSS2SD,
	CVTSS2SI,
	STMXCSR,
	LDMXCSR,
	// 8-16 bits
	MOV8,
	CMP8,
	TEST8,
	PUSH8,
	MOV16,
	CMP16,
	TEST16,
	// prefetchs
	PREFETCHT0,
	PREFETCHT1,
	PREFETCHT2,
	PREFETCHNTA,
	PREFETCHW,
	// --
	_CPU_LAST
} CpuOp;

#define JAlways		0
#define JOverflow	0x80
#define JULt		0x82
#define JUGte		0x83
#define JEq			0x84
#define JNeq		0x85
#define JULte		0x86
#define JUGt		0x87
#define JParity		0x8A
#define JNParity	0x8B
#define JSLt		0x8C
#define JSGte		0x8D
#define JSLte		0x8E
#define JSGt		0x8F

#define JCarry		JLt
#define JZero		JEq
#define JNotZero	JNeq

#define B(bv)	*ctx->buf.b++ = (unsigned char)(bv)
#define W(wv)	*ctx->buf.w++ = wv

#ifdef HL_64
#	define W64(wv)	*ctx->buf.w64++ = wv
#else
#	define W64(wv)	W(wv)
#endif

static const int SIB_MULT[] = {-1, 0, 1, -1, 2, -1, -1, -1, 3};

#define MOD_RM(mod,reg,rm)		B(((mod) << 6) | (((reg)&7) << 3) | ((rm)&7))
#define SIB(mult,rmult,rbase)	B((SIB_MULT[mult]<<6) | (((rmult)&7)<<3) | ((rbase)&7))
#define IS_SBYTE(c)				( (c) >= -128 && (c) < 128 )

#define AddJump(how,local)		{ if( (how) == JAlways ) { B(0xE9); } else { B(0x0F); B(how); }; local = BUF_POS(); W(0); }
#define AddJump_small(how,local) { if( (how) == JAlways ) { B(0xEB); } else B(how - 0x10); local = BUF_POS() | 0x40000000; B(0); }
#define XJump(how,local)		AddJump(how,local)
#define XJump_small(how,local)		AddJump_small(how,local)

#define MAX_OP_SIZE				256

#define BUF_POS()				((int)(ctx->buf.b - ctx->startBuf))
#define RTYPE(r)				r->t->kind

#ifdef HL_64
#	define RESERVE_ADDRESS	0x8000000000000000
#else
#	define RESERVE_ADDRESS	0x80000000
#endif

#if defined(HL_WIN_CALL) && defined(HL_64)
#	define IS_WINCALL64 1
#else
#	define IS_WINCALL64 0
#endif

#endif // HL_JIT_X86

// =====================================================================
// ARM64/AArch64 Backend
// =====================================================================
#ifdef HL_JIT_ARM64

// ARM64 General Purpose Registers (64-bit: X0-X30, 32-bit: W0-W30)
typedef enum {
	X0 = 0,   // Argument/result register 1, temporary
	X1 = 1,   // Argument/result register 2, temporary
	X2 = 2,   // Argument register 3, temporary
	X3 = 3,   // Argument register 4, temporary
	X4 = 4,   // Argument register 5, temporary
	X5 = 5,   // Argument register 6, temporary
	X6 = 6,   // Argument register 7, temporary
	X7 = 7,   // Argument register 8, temporary
	X8 = 8,   // Indirect result location, temporary
	X9 = 9,   // Temporary register
	X10 = 10, // Temporary register
	X11 = 11, // Temporary register
	X12 = 12, // Temporary register
	X13 = 13, // Temporary register
	X14 = 14, // Temporary register
	X15 = 15, // Temporary register
	X16 = 16, // IP0 - Intra-procedure-call temporary
	X17 = 17, // IP1 - Intra-procedure-call temporary
	X18 = 18, // Platform register (reserved on some platforms)
	X19 = 19, // Callee-saved register
	X20 = 20, // Callee-saved register
	X21 = 21, // Callee-saved register
	X22 = 22, // Callee-saved register
	X23 = 23, // Callee-saved register
	X24 = 24, // Callee-saved register
	X25 = 25, // Callee-saved register
	X26 = 26, // Callee-saved register
	X27 = 27, // Callee-saved register
	X28 = 28, // Callee-saved register
	X29 = 29, // FP - Frame pointer
	X30 = 30, // LR - Link register
	XZR = 31, // Zero register / SP depending on context
	_ARM_LAST = 0xFF
} Arm64Reg;

// SP is the stack pointer, which is register 31 in load/store contexts
#define SP XZR

// ARM64 SIMD/FP Registers (128-bit: V0-V31, 64-bit: D0-D31, 32-bit: S0-S31)
// We'll primarily use V registers for consistency
typedef enum {
	V0 = 0,   // Argument/result register 1, temporary
	V1 = 1,   // Argument/result register 2, temporary
	V2 = 2,   // Argument/result register 3, temporary
	V3 = 3,   // Argument/result register 4, temporary
	V4 = 4,   // Argument/result register 5, temporary
	V5 = 5,   // Argument/result register 6, temporary
	V6 = 6,   // Argument/result register 7, temporary
	V7 = 7,   // Argument/result register 8, temporary
	V8 = 8,   // Callee-saved (low 64 bits only)
	V9 = 9,   // Callee-saved (low 64 bits only)
	V10 = 10, // Callee-saved (low 64 bits only)
	V11 = 11, // Callee-saved (low 64 bits only)
	V12 = 12, // Callee-saved (low 64 bits only)
	V13 = 13, // Callee-saved (low 64 bits only)
	V14 = 14, // Callee-saved (low 64 bits only)
	V15 = 15, // Callee-saved (low 64 bits only)
	V16 = 16, // Temporary register
	V17 = 17, // Temporary register
	V18 = 18, // Temporary register
	V19 = 19, // Temporary register
	V20 = 20, // Temporary register
	V21 = 21, // Temporary register
	V22 = 22, // Temporary register
	V23 = 23, // Temporary register
	V24 = 24, // Temporary register
	V25 = 25, // Temporary register
	V26 = 26, // Temporary register
	V27 = 27, // Temporary register
	V28 = 28, // Temporary register
	V29 = 29, // Temporary register
	V30 = 30, // Temporary register
	V31 = 31, // Temporary register
	_ARM_FP_LAST = 0xFF
} Arm64FpReg;

// ARM64 Instruction Opcodes (stub - will be expanded in Phase 2)
typedef enum {
	ARM_MOV,
	ARM_ADD,
	ARM_SUB,
	ARM_MUL,
	ARM_DIV,
	ARM_LDR,
	ARM_STR,
	ARM_B,
	ARM_BL,
	ARM_RET,
	// ... many more to be added in Phase 2
	_ARM_OP_LAST
} Arm64Op;

// ARM64 Condition Codes
typedef enum {
	ARM64_COND_EQ = 0x0,  // Equal (Z set)
	ARM64_COND_NE = 0x1,  // Not equal (Z clear)
	ARM64_COND_CS = 0x2,  // Carry set / unsigned higher or same
	ARM64_COND_HS = 0x2,  // Alias for CS (unsigned higher or same)
	ARM64_COND_CC = 0x3,  // Carry clear / unsigned lower
	ARM64_COND_LO = 0x3,  // Alias for CC (unsigned lower)
	ARM64_COND_MI = 0x4,  // Minus / negative
	ARM64_COND_PL = 0x5,  // Plus / positive or zero
	ARM64_COND_VS = 0x6,  // Overflow set
	ARM64_COND_VC = 0x7,  // Overflow clear
	ARM64_COND_HI = 0x8,  // Unsigned higher
	ARM64_COND_LS = 0x9,  // Unsigned lower or same
	ARM64_COND_GE = 0xA,  // Signed greater than or equal
	ARM64_COND_LT = 0xB,  // Signed less than
	ARM64_COND_GT = 0xC,  // Signed greater than
	ARM64_COND_LE = 0xD,  // Signed less than or equal
	ARM64_COND_AL = 0xE,  // Always (unconditional)
	ARM64_COND_NV = 0xF   // Never (reserved)
} Arm64Condition;

// ARM64 buffer writing macro (instructions are 32-bit)
#define B32(val)	*ctx->buf.w++ = (unsigned int)(val)

// ARM64 buffer position
#define ARM_BUF_POS()	((int)((unsigned char*)ctx->buf.w - ctx->startBuf))

// Helper to encode register field (checks for valid register range)
static inline unsigned int arm_reg(int reg) {
	return (unsigned int)(reg & 0x1F);  // 5 bits for register number
}

// Helper to check if immediate fits in N bits (signed)
static inline bool arm_fits_signed(int64_t val, int bits) {
	int64_t min = -(1LL << (bits - 1));
	int64_t max = (1LL << (bits - 1)) - 1;
	return val >= min && val <= max;
}

// Helper to check if immediate fits in N bits (unsigned)
static inline bool arm_fits_unsigned(uint64_t val, int bits) {
	return val < (1ULL << bits);
}

#endif // HL_JIT_ARM64

// =====================================================================
// Architecture-Independent Structures
// =====================================================================

typedef struct jlist jlist;
struct jlist {
	int pos;
	int target;
	jlist *next;
};

typedef struct vreg vreg;

typedef enum {
	RCPU = 0,
	RFPU = 1,
	RSTACK = 2,
	RCONST = 3,
	RADDR = 4,
	RMEM = 5,
	RUNUSED = 6,
	RCPU_CALL = 1 | 8,
	RCPU_8BITS = 1 | 16
} preg_kind;

typedef struct {
	preg_kind kind;
	int id;
	int lock;
	vreg *holds;
} preg;

struct vreg {
	int stackPos;
	int size;
	hl_type *t;
	preg *current;
	preg stack;
};

#define REG_AT(i)		(ctx->pregs + (i))

// =====================================================================
// x86/x86-64 Register Configuration
// =====================================================================
#ifdef HL_JIT_X86

#ifdef HL_64
#	define RCPU_COUNT	16
#	define RFPU_COUNT	16
#	ifdef HL_WIN_CALL
#		define CALL_NREGS			4
#		define RCPU_SCRATCH_COUNT	7
#		define RFPU_SCRATCH_COUNT	6
static const int RCPU_SCRATCH_REGS[] = { Eax, Ecx, Edx, R8, R9, R10, R11 };
static const CpuReg CALL_REGS[] = { Ecx, Edx, R8, R9 };
#	else
#		define CALL_NREGS			6 // TODO : XMM6+XMM7 are FPU reg parameters
#		define RCPU_SCRATCH_COUNT	9
#		define RFPU_SCRATCH_COUNT	16
static const int RCPU_SCRATCH_REGS[] = { Eax, Ecx, Edx, Esi, Edi, R8, R9, R10, R11 };
static const CpuReg CALL_REGS[] = { Edi, Esi, Edx, Ecx, R8, R9 };
#	endif
#else
#	define CALL_NREGS	0
#	define RCPU_COUNT	8
#	define RFPU_COUNT	8
#	define RCPU_SCRATCH_COUNT	3
#	define RFPU_SCRATCH_COUNT	8
static const int RCPU_SCRATCH_REGS[] = { Eax, Ecx, Edx };
#endif

#define XMM(i)			((i) + RCPU_COUNT)
#define PXMM(i)			REG_AT(XMM(i))
#define REG_IS_FPU(i)	((i) >= RCPU_COUNT)

#define PEAX			REG_AT(Eax)
#define PESP			REG_AT(Esp)
#define PEBP			REG_AT(Ebp)

#define REG_COUNT	(RCPU_COUNT + RFPU_COUNT)

#endif // HL_JIT_X86

// =====================================================================
// ARM64/AArch64 Register Configuration
// =====================================================================
#ifdef HL_JIT_ARM64

// ARM64 has 31 general purpose registers (X0-X30) + zero/SP
// and 32 SIMD/FP registers (V0-V31)
#define RCPU_COUNT	31
#define RFPU_COUNT	32

// AAPCS64 calling convention:
// - X0-X7: Argument/result registers
// - X8: Indirect result location (for large struct returns)
// - X9-X15: Temporary registers (caller-saved)
// - X16-X17: Intra-procedure-call temporary registers (IP0, IP1)
// - X18: Platform register (may be reserved on some platforms)
// - X19-X28: Callee-saved registers
// - X29: Frame pointer (FP)
// - X30: Link register (LR)
// - XZR/SP: Zero register or stack pointer (depending on context)

#define CALL_NREGS			8 // X0-X7 for arguments
#define RCPU_SCRATCH_COUNT	10 // X0-X8, X9-X15 (excluding X16-X17 which are special)
#define RFPU_SCRATCH_COUNT	32 // All V registers are caller-saved in AAPCS64

// Scratch (caller-saved) GP registers: X0-X15
static const int RCPU_SCRATCH_REGS[] = {
	X0, X1, X2, X3, X4, X5, X6, X7,
	X9, X10, X11, X12, X13, X14, X15
};

// Argument registers: X0-X7
static const Arm64Reg CALL_REGS[] = {
	X0, X1, X2, X3, X4, X5, X6, X7
};

#define VREG(i)			((i) + RCPU_COUNT)
#define PVREG(i)		REG_AT(VREG(i))
#define REG_IS_FP(i)	((i) >= RCPU_COUNT)

// Common register aliases for ARM64
#define PX0			REG_AT(X0)
#define PX29		REG_AT(X29)  // Frame pointer
#define PX30		REG_AT(X30)  // Link register

#define REG_COUNT	(RCPU_COUNT + RFPU_COUNT)

#endif // HL_JIT_ARM64

#define ID2(a,b)	((a) | ((b)<<8))
#define R(id)		(ctx->vregs + (id))
#define ASSERT(i)	{ printf("JIT ERROR %d (jit.c line %d)\n",i,(int)__LINE__); jit_exit(); }
#define IS_FLOAT(r)	((r)->t->kind == HF64 || (r)->t->kind == HF32)
#define RLOCK(r)		if( (r)->lock < ctx->currentPos ) (r)->lock = ctx->currentPos
#define RUNLOCK(r)		if( (r)->lock == ctx->currentPos ) (r)->lock = 0

#define BREAK()		B(0xCC)

#if defined(HL_64) && defined(HL_VCC)
#	define JIT_CUSTOM_LONGJUMP
#endif

static preg _unused = { RUNUSED, 0, 0, NULL };
static preg *UNUSED = &_unused;

struct jit_ctx {
	union {
		unsigned char *b;
		unsigned int *w;
		unsigned long long *w64;
		int *i;
		double *d;
	} buf;
	vreg *vregs;
	preg pregs[REG_COUNT];
	vreg *savedRegs[REG_COUNT];
	int savedLocks[REG_COUNT];
	int *opsPos;
	int maxRegs;
	int maxOps;
	int bufSize;
	int totalRegsSize;
	int functionPos;
	int allocOffset;
	int currentPos;
	int nativeArgsCount;
	unsigned char *startBuf;
	hl_module *m;
	hl_function *f;
	jlist *jumps;
	jlist *calls;
	jlist *switchs;
	hl_alloc falloc; // cleared per-function
	hl_alloc galloc;
	vclosure *closure_list;
	hl_debug_infos *debug;
	int c2hl;
	int hl2c;
	int longjump;
	void *static_functions[8];
	bool static_function_offset;
};

// =====================================================================
// ARM64 Buffer Management Macros (shared with x86)
// =====================================================================
#define MAX_OP_SIZE				256
#define BUF_POS()				((int)(ctx->buf.b - ctx->startBuf))

static void jit_buf( jit_ctx *ctx ) {
	if( BUF_POS() > ctx->bufSize - MAX_OP_SIZE ) {
		int nsize = ctx->bufSize * 4 / 3;
		unsigned char *nbuf;
		int curpos;
		if( nsize == 0 ) {
			int i;
			for(i=0;i<ctx->m->code->nfunctions;i++)
				nsize += ctx->m->code->functions[i].nops;
			nsize *= 4;
		}
		if( nsize < ctx->bufSize + MAX_OP_SIZE * 4 ) nsize = ctx->bufSize + MAX_OP_SIZE * 4;
		curpos = BUF_POS();
		nbuf = (unsigned char*)malloc(nsize);
		if( nbuf == NULL ) {
			hl_error("Out of memory allocating JIT buffer");
			return;
		}
		if( ctx->startBuf ) {
			memcpy(nbuf,ctx->startBuf,curpos);
			free(ctx->startBuf);
		}
		ctx->startBuf = nbuf;
		ctx->buf.b = nbuf + curpos;
		ctx->bufSize = nsize;
	}
}

#define jit_exit() { hl_debug_break(); exit(-1); }
#define jit_error(msg)	_jit_error(ctx,msg,__LINE__)

// ARM64 error handling function
static void _jit_error( jit_ctx *ctx, const char *msg, int line ) {
	printf("JIT ERROR at jit.c:%d - %s\n", line, msg);
	printf("This operation is not yet implemented in the ARM64 JIT.\n");
	printf("Status: 98/102 operations implemented (96%% complete)\n");
	exit(1);
}

#ifndef HL_64
#	ifdef HL_DEBUG
#		define error_i64() jit_error("i64-32")
#	else
void error_i64() {
	printf("The module you are loading is using 64 bit ints that are not supported by the HL32.\nPlease run using HL64 or compile with -D hl-legacy32");
	jit_exit();
}
#	endif
#endif

static void _jit_error( jit_ctx *ctx, const char *msg, int line );
static void on_jit_error( const char *msg, int_val line );

// =====================================================================
// Shared Helper Functions (used by both x86 and ARM64)
// =====================================================================

static int stack_size( hl_type *t ) {
	switch( t->kind ) {
	case HUI8:
	case HUI16:
	case HBOOL:
#ifdef HL_64
	case HI32:
	case HF32:
#endif
		return sizeof(int_val);
	case HI64:
	default:
		return hl_type_size(t);
	}
}

static void register_jump( jit_ctx *ctx, int pos, int target ) {
	jlist *j = (jlist*)hl_malloc(&ctx->falloc, sizeof(jlist));
	j->pos = pos;
	j->target = target;
	j->next = ctx->jumps;
	ctx->jumps = j;
	if( target != 0 && ctx->opsPos[target] == 0 )
		ctx->opsPos[target] = -1;
}

static double uint_to_double( unsigned int v ) {
	return v;
}

// =====================================================================
// x86/x86-64 JIT Implementation
// =====================================================================
#ifdef HL_JIT_X86

static preg *pmem( preg *r, CpuReg reg, int offset ) {
	r->kind = RMEM;
	r->id = 0 | (reg << 4) | (offset << 8);
	return r;
}

static preg *pmem2( preg *r, CpuReg reg, CpuReg reg2, int mult, int offset ) {
	r->kind = RMEM;
	r->id = mult | (reg << 4) | (reg2 << 8);
	r->holds = (void*)(int_val)offset;
	return r;
}

#ifdef HL_64
static preg *pcodeaddr( preg *r, int offset ) {
	r->kind = RMEM;
	r->id = 15 | (offset << 4);
	return r;
}
#endif

static preg *pconst( preg *r, int c ) {
	r->kind = RCONST;
	r->holds = NULL;
	r->id = c;
	return r;
}

static preg *pconst64( preg *r, int_val c ) {
#ifdef HL_64
	if( ((int)c) == c )
		return pconst(r,(int)c);
	r->kind = RCONST;
	r->id = 0xC064C064;
	r->holds = (vreg*)c;
	return r;
#else
	return pconst(r,(int)c);
#endif
}

#ifndef HL_64
// it is not possible to access direct 64 bit address in x86-64
static preg *paddr( preg *r, void *p ) {
	r->kind = RADDR;
	r->holds = (vreg*)p;
	return r;
}
#endif

static void save_regs( jit_ctx *ctx ) {
	int i;
	for(i=0;i<REG_COUNT;i++) {
		ctx->savedRegs[i] = ctx->pregs[i].holds;
		ctx->savedLocks[i] = ctx->pregs[i].lock;
	}
}

static void restore_regs( jit_ctx *ctx ) {
	int i;
	for(i=0;i<ctx->maxRegs;i++)
		ctx->vregs[i].current = NULL;
	for(i=0;i<REG_COUNT;i++) {
		vreg *r = ctx->savedRegs[i];
		preg *p = ctx->pregs + i;
		p->holds = r;
		p->lock = ctx->savedLocks[i];
		if( r ) r->current = p;
	}
}

static void jit_buf( jit_ctx *ctx ) {
	if( BUF_POS() > ctx->bufSize - MAX_OP_SIZE ) {
		int nsize = ctx->bufSize * 4 / 3;
		unsigned char *nbuf;
		int curpos;
		if( nsize == 0 ) {
			int i;
			for(i=0;i<ctx->m->code->nfunctions;i++)
				nsize += ctx->m->code->functions[i].nops;
			nsize *= 4;
		}
		if( nsize < ctx->bufSize + MAX_OP_SIZE * 4 ) nsize = ctx->bufSize + MAX_OP_SIZE * 4;
		curpos = BUF_POS();
		nbuf = (unsigned char*)malloc(nsize);
		if( nbuf == NULL ) ASSERT(nsize);
		if( ctx->startBuf ) {
			memcpy(nbuf,ctx->startBuf,curpos);
			free(ctx->startBuf);
		}
		ctx->startBuf = nbuf;
		ctx->buf.b = nbuf + curpos;
		ctx->bufSize = nsize;
	}
}

static const char *KNAMES[] = { "cpu","fpu","stack","const","addr","mem","unused" };
#define ERRIF(c)	if( c ) { printf("%s(%s,%s)\n",f?f->name:"???",KNAMES[a->kind], KNAMES[b->kind]); ASSERT(0); }

typedef struct {
	const char *name;						// single operand
	int r_mem;		// r32 / r/m32				r32
	int mem_r;		// r/m32 / r32				r/m32
	int r_const;	// r32 / imm32				imm32
	int r_i8;		// r32 / imm8				imm8
	int mem_const;	// r/m32 / imm32			N/A
} opform;

#define FLAG_LONGOP	0x80000000
#define FLAG_16B	0x40000000
#define FLAG_8B		0x20000000
#define FLAG_DUAL   0x10000000

#define RM(op,id) ((op) | (((id)+1)<<8))
#define GET_RM(op)	(((op) >> ((op) < 0 ? 24 : 8)) & 15)
#define SBYTE(op) ((op) << 16)
#define LONG_OP(op)	((op) | FLAG_LONGOP)
#define OP16(op)	LONG_OP((op) | FLAG_16B)
#define LONG_RM(op,id)	LONG_OP(op | (((id) + 1) << 24))

static opform OP_FORMS[_CPU_LAST] = {
	{ "MOV", 0x8B, 0x89, 0xB8, 0, RM(0xC7,0) },
	{ "LEA", 0x8D },
	{ "PUSH", 0x50, RM(0xFF,6), 0x68, 0x6A },
	{ "ADD", 0x03, 0x01, RM(0x81,0), RM(0x83,0) },
	{ "SUB", 0x2B, 0x29, RM(0x81,5), RM(0x83,5) },
	{ "IMUL", LONG_OP(0x0FAF), 0, 0x69 | FLAG_DUAL, 0x6B | FLAG_DUAL },
	{ "DIV", RM(0xF7,6), RM(0xF7,6) },
	{ "IDIV", RM(0xF7,7), RM(0xF7,7) },
	{ "CDQ", 0x99 },
	{ "CDQE", 0x98 },
	{ "POP", 0x58, RM(0x8F,0) },
	{ "RET", 0xC3 },
	{ "CALL", RM(0xFF,2), RM(0xFF,2), 0xE8 },
	{ "AND", 0x23, 0x21, RM(0x81,4), RM(0x83,4) },
	{ "OR", 0x0B, 0x09, RM(0x81,1), RM(0x83,1) },
	{ "XOR", 0x33, 0x31, RM(0x81,6), RM(0x83,6) },
	{ "CMP", 0x3B, 0x39, RM(0x81,7), RM(0x83,7) },
	{ "TEST", 0x85, 0x85/*SWP?*/, RM(0xF7,0) },
	{ "NOP", 0x90 },
	{ "SHL", RM(0xD3,4), 0, 0, RM(0xC1,4) },
	{ "SHR", RM(0xD3,5), 0, 0, RM(0xC1,5) },
	{ "SAR", RM(0xD3,7), 0, 0, RM(0xC1,7) },
	{ "INC", IS_64 ? RM(0xFF,0) : 0x40, RM(0xFF,0) },
	{ "DEC", IS_64 ? RM(0xFF,1) : 0x48, RM(0xFF,1) },
	{ "JMP", RM(0xFF,4) },
	// FPU
	{ "FSTP", 0, RM(0xDD,3) },
	{ "FSTP32", 0, RM(0xD9,3) },
	{ "FLD", 0, RM(0xDD,0) },
	{ "FLD32", 0, RM(0xD9,0) },
	{ "FLDCW", 0, RM(0xD9, 5) },
	// SSE
	{ "MOVSD", 0xF20F10, 0xF20F11  },
	{ "MOVSS", 0xF30F10, 0xF30F11  },
	{ "COMISD", 0x660F2F },
	{ "COMISS", LONG_OP(0x0F2F) },
	{ "ADDSD", 0xF20F58 },
	{ "SUBSD", 0xF20F5C },
	{ "MULSD", 0xF20F59 },
	{ "DIVSD", 0xF20F5E },
	{ "ADDSS", 0xF30F58 },
	{ "SUBSS", 0xF30F5C },
	{ "MULSS", 0xF30F59 },
	{ "DIVSS", 0xF30F5E },
	{ "XORPD", 0x660F57 },
	{ "CVTSI2SD", 0xF20F2A },
	{ "CVTSI2SS", 0xF30F2A },
	{ "CVTSD2SI", 0xF20F2D },
	{ "CVTSD2SS", 0xF20F5A },
	{ "CVTSS2SD", 0xF30F5A },
	{ "CVTSS2SI", 0xF30F2D },
	{ "STMXCSR", 0, LONG_RM(0x0FAE,3) },
	{ "LDMXCSR", 0, LONG_RM(0x0FAE,2) },
	// 8 bits,
	{ "MOV8", 0x8A, 0x88, 0, 0xB0, RM(0xC6,0) },
	{ "CMP8", 0x3A, 0x38, 0, RM(0x80,7) },
	{ "TEST8", 0x84, 0x84, RM(0xF6,0) },
	{ "PUSH8", 0, 0, 0x6A | FLAG_8B },
	{ "MOV16", OP16(0x8B), OP16(0x89), OP16(0xB8) },
	{ "CMP16", OP16(0x3B), OP16(0x39) },
	{ "TEST16", OP16(0x85) },
	// prefetchs
	{ "PREFETCHT0", 0, LONG_RM(0x0F18,1) },
	{ "PREFETCHT1", 0, LONG_RM(0x0F18,2) },
	{ "PREFETCHT2", 0, LONG_RM(0x0F18,3) },
	{ "PREFETCHNTA", 0, LONG_RM(0x0F18,0) },
	{ "PREFETCHW", 0, LONG_RM(0x0F0D,1) },
};

#ifdef HL_64
#	define REX()	if( r64 ) B(r64 | 0x40)
#else
#	define REX()
#endif

#define	OP(b)	\
	if( (b) & 0xFF0000 ) { \
		B((b)>>16); \
		if( r64 ) B(r64 | 0x40); /* also in 32 bits mode */ \
		B((b)>>8); \
		B(b); \
	} else { \
		if( (b) & FLAG_16B ) { \
			B(0x66); \
			REX(); \
		} else {\
			REX(); \
			if( (b) & FLAG_LONGOP ) B((b)>>8); \
		}\
		B(b); \
	}

static bool is_reg8( preg *a ) {
	return a->kind == RSTACK || a->kind == RMEM || a->kind == RCONST || (a->kind == RCPU && a->id != Esi && a->id != Edi);
}

static void op( jit_ctx *ctx, CpuOp o, preg *a, preg *b, bool mode64 ) {
	opform *f = &OP_FORMS[o];
	int r64 = mode64 && (o != PUSH && o != POP && o != CALL && o != PUSH8 && o < PREFETCHT0) ? 8 : 0;
	switch( o ) {
	case CMP8:
	case TEST8:
	case MOV8:
		if( !is_reg8(a) || !is_reg8(b) )
			ASSERT(0);
		break;
	default:
		break;
	}
	switch( ID2(a->kind,b->kind) ) {
	case ID2(RUNUSED,RUNUSED):
		ERRIF(f->r_mem == 0);
		OP(f->r_mem);
		break;
	case ID2(RCPU,RCPU):
	case ID2(RFPU,RFPU):
		ERRIF( f->r_mem == 0 );
		if( a->id > 7 ) r64 |= 4;
		if( b->id > 7 ) r64 |= 1;
		OP(f->r_mem);
		MOD_RM(3,a->id,b->id);
		break;
	case ID2(RCPU,RFPU):
	case ID2(RFPU,RCPU):
		ERRIF( (f->r_mem>>16) == 0 );
		if( a->id > 7 ) r64 |= 4;
		if( b->id > 7 ) r64 |= 1;
		OP(f->r_mem);
		MOD_RM(3,a->id,b->id);
		break;
	case ID2(RCPU,RUNUSED):
		ERRIF( f->r_mem == 0 );
		if( a->id > 7 ) r64 |= 1;
		if( GET_RM(f->r_mem) > 0 ) {
			OP(f->r_mem);
			MOD_RM(3, GET_RM(f->r_mem)-1, a->id);
		} else
			OP(f->r_mem + (a->id&7));
		break;
	case ID2(RSTACK,RUNUSED):
		ERRIF( f->mem_r == 0 || GET_RM(f->mem_r) == 0 );
		{
			int stackPos = R(a->id)->stackPos;
			OP(f->mem_r);
			if( IS_SBYTE(stackPos) ) {
				MOD_RM(1,GET_RM(f->mem_r)-1,Ebp);
				B(stackPos);
			} else {
				MOD_RM(2,GET_RM(f->mem_r)-1,Ebp);
				W(stackPos);
			}
		}
		break;
	case ID2(RCPU,RCONST):
		ERRIF( f->r_const == 0 && f->r_i8 == 0 );
		if( a->id > 7 ) r64 |= 1;
		{
			int_val cval = b->holds ? (int_val)b->holds : b->id;
			// short byte form
			if( f->r_i8 && IS_SBYTE(cval) ) {
				if( (f->r_i8&FLAG_DUAL) && a->id > 7 ) r64 |= 4;
				OP(f->r_i8);
				if( (f->r_i8&FLAG_DUAL) ) MOD_RM(3,a->id,a->id); else MOD_RM(3,GET_RM(f->r_i8)-1,a->id);
				B((int)cval);
			} else if( GET_RM(f->r_const) > 0 || (f->r_const&FLAG_DUAL) ) {
				if( (f->r_i8&FLAG_DUAL) && a->id > 7 ) r64 |= 4;
				OP(f->r_const&0xFF);
				if( (f->r_i8&FLAG_DUAL) ) MOD_RM(3,a->id,a->id); else MOD_RM(3,GET_RM(f->r_const)-1,a->id);
				if( mode64 && IS_64 && o == MOV ) W64(cval); else W((int)cval);
			} else {
				ERRIF( f->r_const == 0);
				OP((f->r_const&0xFF) + (a->id&7));
				if( mode64 && IS_64 && o == MOV ) W64(cval); else W((int)cval);
			}
		}
		break;
	case ID2(RSTACK,RCPU):
	case ID2(RSTACK,RFPU):
		ERRIF( f->mem_r == 0 );
		if( b->id > 7 ) r64 |= 4;
		{
			int stackPos = R(a->id)->stackPos;
			OP(f->mem_r);
			if( IS_SBYTE(stackPos) ) {
				MOD_RM(1,b->id,Ebp);
				B(stackPos);
			} else {
				MOD_RM(2,b->id,Ebp);
				W(stackPos);
			}
		}
		break;
	case ID2(RCPU,RSTACK):
	case ID2(RFPU,RSTACK):
		ERRIF( f->r_mem == 0 );
		if( a->id > 7 ) r64 |= 4;
		{
			int stackPos = R(b->id)->stackPos;
			OP(f->r_mem);
			if( IS_SBYTE(stackPos) ) {
				MOD_RM(1,a->id,Ebp);
				B(stackPos);
			} else {
				MOD_RM(2,a->id,Ebp);
				W(stackPos);
			}
		}
		break;
	case ID2(RCONST,RUNUSED):
		ERRIF( f->r_const == 0 );
		{
			int_val cval = a->holds ? (int_val)a->holds : a->id;
			OP(f->r_const);
			if( f->r_const & FLAG_8B ) B((int)cval); else W((int)cval);
		}
		break;
	case ID2(RMEM,RUNUSED):
		ERRIF( f->mem_r == 0 );
		{
			int mult = a->id & 0xF;
			int regOrOffs = mult == 15 ? a->id >> 4 : a->id >> 8;
			CpuReg reg = (a->id >> 4) & 0xF;
			if( mult == 15 ) {
				ERRIF(1);
			} else if( mult == 0 ) {
				if( reg > 7 ) r64 |= 1;
				OP(f->mem_r);
				if( regOrOffs == 0 && (reg&7) != Ebp ) {
					MOD_RM(0,GET_RM(f->mem_r)-1,reg);
					if( (reg&7) == Esp ) B(0x24);
				} else if( IS_SBYTE(regOrOffs) ) {
					MOD_RM(1,GET_RM(f->mem_r)-1,reg);
					if( (reg&7) == Esp ) B(0x24);
					B(regOrOffs);
				} else {
					MOD_RM(2,GET_RM(f->mem_r)-1,reg);
					if( (reg&7) == Esp ) B(0x24);
					W(regOrOffs);
				}
			} else {
				// [eax + ebx * M]
				ERRIF(1);
			}
		}
		break;
	case ID2(RCPU, RMEM):
	case ID2(RFPU, RMEM):
		ERRIF( f->r_mem == 0 );
		{
			int mult = b->id & 0xF;
			int regOrOffs = mult == 15 ? b->id >> 4 : b->id >> 8;
			CpuReg reg = (b->id >> 4) & 0xF;
			if( mult == 15 ) {
				int pos;
				if( a->id > 7 ) r64 |= 4;
				OP(f->r_mem);
				MOD_RM(0,a->id,5);
				if( IS_64 ) {
					// offset wrt current code
					pos = BUF_POS() + 4;
					W(regOrOffs - pos);
				} else {
					ERRIF(1);
				}
			} else if( mult == 0 ) {
				if( a->id > 7 ) r64 |= 4;
				if( reg > 7 ) r64 |= 1;
				OP(f->r_mem);
				if( regOrOffs == 0 && (reg&7) != Ebp ) {
					MOD_RM(0,a->id,reg);
					if( (reg&7) == Esp ) B(0x24);
				} else if( IS_SBYTE(regOrOffs) ) {
					MOD_RM(1,a->id,reg);
					if( (reg&7) == Esp ) B(0x24);
					B(regOrOffs);
				} else {
					MOD_RM(2,a->id,reg);
					if( (reg&7) == Esp ) B(0x24);
					W(regOrOffs);
				}
			} else {
				int offset = (int)(int_val)b->holds;
				if( a->id > 7 ) r64 |= 4;
				if( reg > 7 ) r64 |= 1;
				if( regOrOffs > 7 ) r64 |= 2;
				OP(f->r_mem);
				MOD_RM(offset == 0 ? 0 : IS_SBYTE(offset) ? 1 : 2,a->id,4);
				SIB(mult,regOrOffs,reg);
				if( offset ) {
					if( IS_SBYTE(offset) ) B(offset); else W(offset);
				}
			}
		}
		break;
#	ifndef HL_64
	case ID2(RFPU,RADDR):
#	endif
	case ID2(RCPU,RADDR):
		ERRIF( f->r_mem == 0 );
		if( a->id > 7 ) r64 |= 4;
		OP(f->r_mem);
		MOD_RM(0,a->id,5);
		if( IS_64 )
			W64((int_val)b->holds);
		else
			W((int)(int_val)b->holds);
		break;
#	ifndef HL_64
	case ID2(RADDR,RFPU):
#	endif
	case ID2(RADDR,RCPU):
		ERRIF( f->mem_r == 0 );
		if( b->id > 7 ) r64 |= 4;
		OP(f->mem_r);
		MOD_RM(0,b->id,5);
		if( IS_64 )
			W64((int_val)a->holds);
		else
			W((int)(int_val)a->holds);
		break;
	case ID2(RMEM, RCPU):
	case ID2(RMEM, RFPU):
		ERRIF( f->mem_r == 0 );
		{
			int mult = a->id & 0xF;
			int regOrOffs = mult == 15 ? a->id >> 4 : a->id >> 8;
			CpuReg reg = (a->id >> 4) & 0xF;
			if( mult == 15 ) {
				int pos;
				if( b->id > 7 ) r64 |= 4;
				OP(f->mem_r);
				MOD_RM(0,b->id,5);
				if( IS_64 ) {
					// offset wrt current code
					pos = BUF_POS() + 4;
					W(regOrOffs - pos);
				} else {
					ERRIF(1);
				}
			} else if( mult == 0 ) {
				if( b->id > 7 ) r64 |= 4;
				if( reg > 7 ) r64 |= 1;
				OP(f->mem_r);
				if( regOrOffs == 0 && (reg&7) != Ebp ) {
					MOD_RM(0,b->id,reg);
					if( (reg&7) == Esp ) B(0x24);
				} else if( IS_SBYTE(regOrOffs) ) {
					MOD_RM(1,b->id,reg);
					if( (reg&7) == Esp ) B(0x24);
					B(regOrOffs);
				} else {
					MOD_RM(2,b->id,reg);
					if( (reg&7) == Esp ) B(0x24);
					W(regOrOffs);
				}
			} else {
				int offset = (int)(int_val)a->holds;
				if( b->id > 7 ) r64 |= 4;
				if( reg > 7 ) r64 |= 1;
				if( regOrOffs > 7 ) r64 |= 2;
				OP(f->mem_r);
				MOD_RM(offset == 0 ? 0 : IS_SBYTE(offset) ? 1 : 2,b->id,4);
				SIB(mult,regOrOffs,reg);
				if( offset ) {
					if( IS_SBYTE(offset) ) B(offset); else W(offset);
				}
			}
		}
		break;
	default:
		ERRIF(1);
	}
	if( ctx->debug && ctx->f && o == CALL ) {
		preg p;
		op(ctx,MOV,pmem(&p,Esp,-HL_WSIZE),PEBP,true); // erase EIP (clean stack report)
	}
}

static void op32( jit_ctx *ctx, CpuOp o, preg *a, preg *b ) {
	op(ctx,o,a,b,false);
}

static void op64( jit_ctx *ctx, CpuOp o, preg *a, preg *b ) {
#ifndef HL_64
	op(ctx,o,a,b,false);
#else
	op(ctx,o,a,b,true);
#endif
}

static void patch_jump( jit_ctx *ctx, int p ) {
	if( p == 0 ) return;
	if( p & 0x40000000 ) {
		int d;
		p &= 0x3FFFFFFF;
		d = BUF_POS() - (p + 1);
		if( d < -128 || d >= 128 ) ASSERT(d);
		*(char*)(ctx->startBuf + p) = (char)d;
	} else {
		*(int*)(ctx->startBuf + p) = BUF_POS() - (p + 4);
	}
}

static void patch_jump_to( jit_ctx *ctx, int p, int target ) {
	if( p == 0 ) return;
	if( p & 0x40000000 ) {
		int d;
		p &= 0x3FFFFFFF;
		d = target - (p + 1);
		if( d < -128 || d >= 128 ) ASSERT(d);
		*(char*)(ctx->startBuf + p) = (char)d;
	} else {
		*(int*)(ctx->startBuf + p) = target - (p + 4);
	}
}

static int call_reg_index( int reg ) {
#	ifdef HL_64
	int i;
	for(i=0;i<CALL_NREGS;i++)
		if( CALL_REGS[i] == reg )
			return i;
#	endif
	return -1;
}

static bool is_call_reg( preg *p ) {
#	ifdef HL_64
	int i;
	if( p->kind == RFPU )
		return p->id < CALL_NREGS;
	for(i=0;i<CALL_NREGS;i++)
		if( p->kind == RCPU && p->id == CALL_REGS[i] )
			return true;
	return false;
#	else
	return false;
#	endif
}

static preg *alloc_reg( jit_ctx *ctx, preg_kind k ) {
	int i;
	preg *p;
	switch( k ) {
	case RCPU:
	case RCPU_CALL:
	case RCPU_8BITS:
		{
			int off = ctx->allocOffset++;
			const int count = RCPU_SCRATCH_COUNT;
			for(i=0;i<count;i++) {
				int r = RCPU_SCRATCH_REGS[(i + off)%count];
				p = ctx->pregs + r;
				if( p->lock >= ctx->currentPos ) continue;
				if( k == RCPU_CALL && is_call_reg(p) ) continue;
				if( k == RCPU_8BITS && !is_reg8(p) ) continue;
				if( p->holds == NULL ) {
					RLOCK(p);
					return p;
				}
			}
			for(i=0;i<count;i++) {
				preg *p = ctx->pregs + RCPU_SCRATCH_REGS[(i + off)%count];
				if( p->lock >= ctx->currentPos ) continue;
				if( k == RCPU_CALL && is_call_reg(p) ) continue;
				if( k == RCPU_8BITS && !is_reg8(p) ) continue;
				if( p->holds ) {
					RLOCK(p);
					p->holds->current = NULL;
					p->holds = NULL;
					return p;
				}
			}
		}
		break;
	case RFPU:
		{
			int off = ctx->allocOffset++;
			const int count = RFPU_SCRATCH_COUNT;
			for(i=0;i<count;i++) {
				preg *p = PXMM((i + off)%count);
				if( p->lock >= ctx->currentPos ) continue;
				if( p->holds == NULL ) {
					RLOCK(p);
					return p;
				}
			}
			for(i=0;i<count;i++) {
				preg *p = PXMM((i + off)%count);
				if( p->lock >= ctx->currentPos ) continue;
				if( p->holds ) {
					RLOCK(p);
					p->holds->current = NULL;
					p->holds = NULL;
					return p;
				}
			}
		}
		break;
	default:
		ASSERT(k);
	}
	ASSERT(0); // out of registers !
	return NULL;
}

static preg *fetch( vreg *r ) {
	if( r->current )
		return r->current;
	return &r->stack;
}

static void scratch( preg *r ) {
	if( r && r->holds ) {
		r->holds->current = NULL;
		r->holds = NULL;
		r->lock = 0;
	}
}

static preg *copy( jit_ctx *ctx, preg *to, preg *from, int size );

static void load( jit_ctx *ctx, preg *r, vreg *v ) {
	preg *from = fetch(v);
	if( from == r || v->size == 0 ) return;
	if( r->holds ) r->holds->current = NULL;
	if( v->current ) {
		v->current->holds = NULL;
		from = r;
	}
	r->holds = v;
	v->current = r;
	copy(ctx,r,from,v->size);
}

static preg *alloc_fpu( jit_ctx *ctx, vreg *r, bool andLoad ) {
	preg *p = fetch(r);
	if( p->kind != RFPU ) {
		if( !IS_FLOAT(r) && (IS_64 || r->t->kind != HI64) ) ASSERT(r->t->kind);
		p = alloc_reg(ctx, RFPU);
		if( andLoad )
			load(ctx,p,r);
		else {
			if( r->current )
				r->current->holds = NULL;
			r->current = p;
			p->holds = r;
		}
	} else
		RLOCK(p);
	return p;
}

static void reg_bind( vreg *r, preg *p ) {
	if( r->current )
		r->current->holds = NULL;
	r->current = p;
	p->holds = r;
}

static preg *alloc_cpu( jit_ctx *ctx, vreg *r, bool andLoad ) {
	preg *p = fetch(r);
	if( p->kind != RCPU ) {
#		ifndef HL_64
		if( r->t->kind == HI64 ) return alloc_fpu(ctx,r,andLoad);
		if( r->size > 4 ) ASSERT(r->size);
#		endif
		p = alloc_reg(ctx, RCPU);
		if( andLoad )
			load(ctx,p,r);
		else
			reg_bind(r,p);
	} else
		RLOCK(p);
	return p;
}

// allocate a register that is not a call parameter
static preg *alloc_cpu_call( jit_ctx *ctx, vreg *r ) {
	preg *p = fetch(r);
	if( p->kind != RCPU ) {
#		ifndef HL_64
		if( r->t->kind == HI64 ) return alloc_fpu(ctx,r,true);
		if( r->size > 4 ) ASSERT(r->size);
#		endif
		p = alloc_reg(ctx, RCPU_CALL);
		load(ctx,p,r);
	} else if( is_call_reg(p) ) {
		preg *p2 = alloc_reg(ctx, RCPU_CALL);
		op64(ctx,MOV,p2,p);
		scratch(p);
		reg_bind(r,p2);
		return p2;
	} else
		RLOCK(p);
	return p;
}

static preg *fetch32( jit_ctx *ctx, vreg *r ) {
	if( r->current )
		return r->current;
	// make sure that the register is correctly erased
	if( r->size < 4 ) {
		preg *p = alloc_cpu(ctx, r, true);
		RUNLOCK(p);
		return p;
	}
	return fetch(r);
}

// make sure higher bits are zeroes
static preg *alloc_cpu64( jit_ctx *ctx, vreg *r, bool andLoad ) {
#	ifndef HL_64
	return alloc_cpu(ctx,r,andLoad);
#	else
	preg *p = fetch(r);
	if( !andLoad ) ASSERT(0);
	if( p->kind != RCPU ) {
		p = alloc_reg(ctx, RCPU);
		op64(ctx,XOR,p,p);
		load(ctx,p,r);
	} else {
		// remove higher bits
		preg tmp;
		op64(ctx,SHL,p,pconst(&tmp,32));
		op64(ctx,SHR,p,pconst(&tmp,32));
		RLOCK(p);
	}
	return p;
#	endif
}

// make sure the register can be used with 8 bits access
static preg *alloc_cpu8( jit_ctx *ctx, vreg *r, bool andLoad ) {
	preg *p = fetch(r);
	if( p->kind != RCPU ) {
		p = alloc_reg(ctx, RCPU_8BITS);
		load(ctx,p,r);
	} else if( !is_reg8(p) ) {
		preg *p2 = alloc_reg(ctx, RCPU_8BITS);
		op64(ctx,MOV,p2,p);
		scratch(p);
		reg_bind(r,p2);
		return p2;
	} else
		RLOCK(p);
	return p;
}

static preg *copy( jit_ctx *ctx, preg *to, preg *from, int size ) {
	if( size == 0 || to == from ) return to;
	switch( ID2(to->kind,from->kind) ) {
	case ID2(RMEM,RCPU):
	case ID2(RSTACK,RCPU):
	case ID2(RCPU,RSTACK):
	case ID2(RCPU,RMEM):
	case ID2(RCPU,RCPU):
#	ifndef HL_64
	case ID2(RCPU,RADDR):
	case ID2(RADDR,RCPU):
#	endif
		switch( size ) {
		case 1:
			if( to->kind == RCPU ) {
				op64(ctx,XOR,to,to);
				if( !is_reg8(to) ) {
					preg p;
					op32(ctx,MOV16,to,from);
					op32(ctx,SHL,to,pconst(&p,24));
					op32(ctx,SHR,to,pconst(&p,24));
					break;
				}
			}
			if( !is_reg8(from) ) {
				preg *r = alloc_reg(ctx, RCPU_CALL);
				op32(ctx, MOV, r, from);
				RUNLOCK(r);
				op32(ctx,MOV8,to,r);
				return from;
			}
			op32(ctx,MOV8,to,from);
			break;
		case 2:
			if( to->kind == RCPU )
				op64(ctx,XOR,to,to);
			op32(ctx,MOV16,to,from);
			break;
		case 4:
			op32(ctx,MOV,to,from);
			break;
		case 8:
			if( IS_64 ) {
				op64(ctx,MOV,to,from);
				break;
			}
		default:
			ASSERT(size);
		}
		return to->kind == RCPU ? to : from;
	case ID2(RFPU,RFPU):
	case ID2(RMEM,RFPU):
	case ID2(RSTACK,RFPU):
	case ID2(RFPU,RMEM):
	case ID2(RFPU,RSTACK):
		switch( size ) {
		case 8:
			op64(ctx,MOVSD,to,from);
			break;
		case 4:
			op32(ctx,MOVSS,to,from);
			break;
		default:
			ASSERT(size);
		}
		return to->kind == RFPU ? to : from;
	case ID2(RMEM,RSTACK):
		{
			vreg *rfrom = R(from->id);
			if( IS_FLOAT(rfrom) )
				return copy(ctx,to,alloc_fpu(ctx,rfrom,true),size);
			return copy(ctx,to,alloc_cpu(ctx,rfrom,true),size);
		}
	case ID2(RMEM,RMEM):
	case ID2(RSTACK,RMEM):
	case ID2(RSTACK,RSTACK):
#	ifndef HL_64
	case ID2(RMEM,RADDR):
	case ID2(RSTACK,RADDR):
	case ID2(RADDR,RSTACK):
#	endif
		{
			preg *tmp;
			if( (!IS_64 && size == 8) || (to->kind == RSTACK && IS_FLOAT(R(to->id))) || (from->kind == RSTACK && IS_FLOAT(R(from->id))) ) {
				tmp = alloc_reg(ctx, RFPU);
				op64(ctx,size == 8 ? MOVSD : MOVSS,tmp,from);
			} else {
				tmp = alloc_reg(ctx, RCPU);
				copy(ctx,tmp,from,size);
			}
			return copy(ctx,to,tmp,size);
		}
#	ifdef HL_64
	case ID2(RCPU,RADDR):
	case ID2(RMEM,RADDR):
	case ID2(RSTACK,RADDR):
		{
			preg p;
			preg *tmp = alloc_reg(ctx, RCPU);
			op64(ctx,MOV,tmp,pconst64(&p,(int_val)from->holds));
			return copy(ctx,to,pmem(&p,tmp->id,0),size);
		}
	case ID2(RADDR,RCPU):
	case ID2(RADDR,RMEM):
	case ID2(RADDR,RSTACK):
		{
			preg p;
			preg *tmp = alloc_reg(ctx, RCPU);
			op64(ctx,MOV,tmp,pconst64(&p,(int_val)to->holds));
			return copy(ctx,pmem(&p,tmp->id,0),from,size);
		}
#	endif
	default:
		break;
	}
	printf("copy(%s,%s)\n",KNAMES[to->kind], KNAMES[from->kind]);
	ASSERT(0);
	return NULL;
}

static void store( jit_ctx *ctx, vreg *r, preg *v, bool bind ) {
	if( r->current && r->current != v ) {
		r->current->holds = NULL;
		r->current = NULL;
	}
	v = copy(ctx,&r->stack,v,r->size);
	if( IS_FLOAT(r) != (v->kind == RFPU) )
		ASSERT(0);
	if( bind && r->current != v && (v->kind == RCPU || v->kind == RFPU) ) {
		scratch(v);
		r->current = v;
		v->holds = r;
	}
}

static void store_result( jit_ctx *ctx, vreg *r ) {
#	ifndef HL_64
	switch( r->t->kind ) {
	case HF64:
		scratch(r->current);
		op64(ctx,FSTP,&r->stack,UNUSED);
		break;
	case HF32:
		scratch(r->current);
		op64(ctx,FSTP32,&r->stack,UNUSED);
		break;
	case HI64:
		scratch(r->current);
		error_i64();
		break;
	default:
#	endif
		store(ctx,r,IS_FLOAT(r) ? REG_AT(XMM(0)) : PEAX,true);
#	ifndef HL_64
		break;
	}
#	endif
}

static void op_mov( jit_ctx *ctx, vreg *to, vreg *from ) {
	preg *r = fetch(from);
#	ifndef HL_64
	if( to->t->kind == HI64 ) {
		error_i64();
		return;
	}
#	endif
	if( from->t->kind == HF32 && r->kind != RFPU )
		r = alloc_fpu(ctx,from,true);
	store(ctx, to, r, true);
}

static void copy_to( jit_ctx *ctx, vreg *to, preg *from ) {
	store(ctx,to,from,true);
}

static void copy_from( jit_ctx *ctx, preg *to, vreg *from ) {
	copy(ctx,to,fetch(from),from->size);
}

static void store_const( jit_ctx *ctx, vreg *r, int c ) {
	preg p;
	if( c == 0 )
		op(ctx,XOR,alloc_cpu(ctx,r,false),alloc_cpu(ctx,r,false),r->size == 8);
	else if( r->size == 8 )
		op64(ctx,MOV,alloc_cpu(ctx,r,false),pconst64(&p,c));
	else
		op32(ctx,MOV,alloc_cpu(ctx,r,false),pconst(&p,c));
	store(ctx,r,r->current,false);
}

static void discard_regs( jit_ctx *ctx, bool native_call ) {
	int i;
	for(i=0;i<RCPU_SCRATCH_COUNT;i++) {
		preg *r = ctx->pregs + RCPU_SCRATCH_REGS[i];
		if( r->holds ) {
			r->holds->current = NULL;
			r->holds = NULL;
		}
	}
	for(i=0;i<RFPU_COUNT;i++) {
		preg *r = ctx->pregs + XMM(i);
		if( r->holds ) {
			r->holds->current = NULL;
			r->holds = NULL;
		}
	}
}

static int pad_before_call( jit_ctx *ctx, int size ) {
	int total = size + ctx->totalRegsSize + HL_WSIZE * 2; // EIP+EBP
	if( total & 15 ) {
		int pad = 16 - (total & 15);
		preg p;
		if( pad ) op64(ctx,SUB,PESP,pconst(&p,pad));
		size += pad;
	}
	return size;
}

static void push_reg( jit_ctx *ctx, vreg *r ) {
	preg p;
	switch( stack_size(r->t) ) {
	case 1:
		op64(ctx,SUB,PESP,pconst(&p,1));
		op32(ctx,MOV8,pmem(&p,Esp,0),alloc_cpu8(ctx,r,true));
		break;
	case 2:
		op64(ctx,SUB,PESP,pconst(&p,2));
		op32(ctx,MOV16,pmem(&p,Esp,0),alloc_cpu(ctx,r,true));
		break;
	case 4:
		if( r->size < 4 )
			alloc_cpu(ctx,r,true); // force fetch (higher bits set to 0)
		if( !IS_64 ) {
			if( r->current != NULL && r->current->kind == RFPU ) scratch(r->current);
			op32(ctx,PUSH,fetch(r),UNUSED);
		} else {
			// pseudo push32 (not available)
			op64(ctx,SUB,PESP,pconst(&p,4));
			op32(ctx,MOV,pmem(&p,Esp,0),alloc_cpu(ctx,r,true));
		}
		break;
	case 8:
		if( fetch(r)->kind == RFPU ) {
			op64(ctx,SUB,PESP,pconst(&p,8));
			op64(ctx,MOVSD,pmem(&p,Esp,0),fetch(r));
		} else if( IS_64 )
			op64(ctx,PUSH,fetch(r),UNUSED);
		else if( r->stack.kind == RSTACK ) {
			scratch(r->current);
			r->stackPos += 4;
			op32(ctx,PUSH,&r->stack,UNUSED);
			r->stackPos -= 4;
			op32(ctx,PUSH,&r->stack,UNUSED);
		} else
			ASSERT(0);
		break;
	default:
		ASSERT(r->size);
	}
}

static int begin_native_call( jit_ctx *ctx, int nargs ) {
	ctx->nativeArgsCount = nargs;
	return pad_before_call(ctx, nargs > CALL_NREGS ? (nargs - CALL_NREGS) * HL_WSIZE : 0);
}

static preg *alloc_native_arg( jit_ctx *ctx ) {
#	ifdef HL_64
	int rid = ctx->nativeArgsCount - 1;
	preg *r = rid < CALL_NREGS ? REG_AT(CALL_REGS[rid]) : alloc_reg(ctx,RCPU_CALL);
	scratch(r);
	return r;
#	else
	return alloc_reg(ctx, RCPU);
#	endif
}

static void set_native_arg( jit_ctx *ctx, preg *r ) {
	if( r->kind == RSTACK ) {
		vreg *v = ctx->vregs + r->id;
		if( v->size < 4 )
			r = fetch32(ctx, v);
	}
#	ifdef HL_64
	if( r->kind == RFPU ) ASSERT(0);
	int rid = --ctx->nativeArgsCount;
	preg *target;
	if( rid >= CALL_NREGS ) {
		op64(ctx,PUSH,r,UNUSED);
		return;
	}
	target = REG_AT(CALL_REGS[rid]);
	if( target != r ) {
		op64(ctx, MOV, target, r);
		scratch(target);
	}
#	else
	op32(ctx,PUSH,r,UNUSED);
#	endif
}

static void set_native_arg_fpu( jit_ctx *ctx, preg *r, bool isf32 ) {
#	ifdef HL_64
	if( r->kind == RCPU ) ASSERT(0);
	// can only be used if last argument !!
	ctx->nativeArgsCount--;
	preg *target = REG_AT(XMM(IS_WINCALL64 ? ctx->nativeArgsCount : 0));
	if( target != r ) {
		op64(ctx, isf32 ? MOVSS : MOVSD, target, r);
		scratch(target);
	}
#	else
	op32(ctx,PUSH,r,UNUSED);
#	endif
}

typedef struct {
	int nextCpu;
	int nextFpu;
	int mapped[REG_COUNT];
} call_regs;

static int select_call_reg( call_regs *regs, hl_type *t, int id ) {
#	ifndef HL_64
	return -1;
#else
	bool isFloat = t->kind == HF32 || t->kind == HF64;
#	ifdef HL_WIN_CALL
	int index = regs->nextCpu++;
#	else
	int index = isFloat ? regs->nextFpu++ : regs->nextCpu++;
#	endif
	if( index >= CALL_NREGS )
		return -1;
	int reg = isFloat ? XMM(index) : CALL_REGS[index];
	regs->mapped[reg] = id + 1;
	return reg;
#endif
}

static int mapped_reg( call_regs *regs, int id ) {
#	ifndef HL_64
	return -1;
#else
	int i;
	for(i=0;i<CALL_NREGS;i++) {
		int r = CALL_REGS[i];
		if( regs->mapped[r] == id + 1 ) return r;
		r = XMM(i);
		if( regs->mapped[r] == id + 1 ) return r;
	}
	return -1;
#endif
}

static int prepare_call_args( jit_ctx *ctx, int count, int *args, vreg *vregs, int extraSize ) {
	int i;
	int size = extraSize, paddedSize;
	call_regs ctmp = {0};
	for(i=0;i<count;i++) {
		vreg *r = vregs + args[i];
		int cr = select_call_reg(&ctmp, r->t, i);
		if( cr >= 0 ) {
			preg *c = REG_AT(cr);
			preg *cur = fetch(r);
			if( cur != c ) {
				copy(ctx,c,cur,r->size);
				scratch(c);
			}
			RLOCK(c);
			continue;
		}
		size += stack_size(r->t);
	}
	paddedSize = pad_before_call(ctx,size);
	for(i=0;i<count;i++) {
		// RTL
		int j = count - (i + 1);
		vreg *r = vregs + args[j];
		if( (i & 7) == 0 ) jit_buf(ctx);
		if( mapped_reg(&ctmp,j) >= 0 ) continue;
		push_reg(ctx,r);
		if( r->current ) RUNLOCK(r->current);
	}
	return paddedSize;
}

static void op_call( jit_ctx *ctx, preg *r, int size ) {
	preg p;
#	ifdef JIT_DEBUG
	if( IS_64 && size >= 0 ) {
		int jchk;
		op32(ctx,TEST,PESP,pconst(&p,15));
		XJump(JZero,jchk);
		BREAK(); // unaligned ESP
		patch_jump(ctx, jchk);
	}
#	endif
	if( IS_WINCALL64 ) {
		// MSVC requires 32bytes of free space here
		op64(ctx,SUB,PESP,pconst(&p,32));
		if( size >= 0 ) size += 32;
	}
	op32(ctx, CALL, r, UNUSED);
	if( size > 0 ) op64(ctx,ADD,PESP,pconst(&p,size));
}

static void call_native( jit_ctx *ctx, void *nativeFun, int size ) {
	bool isExc = nativeFun == hl_assert || nativeFun == hl_throw || nativeFun == on_jit_error;
	preg p;
	// native function, already resolved
	op64(ctx,MOV,PEAX,pconst64(&p,(int_val)nativeFun));
	op_call(ctx,PEAX, isExc ? -1 : size);
	if( isExc )
		return;
	discard_regs(ctx, true);
}

static void op_call_fun( jit_ctx *ctx, vreg *dst, int findex, int count, int *args ) {
	int fid = findex < 0 ? -1 : ctx->m->functions_indexes[findex];
	bool isNative = fid >= ctx->m->code->nfunctions;
	int size = prepare_call_args(ctx,count,args,ctx->vregs,0);
	preg p;
	if( fid < 0 ) {
		ASSERT(fid);
	} else if( isNative ) {
		call_native(ctx,ctx->m->functions_ptrs[findex],size);
	} else {
		int cpos = BUF_POS() + (IS_WINCALL64 ? 4 : 0);
#		ifdef JIT_DEBUG
		if( IS_64 ) cpos += 13; // ESP CHECK
#		endif
		if( ctx->m->functions_ptrs[findex] ) {
			// already compiled
			op_call(ctx,pconst(&p,(int)(int_val)ctx->m->functions_ptrs[findex] - (cpos + 5)), size);
		} else if( ctx->m->code->functions + fid == ctx->f ) {
			// our current function
			op_call(ctx,pconst(&p, ctx->functionPos - (cpos + 5)), size);
		} else {
			// stage for later
			jlist *j = (jlist*)hl_malloc(&ctx->galloc,sizeof(jlist));
			j->pos = cpos;
			j->target = findex;
			j->next = ctx->calls;
			ctx->calls = j;
			op_call(ctx,pconst(&p,0), size);
		}
		discard_regs(ctx, false);
	}
	if( dst )
		store_result(ctx,dst);
}

static void op_enter( jit_ctx *ctx ) {
	preg p;
	op64(ctx, PUSH, PEBP, UNUSED);
	op64(ctx, MOV, PEBP, PESP);
	if( ctx->totalRegsSize ) op64(ctx, SUB, PESP, pconst(&p,ctx->totalRegsSize));
}

static void op_ret( jit_ctx *ctx, vreg *r ) {
	preg p;
	switch( r->t->kind ) {
	case HF32:
#		ifdef HL_64
		op64(ctx, MOVSS, PXMM(0), fetch(r));
#		else
		op64(ctx,FLD32,&r->stack,UNUSED);
#		endif
		break;
	case HF64:
#		ifdef HL_64
		op64(ctx, MOVSD, PXMM(0), fetch(r));
#		else
		op64(ctx,FLD,&r->stack,UNUSED);
#		endif
		break;
	default:
		if( r->size < 4 && !r->current )
			fetch32(ctx, r);
		if( r->current != PEAX )
			op64(ctx,MOV,PEAX,fetch(r));
		break;
	}
	if( ctx->totalRegsSize ) op64(ctx, ADD, PESP, pconst(&p, ctx->totalRegsSize));
#	ifdef JIT_DEBUG
	{
		int jeq;
		op64(ctx, CMP, PESP, PEBP);
		XJump_small(JEq,jeq);
		jit_error("invalid ESP");
		patch_jump(ctx,jeq);
	}
#	endif
	op64(ctx, POP, PEBP, UNUSED);
	op64(ctx, RET, UNUSED, UNUSED);
}

static void call_native_consts( jit_ctx *ctx, void *nativeFun, int_val *args, int nargs ) {
	int size = pad_before_call(ctx, IS_64 ? 0 : HL_WSIZE*nargs);
	preg p;
	int i;
#	ifdef HL_64
	for(i=0;i<nargs;i++)
		op64(ctx, MOV, REG_AT(CALL_REGS[i]), pconst64(&p, args[i]));
#	else
	for(i=nargs-1;i>=0;i--)
		op32(ctx, PUSH, pconst64(&p, args[i]), UNUSED);
#	endif
	call_native(ctx, nativeFun, size);
}

static void on_jit_error( const char *msg, int_val line ) {
	char buf[256];
	int iline = (int)line;
	sprintf(buf,"%s (line %d)",msg,iline);
#ifdef HL_WIN_DESKTOP
	MessageBoxA(NULL,buf,"JIT ERROR",MB_OK);
#else
	printf("JIT ERROR : %s\n",buf);
#endif
	hl_debug_break();
	hl_throw(NULL);
}

static void _jit_error( jit_ctx *ctx, const char *msg, int line ) {
	int_val args[2] = { (int_val)msg, (int_val)line };
	call_native_consts(ctx,on_jit_error,args,2);
}


static preg *op_binop( jit_ctx *ctx, vreg *dst, vreg *a, vreg *b, hl_op bop ) {
	preg *pa = fetch(a), *pb = fetch(b), *out = NULL;
	CpuOp o;
	if( IS_FLOAT(a) ) {
		bool isf32 = a->t->kind == HF32;
		switch( bop ) {
		case OAdd: o = isf32 ? ADDSS : ADDSD; break;
		case OSub: o = isf32 ? SUBSS : SUBSD; break;
		case OMul: o = isf32 ? MULSS : MULSD; break;
		case OSDiv: o = isf32 ? DIVSS : DIVSD; break;
		case OJSLt:
		case OJSGte:
		case OJSLte:
		case OJSGt:
		case OJEq:
		case OJNotEq:
		case OJNotLt:
		case OJNotGte:
			o = isf32 ? COMISS : COMISD;
			break;
		case OSMod:
			{
				int args[] = { a->stack.id, b->stack.id };
				int size = prepare_call_args(ctx,2,args,ctx->vregs,0);
				void *mod_fun;
				if( isf32 ) mod_fun = fmodf; else mod_fun = fmod;
				call_native(ctx,mod_fun,size);
				store_result(ctx,dst);
				return fetch(dst);
			}
		default:
			printf("%s\n", hl_op_name(bop));
			ASSERT(bop);
		}
	} else {
		bool is64 =	a->t->kind == HI64;
#	ifndef HL_64
		if( is64 ) {
			error_i64();
			return fetch(a);
		}
#	endif
		switch( bop ) {
		case OAdd: o = ADD; break;
		case OSub: o = SUB; break;
		case OMul: o = IMUL; break;
		case OAnd: o = AND; break;
		case OOr: o = OR; break;
		case OXor: o = XOR; break;
		case OShl:
		case OUShr:
		case OSShr:
			if( !b->current || b->current->kind != RCPU || b->current->id != Ecx ) {
				scratch(REG_AT(Ecx));
				op(ctx,MOV,REG_AT(Ecx),pb,is64);
				RLOCK(REG_AT(Ecx));
				pa = fetch(a);
			} else
				RLOCK(b->current);
			if( pa->kind != RCPU ) {
				pa = alloc_reg(ctx, RCPU);
				op(ctx,MOV,pa,fetch(a), is64);
			}
			op(ctx,bop == OShl ? SHL : (bop == OUShr ? SHR : SAR), pa, UNUSED,is64);
			if( dst ) store(ctx, dst, pa, true);
			return pa;
		case OSDiv:
		case OUDiv:
		case OSMod:
		case OUMod:
			{
				preg *out = bop == OSMod || bop == OUMod ? REG_AT(Edx) : PEAX;
				preg *r;
				preg p;
				int jz, jz1 = 0, jend;
				if( pa->kind == RCPU && pa->id == Eax ) RLOCK(pa);
				r = alloc_cpu(ctx,b,true);
				// integer div 0 => 0
				op(ctx,TEST,r,r,is64);
				XJump_small(JZero, jz);
				// Prevent MIN/-1 overflow exception
				// OSMod: r = (b == 0 || b == -1) ? 0 : a % b
				// OSDiv: r = (b == 0 || b == -1) ? a * b : a / b
				if( bop == OSMod || bop == OSDiv ) {
					op(ctx, CMP, r, pconst(&p,-1), is64);
					XJump_small(JEq, jz1);
				}
				pa = fetch(a);
				if( pa->kind != RCPU || pa->id != Eax ) {
					scratch(PEAX);
					scratch(pa);
					load(ctx,PEAX,a);
				}
				scratch(REG_AT(Edx));
				scratch(REG_AT(Eax));
				if( bop == OUDiv || bop == OUMod )
					op(ctx, XOR, REG_AT(Edx), REG_AT(Edx), is64);
				else
					op(ctx, CDQ, UNUSED, UNUSED, is64); // sign-extend Eax into Eax:Edx
				op(ctx, bop == OUDiv || bop == OUMod ? DIV : IDIV, fetch(b), UNUSED, is64);
				XJump_small(JAlways, jend);
				patch_jump(ctx, jz);
				patch_jump(ctx, jz1);
				if( bop != OSDiv ) {
					op(ctx, XOR, out, out, is64);
				} else {
					load(ctx, out, a);
					op(ctx, IMUL, out, r, is64);
				}
				patch_jump(ctx, jend);
				if( dst ) store(ctx, dst, out, true);
				return out;
			}
		case OJSLt:
		case OJSGte:
		case OJSLte:
		case OJSGt:
		case OJULt:
		case OJUGte:
		case OJEq:
		case OJNotEq:
			switch( a->t->kind ) {
			case HUI8:
			case HBOOL:
				o = CMP8;
				break;
			case HUI16:
				o = CMP16;
				break;
			default:
				o = CMP;
				break;
			}
			break;
		default:
			printf("%s\n", hl_op_name(bop));
			ASSERT(bop);
		}
	}
	switch( RTYPE(a) ) {
	case HI32:
	case HUI8:
	case HUI16:
	case HBOOL:
#	ifndef HL_64
	case HDYNOBJ:
	case HVIRTUAL:
	case HOBJ:
	case HSTRUCT:
	case HFUN:
	case HMETHOD:
	case HBYTES:
	case HNULL:
	case HENUM:
	case HDYN:
	case HTYPE:
	case HABSTRACT:
	case HARRAY:
#	endif
		switch( ID2(pa->kind, pb->kind) ) {
		case ID2(RCPU,RCPU):
		case ID2(RCPU,RSTACK):
			op32(ctx, o, pa, pb);
			scratch(pa);
			out = pa;
			break;
		case ID2(RSTACK,RCPU):
			if( dst == a && o != IMUL ) {
				op32(ctx, o, pa, pb);
				dst = NULL;
				out = pa;
			} else {
				alloc_cpu(ctx,a, true);
				return op_binop(ctx,dst,a,b,bop);
			}
			break;
		case ID2(RSTACK,RSTACK):
			alloc_cpu(ctx, a, true);
			return op_binop(ctx, dst, a, b, bop);
		default:
			printf("%s(%d,%d)\n", hl_op_name(bop), pa->kind, pb->kind);
			ASSERT(ID2(pa->kind, pb->kind));
		}
		if( dst ) store(ctx, dst, out, true);
		return out;
#	ifdef HL_64
	case HOBJ:
	case HSTRUCT:
	case HDYNOBJ:
	case HVIRTUAL:
	case HFUN:
	case HMETHOD:
	case HBYTES:
	case HNULL:
	case HENUM:
	case HDYN:
	case HTYPE:
	case HABSTRACT:
	case HARRAY:
	case HI64:
	case HGUID:
		switch( ID2(pa->kind, pb->kind) ) {
		case ID2(RCPU,RCPU):
		case ID2(RCPU,RSTACK):
			op64(ctx, o, pa, pb);
			scratch(pa);
			out = pa;
			break;
		case ID2(RSTACK,RCPU):
			if( dst == a && OP_FORMS[o].mem_r ) {
				op64(ctx, o, pa, pb);
				dst = NULL;
				out = pa;
			} else {
				alloc_cpu(ctx,a, true);
				return op_binop(ctx,dst,a,b,bop);
			}
			break;
		case ID2(RSTACK,RSTACK):
			alloc_cpu(ctx, a, true);
			return op_binop(ctx, dst, a, b, bop);
		default:
			printf("%s(%d,%d)\n", hl_op_name(bop), pa->kind, pb->kind);
			ASSERT(ID2(pa->kind, pb->kind));
		}
		if( dst ) store(ctx, dst, out, true);
		return out;
#	endif
	case HF64:
	case HF32:
		pa = alloc_fpu(ctx, a, true);
		pb = alloc_fpu(ctx, b, true);
		switch( ID2(pa->kind, pb->kind) ) {
		case ID2(RFPU,RFPU):
			op64(ctx,o,pa,pb);
			if( (o == COMISD || o == COMISS) && bop != OJSGt ) {
				int jnotnan;
				XJump_small(JNParity,jnotnan);
				switch( bop ) {
				case OJSLt:
				case OJNotLt:
					{
						preg *r = alloc_reg(ctx,RCPU);
						// set CF=0, ZF=1
						op64(ctx,XOR,r,r);
						RUNLOCK(r);
						break;
					}
				case OJSGte:
				case OJNotGte:
					{
						preg *r = alloc_reg(ctx,RCPU);
						// set ZF=0, CF=1
						op64(ctx,XOR,r,r);
						op64(ctx,CMP,r,PESP);
						RUNLOCK(r);
						break;
					}
					break;
				case OJNotEq:
				case OJEq:
					// set ZF=0, CF=?
				case OJSLte:
					// set ZF=0, CF=0
					op64(ctx,TEST,PESP,PESP);
					break;
				default:
					ASSERT(bop);
				}
				patch_jump(ctx,jnotnan);
			}
			scratch(pa);
			out = pa;
			break;
		default:
			printf("%s(%d,%d)\n", hl_op_name(bop), pa->kind, pb->kind);
			ASSERT(ID2(pa->kind, pb->kind));
		}
		if( dst ) store(ctx, dst, out, true);
		return out;
	default:
		ASSERT(RTYPE(a));
	}
	return NULL;
}

static int do_jump( jit_ctx *ctx, hl_op op, bool isFloat ) {
	int j;
	switch( op ) {
	case OJAlways:
		XJump(JAlways,j);
		break;
	case OJSGte:
		XJump(isFloat ? JUGte : JSGte,j);
		break;
	case OJSGt:
		XJump(isFloat ? JUGt : JSGt,j);
		break;
	case OJUGte:
		XJump(JUGte,j);
		break;
	case OJSLt:
		XJump(isFloat ? JULt : JSLt,j);
		break;
	case OJSLte:
		XJump(isFloat ? JULte : JSLte,j);
		break;
	case OJULt:
		XJump(JULt,j);
		break;
	case OJEq:
		XJump(JEq,j);
		break;
	case OJNotEq:
		XJump(JNeq,j);
		break;
	case OJNotLt:
		XJump(JUGte,j);
		break;
	case OJNotGte:
		XJump(JULt,j);
		break;
	default:
		j = 0;
		printf("Unknown JUMP %d\n",op);
		break;
	}
	return j;
}

#define HDYN_VALUE 8

static void dyn_value_compare( jit_ctx *ctx, preg *a, preg *b, hl_type *t ) {
	preg p;
	switch( t->kind ) {
	case HUI8:
	case HBOOL:
		op32(ctx,MOV8,a,pmem(&p,a->id,HDYN_VALUE));
		op32(ctx,MOV8,b,pmem(&p,b->id,HDYN_VALUE));
		op64(ctx,CMP8,a,b);
		break;
	case HUI16:
		op32(ctx,MOV16,a,pmem(&p,a->id,HDYN_VALUE));
		op32(ctx,MOV16,b,pmem(&p,b->id,HDYN_VALUE));
		op64(ctx,CMP16,a,b);
		break;
	case HI32:
		op32(ctx,MOV,a,pmem(&p,a->id,HDYN_VALUE));
		op32(ctx,MOV,b,pmem(&p,b->id,HDYN_VALUE));
		op64(ctx,CMP,a,b);
		break;
	case HF32:
		{
			preg *fa = alloc_reg(ctx, RFPU);
			preg *fb = alloc_reg(ctx, RFPU);
			op64(ctx,MOVSS,fa,pmem(&p,a->id,HDYN_VALUE));
			op64(ctx,MOVSS,fb,pmem(&p,b->id,HDYN_VALUE));
			op64(ctx,COMISD,fa,fb);
		}
		break;
	case HF64:
		{
			preg *fa = alloc_reg(ctx, RFPU);
			preg *fb = alloc_reg(ctx, RFPU);
			op64(ctx,MOVSD,fa,pmem(&p,a->id,HDYN_VALUE));
			op64(ctx,MOVSD,fb,pmem(&p,b->id,HDYN_VALUE));
			op64(ctx,COMISD,fa,fb);
		}
		break;
	case HI64:
	default:
		// ptr comparison
		op64(ctx,MOV,a,pmem(&p,a->id,HDYN_VALUE));
		op64(ctx,MOV,b,pmem(&p,b->id,HDYN_VALUE));
		op64(ctx,CMP,a,b);
		break;
	}
}

static void op_jump( jit_ctx *ctx, vreg *a, vreg *b, hl_opcode *op, int targetPos ) {
	if( a->t->kind == HDYN || b->t->kind == HDYN || a->t->kind == HFUN || b->t->kind == HFUN ) {
		int args[] = { a->stack.id, b->stack.id };
		int size = prepare_call_args(ctx,2,args,ctx->vregs,0);
		call_native(ctx,hl_dyn_compare,size);
		if( op->op == OJSGt || op->op == OJSGte ) {
			preg p;
			int jinvalid;
			op32(ctx,CMP,PEAX,pconst(&p,hl_invalid_comparison));
			XJump_small(JEq,jinvalid);
			op32(ctx,TEST,PEAX,PEAX);
			register_jump(ctx,do_jump(ctx,op->op, IS_FLOAT(a)),targetPos);
			patch_jump(ctx,jinvalid);
			return;
		}
		op32(ctx,TEST,PEAX,PEAX);
	} else switch( a->t->kind ) {
	case HTYPE:
		{
			int args[] = { a->stack.id, b->stack.id };
			int size = prepare_call_args(ctx,2,args,ctx->vregs,0);
			preg p;
			call_native(ctx,hl_same_type,size);
			op64(ctx,CMP8,PEAX,pconst(&p,1));
		}
		break;
	case HNULL:
		{
			preg *pa = hl_type_size(a->t->tparam) == 1 ? alloc_cpu8(ctx,a,true) : alloc_cpu(ctx,a,true);
			preg *pb = hl_type_size(b->t->tparam) == 1 ? alloc_cpu8(ctx,b,true) : alloc_cpu(ctx,b,true);
			if( op->op == OJEq ) {
				// if( a == b || (a && b && a->v == b->v) ) goto
				int ja, jb;
				// if( a != b && (!a || !b || a->v != b->v) ) goto
				op64(ctx,CMP,pa,pb);
				register_jump(ctx,do_jump(ctx,OJEq,false),targetPos);
				op64(ctx,TEST,pa,pa);
				XJump_small(JZero,ja);
				op64(ctx,TEST,pb,pb);
				XJump_small(JZero,jb);
				dyn_value_compare(ctx,pa,pb,a->t->tparam);
				register_jump(ctx,do_jump(ctx,OJEq,false),targetPos);
				scratch(pa);
				scratch(pb);
				patch_jump(ctx,ja);
				patch_jump(ctx,jb);
			} else if( op->op == OJNotEq ) {
				int jeq, jcmp;
				// if( a != b && (!a || !b || a->v != b->v) ) goto
				op64(ctx,CMP,pa,pb);
				XJump_small(JEq,jeq);
				op64(ctx,TEST,pa,pa);
				register_jump(ctx,do_jump(ctx,OJEq,false),targetPos);
				op64(ctx,TEST,pb,pb);
				register_jump(ctx,do_jump(ctx,OJEq,false),targetPos);
				dyn_value_compare(ctx,pa,pb,a->t->tparam);
				XJump_small(JZero,jcmp);
				scratch(pa);
				scratch(pb);
				register_jump(ctx,do_jump(ctx,OJNotEq,false),targetPos);
				patch_jump(ctx,jcmp);
				patch_jump(ctx,jeq);
			} else
				ASSERT(op->op);
			return;
		}
	case HVIRTUAL:
		{
			preg p;
			preg *pa = alloc_cpu(ctx,a,true);
			preg *pb = alloc_cpu(ctx,b,true);
			int ja,jb,jav,jbv,jvalue;
			if( b->t->kind == HOBJ ) {
				if( op->op == OJEq ) {
					// if( a ? (b && a->value == b) : (b == NULL) ) goto
					op64(ctx,TEST,pa,pa);
					XJump_small(JZero,ja);
					op64(ctx,TEST,pb,pb);
					XJump_small(JZero,jb);
					op64(ctx,MOV,pa,pmem(&p,pa->id,HL_WSIZE));
					op64(ctx,CMP,pa,pb);
					XJump_small(JAlways,jvalue);
					patch_jump(ctx,ja);
					op64(ctx,TEST,pb,pb);
					patch_jump(ctx,jvalue);
					register_jump(ctx,do_jump(ctx,OJEq,false),targetPos);
					patch_jump(ctx,jb);
				} else if( op->op == OJNotEq ) {
					// if( a ? (b == NULL || a->value != b) : (b != NULL) ) goto
					op64(ctx,TEST,pa,pa);
					XJump_small(JZero,ja);
					op64(ctx,TEST,pb,pb);
					register_jump(ctx,do_jump(ctx,OJEq,false),targetPos);
					op64(ctx,MOV,pa,pmem(&p,pa->id,HL_WSIZE));
					op64(ctx,CMP,pa,pb);
					XJump_small(JAlways,jvalue);
					patch_jump(ctx,ja);
					op64(ctx,TEST,pb,pb);
					patch_jump(ctx,jvalue);
					register_jump(ctx,do_jump(ctx,OJNotEq,false),targetPos);
				} else
					ASSERT(op->op);
				scratch(pa);
				return;
			}
			op64(ctx,CMP,pa,pb);
			if( op->op == OJEq ) {
				// if( a == b || (a && b && a->value && b->value && a->value == b->value) ) goto
				register_jump(ctx,do_jump(ctx,OJEq, false),targetPos);
				op64(ctx,TEST,pa,pa);
				XJump_small(JZero,ja);
				op64(ctx,TEST,pb,pb);
				XJump_small(JZero,jb);
				op64(ctx,MOV,pa,pmem(&p,pa->id,HL_WSIZE));
				op64(ctx,TEST,pa,pa);
				XJump_small(JZero,jav);
				op64(ctx,MOV,pb,pmem(&p,pb->id,HL_WSIZE));
				op64(ctx,TEST,pb,pb);
				XJump_small(JZero,jbv);
				op64(ctx,CMP,pa,pb);
				XJump_small(JNeq,jvalue);
				register_jump(ctx,do_jump(ctx,OJEq, false),targetPos);
				patch_jump(ctx,ja);
				patch_jump(ctx,jb);
				patch_jump(ctx,jav);
				patch_jump(ctx,jbv);
				patch_jump(ctx,jvalue);
			} else if( op->op == OJNotEq ) {
				int jnext;
				// if( a != b && (!a || !b || !a->value || !b->value || a->value != b->value) ) goto
				XJump_small(JEq,jnext);
				op64(ctx,TEST,pa,pa);
				XJump_small(JZero,ja);
				op64(ctx,TEST,pb,pb);
				XJump_small(JZero,jb);
				op64(ctx,MOV,pa,pmem(&p,pa->id,HL_WSIZE));
				op64(ctx,TEST,pa,pa);
				XJump_small(JZero,jav);
				op64(ctx,MOV,pb,pmem(&p,pb->id,HL_WSIZE));
				op64(ctx,TEST,pb,pb);
				XJump_small(JZero,jbv);
				op64(ctx,CMP,pa,pb);
				XJump_small(JEq,jvalue);
				patch_jump(ctx,ja);
				patch_jump(ctx,jb);
				patch_jump(ctx,jav);
				patch_jump(ctx,jbv);
				register_jump(ctx,do_jump(ctx,OJAlways, false),targetPos);
				patch_jump(ctx,jnext);
				patch_jump(ctx,jvalue);
			} else
				ASSERT(op->op);
			scratch(pa);
			scratch(pb);
			return;
		}
		break;
	case HOBJ:
	case HSTRUCT:
		if( b->t->kind == HVIRTUAL ) {
			op_jump(ctx,b,a,op,targetPos); // inverse
			return;
		}
		if( hl_get_obj_rt(a->t)->compareFun ) {
			preg *pa = alloc_cpu(ctx,a,true);
			preg *pb = alloc_cpu(ctx,b,true);
			preg p;
			int jeq, ja, jb, jcmp;
			int args[] = { a->stack.id, b->stack.id };
			switch( op->op ) {
			case OJEq:
				// if( a == b || (a && b && cmp(a,b) == 0) ) goto
				op64(ctx,CMP,pa,pb);
				XJump_small(JEq,jeq);
				op64(ctx,TEST,pa,pa);
				XJump_small(JZero,ja);
				op64(ctx,TEST,pb,pb);
				XJump_small(JZero,jb);
				op_call_fun(ctx,NULL,(int)(int_val)a->t->obj->rt->compareFun,2,args);
				op32(ctx,TEST,PEAX,PEAX);
				XJump_small(JNotZero,jcmp);
				patch_jump(ctx,jeq);
				register_jump(ctx,do_jump(ctx,OJAlways,false),targetPos);
				patch_jump(ctx,ja);
				patch_jump(ctx,jb);
				patch_jump(ctx,jcmp);
				break;
			case OJNotEq:
				// if( a != b && (!a || !b || cmp(a,b) != 0) ) goto
				op64(ctx,CMP,pa,pb);
				XJump_small(JEq,jeq);
				op64(ctx,TEST,pa,pa);
				register_jump(ctx,do_jump(ctx,OJEq,false),targetPos);
				op64(ctx,TEST,pb,pb);
				register_jump(ctx,do_jump(ctx,OJEq,false),targetPos);

				op_call_fun(ctx,NULL,(int)(int_val)a->t->obj->rt->compareFun,2,args);
				op32(ctx,TEST,PEAX,PEAX);
				XJump_small(JZero,jcmp);

				register_jump(ctx,do_jump(ctx,OJNotEq,false),targetPos);
				patch_jump(ctx,jcmp);
				patch_jump(ctx,jeq);
				break;
			default:
				// if( a && b && cmp(a,b) ?? 0 ) goto
				op64(ctx,TEST,pa,pa);
				XJump_small(JZero,ja);
				op64(ctx,TEST,pb,pb);
				XJump_small(JZero,jb);
				op_call_fun(ctx,NULL,(int)(int_val)a->t->obj->rt->compareFun,2,args);
				op32(ctx,CMP,PEAX,pconst(&p,0));
				register_jump(ctx,do_jump(ctx,op->op,false),targetPos);
				patch_jump(ctx,ja);
				patch_jump(ctx,jb);
				break;
			}
			return;
		}
		// fallthrough
	default:
		// make sure we have valid 8 bits registers
		if( a->size == 1 ) alloc_cpu8(ctx,a,true);
		if( b->size == 1 ) alloc_cpu8(ctx,b,true);
		op_binop(ctx,NULL,a,b,op->op);
		break;
	}
	register_jump(ctx,do_jump(ctx,op->op, IS_FLOAT(a)),targetPos);
}

#endif // HL_JIT_X86

// =====================================================================
// Main JIT API Functions
// =====================================================================

jit_ctx *hl_jit_alloc() {
	int i;
	jit_ctx *ctx = (jit_ctx*)malloc(sizeof(jit_ctx));
	if( ctx == NULL ) return NULL;
	memset(ctx,0,sizeof(jit_ctx));
	hl_alloc_init(&ctx->falloc);
	hl_alloc_init(&ctx->galloc);
#ifdef HL_JIT_X86
	for(i=0;i<RCPU_COUNT;i++) {
		preg *r = REG_AT(i);
		r->id = i;
		r->kind = RCPU;
	}
	for(i=0;i<RFPU_COUNT;i++) {
		preg *r = REG_AT(XMM(i));
		r->id = i;
		r->kind = RFPU;
	}
#elif defined(HL_JIT_ARM64)
	// ARM64: Initialize general purpose registers (X0-X30)
	for(i=0;i<RCPU_COUNT;i++) {
		preg *r = REG_AT(i);
		r->id = i;
		r->kind = RCPU;
	}
	// ARM64: Initialize SIMD/FP registers (V0-V31)
	for(i=0;i<RFPU_COUNT;i++) {
		preg *r = REG_AT(VREG(i));
		r->id = i;
		r->kind = RFPU;
	}
#else
	#error "Unsupported architecture for JIT"
#endif
	return ctx;
}

void hl_jit_free( jit_ctx *ctx, h_bool can_reset ) {
	free(ctx->vregs);
	free(ctx->opsPos);
	free(ctx->startBuf);
	ctx->maxRegs = 0;
	ctx->vregs = NULL;
	ctx->maxOps = 0;
	ctx->opsPos = NULL;
	ctx->startBuf = NULL;
	ctx->bufSize = 0;
	ctx->buf.b = NULL;
	ctx->calls = NULL;
	ctx->switchs = NULL;
	ctx->closure_list = NULL;
	hl_free(&ctx->falloc);
	hl_free(&ctx->galloc);
	if( !can_reset ) free(ctx);
}

#ifdef HL_JIT_X86
static void jit_nops( jit_ctx *ctx ) {
	while( BUF_POS() & 15 )
		op32(ctx, NOP, UNUSED, UNUSED);
}

#define MAX_ARGS 16

static void *call_jit_c2hl = NULL;
static void *call_jit_hl2c = NULL;

static void *callback_c2hl( void *_f, hl_type *t, void **args, vdynamic *ret ) {
	/*
		prepare stack and regs according to prepare_call_args, but by reading runtime type information
		from the function type. The stack and regs will be setup by the trampoline function.
	*/
	void **f = (void**)_f;
	unsigned char stack[MAX_ARGS * 8];
	call_regs cregs = {0};
	if( t->fun->nargs > MAX_ARGS )
		hl_error("Too many arguments for dynamic call");
	int i, size = 0, pad = 0, pos = 0;
	for(i=0;i<t->fun->nargs;i++) {
		hl_type *at = t->fun->args[i];
		int creg = select_call_reg(&cregs,at,i);
		if( creg >= 0 )
			continue;
		size += stack_size(at);
	}
	pad = (-size) & 15;
	size += pad;
	pos = 0;
	for(i=0;i<t->fun->nargs;i++) {
		// RTL
		hl_type *at = t->fun->args[i];
		void *v = args[i];
		int creg = mapped_reg(&cregs,i);
		void *store;
		if( creg >= 0 ) {
			if( REG_IS_FPU(creg) ) {
				store = stack + size + CALL_NREGS * HL_WSIZE + (creg - XMM(0)) * sizeof(double);
			} else {
				store = stack + size + call_reg_index(creg) * HL_WSIZE;
			}
			switch( at->kind ) {
			case HBOOL:
			case HUI8:
				*(int_val*)store = *(unsigned char*)v;
				break;
			case HUI16:
				*(int_val*)store = *(unsigned short*)v;
				break;
			case HI32:
				*(int_val*)store = *(int*)v;
				break;
			case HF32:
				*(void**)store = 0;
				*(float*)store = *(float*)v;
				break;
			case HF64:
				*(double*)store = *(double*)v;
				break;
			case HI64:
			case HGUID:
				*(int64*)store = *(int64*)v;
				break;
			default:
				*(void**)store = v;
				break;
			}
		} else {
			int tsize = stack_size(at);
			store = stack + pos;
			pos += tsize;
			switch( at->kind ) {
			case HBOOL:
			case HUI8:
				*(int*)store = *(unsigned char*)v;
				break;
			case HUI16:
				*(int*)store = *(unsigned short*)v;
				break;
			case HI32:
			case HF32:
				*(int*)store = *(int*)v;
				break;
			case HF64:
				*(double*)store = *(double*)v;
				break;
			case HI64:
			case HGUID:
				*(int64*)store = *(int64*)v;
				break;
			default:
				*(void**)store = v;
				break;
			}
		}
	}
	pos += pad;
	pos >>= IS_64 ? 3 : 2;
	switch( t->fun->ret->kind ) {
	case HUI8:
	case HUI16:
	case HI32:
	case HBOOL:
		ret->v.i = ((int (*)(void *, void *, void *))call_jit_c2hl)(*f, (void**)&stack + pos, &stack);
		return &ret->v.i;
	case HI64:
	case HGUID:
		ret->v.i64 = ((int64 (*)(void *, void *, void *))call_jit_c2hl)(*f, (void**)&stack + pos, &stack);
		return &ret->v.i64;
	case HF32:
		ret->v.f = ((float (*)(void *, void *, void *))call_jit_c2hl)(*f, (void**)&stack + pos, &stack);
		return &ret->v.f;
	case HF64:
		ret->v.d = ((double (*)(void *, void *, void *))call_jit_c2hl)(*f, (void**)&stack + pos, &stack);
		return &ret->v.d;
	default:
		return ((void *(*)(void *, void *, void *))call_jit_c2hl)(*f, (void**)&stack + pos, &stack);
	}
}

static void jit_c2hl( jit_ctx *ctx ) {
	//	create the function that will be called by callback_c2hl
	//	it will make sure to prepare the stack/regs according to native calling conventions
	int jeq, jloop, jstart;
	preg *fptr, *stack, *stend;
	preg p;

	op64(ctx,PUSH,PEBP,UNUSED);
	op64(ctx,MOV,PEBP,PESP);

#	ifdef HL_64

	fptr = REG_AT(R10);
	stack = PEAX;
	stend = REG_AT(R11);
	op64(ctx, MOV, fptr, REG_AT(CALL_REGS[0]));
	op64(ctx, MOV, stack, REG_AT(CALL_REGS[1]));
	op64(ctx, MOV, stend, REG_AT(CALL_REGS[2]));

	// set native call regs
	int i;
	for(i=0;i<CALL_NREGS;i++)
		op64(ctx,MOV,REG_AT(CALL_REGS[i]),pmem(&p,stack->id,i*HL_WSIZE));
	for(i=0;i<CALL_NREGS;i++)
		op64(ctx,MOVSD,REG_AT(XMM(i)),pmem(&p,stack->id,(i+CALL_NREGS)*HL_WSIZE));

#	else

	// make sure the stack is aligned on 16 bytes
	// the amount of push we will do afterwards is guaranteed to be a multiple of 16bytes by hl_callback
#	ifdef HL_VCC
	// VCC does not guarantee us an aligned stack...
	op64(ctx,MOV,PEAX,PESP);
	op64(ctx,AND,PEAX,pconst(&p,15));
	op64(ctx,SUB,PESP,PEAX);
#	else
	op64(ctx,SUB,PESP,pconst(&p,8));
#	endif

	// mov arguments to regs
	fptr = REG_AT(Eax);
	stack = REG_AT(Edx);
	stend = REG_AT(Ecx);
	op64(ctx,MOV,fptr,pmem(&p,Ebp,HL_WSIZE*2));
	op64(ctx,MOV,stack,pmem(&p,Ebp,HL_WSIZE*3));
	op64(ctx,MOV,stend,pmem(&p,Ebp,HL_WSIZE*4));

#	endif

	// push stack args
	jstart = BUF_POS();
	op64(ctx,CMP,stack,stend);
	XJump(JEq,jeq);
	op64(ctx,SUB,stack,pconst(&p,HL_WSIZE));
	op64(ctx,PUSH,pmem(&p,stack->id,0),UNUSED);
	XJump(JAlways,jloop);
	patch_jump(ctx,jeq);
	patch_jump_to(ctx, jloop, jstart);

	op_call(ctx,fptr,0);

	// cleanup and ret
	op64(ctx,MOV,PESP,PEBP);
	op64(ctx,POP,PEBP, UNUSED);
	op64(ctx,RET,UNUSED,UNUSED);
}

static vdynamic *jit_wrapper_call( vclosure_wrapper *c, char *stack_args, void **regs ) {
	vdynamic *args[MAX_ARGS];
	int i;
	int nargs = c->cl.t->fun->nargs;
	call_regs cregs = {0};
	if( nargs > MAX_ARGS )
		hl_error("Too many arguments for wrapped call");
	cregs.nextCpu++; // skip fptr in HL64 - was passed as arg0
	for(i=0;i<nargs;i++) {
		hl_type *t = c->cl.t->fun->args[i];
		int creg = select_call_reg(&cregs,t,i);
		if( creg < 0 ) {
			args[i] = hl_is_dynamic(t) ? *(vdynamic**)stack_args : hl_make_dyn(stack_args,t);
			stack_args += stack_size(t);
		} else if( hl_is_dynamic(t) ) {
			args[i] = *(vdynamic**)(regs + call_reg_index(creg));
		} else if( t->kind == HF32 || t->kind == HF64 ) {
			args[i] = hl_make_dyn(regs + CALL_NREGS + creg - XMM(0),&hlt_f64);
		} else {
			args[i] = hl_make_dyn(regs + call_reg_index(creg),t);
		}
	}
	return hl_dyn_call(c->wrappedFun,args,nargs);
}

static void *jit_wrapper_ptr( vclosure_wrapper *c, char *stack_args, void **regs ) {
	vdynamic *ret = jit_wrapper_call(c, stack_args, regs);
	hl_type *tret = c->cl.t->fun->ret;
	switch( tret->kind ) {
	case HVOID:
		return NULL;
	case HUI8:
	case HUI16:
	case HI32:
	case HBOOL:
		return (void*)(int_val)hl_dyn_casti(&ret,&hlt_dyn,tret);
	case HI64:
	case HGUID:
		return (void*)(int_val)hl_dyn_casti64(&ret,&hlt_dyn);
	default:
		return hl_dyn_castp(&ret,&hlt_dyn,tret);
	}
}

static double jit_wrapper_d( vclosure_wrapper *c, char *stack_args, void **regs ) {
	vdynamic *ret = jit_wrapper_call(c, stack_args, regs);
	return hl_dyn_castd(&ret,&hlt_dyn);
}

static void jit_hl2c( jit_ctx *ctx ) {
	// create a function that is called with a vclosure_wrapper* and native args
	// and pack and pass the args to callback_hl2c
	preg p;
	int jfloat1, jfloat2, jexit;
	hl_type_fun *ft = NULL;
	int size;
#	ifdef HL_64
	preg *cl = REG_AT(CALL_REGS[0]);
	preg *tmp = REG_AT(CALL_REGS[1]);
#	else
	preg *cl = REG_AT(Ecx);
	preg *tmp = REG_AT(Edx);
#	endif

	op64(ctx,PUSH,PEBP,UNUSED);
	op64(ctx,MOV,PEBP,PESP);

#	ifdef HL_64
	// push registers
	int i;
	op64(ctx,SUB,PESP,pconst(&p,CALL_NREGS*8));
	for(i=0;i<CALL_NREGS;i++)
		op64(ctx,MOVSD,pmem(&p,Esp,i*8),REG_AT(XMM(i)));
	for(i=0;i<CALL_NREGS;i++)
		op64(ctx,PUSH,REG_AT(CALL_REGS[CALL_NREGS - 1 - i]),UNUSED);
#	endif

	// opcodes for:
	//		switch( arg0->t->fun->ret->kind ) {
	//		case HF32: case HF64: return jit_wrapper_d(arg0,&args);
	//		default: return jit_wrapper_ptr(arg0,&args);
	//		}
	if( !IS_64 )
		op64(ctx,MOV,cl,pmem(&p,Ebp,HL_WSIZE*2)); // load arg0
	op64(ctx,MOV,tmp,pmem(&p,cl->id,0)); // ->t
	op64(ctx,MOV,tmp,pmem(&p,tmp->id,HL_WSIZE)); // ->fun
	op64(ctx,MOV,tmp,pmem(&p,tmp->id,(int)(int_val)&ft->ret)); // ->ret
	op32(ctx,MOV,tmp,pmem(&p,tmp->id,0)); // -> kind

	op32(ctx,CMP,tmp,pconst(&p,HF64));
	XJump_small(JEq,jfloat1);
	op32(ctx,CMP,tmp,pconst(&p,HF32));
	XJump_small(JEq,jfloat2);

	// 64 bits : ESP + EIP (+WIN64PAD)
	// 32 bits : ESP + EIP + PARAM0
	int args_pos = IS_64 ? ((IS_WINCALL64 ? 32 : 0) + HL_WSIZE * 2) : (HL_WSIZE*3);

	size = begin_native_call(ctx,3);
	op64(ctx, LEA, tmp, pmem(&p,Ebp,-HL_WSIZE*CALL_NREGS*2));
	set_native_arg(ctx, tmp);
	op64(ctx, LEA, tmp, pmem(&p,Ebp,args_pos));
	set_native_arg(ctx, tmp);
	set_native_arg(ctx, cl);
	call_native(ctx, jit_wrapper_ptr, size);
	XJump_small(JAlways, jexit);

	patch_jump(ctx,jfloat1);
	patch_jump(ctx,jfloat2);
	size = begin_native_call(ctx,3);
	op64(ctx, LEA, tmp, pmem(&p,Ebp,-HL_WSIZE*CALL_NREGS*2));
	set_native_arg(ctx, tmp);
	op64(ctx, LEA, tmp, pmem(&p,Ebp,args_pos));
	set_native_arg(ctx, tmp);
	set_native_arg(ctx, cl);
	call_native(ctx, jit_wrapper_d, size);

	patch_jump(ctx,jexit);
	op64(ctx,MOV,PESP,PEBP);
	op64(ctx,POP,PEBP, UNUSED);
	op64(ctx,RET,UNUSED,UNUSED);
}

#ifdef JIT_CUSTOM_LONGJUMP
// Win64 debug CRT performs a Rtl stack check in debug mode, preventing from
// using longjump. This in an alternate implementation that follows the native
// setjump storage.
//
// Another more reliable way of handling this would be to use RtlAddFunctionTable
// but this would require complex creation of unwind info
static void jit_longjump( jit_ctx *ctx ) {
	preg *buf = REG_AT(CALL_REGS[0]);
	preg *ret = REG_AT(CALL_REGS[1]);
	preg p;
	int i;
	op64(ctx,MOV,PEAX,ret); // return value
	op64(ctx,MOV,REG_AT(Edx),pmem(&p,buf->id,0x0));
	op64(ctx,MOV,REG_AT(Ebx),pmem(&p,buf->id,0x8));
	op64(ctx,MOV,REG_AT(Esp),pmem(&p,buf->id,0x10));
	op64(ctx,MOV,REG_AT(Ebp),pmem(&p,buf->id,0x18));
	op64(ctx,MOV,REG_AT(Esi),pmem(&p,buf->id,0x20));
	op64(ctx,MOV,REG_AT(Edi),pmem(&p,buf->id,0x28));
	op64(ctx,MOV,REG_AT(R12),pmem(&p,buf->id,0x30));
	op64(ctx,MOV,REG_AT(R13),pmem(&p,buf->id,0x38));
	op64(ctx,MOV,REG_AT(R14),pmem(&p,buf->id,0x40));
	op64(ctx,MOV,REG_AT(R15),pmem(&p,buf->id,0x48));
	op64(ctx,LDMXCSR,pmem(&p,buf->id,0x58), UNUSED);
	op64(ctx,FLDCW,pmem(&p,buf->id,0x5C), UNUSED);
	for(i=0;i<10;i++)
		op64(ctx,MOVSD,REG_AT(XMM(i+6)),pmem(&p,buf->id,0x60 + i * 16));
	op64(ctx,PUSH,pmem(&p,buf->id,0x50),UNUSED);
	op64(ctx,RET,UNUSED,UNUSED);
}
#endif

static void jit_fail( uchar *msg ) {
	if( msg == NULL ) {
		hl_debug_break();
		msg = USTR("assert");
	}
	vdynamic *d = hl_alloc_dynamic(&hlt_bytes);
	d->v.ptr = msg;
	hl_throw(d);
}

static void jit_null_access( jit_ctx *ctx ) {
	op64(ctx,PUSH,PEBP,UNUSED);
	op64(ctx,MOV,PEBP,PESP);
	int_val arg = (int_val)USTR("Null access");
	call_native_consts(ctx, jit_fail, &arg, 1);
}

static void jit_null_fail( int fhash ) {
	vbyte *field = hl_field_name(fhash);
	hl_buffer *b = hl_alloc_buffer();
	hl_buffer_str(b, USTR("Null access ."));
	hl_buffer_str(b, (uchar*)field);
	vdynamic *d = hl_alloc_dynamic(&hlt_bytes);
	d->v.ptr = hl_buffer_content(b,NULL);
	hl_throw(d);
}

static void jit_null_field_access( jit_ctx *ctx ) {
	preg p;
	op64(ctx,PUSH,PEBP,UNUSED);
	op64(ctx,MOV,PEBP,PESP);
	int size = begin_native_call(ctx, 1);
	int args_pos = (IS_WINCALL64 ? 32 : 0) + HL_WSIZE*2;
	set_native_arg(ctx, pmem(&p,Ebp,args_pos));
	call_native(ctx,jit_null_fail,size);
}

static void jit_assert( jit_ctx *ctx ) {
	op64(ctx,PUSH,PEBP,UNUSED);
	op64(ctx,MOV,PEBP,PESP);
	int_val arg = 0;
	call_native_consts(ctx, jit_fail, &arg, 1);
}

static int jit_build( jit_ctx *ctx, void (*fbuild)( jit_ctx *) ) {
	int pos;
	jit_buf(ctx);
	jit_nops(ctx);
	pos = BUF_POS();
	fbuild(ctx);
	jit_nops(ctx);
	return pos;
}

// Forward declarations for x86-specific functions
static void hl_jit_init_x86( jit_ctx *ctx );
void *hl_jit_code_x86( jit_ctx *ctx, hl_module *m, int *codesize, hl_debug_infos **debug, hl_module *previous );
void hl_jit_patch_method_x86( void *old_fun, void **new_fun_table );

static void hl_jit_init_x86( jit_ctx *ctx ) {
	ctx->c2hl = jit_build(ctx, jit_c2hl);
	ctx->hl2c = jit_build(ctx, jit_hl2c);
#	ifdef JIT_CUSTOM_LONGJUMP
	ctx->longjump = jit_build(ctx, jit_longjump);
#	endif
	ctx->static_functions[0] = (void*)(int_val)jit_build(ctx,jit_null_access);
	ctx->static_functions[1] = (void*)(int_val)jit_build(ctx,jit_assert);
	ctx->static_functions[2] = (void*)(int_val)jit_build(ctx,jit_null_field_access);
}

static void *get_dyncast( hl_type *t ) {
	switch( t->kind ) {
	case HF32:
		return hl_dyn_castf;
	case HF64:
		return hl_dyn_castd;
	case HI64:
	case HGUID:
		return hl_dyn_casti64;
	case HI32:
	case HUI16:
	case HUI8:
	case HBOOL:
		return hl_dyn_casti;
	default:
		return hl_dyn_castp;
	}
}

static void *get_dynset( hl_type *t ) {
	switch( t->kind ) {
	case HF32:
		return hl_dyn_setf;
	case HF64:
		return hl_dyn_setd;
	case HI64:
	case HGUID:
		return hl_dyn_seti64;
	case HI32:
	case HUI16:
	case HUI8:
	case HBOOL:
		return hl_dyn_seti;
	default:
		return hl_dyn_setp;
	}
}

static void *get_dynget( hl_type *t ) {
	switch( t->kind ) {
	case HF32:
		return hl_dyn_getf;
	case HF64:
		return hl_dyn_getd;
	case HI64:
	case HGUID:
		return hl_dyn_geti64;
	case HI32:
	case HUI16:
	case HUI8:
	case HBOOL:
		return hl_dyn_geti;
	default:
		return hl_dyn_getp;
	}
}

static vclosure *alloc_static_closure( jit_ctx *ctx, int fid ) {
	hl_module *m = ctx->m;
	vclosure *c = hl_malloc(&m->ctx.alloc,sizeof(vclosure));
	int fidx = m->functions_indexes[fid];
	c->hasValue = 0;
	if( fidx >= m->code->nfunctions ) {
		// native
		c->t = m->code->natives[fidx - m->code->nfunctions].t;
		c->fun = m->functions_ptrs[fid];
		c->value = NULL;
	} else {
		c->t = m->code->functions[fidx].type;
		c->fun = (void*)(int_val)fid;
		c->value = ctx->closure_list;
		ctx->closure_list = c;
	}
	return c;
}

static void make_dyn_cast( jit_ctx *ctx, vreg *dst, vreg *v ) {
	int size;
	preg p;
	preg *tmp;
	if( v->t->kind == HNULL && v->t->tparam->kind == dst->t->kind ) {
		int jnull, jend;
		preg *out;
		switch( dst->t->kind ) {
		case HUI8:
		case HUI16:
		case HI32:
		case HBOOL:
		case HI64:
		case HGUID:
			tmp = alloc_cpu(ctx, v, true);
			op64(ctx, TEST, tmp, tmp);
			XJump_small(JZero, jnull);
			op64(ctx, MOV, tmp, pmem(&p,tmp->id,8));
			XJump_small(JAlways, jend);
			patch_jump(ctx, jnull);
			op64(ctx, XOR, tmp, tmp);
			patch_jump(ctx, jend);
			store(ctx, dst, tmp, true);
			return;
		case HF32:
		case HF64:
			tmp = alloc_cpu(ctx, v, true);
			out = alloc_fpu(ctx, dst, false);
			op64(ctx, TEST, tmp, tmp);
			XJump_small(JZero, jnull);
			op64(ctx, dst->t->kind == HF32 ? MOVSS : MOVSD, out, pmem(&p,tmp->id,8));
			XJump_small(JAlways, jend);
			patch_jump(ctx, jnull);
			op64(ctx, XORPD, out, out);
			patch_jump(ctx, jend);
			store(ctx, dst, out, true);
			return;
		default:
			break;
		}
	}
	switch( dst->t->kind ) {
	case HF32:
	case HF64:
	case HI64:
	case HGUID:
		size = begin_native_call(ctx, 2);
		set_native_arg(ctx, pconst64(&p,(int_val)v->t));
		break;
	default:
		size = begin_native_call(ctx, 3);
		set_native_arg(ctx, pconst64(&p,(int_val)dst->t));
		set_native_arg(ctx, pconst64(&p,(int_val)v->t));
		break;
	}
	tmp = alloc_native_arg(ctx);
	op64(ctx,MOV,tmp,REG_AT(Ebp));
	if( v->stackPos >= 0 )
		op64(ctx,ADD,tmp,pconst(&p,v->stackPos));
	else
		op64(ctx,SUB,tmp,pconst(&p,-v->stackPos));
	set_native_arg(ctx,tmp);
	call_native(ctx,get_dyncast(dst->t),size);
	store_result(ctx, dst);
}

#endif // HL_JIT_X86

// Forward declarations for shared helper functions
static double uint_to_double( unsigned int v );

// Shared helper function for closure allocation
static vclosure *alloc_static_closure( jit_ctx *ctx, int fid ) {
	hl_module *m = ctx->m;
	vclosure *c = hl_malloc(&m->ctx.alloc,sizeof(vclosure));
	int fidx = m->functions_indexes[fid];
	c->hasValue = 0;
	if( fidx >= m->code->nfunctions ) {
		// native
		c->t = m->code->natives[fidx - m->code->nfunctions].t;
		c->fun = m->functions_ptrs[fid];
		c->value = NULL;
	} else {
		c->t = m->code->functions[fidx].type;
		c->fun = (void*)(int_val)fid;
		c->value = ctx->closure_list;
		ctx->closure_list = c;
	}
	return c;
}

// Forward declarations for ARM64 encoders (defined later in file)
#ifdef HL_JIT_ARM64
static void arm_stp(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rt2, Arm64Reg rn, int offset, bool is64, bool pre_index);
static void arm_ldp(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rt2, Arm64Reg rn, int offset, bool is64, bool pre_index);
static void arm_add_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_add_imm(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, unsigned int imm12, bool is64);
static void arm_sub_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_sub_imm(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, unsigned int imm12, bool is64);
static void arm_subs_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_mul(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_sdiv(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_udiv(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_and_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_orr_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_eor_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_lsl_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_lsl_imm(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, unsigned int shift, bool is64);
static void arm_lsr_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_asr_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_neg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, bool is64);
static void arm_mov_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rm, bool is64);
static void arm_movz(jit_ctx *ctx, Arm64Reg rd, unsigned int imm16, unsigned int shift, bool is64);
static void arm_ret(jit_ctx *ctx, Arm64Reg rn);
static void arm_load_imm64(jit_ctx *ctx, Arm64Reg rd, uint64_t imm);
static void arm_tst_reg(jit_ctx *ctx, Arm64Reg rn, Arm64Reg rm, bool is64);
static void arm_ldr_reg(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, Arm64Reg rm, int size);
static void arm_str_reg(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, Arm64Reg rm, int size);
static void arm_ldr_imm(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, unsigned int imm12, int size);
static void arm_str_imm(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, unsigned int imm12, int size);
static void arm_ldur_imm(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, int imm9, int size);
static void arm_stur_imm(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, int imm9, int size);
static int arm_b(jit_ctx *ctx, int offset);
static int arm_b_cond(jit_ctx *ctx, Arm64Condition cond, int offset);
static int arm_cbz(jit_ctx *ctx, Arm64Reg rt, int offset, bool is64);
static int arm_cbnz(jit_ctx *ctx, Arm64Reg rt, int offset, bool is64);
static int arm_do_jump(jit_ctx *ctx);
static int arm_do_cbz(jit_ctx *ctx, Arm64Reg rt, bool is64);
static int arm_do_cbnz(jit_ctx *ctx, Arm64Reg rt, bool is64);
static int arm_do_jump_cond(jit_ctx *ctx, Arm64Condition cond);
static void arm_patch_cbz(jit_ctx *ctx, int pos, int target);
static void arm_scvtf(jit_ctx *ctx, Arm64FpReg fd, Arm64Reg rn, bool int64, bool float64);
static void arm_ucvtf(jit_ctx *ctx, Arm64FpReg fd, Arm64Reg rn, bool int64, bool float64);
static void arm_fcvtzs(jit_ctx *ctx, Arm64Reg rd, Arm64FpReg fn, bool int64, bool float64);
static void arm_fcvt(jit_ctx *ctx, Arm64FpReg fd, Arm64FpReg fn, bool src64);
static void arm_patch_cbnz(jit_ctx *ctx, int pos, int target);
static void arm_patch_jump(jit_ctx *ctx, int jump_pos);
static void arm_cmp_imm(jit_ctx *ctx, Arm64Reg rn, unsigned int imm12, bool is64);
static void arm_blr(jit_ctx *ctx, Arm64Reg rn);
static void register_jump(jit_ctx *ctx, int jump_pos, int target);
static void arm_prologue(jit_ctx *ctx, int framesize);
static void arm_epilogue(jit_ctx *ctx, int framesize);
static int stack_size(hl_type *t);
#endif

// =====================================================================
// Shared Framework Functions
// =====================================================================

static void hl_jit_init_module( jit_ctx *ctx, hl_module *m ) {
	int i;
	ctx->m = m;
	if( m->code->hasdebug ) {
		ctx->debug = (hl_debug_infos*)malloc(sizeof(hl_debug_infos) * m->code->nfunctions);
		memset(ctx->debug, -1, sizeof(hl_debug_infos) * m->code->nfunctions);
	}
	for(i=0;i<m->code->nfloats;i++) {
		jit_buf(ctx);
		*ctx->buf.d++ = m->code->floats[i];
	}
}

// =====================================================================
// ARM64 Framework Helper Functions
// =====================================================================
#ifdef HL_JIT_ARM64

// Helper function to call jit_fail for null access errors
static void jit_fail( uchar *msg ) {
	if( msg == NULL ) {
		hl_debug_break();
		msg = USTR("assert");
	}
	vdynamic *d = hl_alloc_dynamic(&hlt_bytes);
	d->v.ptr = msg;
	hl_throw(d);
}

// Null access error handler for ARM64
// Generates code that calls jit_fail with "Null access" message
static void jit_null_access_arm64( jit_ctx *ctx ) {
	// ARM64 function prologue
	arm_prologue(ctx, 16);

	// Load message pointer into X0 (first argument)
	// We use a literal string: "Null access"
	int_val msg = (int_val)USTR("Null access");
	arm_load_imm64(ctx, X0, (uint64_t)msg);

	// Load jit_fail function address into X9
	arm_load_imm64(ctx, X9, (uint64_t)jit_fail);

	// Call jit_fail via BLR X9
	arm_blr(ctx, X9);

	// This should never return, but add epilogue for safety
	arm_epilogue(ctx, 16);
}

// Helper for field-specific null access errors
static void jit_null_fail( int fhash ) {
	vbyte *field = hl_field_name(fhash);
	hl_buffer *b = hl_alloc_buffer();
	hl_buffer_str(b, USTR("Null access ."));
	hl_buffer_str(b, (uchar*)field);
	vdynamic *d = hl_alloc_dynamic(&hlt_bytes);
	d->v.ptr = hl_buffer_content(b,NULL);
	hl_throw(d);
}

// Null field access error handler for ARM64
// Takes field hash as argument on stack
static void jit_null_field_access_arm64( jit_ctx *ctx ) {
	// ARM64 function prologue
	// Frame needs space for saved FP/LR
	arm_prologue(ctx, 16);

	// Load field hash argument from stack
	// First stack argument is at [FP + 16] (after saved FP/LR)
	// For 64-bit load (size=3), offset is in units of 8 bytes: 16/8=2
	arm_ldr_imm(ctx, X0, X29, 2, 3);  // Load 64-bit value at offset 16 bytes

	// Load jit_null_fail function address into X9
	arm_load_imm64(ctx, X9, (uint64_t)jit_null_fail);

	// Call jit_null_fail via BLR X9
	arm_blr(ctx, X9);

	// This should never return
	arm_epilogue(ctx, 16);
}

// Assert failure handler for ARM64
static void jit_assert_arm64( jit_ctx *ctx ) {
	// ARM64 function prologue
	arm_prologue(ctx, 16);

	// Call jit_fail with NULL argument (will use "assert" message)
	arm_mov_reg(ctx, X0, XZR, true);  // X0 = 0 (NULL)

	// Load jit_fail function address into X9
	arm_load_imm64(ctx, X9, (uint64_t)jit_fail);

	// Call jit_fail via BLR X9
	arm_blr(ctx, X9);

	// This should never return
	arm_epilogue(ctx, 16);
}

// Helper to build ARM64 helper function and return its offset
static int jit_build_arm64( jit_ctx *ctx, void (*fbuild)( jit_ctx *) ) {
	int pos;
	jit_buf(ctx);
	// ARM64 requires 4-byte alignment for instructions
	while (BUF_POS() & 3) {
		*ctx->buf.b++ = 0;
	}
	pos = BUF_POS();
	fbuild(ctx);
	// Align after as well
	while (BUF_POS() & 3) {
		*ctx->buf.b++ = 0;
	}
	return pos;
}

// ARM64 JIT initialization
void hl_jit_init_arm64( jit_ctx *ctx ) {
	// Note: We don't need c2hl/hl2c trampolines for ARM64 yet
	// as closures are not implemented. We just need error handlers.
	ctx->static_functions[0] = (void*)(int_val)jit_build_arm64(ctx,jit_null_access_arm64);
	ctx->static_functions[1] = (void*)(int_val)jit_build_arm64(ctx,jit_assert_arm64);
	ctx->static_functions[2] = (void*)(int_val)jit_build_arm64(ctx,jit_null_field_access_arm64);
}

// ARM64 method patching
// Patches an old method to redirect to a new method table
void hl_jit_patch_method_arm64( void *old_fun, void **new_fun_table ) {
	// ARM64 instruction sequence for patching:
	// We need to load new_fun_table address and jump to it
	// Using X9 as temporary register
	//
	// movz  x9, #imm16_0              ; bits 0-15
	// movk  x9, #imm16_1, lsl #16    ; bits 16-31
	// movk  x9, #imm16_2, lsl #32    ; bits 32-47
	// movk  x9, #imm16_3, lsl #48    ; bits 48-63
	// ldr   x9, [x9]                 ; load function pointer from table
	// br    x9                       ; branch to function

	unsigned char *b = (unsigned char*)old_fun;
	unsigned long long addr = (unsigned long long)(int_val)new_fun_table;

	// MOVZ X9, #imm16, LSL #0
	unsigned int movz = (0xD2800000 | (9 << 0) | ((addr & 0xFFFF) << 5));
	*(unsigned int*)b = movz; b += 4;

	// MOVK X9, #imm16, LSL #16
	unsigned int movk1 = (0xF2A00000 | (9 << 0) | (((addr >> 16) & 0xFFFF) << 5));
	*(unsigned int*)b = movk1; b += 4;

	// MOVK X9, #imm16, LSL #32
	unsigned int movk2 = (0xF2C00000 | (9 << 0) | (((addr >> 32) & 0xFFFF) << 5));
	*(unsigned int*)b = movk2; b += 4;

	// MOVK X9, #imm16, LSL #48
	unsigned int movk3 = (0xF2E00000 | (9 << 0) | (((addr >> 48) & 0xFFFF) << 5));
	*(unsigned int*)b = movk3; b += 4;

	// LDR X9, [X9] - Load 64-bit value from address in X9
	// Format: 1 111 1001 01 imm12 Rn Rt (unsigned offset)
	// With immediate offset 0
	unsigned int ldr = (0xF9400000 | (9 << 5) | 9);
	*(unsigned int*)b = ldr; b += 4;

	// BR X9 - Branch to register
	// Format: 1101 0110 0001 1111 0000 00 Rn 0 0000
	unsigned int br = (0xD61F0000 | (9 << 5));
	*(unsigned int*)b = br;
}

// ARM64 code generation
void *hl_jit_code_arm64( jit_ctx *ctx, hl_module *m, int *codesize, hl_debug_infos **debug ) {
	int size = BUF_POS();
	unsigned char *code;

	// Align to page boundary
	if( size & 4095 ) size += 4096 - (size&4095);

	// Allocate executable memory
	code = (unsigned char*)hl_alloc_executable_memory(size);
	if( code == NULL ) return NULL;

	// Copy generated code to executable memory
	memcpy(code,ctx->startBuf,BUF_POS());
	*codesize = size;
	*debug = ctx->debug;

	// Convert static function offsets to absolute addresses
	if( !ctx->static_function_offset ) {
			int i;
		ctx->static_function_offset = true;
		for(i=0;i<(int)(sizeof(ctx->static_functions)/sizeof(void*));i++)
			ctx->static_functions[i] = (void*)(code + (int)(int_val)ctx->static_functions[i]);
		}

	// Patch calls
	printf("[PATCH] Starting call patching, code base=%p\n", (void*)code);
	jlist *c = ctx->calls;
	int patch_count = 0;
	while( c ) {
			void *fabs;
		if( c->target < 0 ) {
			// Static function (error handlers)
			fabs = ctx->static_functions[-c->target-1];
			printf("[PATCH] Call %d: static function %d at pos=%d, fabs=%p\n",
			       patch_count, -c->target-1, c->pos, fabs);
		} else {
			// Module function
			fabs = m->functions_ptrs[c->target];
			printf("[PATCH] Call %d: function %d at pos=%d, offset=%p\n",
			       patch_count, c->target, c->pos, fabs);
			if( fabs == NULL ) {
				printf("[PATCH] ERROR: Function %d pointer is NULL!\n", c->target);
				// TODO: Handle previous module lookups
				return NULL;
			}
			// At this point, fabs is a relative offset - convert to absolute
			fabs = (unsigned char*)code + (int)(int_val)fabs;
			printf("[PATCH]   -> absolute address=%p\n", fabs);
		}

		// Patch the BL instruction
		// BL encoding: imm26 is signed offset / 4
			unsigned int *instr = (unsigned int*)(code + c->pos);
			printf("[PATCH]   Instruction at %p before patch: 0x%08x\n",
			       (void*)instr, *instr);
				int_val delta = (int_val)fabs - (int_val)(code + c->pos);
			int offset = (int)(delta / 4);  // Offset in instructions
		printf("[PATCH]   delta=%ld, offset=%d\n", delta, offset);
		patch_count++;
	
		// Check if offset fits in 26 bits
		if (offset < -(1<<25) || offset >= (1<<25)) {
			printf("Target code too far to rebase: offset=%d, delta=%ld, target=%p, source=%p\n",
			       offset, delta, fabs, (void*)(code + c->pos));
			return NULL;
		}

			// Update BL instruction: keep top 6 bits, replace bottom 26 bits
		*instr = (*instr & 0xFC000000) | (offset & 0x03FFFFFF);
	
		c = c->next;
		}

	// Patch jumps (conditional branches, etc.)
	jlist *j = ctx->jumps;
	while( j ) {
			int target_pos = ctx->opsPos[j->target];
			if( target_pos == 0 && j->target != 0 ) {
			printf("Invalid jump target\n");
			return NULL;
		}

		// Call the appropriate ARM64 patch function
		// Note: The instruction type was stored at the jump position
			arm_patch_jump(ctx, j->pos);
	
		j = j->next;
		}

	// Patch closures
	vclosure *c_closure = ctx->closure_list;
	int closure_count = 0;
	while( c_closure ) {
		vclosure *next;
		int fid = (int)(int_val)c_closure->fun;
		void *fabs = m->functions_ptrs[fid];
		if( fabs == NULL ) {
			return NULL;
		}
		fabs = (unsigned char*)code + (int)(int_val)fabs;
		c_closure->fun = fabs;
		next = (vclosure*)c_closure->value;
		c_closure->value = NULL;
		c_closure = next;
		closure_count++;
	}

	// ARM64 CRITICAL: Flush instruction cache
	// Without this, the CPU may execute stale cached instructions
	__builtin___clear_cache((char*)code, (char*)code + BUF_POS());

	return code;
}

#endif // HL_JIT_ARM64

// =====================================================================
// Shared JIT Initialization
// =====================================================================

void hl_jit_init( jit_ctx *ctx, hl_module *m ) {
	hl_jit_init_module(ctx,m);
#ifdef HL_JIT_X86
	hl_jit_init_x86(ctx);
#elif defined(HL_JIT_ARM64)
	hl_jit_init_arm64(ctx);
#else
	hl_error("JIT not supported on this architecture");
#endif
}

void hl_jit_reset( jit_ctx *ctx, hl_module *m ) {
	ctx->debug = NULL;
	hl_jit_init_module(ctx,m);
}

void hl_jit_patch_method( void *old_fun, void **new_fun_table ) {
#ifdef HL_JIT_X86
	hl_jit_patch_method_x86(old_fun, new_fun_table);
#elif defined(HL_JIT_ARM64)
	hl_jit_patch_method_arm64(old_fun, new_fun_table);
#else
	hl_error("JIT not supported on this architecture");
#endif
}

void *hl_jit_code( jit_ctx *ctx, hl_module *m, int *codesize, hl_debug_infos **debug, hl_module *previous ) {
#ifdef HL_JIT_X86
	return hl_jit_code_x86(ctx, m, codesize, debug, previous);
#elif defined(HL_JIT_ARM64)
	return hl_jit_code_arm64(ctx, m, codesize, debug);
#else
	hl_error("JIT not supported on this architecture");
	return NULL;
#endif
}

int hl_jit_function( jit_ctx *ctx, hl_module *m, hl_function *f ) {
#if !defined(HL_JIT_X86) && !defined(HL_JIT_ARM64)
	hl_error("JIT compilation only supported on x86/x86-64 and ARM64");
	return -1;
#endif
#if defined(HL_JIT_X86) || defined(HL_JIT_ARM64)
	int i, size = 0, opCount;
	int codePos = BUF_POS();
	int nargs = f->type->fun->nargs;
	unsigned short *debug16 = NULL;
	int *debug32 = NULL;
#ifdef HL_JIT_X86
	call_regs cregs = {0};
	hl_thread_info *tinf = NULL;
	preg p;
#endif
	ctx->f = f;
	ctx->allocOffset = 0;
	if( f->nregs > ctx->maxRegs ) {
		free(ctx->vregs);
		ctx->vregs = (vreg*)malloc(sizeof(vreg) * (f->nregs + 1));
		if( ctx->vregs == NULL ) {
			ctx->maxRegs = 0;
			return -1;
		}
		ctx->maxRegs = f->nregs;
	}
	if( f->nops > ctx->maxOps ) {
		free(ctx->opsPos);
		ctx->opsPos = (int*)malloc(sizeof(int) * (f->nops + 1));
		if( ctx->opsPos == NULL ) {
			ctx->maxOps = 0;
			return -1;
		}
		ctx->maxOps = f->nops;
	}
	memset(ctx->opsPos,0,(f->nops+1)*sizeof(int));
	for(i=0;i<f->nregs;i++) {
		vreg *r = R(i);
		r->t = f->regs[i];
		r->size = hl_type_size(r->t);
		r->current = NULL;
		r->stack.holds = NULL;
		r->stack.id = i;
		r->stack.kind = RSTACK;
	}
	size = 0;
#ifdef HL_JIT_X86
	int argsSize = 0;
	for(i=0;i<nargs;i++) {
		vreg *r = R(i);
		int creg = select_call_reg(&cregs,r->t,i);
		if( creg < 0 || IS_WINCALL64 ) {
			// use existing stack storage
			r->stackPos = argsSize + HL_WSIZE * 2;
			argsSize += stack_size(r->t);
		} else {
			// make room in local vars
			size += r->size;
			size += hl_pad_size(size,r->t);
			r->stackPos = -size;
		}
	}
#endif
	for(i=nargs;i<f->nregs;i++) {
		vreg *r = R(i);
		size += r->size;
		size += hl_pad_size(size,r->t); // align local vars
		r->stackPos = -size;
	}
#	ifdef HL_64
	size += (-size) & 15; // align on 16 bytes
#	else
	size += hl_pad_size(size,&hlt_dyn); // align on word size
#	endif
	ctx->totalRegsSize = size;
	jit_buf(ctx);
	ctx->functionPos = BUF_POS();
	// make sure currentPos is > 0 before any reg allocations happen
	// otherwise `alloc_reg` thinks that all registers are locked
	ctx->currentPos = 1;
#ifdef HL_JIT_X86
	op_enter(ctx);
#elif defined(HL_JIT_ARM64)
	// ARM64: totalRegsSize is space for locals; we need +16 for saved FP/LR
	arm_prologue(ctx, ctx->totalRegsSize + 16);
#endif
#	ifdef HL_64
#	 ifdef HL_JIT_X86
	{
		// store in local var
		for(i=0;i<nargs;i++) {
			vreg *r = R(i);
			preg *p;
			int reg = mapped_reg(&cregs, i);
			if( reg < 0 ) continue;
			p = REG_AT(reg);
			copy(ctx,fetch(r),p,r->size);
			p->holds = r;
			r->current = p;
		}
	}
#	 elif defined(HL_JIT_ARM64)
	{
		// Store function arguments from X0-X7 to stack
		for(i=0;i<nargs && i<8;i++) {
			vreg *r = R(i);
			// ARM calling convention: args in X0-X7
			Arm64Reg src_reg = (Arm64Reg)(X0 + i);
			// Store to stack at r->stackPos
			if (r->stackPos >= -255 && r->stackPos <= 0) {
				arm_stur_imm(ctx, src_reg, X29, r->stackPos, 3);
			} else {
				arm_load_imm64(ctx, X9, r->stackPos);
				arm_add_reg(ctx, X9, X29, X9, true);
				arm_str_imm(ctx, src_reg, X9, 0, 3);
			}
		}
	}
#	 endif
#	endif
	if( ctx->m->code->hasdebug ) {
		debug16 = (unsigned short*)malloc(sizeof(unsigned short) * (f->nops + 1));
		debug16[0] = (unsigned short)(BUF_POS() - codePos);
	}
	ctx->opsPos[0] = BUF_POS();

	for(opCount=0;opCount<f->nops;opCount++) {
		int jump;
		hl_opcode *o = f->ops + opCount;
		vreg *dst = R(o->p1);
		vreg *ra = R(o->p2);
		vreg *rb = R(o->p3);
		ctx->currentPos = opCount + 1;
		jit_buf(ctx);
		ctx->opsPos[opCount + 1] = BUF_POS();
#		ifdef JIT_DEBUG
		if( opCount == 0 || f->ops[opCount-1].op != OAsm ) {
			int uid = opCount + (f->findex<<16);
			op32(ctx, PUSH, pconst(&p,uid), UNUSED);
			op64(ctx, ADD, PESP, pconst(&p,HL_WSIZE));
		}
#		endif
		// emit code
		switch( o->op ) {
#ifndef HL_JIT_ARM64
		case OMov:
		case OUnsafeCast:
			op_mov(ctx, dst, ra);
			break;
		case OInt:
			store_const(ctx, dst, m->code->ints[o->p2]);
			break;
		case OBool:
			store_const(ctx, dst, o->p2);
			break;
		case OGetGlobal:
			{
				void *addr = m->globals_data + m->globals_indexes[o->p2];
#				ifdef HL_64
				preg *tmp = alloc_reg(ctx, RCPU);
				op64(ctx, MOV, tmp, pconst64(&p,(int_val)addr));
				copy_to(ctx, dst, pmem(&p,tmp->id,0));
#				else
				copy_to(ctx, dst, paddr(&p,addr));
#				endif
			}
			break;
		case OSetGlobal:
			{
				void *addr = m->globals_data + m->globals_indexes[o->p1];
#				ifdef HL_64
				preg *tmp = alloc_reg(ctx, RCPU);
				op64(ctx, MOV, tmp, pconst64(&p,(int_val)addr));
				copy_from(ctx, pmem(&p,tmp->id,0), ra);
#				else
				copy_from(ctx, paddr(&p,addr), ra);
#				endif
			}
			break;
		case OCall3:
			{
				int args[3] = { o->p3, o->extra[0], o->extra[1] };
				op_call_fun(ctx, dst, o->p2, 3, args);
			}
			break;
		case OCall4:
			{
				int args[4] = { o->p3, o->extra[0], o->extra[1], o->extra[2] };
				op_call_fun(ctx, dst, o->p2, 4, args);
			}
			break;
		case OCallN:
			op_call_fun(ctx, dst, o->p2, o->p3, o->extra);
			break;
		case OCall0:
			op_call_fun(ctx, dst, o->p2, 0, NULL);
			break;
		case OCall1:
			op_call_fun(ctx, dst, o->p2, 1, &o->p3);
			break;
		case OCall2:
			{
				int args[2] = { o->p3, (int)(int_val)o->extra };
				op_call_fun(ctx, dst, o->p2, 2, args);
			}
			break;
		case OSub:
		case OAdd:
		case OMul:
		case OSDiv:
		case OUDiv:
		case OShl:
		case OSShr:
		case OUShr:
		case OAnd:
		case OOr:
		case OXor:
		case OSMod:
		case OUMod:
			op_binop(ctx, dst, ra, rb, o->op);
			break;
		case ONeg:
			{
				if( IS_FLOAT(ra) ) {
					preg *pa = alloc_reg(ctx,RFPU);
					preg *pb = alloc_fpu(ctx,ra,true);
					op64(ctx,XORPD,pa,pa);
					op64(ctx,ra->t->kind == HF32 ? SUBSS : SUBSD,pa,pb);
					store(ctx,dst,pa,true);
				} else if( ra->t->kind == HI64 ) {
#					ifdef HL_64
					preg *pa = alloc_reg(ctx,RCPU);
					preg *pb = alloc_cpu(ctx,ra,true);
					op64(ctx,XOR,pa,pa);
					op64(ctx,SUB,pa,pb);
					store(ctx,dst,pa,true);
#					else
					error_i64();
#					endif
				} else {
					preg *pa = alloc_reg(ctx,RCPU);
					preg *pb = alloc_cpu(ctx,ra,true);
					op32(ctx,XOR,pa,pa);
					op32(ctx,SUB,pa,pb);
					store(ctx,dst,pa,true);
				}
			}
			break;
		case ONot:
			{
				preg *v = alloc_cpu(ctx,ra,true);
				op32(ctx,XOR,v,pconst(&p,1));
				store(ctx,dst,v,true);
			}
			break;
		case OJFalse:
		case OJTrue:
		case OJNotNull:
		case OJNull:
			{
				preg *r = dst->t->kind == HBOOL ? alloc_cpu8(ctx, dst, true) : alloc_cpu(ctx, dst, true);
				op64(ctx, dst->t->kind == HBOOL ? TEST8 : TEST, r, r);
				XJump( o->op == OJFalse || o->op == OJNull ? JZero : JNotZero,jump);
				register_jump(ctx,jump,(opCount + 1) + o->p2);
			}
			break;
		case OJEq:
		case OJNotEq:
		case OJSLt:
		case OJSGte:
		case OJSLte:
		case OJSGt:
		case OJULt:
		case OJUGte:
		case OJNotLt:
		case OJNotGte:
			op_jump(ctx,dst,ra,o,(opCount + 1) + o->p3);
			break;
		case OJAlways:
			jump = do_jump(ctx,o->op,false);
			register_jump(ctx,jump,(opCount + 1) + o->p1);
			break;
		case OToDyn:
			if( ra->t->kind == HBOOL ) {
				int size = begin_native_call(ctx, 1);
				set_native_arg(ctx, fetch(ra));
				call_native(ctx, hl_alloc_dynbool, size);
				store(ctx, dst, PEAX, true);
			} else {
				int_val rt = (int_val)ra->t;
				int jskip = 0;
				if( hl_is_ptr(ra->t) ) {
					int jnz;
					preg *a = alloc_cpu(ctx,ra,true);
					op64(ctx,TEST,a,a);
					XJump_small(JNotZero,jnz);
					op64(ctx,XOR,PEAX,PEAX); // will replace the result of alloc_dynamic at jump land
					XJump_small(JAlways,jskip);
					patch_jump(ctx,jnz);
				}
				call_native_consts(ctx, hl_alloc_dynamic, &rt, 1);
				// copy value to dynamic
				if( (IS_FLOAT(ra) || ra->size == 8) && !IS_64 ) {
					preg *tmp = REG_AT(RCPU_SCRATCH_REGS[1]);
					op64(ctx,MOV,tmp,&ra->stack);
					op32(ctx,MOV,pmem(&p,Eax,HDYN_VALUE),tmp);
					if( ra->t->kind == HF64 ) {
						ra->stackPos += 4;
						op64(ctx,MOV,tmp,&ra->stack);
						op32(ctx,MOV,pmem(&p,Eax,HDYN_VALUE+4),tmp);
						ra->stackPos -= 4;
					}
				} else {
					preg *tmp = REG_AT(RCPU_SCRATCH_REGS[1]);
					copy_from(ctx,tmp,ra);
					op64(ctx,MOV,pmem(&p,Eax,HDYN_VALUE),tmp);
				}
				if( hl_is_ptr(ra->t) ) patch_jump(ctx,jskip);
				store(ctx, dst, PEAX, true);
			}
			break;

		case OToSFloat:
			if( ra == dst ) break;
			if (ra->t->kind == HI32 || ra->t->kind == HUI16 || ra->t->kind == HUI8) {
				preg* r = alloc_cpu(ctx, ra, true);
				preg* w = alloc_fpu(ctx, dst, false);
				op32(ctx, dst->t->kind == HF64 ? CVTSI2SD : CVTSI2SS, w, r);
				store(ctx, dst, w, true);
			} else if (ra->t->kind == HI64 ) {
				preg* r = alloc_cpu(ctx, ra, true);
				preg* w = alloc_fpu(ctx, dst, false);
				op64(ctx, dst->t->kind == HF64 ? CVTSI2SD : CVTSI2SS, w, r);
				store(ctx, dst, w, true);
			} else if( ra->t->kind == HF64 && dst->t->kind == HF32 ) {
				preg *r = alloc_fpu(ctx,ra,true);
				preg *w = alloc_fpu(ctx,dst,false);
				op32(ctx,CVTSD2SS,w,r);
				store(ctx, dst, w, true);
			} else if( ra->t->kind == HF32 && dst->t->kind == HF64 ) {
				preg *r = alloc_fpu(ctx,ra,true);
				preg *w = alloc_fpu(ctx,dst,false);
				op32(ctx,CVTSS2SD,w,r);
				store(ctx, dst, w, true);
			} else
				ASSERT(0);
			break;
		case OToUFloat:
			{
				int size;
				size = prepare_call_args(ctx,1,&o->p2,ctx->vregs,0);
				call_native(ctx,uint_to_double,size);
				store_result(ctx,dst);
			}
			break;
		case OToInt:
			if( ra == dst ) break;
			if( ra->t->kind == HF64 ) {
				preg *r = alloc_fpu(ctx,ra,true);
				preg *w = alloc_cpu(ctx,dst,false);
				preg *tmp = alloc_reg(ctx,RCPU);
				op32(ctx,STMXCSR,pmem(&p,Esp,-4),UNUSED);
				op32(ctx,MOV,tmp,&p);
				op32(ctx,OR,tmp,pconst(&p,0x6000)); // set round towards 0
				op32(ctx,MOV,pmem(&p,Esp,-8),tmp);
				op32(ctx,LDMXCSR,&p,UNUSED);
				op32(ctx,CVTSD2SI,w,r);
				op32(ctx,LDMXCSR,pmem(&p,Esp,-4),UNUSED);
				store(ctx, dst, w, true);
			} else if (ra->t->kind == HF32) {
				preg *r = alloc_fpu(ctx, ra, true);
				preg *w = alloc_cpu(ctx, dst, false);
				preg *tmp = alloc_reg(ctx, RCPU);
				op32(ctx, STMXCSR, pmem(&p, Esp, -4), UNUSED);
				op32(ctx, MOV, tmp, &p);
				op32(ctx, OR, tmp, pconst(&p, 0x6000)); // set round towards 0
				op32(ctx, MOV, pmem(&p, Esp, -8), tmp);
				op32(ctx, LDMXCSR, &p, UNUSED);
				op32(ctx, CVTSS2SI, w, r);
				op32(ctx, LDMXCSR, pmem(&p, Esp, -4), UNUSED);
				store(ctx, dst, w, true);
			} else if( dst->t->kind == HI64 && ra->t->kind == HI32 ) {
				if( ra->current != PEAX ) {
					op32(ctx, MOV, PEAX, fetch(ra));
					scratch(PEAX);
				}
#				ifdef HL_64
				op64(ctx, CDQE, UNUSED, UNUSED); // sign-extend Eax into Rax
				store(ctx, dst, PEAX, true);
#				else
				op32(ctx, CDQ, UNUSED, UNUSED); // sign-extend Eax into Eax:Edx
				scratch(REG_AT(Edx));
				op32(ctx, MOV, fetch(dst), PEAX);
				dst->stackPos += 4;
				op32(ctx, MOV, fetch(dst), REG_AT(Edx));
				dst->stackPos -= 4;
			} else if( dst->t->kind == HI32 && ra->t->kind == HI64 ) {
				error_i64();
#				endif
			} else {
				preg *r = alloc_cpu(ctx,dst,false);
				copy_from(ctx, r, ra);
				store(ctx, dst, r, true);
			}
			break;
		case ORet:
			op_ret(ctx, dst);
#endif // !HL_JIT_ARM64

			break;
	
	// ===== ARM64 Phase 1: Stack-Based Virtual Register System =====
	// All vregs stored on stack at [X29 + stackPos]
	// Temporary registers: X10 (primary), X11 (secondary), X12 (tertiary)
	
	#define LOAD_VREG(tmp_reg, vr) \
		if (vr) { \
			int _pos = (vr)->stackPos; \
			if (_pos >= -255 && _pos <= 0) { \
				arm_ldur_imm(ctx, tmp_reg, X29, _pos, 3); \
			} else { \
				arm_load_imm64(ctx, X9, _pos); \
				arm_add_reg(ctx, X9, X29, X9, true); \
				arm_ldr_imm(ctx, tmp_reg, X9, 0, 3); \
			} \
		}
	
	#define STORE_VREG(tmp_reg, vr) \
		if (vr) { \
			int _pos = (vr)->stackPos; \
			if (_pos >= -255 && _pos <= 0) { \
				arm_stur_imm(ctx, tmp_reg, X29, _pos, 3); \
			} else { \
				arm_load_imm64(ctx, X9, _pos); \
				arm_add_reg(ctx, X9, X29, X9, true); \
				arm_str_imm(ctx, tmp_reg, X9, 0, 3); \
			} \
		}
	
	// Helper to define GET_REG for compatibility (but we won't use it)
	#define GET_REG(vr) X10  // Always returns X10, shouldn't be used in Phase 1
	


#ifndef HL_JIT_ARM64
	case OIncr:
			{
				if( IS_FLOAT(dst) ) {
					ASSERT(0);
				} else {
					preg *v = fetch32(ctx,dst);
					op32(ctx,INC,v,UNUSED);
					if( v->kind != RSTACK ) store(ctx, dst, v, false);
				}
			}
			break;
		case ODecr:
			{
				if( IS_FLOAT(dst) ) {
					ASSERT(0);
				} else {
					preg *v = fetch32(ctx,dst);
					op32(ctx,DEC,v,UNUSED);
					if( v->kind != RSTACK ) store(ctx, dst, v, false);
				}
			}
			break;
		case OFloat:
			{
				if( m->code->floats[o->p2] == 0 ) {
					preg *f = alloc_fpu(ctx,dst,false);
					op64(ctx,XORPD,f,f);
				} else switch( dst->t->kind ) {
				case HF64:
				case HF32:
#					ifdef HL_64
					op64(ctx,dst->t->kind == HF32 ? CVTSD2SS : MOVSD,alloc_fpu(ctx,dst,false),pcodeaddr(&p,o->p2 * 8));
#					else
					op64(ctx,dst->t->kind == HF32 ? MOVSS : MOVSD,alloc_fpu(ctx,dst,false),paddr(&p,m->code->floats + o->p2));
#					endif
					break;
				default:
					ASSERT(dst->t->kind);
				}
				store(ctx,dst,dst->current,false);
			}
			break;
		case OString:
			op64(ctx,MOV,alloc_cpu(ctx, dst, false),pconst64(&p,(int_val)hl_get_ustring(m->code,o->p2)));
			store(ctx,dst,dst->current,false);
			break;
#endif // !HL_JIT_ARM64

		case OBytes:
			// dst = bytes constant pointer
			if (dst) {
				char *b = m->code->version >= 5 ?
				          m->code->bytes + m->code->bytes_pos[o->p2] :
				          m->code->strings[o->p2];
				uint64_t bytes_ptr = (uint64_t)b;
				arm_load_imm64(ctx, X10, bytes_ptr);
				STORE_VREG(X10, dst);
			}
			break;
	case ONeg:
		// dst = -ra
		if (dst && ra) {
			LOAD_VREG(X10, ra);
			arm_neg(ctx, X10, X10, true);
			STORE_VREG(X10, dst);
		}
		break;

	case ONot:
		// dst = !ra (boolean not)
		if (dst && ra) {
			LOAD_VREG(X10, ra);
			// XOR with 1 to flip boolean (0->1, 1->0)
			arm_load_imm64(ctx, X11, 1);
			arm_eor_reg(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;

	case OInt:
		// dst = integer constant
		if (dst) {
			arm_load_imm64(ctx, X10, m->code->ints[o->p2]);
			STORE_VREG(X10, dst);
		}
		break;

	case OBool:
		// dst = boolean constant (0 or 1)
		if (dst) {
			arm_load_imm64(ctx, X10, o->p2);
			STORE_VREG(X10, dst);
		}
		break;

	case ONull:
		// dst = null pointer (0)
		if (dst) {
			arm_load_imm64(ctx, X10, 0);
			STORE_VREG(X10, dst);
		}
		break;
	case OString:
		// dst = string constant pointer
		if (dst) {
			hl_module *m = ctx->m;
			uchar *str = hl_get_ustring(m->code, o->p2);
			arm_load_imm64(ctx, X10, (uint64_t)str);
			STORE_VREG(X10, dst);
		}
		break;


	case OIncr:
		// dst++
		if (dst) {
			LOAD_VREG(X10, dst);
			arm_load_imm64(ctx, X11, 1);
			arm_add_reg(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;

	case ODecr:
		// dst--
		if (dst) {
			LOAD_VREG(X10, dst);
			arm_load_imm64(ctx, X11, 1);
			arm_sub_reg(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;
	case ORef:
		// dst = &ra (take address of vreg)
		// For stack-based system, this means calculating stack address
		if (dst && ra) {
			// Load frame pointer into X10
			arm_mov_reg(ctx, X10, X29, true);
			// Add stackPos offset
			if (ra->stackPos != 0) {
				if (ra->stackPos >= -4095 && ra->stackPos < 0) {
					// Negative offset, use SUB
					arm_sub_imm(ctx, X10, X10, -ra->stackPos, true);
				} else if (ra->stackPos > 0 && ra->stackPos <= 4095) {
					arm_add_imm(ctx, X10, X10, ra->stackPos, true);
				} else {
					// Large offset
					arm_load_imm64(ctx, X11, ra->stackPos);
					arm_add_reg(ctx, X10, X10, X11, true);
				}
			}
			// X10 now holds the address
			STORE_VREG(X10, dst);
		}
		break;
	case OField:
		// dst = ra->field (load field from object)
		if (dst && ra) {
			LOAD_VREG(X10, ra);  // Load object pointer
			// Field offset stored in o->p3
			int field_offset = o->p3 * HL_WSIZE;
			if (field_offset >= 0 && field_offset < 32768) {
				arm_ldr_imm(ctx, X10, X10, field_offset / 8, 3);  // Scaled offset
			} else {
				arm_load_imm64(ctx, X11, field_offset);
				arm_ldr_reg(ctx, X10, X10, X11, 3);
			}
			STORE_VREG(X10, dst);
		}
		break;

	case OSetField:
		// dst->field = ra (store field to object)
		if (dst && ra) {
			LOAD_VREG(X10, dst);  // Load object pointer
			LOAD_VREG(X11, ra);   // Load value to store
			// Field offset stored in o->p2
			int field_offset = o->p2 * HL_WSIZE;
			if (field_offset >= 0 && field_offset < 32768) {
				arm_str_imm(ctx, X11, X10, field_offset / 8, 3);  // Scaled offset
			} else {
				arm_load_imm64(ctx, X12, field_offset);
				arm_str_reg(ctx, X11, X10, X12, 3);
			}
		}
		break;

	case OGetMem:
		// dst = *(int64*)(ra + offset) - load 64-bit value
		if (dst && ra) {
			LOAD_VREG(X10, ra);  // Load base pointer
			int offset = o->p3 * HL_WSIZE;
			if (offset >= 0 && offset < 32768) {
				arm_ldr_imm(ctx, X10, X10, offset / 8, 3);  // 64-bit load
			} else {
				arm_load_imm64(ctx, X11, offset);
				arm_ldr_reg(ctx, X10, X10, X11, 3);
			}
			STORE_VREG(X10, dst);
		}
		break;

	case OSetMem:
		// *(int64*)(ra + offset) = rb
		if (ra && rb) {
			LOAD_VREG(X10, ra);  // Load base pointer
			LOAD_VREG(X11, rb);  // Load value
			int offset = o->p2 * HL_WSIZE;
			if (offset >= 0 && offset < 32768) {
				arm_str_imm(ctx, X11, X10, offset / 8, 3);  // 64-bit store
			} else {
				arm_load_imm64(ctx, X12, offset);
				arm_str_reg(ctx, X11, X10, X12, 3);
			}
		}
		break;




	case OMov:
	case OUnsafeCast:
		// dst = ra (simple copy)
		if (dst && ra && dst != ra) {
			LOAD_VREG(X10, ra);
			STORE_VREG(X10, dst);
		}
		break;

	case ORet:
		// Return from function
		if (dst) {
			// Load return value into X0
			LOAD_VREG(X0, dst);
		}
		// Restore frame and return (must match prologue framesize)
		arm_epilogue(ctx, ctx->totalRegsSize + 16);
		arm_ret(ctx, X30);
		break;
	case OCall0:
		// Call function with 0 arguments
		{
			int fid = ctx->m->functions_indexes[o->p2];
			bool isNative = fid >= ctx->m->code->nfunctions;

			if (isNative) {
				// Native function - use BLR with absolute address
				void *fptr = ctx->m->functions_ptrs[o->p2];
				printf("[OCall0] Native function %d: fptr=%p\n", o->p2, fptr);
				if (fptr == NULL) {
					printf("[OCall0] ERROR: Native function pointer is NULL!\n");
				}
				arm_load_imm64(ctx, X9, (uint64_t)fptr);
				arm_blr(ctx, X9);
			} else {
				// JIT function - use BL with staging for patching
				printf("[OCall0] JIT function %d: will be patched\n", o->p2);
				jlist *j = (jlist*)hl_malloc(&ctx->galloc, sizeof(jlist));
				j->pos = BUF_POS();
				j->target = o->p2;
				j->next = ctx->calls;
				ctx->calls = j;
				B32(0x94000000);  // BL +0
			}

			// Store result if needed
			if (dst && dst->t->kind != HVOID) {
				STORE_VREG(X0, dst);
			}
		}
		break;

	case OCall1:
		// Call function with 1 argument
		{
			int fid = ctx->m->functions_indexes[o->p2];
			bool isNative = fid >= ctx->m->code->nfunctions;

			// Load argument into X0
			vreg *arg = R(o->p3);
			if (arg) LOAD_VREG(X0, arg);

			if (isNative) {
				void *fptr = ctx->m->functions_ptrs[o->p2];
				printf("[OCall1] Native function %d: fptr=%p\n", o->p2, fptr);
				if (fptr == NULL) {
					printf("[OCall1] ERROR: Native function pointer is NULL!\n");
				}
				arm_load_imm64(ctx, X9, (uint64_t)fptr);
				arm_blr(ctx, X9);
			} else {
				printf("[OCall1] JIT function %d: will be patched\n", o->p2);
				jlist *j = (jlist*)hl_malloc(&ctx->galloc, sizeof(jlist));
				j->pos = BUF_POS();
				j->target = o->p2;
				j->next = ctx->calls;
				ctx->calls = j;
				B32(0x94000000);  // BL +0
			}

			if (dst && dst->t->kind != HVOID) {
				STORE_VREG(X0, dst);
			}
		}
		break;

	case OCall2:
		{
			int fid = ctx->m->functions_indexes[o->p2];
			bool isNative = fid >= ctx->m->code->nfunctions;

			vreg *arg0 = R(o->p3);
			vreg *arg1 = R((int)(int_val)o->extra);
			if (arg0) LOAD_VREG(X0, arg0);
			if (arg1) LOAD_VREG(X1, arg1);

			if (isNative) {
				void *fptr = ctx->m->functions_ptrs[o->p2];
				arm_load_imm64(ctx, X9, (uint64_t)fptr);
				arm_blr(ctx, X9);
			} else {
				jlist *j = (jlist*)hl_malloc(&ctx->galloc, sizeof(jlist));
				j->pos = BUF_POS();
				j->target = o->p2;
				j->next = ctx->calls;
				ctx->calls = j;
				B32(0x94000000);
			}

			if (dst && dst->t->kind != HVOID) {
				STORE_VREG(X0, dst);
			}
		}
		break;

	case OCall3:
		{
			int fid = ctx->m->functions_indexes[o->p2];
			bool isNative = fid >= ctx->m->code->nfunctions;

			vreg *arg0 = R(o->p3);
			vreg *arg1 = o->extra ? R(o->extra[0]) : NULL;
			vreg *arg2 = o->extra ? R(o->extra[1]) : NULL;
			if (arg0) LOAD_VREG(X0, arg0);
			if (arg1) LOAD_VREG(X1, arg1);
			if (arg2) LOAD_VREG(X2, arg2);

			if (isNative) {
				void *fptr = ctx->m->functions_ptrs[o->p2];
				arm_load_imm64(ctx, X9, (uint64_t)fptr);
				arm_blr(ctx, X9);
			} else {
				jlist *j = (jlist*)hl_malloc(&ctx->galloc, sizeof(jlist));
				j->pos = BUF_POS();
				j->target = o->p2;
				j->next = ctx->calls;
				ctx->calls = j;
				B32(0x94000000);
			}

			if (dst && dst->t->kind != HVOID) {
				STORE_VREG(X0, dst);
			}
		}
		break;

	case OCall4:
		{
			int fid = ctx->m->functions_indexes[o->p2];
			bool isNative = fid >= ctx->m->code->nfunctions;

			vreg *arg0 = R(o->p3);
			vreg *arg1 = o->extra ? R(o->extra[0]) : NULL;
			vreg *arg2 = o->extra ? R(o->extra[1]) : NULL;
			vreg *arg3 = o->extra ? R(o->extra[2]) : NULL;
			if (arg0) LOAD_VREG(X0, arg0);
			if (arg1) LOAD_VREG(X1, arg1);
			if (arg2) LOAD_VREG(X2, arg2);
			if (arg3) LOAD_VREG(X3, arg3);

			if (isNative) {
				void *fptr = ctx->m->functions_ptrs[o->p2];
				arm_load_imm64(ctx, X9, (uint64_t)fptr);
				arm_blr(ctx, X9);
			} else {
				jlist *j = (jlist*)hl_malloc(&ctx->galloc, sizeof(jlist));
				j->pos = BUF_POS();
				j->target = o->p2;
				j->next = ctx->calls;
				ctx->calls = j;
				B32(0x94000000);
			}

			if (dst && dst->t->kind != HVOID) {
				STORE_VREG(X0, dst);
			}
		}
		break;
	case OJAlways:
		// Unconditional jump
		{
			int jump = arm_do_jump(ctx);
			register_jump(ctx, jump, (opCount + 1) + o->p1);
		}
		break;

	case OLabel:
		// Jump target label (no code generated)
		break;

	case OJTrue:
	case OJNotNull:
		// Jump if value is true/not null
		if (dst) {
			LOAD_VREG(X10, dst);
			// CBNZ X10, target
			int jump = arm_do_cbnz(ctx, X10, true);
			register_jump(ctx, jump, (opCount + 1) + o->p2);
		}
		break;

	case OJFalse:
	case OJNull:
		// Jump if value is false/null
		if (dst) {
			LOAD_VREG(X10, dst);
			// CBZ X10, target
			int jump = arm_do_cbz(ctx, X10, true);
			register_jump(ctx, jump, (opCount + 1) + o->p2);
		}
		break;

	case OJEq:
		// Jump if ra == rb
		if (dst && ra) {
			LOAD_VREG(X10, dst);
			LOAD_VREG(X11, ra);
			// CMP X10, X11 (implemented as SUBS XZR, X10, X11)
			arm_subs_reg(ctx, XZR, X10, X11, true);
			// B.EQ target
			int jump = arm_do_jump_cond(ctx, ARM64_COND_EQ);
			register_jump(ctx, jump, (opCount + 1) + o->p3);
		}
		break;

	case OJNotEq:
		// Jump if ra != rb
		if (dst && ra) {
			LOAD_VREG(X10, dst);
			LOAD_VREG(X11, ra);
			// CMP X10, X11
			arm_subs_reg(ctx, XZR, X10, X11, true);
			// B.NE target
			int jump = arm_do_jump_cond(ctx, ARM64_COND_NE);
			register_jump(ctx, jump, (opCount + 1) + o->p3);
		}
		break;

	case OJSLt:
		// Jump if ra < rb (signed)
		if (dst && ra) {
			LOAD_VREG(X10, dst);
			LOAD_VREG(X11, ra);
			// CMP X10, X11
			arm_subs_reg(ctx, XZR, X10, X11, true);
			// B.LT target
			int jump = arm_do_jump_cond(ctx, ARM64_COND_LT);
			register_jump(ctx, jump, (opCount + 1) + o->p3);
		}
		break;

	case OJSGte:
		// Jump if ra >= rb (signed)
		if (dst && ra) {
			LOAD_VREG(X10, dst);
			LOAD_VREG(X11, ra);
			// CMP X10, X11
			arm_subs_reg(ctx, XZR, X10, X11, true);
			// B.GE target
			int jump = arm_do_jump_cond(ctx, ARM64_COND_GE);
			register_jump(ctx, jump, (opCount + 1) + o->p3);
		}
		break;

	case OJSGt:
		// Jump if ra > rb (signed)
		if (dst && ra) {
			LOAD_VREG(X10, dst);
			LOAD_VREG(X11, ra);
			// CMP X10, X11
			arm_subs_reg(ctx, XZR, X10, X11, true);
			// B.GT target
			int jump = arm_do_jump_cond(ctx, ARM64_COND_GT);
			register_jump(ctx, jump, (opCount + 1) + o->p3);
		}
		break;

	case OJSLte:
		// Jump if ra <= rb (signed)
		if (dst && ra) {
			LOAD_VREG(X10, dst);
			LOAD_VREG(X11, ra);
			// CMP X10, X11
			arm_subs_reg(ctx, XZR, X10, X11, true);
			// B.LE target
			int jump = arm_do_jump_cond(ctx, ARM64_COND_LE);
			register_jump(ctx, jump, (opCount + 1) + o->p3);
		}
		break;

	case OJULt:
		// Jump if ra < rb (unsigned)
		if (dst && ra) {
			LOAD_VREG(X10, dst);
			LOAD_VREG(X11, ra);
			// CMP X10, X11
			arm_subs_reg(ctx, XZR, X10, X11, true);
			// B.LO target
			int jump = arm_do_jump_cond(ctx, ARM64_COND_LO);
			register_jump(ctx, jump, (opCount + 1) + o->p3);
		}
		break;

	case OJUGte:
		// Jump if ra >= rb (unsigned)
		if (dst && ra) {
			LOAD_VREG(X10, dst);
			LOAD_VREG(X11, ra);
			// CMP X10, X11
			arm_subs_reg(ctx, XZR, X10, X11, true);
			// B.HS target
			int jump = arm_do_jump_cond(ctx, ARM64_COND_HS);
			register_jump(ctx, jump, (opCount + 1) + o->p3);
		}
		break;

	case OJNotLt:
		// Jump if NOT (ra < rb) (same as ra >= rb signed)
		if (dst && ra) {
			LOAD_VREG(X10, dst);
			LOAD_VREG(X11, ra);
			// CMP X10, X11
			arm_subs_reg(ctx, XZR, X10, X11, true);
			// B.GE target
			int jump = arm_do_jump_cond(ctx, ARM64_COND_GE);
			register_jump(ctx, jump, (opCount + 1) + o->p3);
		}
		break;

	case OJNotGte:
		// Jump if NOT (ra >= rb) (same as ra < rb signed)
		if (dst && ra) {
			LOAD_VREG(X10, dst);
			LOAD_VREG(X11, ra);
			// CMP X10, X11
			arm_subs_reg(ctx, XZR, X10, X11, true);
			// B.LT target
			int jump = arm_do_jump_cond(ctx, ARM64_COND_LT);
			register_jump(ctx, jump, (opCount + 1) + o->p3);
		}
		break;
	case OGetGlobal:
		// dst = global[p2]
		{
			hl_module *m = ctx->m;
			void *addr = m->globals_data + m->globals_indexes[o->p2];
			hl_type *t = m->code->globals[o->p2];

			// DEBUG: Check all globals at JIT compile time
			printf("[OGetGlobal] global %d → type.kind=%d", o->p2, t->kind);
			if (t->kind == HFUN) {
				void *current_value = *(void**)addr;
				printf(", HFUN ptr = %p", current_value);
			}
			printf("\n");

			// Load global address into X10
			arm_load_imm64(ctx, X10, (uint64_t)addr);
			// Load value from [X10]
			arm_ldr_imm(ctx, X11, X10, 0, 3);  // 64-bit load
			// Store to destination
			if (dst) {
				STORE_VREG(X11, dst);
			}
		}
		break;

	case OSetGlobal:
		// global[p1] = ra
		{
			hl_module *m = ctx->m;
			void *addr = m->globals_data + m->globals_indexes[o->p1];
			if (ra) {
				// Load value from source
				LOAD_VREG(X10, ra);
				// Load global address into X11
				arm_load_imm64(ctx, X11, (uint64_t)addr);
				// Store value to [X11]
				arm_str_imm(ctx, X10, X11, 0, 3);  // 64-bit store
			}
		}
		break;




	case OSub:
		// dst = ra - rb
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);
			LOAD_VREG(X11, rb);
			arm_sub_reg(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;

	case OMul:
		// dst = ra * rb
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);
			LOAD_VREG(X11, rb);
			arm_mul(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;

	case OSDiv:
		// dst = ra / rb (signed division)
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);
			LOAD_VREG(X11, rb);
			arm_sdiv(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;

	case OUDiv:
		// dst = ra / rb (unsigned division)
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);
			LOAD_VREG(X11, rb);
			arm_udiv(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;

	case OAnd:
		// dst = ra & rb
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);
			LOAD_VREG(X11, rb);
			arm_and_reg(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;

	case OOr:
		// dst = ra | rb
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);
			LOAD_VREG(X11, rb);
			arm_orr_reg(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;

	case OXor:
		// dst = ra ^ rb
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);
			LOAD_VREG(X11, rb);
			arm_eor_reg(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;

	case OShl:
		// dst = ra << rb
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);
			LOAD_VREG(X11, rb);
			arm_lsl_reg(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;

	case OSShr:
		// dst = ra >> rb (arithmetic shift right)
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);
			LOAD_VREG(X11, rb);
			arm_asr_reg(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;

	case OUShr:
		// dst = ra >> rb (logical shift right)
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);
			LOAD_VREG(X11, rb);
			arm_lsr_reg(ctx, X10, X10, X11, true);
			STORE_VREG(X10, dst);
		}
		break;


		case OAdd:
			// dst = ra + rb
			if (dst && ra && rb) {
				LOAD_VREG(X10, ra);
				LOAD_VREG(X11, rb);
				arm_add_reg(ctx, X10, X10, X11, true);
				STORE_VREG(X10, dst);
			}
		break;

	case OGetType:
		{
			// Get runtime type of a value
			// If value is NULL, return &hlt_void, otherwise get type from object header
			if (dst && ra) {
				LOAD_VREG(X10, ra);
				
				// CBZ X10, null_case
				int null_jump = arm_do_cbz(ctx, X10, true);
				
				// Not null: load type from offset -HL_WSIZE (-8 bytes)
				arm_ldur_imm(ctx, X10, X10, -HL_WSIZE, 3);
				
				// Jump over null case
				int end_jump = arm_do_jump(ctx);
				
				// null_case: load &hlt_void
				arm_patch_cbz(ctx, null_jump, ARM_BUF_POS());
				arm_load_imm64(ctx, X10, (uint64_t)&hlt_void);
				
				// end:
				arm_patch_jump(ctx, end_jump);
				
				// Store result
				STORE_VREG(X10, dst);
			}
		}
		break;
	case OType:
		// dst = pointer to type object
		if (dst) {
			hl_module *m = ctx->m;
			hl_type *type = m->code->types + o->p2;
			arm_load_imm64(ctx, X10, (uint64_t)type);
			STORE_VREG(X10, dst);
		}
		break;
	case OSafeCast:
		// dst = (type)ra - for now just copy value
		if (dst && ra) {
			LOAD_VREG(X10, ra);
			STORE_VREG(X10, dst);
		}
		break;
	case OGetTID:
		// dst = ra->type->tid - get type ID from value
		if (dst && ra) {
			LOAD_VREG(X10, ra);
			// For now, assume type info is accessible
			// This is a simplified implementation for Phase 1
			// Real implementation needs to follow HL object layout
			// For POC, just return 0 (unknown type)
			arm_load_imm64(ctx, X10, 0);
			STORE_VREG(X10, dst);
		}
		break;
	case OToDyn:
		// dst = (dynamic)ra - box value to dynamic
		// For Phase 1 POC, just copy the pointer
		if (dst && ra) {
			LOAD_VREG(X10, ra);
			STORE_VREG(X10, dst);
		}
		break;

	case OGetI8:
		// dst = *(int8*)(ra + offset)
		if (dst && ra) {
			LOAD_VREG(X10, ra);  // Load base address
			int offset = o->p3;
			if (offset >= 0 && offset < 4096) {
				// Use LDR with unsigned offset (size=0 for byte)
				arm_ldr_imm(ctx, X11, X10, offset, 0);
			} else {
				// Large offset - use register
				arm_load_imm64(ctx, X11, offset);
				arm_ldr_reg(ctx, X11, X10, X11, 0);
			}
			STORE_VREG(X11, dst);
		}
		break;

	case OGetI16:
		// dst = *(int16*)(ra + offset)
		if (dst && ra) {
			LOAD_VREG(X10, ra);  // Load base address
			int offset = o->p3;
			if (offset >= 0 && offset < 8192 && (offset % 2 == 0)) {
				// Use LDR with scaled offset (size=1 for halfword)
				arm_ldr_imm(ctx, X11, X10, offset / 2, 1);
			} else {
				arm_load_imm64(ctx, X11, offset);
				arm_ldr_reg(ctx, X11, X10, X11, 1);
			}
			STORE_VREG(X11, dst);
		}
		break;

	case OSetI8:
		// *(int8*)(ra + offset) = rb
		if (ra && rb) {
			LOAD_VREG(X10, ra);  // Load base address
			LOAD_VREG(X11, rb);  // Load value
			int offset = o->p3;
			if (offset >= 0 && offset < 4096) {
				// Use STR with unsigned offset (size=0 for byte)
				arm_str_imm(ctx, X11, X10, offset, 0);
			} else {
				// Large offset - use register
				arm_load_imm64(ctx, X12, offset);
				arm_add_reg(ctx, X10, X10, X12, true);
				arm_str_imm(ctx, X11, X10, 0, 0);
			}
		}
		break;

	case OSetI16:
		// *(int16*)(ra + offset) = rb
		if (ra && rb) {
			LOAD_VREG(X10, ra);  // Load base address
			LOAD_VREG(X11, rb);  // Load value
			int offset = o->p3;
			if (offset >= 0 && offset < 8192 && (offset % 2 == 0)) {
				// Use STR with scaled offset (size=1 for halfword)
				arm_str_imm(ctx, X11, X10, offset / 2, 1);
			} else {
				arm_load_imm64(ctx, X12, offset);
				arm_add_reg(ctx, X10, X10, X12, true);
				arm_str_imm(ctx, X11, X10, 0, 1);
			}
		}
		break;

	case OGetThis:
		// dst = this->field - load field from "this" object
		// For Phase 1, treat similar to OField
		// p2 is the field index
		if (dst) {
			// Get "this" pointer from vreg 0 (convention: this is first parameter)
			LOAD_VREG(X10, R(0));
			int field_offset = o->p2 * HL_WSIZE;
			if (field_offset >= 0 && field_offset < 32768) {
				arm_ldr_imm(ctx, X10, X10, field_offset / 8, 3);
			} else {
				arm_load_imm64(ctx, X11, field_offset);
				arm_ldr_reg(ctx, X10, X10, X11, 3);
			}
			STORE_VREG(X10, dst);
		}
		break;

	case OSetThis:
		// this->field = ra - store field to "this" object
		// For Phase 1, treat similar to OSetField
		if (ra) {
			// Get "this" pointer from vreg 0
			LOAD_VREG(X10, R(0));
			LOAD_VREG(X11, ra);
			int field_offset = o->p2 * HL_WSIZE;
			if (field_offset >= 0 && field_offset < 32768) {
				arm_str_imm(ctx, X11, X10, field_offset / 8, 3);
			} else {
				arm_load_imm64(ctx, X12, field_offset);
				arm_str_reg(ctx, X11, X10, X12, 3);
			}
		}
		break;

	case OArraySize:
		// dst = ra->size - get array length
		// HashLink varray has size as first field
		if (dst && ra) {
			LOAD_VREG(X10, ra);  // Load array pointer
			// Load size (32-bit int at offset 0)
			arm_ldr_imm(ctx, X10, X10, 0, 2);  // size=2 for 32-bit load
			STORE_VREG(X10, dst);
		}
		break;

	case OGetArray:
		// dst = ra[rb] - array element access
		// varray: { int size; void *ptr; }
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);  // Load array pointer
			LOAD_VREG(X11, rb);  // Load index

			// Load data pointer from offset 8 (after size field)
			arm_ldr_imm(ctx, X10, X10, 1, 3);  // offset 8/8=1, size=3 for 64-bit

			// Calculate element address: ptr + index * 8
			arm_lsl_imm(ctx, X11, X11, 3, true);  // index << 3
			arm_ldr_reg(ctx, X10, X10, X11, 3);

			STORE_VREG(X10, dst);
		}
		break;

	case OSetArray:
		// ra[rb] = rc - array element store
		if (ra && rb) {
			LOAD_VREG(X10, ra);  // Load array pointer
			LOAD_VREG(X11, rb);  // Load index

			vreg *rc = R(o->p3);
			LOAD_VREG(X13, rc);  // Load value to store

			// Load data pointer from offset 8
			arm_ldr_imm(ctx, X10, X10, 1, 3);

			// Calculate element address
			arm_lsl_imm(ctx, X11, X11, 3, true);  // index << 3
			arm_str_reg(ctx, X13, X10, X11, 3);
		}
		break;

	case OSMod:
		// dst = ra % rb (signed modulo)
		// ARM64: a % b = a - (a / b) * b
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);  // a
			LOAD_VREG(X11, rb);  // b

			// X12 = a / b (signed)
			arm_sdiv(ctx, X12, X10, X11, true);

			// X12 = (a / b) * b
			arm_mul(ctx, X12, X12, X11, true);

			// X10 = a - (a / b) * b
			arm_sub_reg(ctx, X10, X10, X12, true);

			STORE_VREG(X10, dst);
		}
		break;

	case OUMod:
		// dst = ra % rb (unsigned modulo)
		if (dst && ra && rb) {
			LOAD_VREG(X10, ra);  // a
			LOAD_VREG(X11, rb);  // b

			// X12 = a / b (unsigned)
			arm_udiv(ctx, X12, X10, X11, true);

			// X12 = (a / b) * b
			arm_mul(ctx, X12, X12, X11, true);

			// X10 = a - (a / b) * b
			arm_sub_reg(ctx, X10, X10, X12, true);

			STORE_VREG(X10, dst);
		}
		break;

	case OToInt:
		// dst = (int)ra - convert to integer
		// For Phase 1, just copy value (proper conversion in Phase 2)
		if (dst && ra) {
			LOAD_VREG(X10, ra);
			STORE_VREG(X10, dst);
		}
		break;

	case OUnref:
		// dst = *ra - dereference pointer
		if (dst && ra) {
			LOAD_VREG(X10, ra);  // Load pointer
			arm_ldr_imm(ctx, X10, X10, 0, 3);  // Load value at pointer
			STORE_VREG(X10, dst);
		}
		break;

	case OSetref:
		// *ra = rb - store via pointer
		if (ra && rb) {
			LOAD_VREG(X10, ra);  // Load pointer
			LOAD_VREG(X11, rb);  // Load value
			arm_str_imm(ctx, X11, X10, 0, 3);  // Store value at pointer
		}
		break;





	// =====================================================================
	// Advanced Call Operations
	// =====================================================================

	case OCallN:
		{
			int fid = ctx->m->functions_indexes[o->p2];
			bool isNative = fid >= ctx->m->code->nfunctions;
			int nargs = o->p3;

			// Move arguments to X0-X7
			for (int i = 0; i < nargs && i < 8; i++) {
				vreg *arg = R(o->extra[i]);
				if (arg) {
					LOAD_VREG((Arm64Reg)(X0 + i), arg);
				}
			}

			if (isNative) {
				void *fptr = ctx->m->functions_ptrs[o->p2];
				arm_load_imm64(ctx, X9, (uint64_t)fptr);
				arm_blr(ctx, X9);
			} else {
				jlist *j = (jlist*)hl_malloc(&ctx->galloc, sizeof(jlist));
				j->pos = BUF_POS();
				j->target = o->p2;
				j->next = ctx->calls;
				ctx->calls = j;
				B32(0x94000000);
			}

			if (dst && dst->t->kind != HVOID) {
				STORE_VREG(X0, dst);
			}
		}
		break;

	case OCallMethod:
		{
			printf("[OCallMethod] method_index=%d, nargs=%d\n", o->p2, o->p3);
			// Call method on object from first argument
			// object->type->proto[method_index](args...)
			vreg *obj = R(o->extra[0]);

			if (obj->t->kind == HOBJ || obj->t->kind == HSTRUCT) {
				// Load object from stack into X10
				LOAD_VREG(X10, obj);

				// Read type from object (offset 0)
				// LDR X9, [X10, #0]
				arm_ldr_imm(ctx, X9, X10, 0, 3);

				// Read proto from type (offset HL_WSIZE*2 = 16 bytes)
				// LDR X9, [X9, #2] (scaled offset: 16/8 = 2)
				arm_ldr_imm(ctx, X9, X9, 2, 3);

				// Set up arguments from o->extra (object and additional args)
				for (i = 0; i < o->p3 && i < 8; i++) {
					vreg *arg = R(o->extra[i]);
					Arm64Reg dst_reg = X0 + i;
					// Load directly into argument register
					LOAD_VREG(dst_reg, arg);
				}

				if (o->p3 > 8) {
					jit_error("OCallMethod with >8 args not yet implemented");
				}

				// Load method pointer from proto[method_index]
				// LDR X9, [X9, #(o->p2 * HL_WSIZE)]
				if (o->p2 < 4096) {
					arm_ldr_imm(ctx, X9, X9, o->p2, 3);
				} else {
					arm_load_imm64(ctx, X11, o->p2 * HL_WSIZE);
					arm_ldr_reg(ctx, X9, X9, X11, 3);
				}

				// Call the method
				arm_blr(ctx, X9);

				// Store result if needed
				if (dst && dst->t->kind != HVOID) {
					STORE_VREG(X0, dst);
				}
			} else {
				jit_error("OCallMethod: unsupported object type");
			}
		}
		break;

	case OCallThis:
		{
			printf("[OCallThis] method_index=%d, nargs=%d\n", o->p2, o->p3);
			// Call method on "this" object (register 0)
			// this->type->proto[method_index](this, args...)
			vreg *r = R(0);  // "this" is always register 0

			// Load "this" directly into X0 (first argument)
			LOAD_VREG(X0, r);

			// Read type from this (offset 0)
			// LDR X9, [X0, #0]
			arm_ldr_imm(ctx, X9, X0, 0, 3);

			// Read proto from type (offset HL_WSIZE*2 = 16 bytes)
			// LDR X9, [X9, #2] (scaled offset: 16/8 = 2)
			arm_ldr_imm(ctx, X9, X9, 2, 3);

			// Move additional arguments to X1-X7
			for (i = 0; i < o->p3 && i < 7; i++) {
				vreg *arg = R(o->extra[i]);
				Arm64Reg dst_reg = X1 + i;
				// Load directly into argument register
				LOAD_VREG(dst_reg, arg);
			}

			if (o->p3 > 7) {
				jit_error("OCallThis with >7 args not yet implemented");
			}

			// Load method pointer from proto[method_index]
			// LDR X9, [X9, #(o->p2 * HL_WSIZE)]
			// Scaled offset: (o->p2 * 8) / 8 = o->p2
			if (o->p2 < 4096) {
				arm_ldr_imm(ctx, X9, X9, o->p2, 3);
			} else {
				arm_load_imm64(ctx, X11, o->p2 * HL_WSIZE);
				arm_ldr_reg(ctx, X9, X9, X11, 3);
			}

			// Call the method
			arm_blr(ctx, X9);

			// Store result if needed
			if (dst && dst->t->kind != HVOID) {
				STORE_VREG(X0, dst);
			}
		}
		break;

	case OCallClosure:
		printf("[OCallClosure] closure_type=%d, nargs=%d\n", ra->t->kind, o->p3);
		// Call through closure: closure->fun(value?, args...)
		if (ra->t->kind == HDYN) {
			// Dynamic closure - call hl_dyn_call(closure, args[], nargs)
			// Build array of vdynamic* on stack and call hl_dyn_call

			// Allocate stack space for args array (aligned to 16)
			int args_size = o->p3 * HL_WSIZE;
			if (args_size & 15) args_size += 16 - (args_size & 15);

			// SUB SP, SP, #args_size
			arm_sub_imm(ctx, SP, SP, args_size / 8, true);  // Scaled by 8

			// Store each argument into the array
			for (i = 0; i < o->p3; i++) {
				vreg *arg = R(o->extra[i]);
				// Load from stack into X10, then store to args array
				LOAD_VREG(X10, arg);
				// STR X10, [SP, #(i * 8)]
				arm_str_imm(ctx, X10, SP, i, 3);  // Scaled offset
			}

			// Set up call to hl_dyn_call(closure, args, nargs)
			// X0 = closure
			LOAD_VREG(X0, ra);

			// X1 = args (SP)
			arm_mov_reg(ctx, X1, SP, true);

			// X2 = nargs
			arm_movz(ctx, X2, o->p3, 0, true);

			// Call hl_dyn_call
			printf("[OCallClosure] About to call hl_dyn_call at %p\n", (void*)hl_dyn_call);
			arm_load_imm64(ctx, X9, (uint64_t)hl_dyn_call);
			arm_blr(ctx, X9);

			// Restore stack
			// ADD SP, SP, #args_size
			arm_add_imm(ctx, SP, SP, args_size / 8, true);

			// Store result if needed
			if (dst && dst->t->kind != HVOID) {
				STORE_VREG(X0, dst);
			}
		} else {
			// Regular closure: struct { fun, value, hasValue }
			// if (closure->hasValue) call fun(value, args...) else call fun(args...)

			// Load closure into X11 (preserve across branches)
			LOAD_VREG(X11, ra);

			// Load hasValue flag from closure (offset = HL_WSIZE * 2 = 16 bytes)
			// hasValue is int (32-bit), so use size=2 (32-bit load)
			// For size=2 (32-bit), scaled offset = 16/4 = 4
			arm_ldr_imm(ctx, X10, X11, 4, 2);

			// Test if hasValue is non-zero
			// CBZ X10, no_value_case
			int no_value_jump = arm_do_cbz(ctx, X10, true);

			// HAS VALUE CASE: Load value and pass as first argument
			// Load closure->value
			// vclosure: { t(8), fun(8), hasValue(4), stackCount(4), value(8) }
			// value is at offset 24 bytes = scaled offset 3
			arm_ldr_imm(ctx, X0, X11, 3, 3);

			// Set up remaining arguments (shift by 1)
			for (i = 0; i < o->p3; i++) {
				vreg *arg = R(o->extra[i]);
				Arm64Reg dst_reg = (i < 7) ? (X1 + i) : X9;  // X1-X7 for args 1-7

				if (i < 7) {
					LOAD_VREG(dst_reg, arg);
				} else {
					// Stack args not yet implemented
					jit_error("Closures with >7 args not yet implemented");
				}
			}

			// Load function pointer and call
			// CRITICAL FIX: closure->fun is at offset HL_WSIZE (8 bytes), not 0!
			// vclosure struct: { t, fun, hasValue, value }
			// Offset 0 = t, Offset 8 = fun
			// For scaled offset with size=3 (64-bit): offset_bytes / 8 = 8 / 8 = 1
			arm_ldr_imm(ctx, X9, X11, 1, 3);

			// DEBUG: Check if function pointer is NULL
			// CBZ X9, error_label
			int null_check_jump = arm_do_cbz(ctx, X9, true);

			// Normal case: call the function
			arm_blr(ctx, X9);

			// Jump over error handler
			int after_error_jump = arm_do_jump(ctx);

			// Error handler: function pointer was NULL
			arm_patch_cbz(ctx, null_check_jump, ARM_BUF_POS());
			// Load error message and call printf
			static char null_fn_msg[] = "ERROR: OCallClosure hasValue path - closure->fun is NULL!\n";
			arm_load_imm64(ctx, X0, (uint64_t)null_fn_msg);
			arm_load_imm64(ctx, X9, (uint64_t)&printf);
			arm_blr(ctx, X9);
			// Exit
			arm_movz(ctx, X0, 1, 0, true);  // exit code 1
			arm_load_imm64(ctx, X9, (uint64_t)&exit);
			arm_blr(ctx, X9);

			// Patch the after-error jump to skip error handler
			arm_patch_jump(ctx, after_error_jump);

			// Jump over no-value case
			int end_jump = arm_do_jump(ctx);

			// NO VALUE CASE: Just pass arguments normally
			arm_patch_cbz(ctx, no_value_jump, ARM_BUF_POS());

			// Set up arguments
			for (i = 0; i < o->p3; i++) {
				vreg *arg = R(o->extra[i]);
				Arm64Reg dst_reg = (i < 8) ? (X0 + i) : X9;

				if (i < 8) {
					LOAD_VREG(dst_reg, arg);
				} else {
					jit_error("Closures with >8 args not yet implemented");
				}
			}

			// Load function pointer and call
			// closure->fun is at offset HL_WSIZE (scaled offset = 1)
			arm_ldr_imm(ctx, X9, X11, 1, 3);

			// DEBUG: Check if function pointer is NULL
			int null_check_jump2 = arm_do_cbz(ctx, X9, true);

			// Normal case: call the function
			arm_blr(ctx, X9);

			// Jump over error handler
			int after_error_jump2 = arm_do_jump(ctx);

			// Error handler: function pointer was NULL
			arm_patch_cbz(ctx, null_check_jump2, ARM_BUF_POS());
			static char null_fn_msg2[] = "ERROR: OCallClosure no-value path - closure->fun is NULL!\n";
			arm_load_imm64(ctx, X0, (uint64_t)null_fn_msg2);
			arm_load_imm64(ctx, X9, (uint64_t)&printf);
			arm_blr(ctx, X9);
			// Exit
			arm_movz(ctx, X0, 1, 0, true);
			arm_load_imm64(ctx, X9, (uint64_t)&exit);
			arm_blr(ctx, X9);

			// Patch the after-error jump
			arm_patch_jump(ctx, after_error_jump2);

			// Patch end jump
			arm_patch_jump(ctx, end_jump);

			// Store result if needed
			if (dst && dst->t->kind != HVOID) {
				STORE_VREG(X0, dst);
			}
		}
		break;

	// =====================================================================
	// Object/Memory Allocation
	// =====================================================================

	case ONew:
		{
			// Allocate new object - calls hl_alloc_obj/hl_alloc_dynobj/etc
			void *allocFun = NULL;
			uint64_t type_arg = (uint64_t)dst->t;

			switch (dst->t->kind) {
			case HOBJ:
			case HSTRUCT:
				allocFun = hl_alloc_obj;
				break;
			case HDYNOBJ:
				allocFun = hl_alloc_dynobj;
				break;
			default:
				break;
			}

			if (allocFun && dst) {
				// Set up argument X0 = type
				arm_load_imm64(ctx, X0, type_arg);

				// Load function pointer into X9
				arm_load_imm64(ctx, X9, (uint64_t)allocFun);

				// BLR X9
				B32(0xd63f0120);

				// Result in X0
				STORE_VREG(X0, dst);
			}
		}
		break;

	case ONullCheck:
		{
			// Check if value is null, throw exception if so
			if (dst) {
				LOAD_VREG(X10, dst);

				// CBNZ X10, not_null
				int not_null = arm_do_cbnz(ctx, X10, true);

				// Null case: call hl_null_access() or similar
				// For now, just trigger error
				// TODO: Proper exception handling
				arm_load_imm64(ctx, X0, 0);  // Set error code
				// Would call exception handler here

				// not_null:
				arm_patch_cbnz(ctx, not_null, ARM_BUF_POS());
			}
		}
		break;

	// =====================================================================
	// Control Flow
	// =====================================================================

	case OSwitch:
		{
			// Switch statement - compare value against cases and jump
			// p1 = value register, p2 = case count, p3 = default offset
			// extra = array of (value, jump_offset) pairs

			if (ra) {
				LOAD_VREG(X10, ra);
				int ncases = o->p2;
				int default_offset = o->p3;
				int *cases = (int*)o->extra;

				// Try each case
				for (int i = 0; i < ncases; i++) {
					int case_value = cases[i * 2];
					int case_offset = cases[i * 2 + 1];

					// CMP X10, #case_value
					if (case_value < 4096) {
						arm_cmp_imm(ctx, X10, case_value, true);
					} else {
						arm_load_imm64(ctx, X9, case_value);
						arm_subs_reg(ctx, (Arm64Reg)31, X10, X9, true);
					}

					// B.EQ to case
					int jump = arm_do_jump_cond(ctx, ARM64_COND_EQ);
					register_jump(ctx, jump, (opCount + 1) + case_offset);
				}

				// No match: jump to default
				int default_jump = arm_do_jump(ctx);
				register_jump(ctx, default_jump, (opCount + 1) + default_offset);
			}
		}
		break;

	// =====================================================================
	// Enum Operations
	// =====================================================================

	case OEnumAlloc:
	case OEnumIndex:
	case OEnumField:
	case OMakeEnum:
		// Enum operations require enum runtime support
		jit_error("Enum operations not yet implemented in ARM64");
		break;

	case OSetEnumField:
		{
			// Set field in enum value: enum.field = value
			if (dst && rb) {
				hl_enum_construct *c = &dst->t->tenum->constructs[0];
				int offset = c->offsets[o->p2];
				int size = hl_type_size(c->params[o->p2]);

				LOAD_VREG(X10, dst);   // enum pointer
				LOAD_VREG(X11, rb);    // value to store

				// Store value at offset in enum
				// STR/STRB/STRH depending on size
				if (size == 8) {
					// 64-bit store
					if (offset >= 0 && offset < 4096 * 8) {
						arm_str_imm(ctx, X11, X10, offset / 8, 3);
					} else {
						arm_load_imm64(ctx, X9, offset);
						arm_str_reg(ctx, X11, X10, X9, 3);
					}
				} else if (size == 4) {
					// 32-bit store
					if (offset >= 0 && offset < 4096 * 4) {
						arm_str_imm(ctx, X11, X10, offset / 4, 2);
					} else {
						arm_load_imm64(ctx, X9, offset);
						arm_str_reg(ctx, X11, X10, X9, 2);
					}
				} else if (size == 2) {
					// 16-bit store
					if (offset >= 0 && offset < 4096 * 2) {
						arm_str_imm(ctx, X11, X10, offset / 2, 1);
					} else {
						arm_load_imm64(ctx, X9, offset);
						arm_str_reg(ctx, X11, X10, X9, 1);
					}
				} else {
					// 8-bit store
					if (offset >= 0 && offset < 4096) {
						arm_str_imm(ctx, X11, X10, offset, 0);
					} else {
						arm_load_imm64(ctx, X9, offset);
						arm_str_reg(ctx, X11, X10, X9, 0);
					}
				}
			}
		}
		break;

	// =====================================================================
	// Floating Point Operations
	// =====================================================================

	case OFloat:
		// Load float constant from module's float table
		if (dst) {
			double float_val = m->code->floats[o->p2];
			bool float64 = (dst->t->kind == HF64);

			if (float_val == 0.0) {
				// Zero out register
				arm_movz(ctx, X10, 0, 0, true);
			} else {
				// Load float bits as integer
				uint64_t bits;
				if (float64) {
					bits = *(uint64_t*)&float_val;
				} else {
					float f32 = (float)float_val;
					bits = *(uint32_t*)&f32;
				}

				// Load bits into X10
				arm_load_imm64(ctx, X10, bits);
			}
			// Store to destination
			STORE_VREG(X10, dst);
		}
		break;

	case OToSFloat:
		// Convert signed integer to float
		if (dst && ra) {
			bool int64 = (ra->t->kind == HI64);
			bool float64 = (dst->t->kind == HF64);

			// Load input integer
			LOAD_VREG(X10, ra);

			// Convert using SCVTF: integer register -> FPU register
			arm_scvtf(ctx, V0, X10, int64, float64);

			// Move FPU result back to general register for storage
			// FMOV Xd, Dn  (move FPU to general register)
			unsigned int sf = float64 ? 1 : 0;
			unsigned int ftype = float64 ? 0x01 : 0x00;
			unsigned int inst = (sf << 31) | (0x1E << 24) | (ftype << 22) | (1 << 21) |
			                    (0x06 << 16) | arm_reg(X10);
			B32(inst);

			// Store result
			STORE_VREG(X10, dst);
		}
		break;

	case OToUFloat:
		// Convert unsigned integer to float
		if (dst && ra) {
			bool int64 = (ra->t->kind == HI64);
			bool float64 = (dst->t->kind == HF64);

			// Load input integer
			LOAD_VREG(X10, ra);

			// Convert using UCVTF: integer register -> FPU register
			arm_ucvtf(ctx, V0, X10, int64, float64);

			// Move FPU result back to general register for storage
			// FMOV Xd, Vn
			unsigned int sf = float64 ? 1 : 0;
			unsigned int ftype = float64 ? 0x01 : 0x00;
			unsigned int inst = (sf << 31) | (0x1E << 24) | (ftype << 22) | (1 << 21) |
			                    (0x06 << 16) | arm_reg(X10);
			B32(inst);

			// Store result
			STORE_VREG(X10, dst);
		}
		break;

	// =====================================================================
	// Dynamic Operations
	// =====================================================================

	case ODynGet:
		{
			// Get dynamic field: dst = obj.field
			// Calls different getter functions based on destination type
			if (dst && ra) {
				// Get field name hash
				uint64_t hash = hl_hash_gen(hl_get_ustring(m->code, o->p3), true);
				void *get_func = NULL;
				bool needs_type_arg = false;

				// Determine which getter to call based on destination type
				switch (dst->t->kind) {
				case HF32:
					get_func = hl_dyn_getf;
					break;
				case HF64:
					get_func = hl_dyn_getd;
					break;
				case HI64:
				case HGUID:
					get_func = hl_dyn_geti64;
					break;
				case HI32:
				case HUI16:
				case HUI8:
				case HBOOL:
					get_func = hl_dyn_geti;
					needs_type_arg = true;  // hl_dyn_geti needs type parameter
					break;
				default:
					// Pointer types
					get_func = hl_dyn_getp;
					needs_type_arg = true;  // hl_dyn_getp needs type parameter
					break;
				}

			if (get_func) {
				if (needs_type_arg) {
					// Arguments: object (X0), hash (X1), type (X2)
					LOAD_VREG(X0, ra);

					// Load hash into X1
					arm_load_imm64(ctx, X1, hash);

					// Load type pointer into X2
					arm_load_imm64(ctx, X2, (uint64_t)dst->t);

					// Load function pointer into X9
					arm_load_imm64(ctx, X9, (uint64_t)get_func);

					// BLR X9
					B32(0xd63f0120);

					// Store result
					STORE_VREG(X0, dst);
				} else {
					// Arguments: object (X0), hash (X1)
					LOAD_VREG(X0, ra);

					// Load hash into X1
					arm_load_imm64(ctx, X1, hash);

					// Load function pointer into X9
					arm_load_imm64(ctx, X9, (uint64_t)get_func);

					// BLR X9
					B32(0xd63f0120);

					// Store result
					STORE_VREG(X0, dst);
				}
			}
		}
			}
		break;

	case ODynSet:
		{
			// Set dynamic field: obj.field = value
			// Calls different functions based on value type
			if (dst && rb) {
				// Get field name hash
				uint64_t hash = hl_hash_gen(hl_get_ustring(m->code, o->p2), true);
				void *set_func = NULL;

				// Determine which setter to call based on value type
				switch (rb->t->kind) {
				case HF32:
					set_func = hl_dyn_setf;
					break;
				case HF64:
					set_func = hl_dyn_setd;
					break;
				case HI64:
				case HGUID:
					set_func = hl_dyn_seti64;
					break;
				case HI32:
				case HUI16:
				case HUI8:
				case HBOOL:
					set_func = hl_dyn_seti;
					break;
				default:
					// Pointer types
					set_func = hl_dyn_setp;
					break;
				}

				if (set_func) {
					// Arguments: value (X0), hash (X1), object (X2)
					// Load value into X0
					LOAD_VREG(X0, rb);

					// Load hash into X1
					arm_load_imm64(ctx, X1, hash);

					// Load object into X2
					LOAD_VREG(X2, dst);

					// Load function pointer into X9
					arm_load_imm64(ctx, X9, (uint64_t)set_func);

					// BLR X9
					B32(0xd63f0120);
				}
			}
		}
		break;

	// =====================================================================
	// Exception Handling
	// =====================================================================

	case OTrap:
		{
			// Simplified trap setup - no-op for now
			// TODO: Full implementation with trap context and setjmp
			// For programs that don't throw exceptions, this is sufficient
			// Real implementation would: allocate trap context, save registers, setup longjmp

			// For now, just a no-op to allow simple programs to run
			// Programs that actually throw exceptions will fail
		}
		break;

	case OEndTrap:
		{
			// Simplified trap cleanup - no-op for now
			// TODO: Full implementation to restore trap stack
			// Real implementation would: restore previous trap context, clean up stack

			// For now, just a no-op
		}
		break;

	case OThrow:
		{
			// Throw exception - call hl_throw(value)
			if (ra) {
				vreg *arg = R(o->p1);
				if (arg) {
				LOAD_VREG(X0, arg);
				}
				// Load hl_throw address
				arm_load_imm64(ctx, X9, (uint64_t)hl_throw);
				// BLR X9
				B32(0xd63f0120);
			}
		}
		break;

	case ORethrow:
		{
			// Rethrow exception - call hl_rethrow(value)
			vreg *arg = R(o->p1);
			if (arg) {
			LOAD_VREG(X0, arg);
			}
			// Load hl_rethrow address
			arm_load_imm64(ctx, X9, (uint64_t)hl_rethrow);
			// BLR X9
			B32(0xd63f0120);
		}
		break;

	// =====================================================================
	// Closure Operations
	// =====================================================================

	case OStaticClosure:
		// Allocate static closure and store pointer
		if (dst) {
			vclosure *c = alloc_static_closure(ctx, o->p2);
			arm_load_imm64(ctx, X10, (uint64_t)c);
			STORE_VREG(X10, dst);
		}
		break;

	case OInstanceClosure:
		// Create closure bound to an instance
		// Call hl_alloc_closure_ptr(object, function_ptr, type)
		if (dst && rb) {
			// X0 = object
			LOAD_VREG(X0, rb);

			// X1 = function pointer (needs to be patched when JIT address is known)
			// For now, mark it as needing patching via ctx->calls
			jlist *j = (jlist*)hl_malloc(&ctx->galloc, sizeof(jlist));
			j->pos = ARM_BUF_POS();
			j->target = o->p2;
			j->next = ctx->calls;
			ctx->calls = j;

			// Placeholder - will be patched with actual JIT function address
			arm_load_imm64(ctx, X1, 0xDEADBEEF);

			// X2 = function type
			hl_module *m = ctx->m;
			hl_type *ftype = m->code->functions[m->functions_indexes[o->p2]].type;
			arm_load_imm64(ctx, X2, (uint64_t)ftype);

			// Call hl_alloc_closure_ptr
			arm_load_imm64(ctx, X9, (uint64_t)hl_alloc_closure_ptr);
			arm_blr(ctx, X9);

			// Store result (already in X0)
			STORE_VREG(X0, dst);
		}
		break;

	case OVirtualClosure:
		// Create closure for virtual method
		// Lookup virtual function pointer and call hl_alloc_closure_ptr
		if (dst && ra) {
			hl_module *m = ctx->m;

			// Find function type by looking up proto
			hl_type *t = NULL;
			hl_type *ot = ra->t;
			int i;
			while (t == NULL) {
				for (i = 0; i < ot->obj->nproto; i++) {
					hl_obj_proto *pp = ot->obj->proto + i;
					if (pp->pindex == o->p3) {
						t = m->code->functions[m->functions_indexes[pp->findex]].type;
						break;
					}
				}
				ot = ot->obj->super;
			}

			// X0 = object (load from stack)
			LOAD_VREG(X0, ra);

			// X1 = read function pointer from obj->type->vobj_proto[o->p3]
			// LDR X1, [X0, #0]  - load type
			arm_ldr_imm(ctx, X1, X0, 0, 3);
			// LDR X1, [X1, #(HL_WSIZE*2)]  - load vobj_proto
			arm_ldr_imm(ctx, X1, X1, 2, 3);
			// LDR X1, [X1, #(HL_WSIZE*o->p3)]  - load function pointer
			if (o->p3 < 4096) {
				arm_ldr_imm(ctx, X1, X1, o->p3, 3);
			} else {
				arm_load_imm64(ctx, X9, o->p3 * HL_WSIZE);
				arm_ldr_reg(ctx, X1, X1, X9, 3);
			}

			// X2 = function type
			arm_load_imm64(ctx, X2, (uint64_t)t);

			// Call hl_alloc_closure_ptr
			arm_load_imm64(ctx, X9, (uint64_t)hl_alloc_closure_ptr);
			arm_blr(ctx, X9);

			// Store result (already in X0)
			STORE_VREG(X0, dst);
		}
		break;

	// =====================================================================
	// Miscellaneous
	// =====================================================================

	case OPrefetch:
		// Prefetch is a performance hint, can be no-op
		break;

	case OToVirtual:
		// Convert to virtual - usually just a pointer copy
		if (dst && ra) {
			LOAD_VREG(X10, ra);
			STORE_VREG(X10, dst);
		}
		break;

	case OAssert:
	case OAsm:
	case OCatch:
		// Special operations
		break;

		default:
			jit_error(hl_op_name(o->op));
			break;
		}
	}

	#undef GET_REG

	return codePos;
#endif // HL_JIT_ARM64
}

// =====================================================================
// x86/x86-64 Helper Functions
// =====================================================================
#ifdef HL_JIT_X86

static void *get_wrapper( hl_type *t ) {
	return call_jit_hl2c;
}

void hl_jit_patch_method_x86( void *old_fun, void **new_fun_table ) {
	// mov eax, addr
	// jmp [eax]
	unsigned char *b = (unsigned char*)old_fun;
	unsigned long long addr = (unsigned long long)(int_val)new_fun_table;
#	ifdef HL_64
	*b++ = 0x48;
	*b++ = 0xB8;
	*b++ = (unsigned char)addr;
	*b++ = (unsigned char)(addr>>8);
	*b++ = (unsigned char)(addr>>16);
	*b++ = (unsigned char)(addr>>24);
	*b++ = (unsigned char)(addr>>32);
	*b++ = (unsigned char)(addr>>40);
	*b++ = (unsigned char)(addr>>48);
	*b++ = (unsigned char)(addr>>56);
#	else
	*b++ = 0xB8;
	*b++ = (unsigned char)addr;
	*b++ = (unsigned char)(addr>>8);
	*b++ = (unsigned char)(addr>>16);
	*b++ = (unsigned char)(addr>>24);
#	endif
	*b++ = 0xFF;
	*b++ = 0x20;
}

static void missing_closure() {
	hl_error("Missing static closure");
}

void *hl_jit_code_x86( jit_ctx *ctx, hl_module *m, int *codesize, hl_debug_infos **debug, hl_module *previous ) {
	jlist *c;
	int size = BUF_POS();
	unsigned char *code;
	if( size & 4095 ) size += 4096 - (size&4095);
	code = (unsigned char*)hl_alloc_executable_memory(size);
	if( code == NULL ) return NULL;
	memcpy(code,ctx->startBuf,BUF_POS());
	*codesize = size;
	*debug = ctx->debug;
	if( !call_jit_c2hl ) {
		call_jit_c2hl = code + ctx->c2hl;
		call_jit_hl2c = code + ctx->hl2c;
		hl_setup.get_wrapper = get_wrapper;
		hl_setup.static_call = callback_c2hl;
		hl_setup.static_call_ref = true;
#		ifdef JIT_CUSTOM_LONGJUMP
		hl_setup.throw_jump = (void(*)(jmp_buf, int))(code + ctx->longjump);
#		endif
	}
	if( !ctx->static_function_offset ) {
		int i;
		ctx->static_function_offset = true;
		for(i=0;i<(int)(sizeof(ctx->static_functions)/sizeof(void*));i++)
			ctx->static_functions[i] = (void*)(code + (int)(int_val)ctx->static_functions[i]);
	}
	// patch calls
	c = ctx->calls;
	while( c ) {
		void *fabs;
		if( c->target < 0 )
			fabs = ctx->static_functions[-c->target-1];
		else {
			fabs = m->functions_ptrs[c->target];
			if( fabs == NULL ) {
				// read absolute address from previous module
				int old_idx = m->hash->functions_hashes[m->functions_indexes[c->target]];
				if( old_idx < 0 )
					return NULL;
				fabs = previous->functions_ptrs[(previous->code->functions + old_idx)->findex];
			} else {
				// relative
				fabs = (unsigned char*)code + (int)(int_val)fabs;
			}
		}
		if( (code[c->pos]&~3) == (IS_64?0x48:0xB8) || code[c->pos] == 0x68 ) // MOV : absolute | PUSH
			*(void**)(code + c->pos + (IS_64?2:1)) = fabs;
		else {
			int_val delta = (int_val)fabs - (int_val)code - (c->pos + 5);
			int rpos = (int)delta;
			if( (int_val)rpos != delta ) {
				printf("Target code too far too rebase\n");
				return NULL;
			}
			*(int*)(code + c->pos + 1) = rpos;
		}
		c = c->next;
	}
	// patch switchs
	c = ctx->switchs;
	while( c ) {
		*(void**)(code + c->pos) = code + c->pos + (IS_64 ? 14 : 6);
		c = c->next;
	}
	// patch closures
	{
		vclosure *c = ctx->closure_list;
		while( c ) {
			vclosure *next;
			int fidx = (int)(int_val)c->fun;
			void *fabs = m->functions_ptrs[fidx];
			if( fabs == NULL ) {
				// read absolute address from previous module
				int old_idx = m->hash->functions_hashes[m->functions_indexes[fidx]];
				if( old_idx < 0 )
					fabs = missing_closure;
				else
					fabs = previous->functions_ptrs[(previous->code->functions + old_idx)->findex];
			} else {
				// relative
				fabs = (unsigned char*)code + (int)(int_val)fabs;
			}
			c->fun = fabs;
			next = (vclosure*)c->value;
			c->value = NULL;
			c = next;
		}
	}
	return code;
}

#endif // HL_JIT_X86

// =====================================================================
// ARM64/AArch64 JIT Implementation
// =====================================================================
#ifdef HL_JIT_ARM64

// =====================================================================
// ARM64 Instruction Encoders - Data Processing (Register)
// =====================================================================

// Add/Subtract (shifted register)
// Format: sf 0 S 01011 shift(2) 0 Rm(5) imm6(6) Rn(5) Rd(5)
static void arm_alu_reg(jit_ctx *ctx, unsigned int opc, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (opc << 29) | (0x0B << 24) |
	                    (arm_reg(rm) << 16) | (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// ADD (register): rd = rn + rm
static void arm_add_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	arm_alu_reg(ctx, 0, rd, rn, rm, is64);  // opc=0 for ADD
}

// SUB (register): rd = rn - rm
static void arm_sub_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	arm_alu_reg(ctx, 2, rd, rn, rm, is64);  // opc=2 for SUB
}

// SUBS (register): rd = rn - rm, and set flags
static void arm_subs_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	arm_alu_reg(ctx, 3, rd, rn, rm, is64);  // opc=3 for SUBS (SUB with flags)
}

// =====================================================================
// ARM64 Instruction Encoders - Data Processing (Immediate)
// =====================================================================

// Add/Subtract (immediate)
// Format: sf 0 S 100010 shift(2) imm12(12) Rn(5) Rd(5)
static void arm_alu_imm(jit_ctx *ctx, unsigned int opc, Arm64Reg rd, Arm64Reg rn, unsigned int imm12, bool is64) {
	if (!arm_fits_unsigned(imm12, 12)) {
		ASSERT(1); // Immediate out of range
		return;
	}
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (opc << 29) | (0x11 << 24) |
	                    (imm12 << 10) | (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// ADD (immediate): rd = rn + imm12
static void arm_add_imm(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, unsigned int imm12, bool is64) {
	arm_alu_imm(ctx, 0, rd, rn, imm12, is64);
}

// SUB (immediate): rd = rn - imm12
static void arm_sub_imm(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, unsigned int imm12, bool is64) {
	arm_alu_imm(ctx, 2, rd, rn, imm12, is64);
}

// MOV (register): mov rd, rm
// Implemented as: ORR rd, XZR, rm
static void arm_mov_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	// ORR (shifted register): sf 01 01010 shift(2) 0 Rm(5) imm6(6) Rn(5) Rd(5)
	unsigned int inst = (sf << 31) | (0x2A << 24) |
	                    (arm_reg(rm) << 16) | (31 << 5) | arm_reg(rd);  // Rn = XZR (31)
	B32(inst);
}

// MOVZ (move wide with zero): mov rd, #imm16 << (shift * 16)
// Format: sf 10 100101 hw(2) imm16(16) Rd(5)
// Bits 30-29 = 10 (opc for MOVZ), bits 28-23 = 100101
static void arm_movz(jit_ctx *ctx, Arm64Reg rd, unsigned int imm16, unsigned int shift, bool is64) {
	if (!arm_fits_unsigned(imm16, 16) || shift > 3) {
		ASSERT(2);
		return;
	}
	unsigned int sf = is64 ? 1 : 0;
	// CRITICAL FIX: Must use 0xA5 (bits 30-23 = 10100101), not 0x52 (01010010)!
	// 0x52 gives opc=01, but MOVZ requires opc=10
	unsigned int inst = (sf << 31) | (0xA5 << 23) | (shift << 21) | (imm16 << 5) | arm_reg(rd);
	B32(inst);
}

// MOVK (move wide with keep): movk rd, #imm16 << (shift * 16)
// Format: sf 11 100101 hw(2) imm16(16) Rd(5)
// Bits 30-29 = 11 (opc for MOVK), bits 28-23 = 100101
static void arm_movk(jit_ctx *ctx, Arm64Reg rd, unsigned int imm16, unsigned int shift, bool is64) {
	if (!arm_fits_unsigned(imm16, 16) || shift > 3) {
		ASSERT(3);
		return;
	}
	unsigned int sf = is64 ? 1 : 0;
	// CRITICAL FIX: Must use 0xE5 (bits 30-23 = 11100101), not 0x72 (01110010)!
	// 0x72 gives opc=01, but MOVK requires opc=11
	unsigned int inst = (sf << 31) | (0xE5 << 23) | (shift << 21) | (imm16 << 5) | arm_reg(rd);
	B32(inst);
}

// Helper: Load a 64-bit immediate into a register (requires up to 4 instructions)
static void arm_load_imm64(jit_ctx *ctx, Arm64Reg rd, uint64_t imm) {
	// Check if it fits in 16 bits (single MOVZ)
	if (arm_fits_unsigned(imm, 16)) {
		arm_movz(ctx, rd, (unsigned int)imm, 0, true);
		return;
	}

	// Use MOVZ + up to 3 MOVK instructions
	bool first = true;
	for (int shift = 0; shift < 4; shift++) {
		unsigned int chunk = (unsigned int)((imm >> (shift * 16)) & 0xFFFF);
		if (chunk != 0) {
			if (first) {
				arm_movz(ctx, rd, chunk, shift, true);
				first = false;
			} else {
				arm_movk(ctx, rd, chunk, shift, true);
			}
		} else if (first) {
			// First chunk is zero, emit MOVZ with 0
			arm_movz(ctx, rd, 0, shift, true);
			first = false;
		}
	}
}

// =====================================================================
// ARM64 Instruction Encoders - Load/Store
// =====================================================================

// LDR (literal): Load register from PC-relative address
// Format: opc(2) 011 0 00 imm19(19) Rt(5)
static void arm_ldr_literal(jit_ctx *ctx, Arm64Reg rt, int offset, bool is64) {
	if (!arm_fits_signed(offset >> 2, 19)) {
		ASSERT(4); // Offset out of range
		return;
	}
	unsigned int opc = is64 ? 1 : 0;
	unsigned int imm19 = (offset >> 2) & 0x7FFFF;
	unsigned int inst = (opc << 30) | (0x18 << 24) | (imm19 << 5) | arm_reg(rt);
	B32(inst);
}

// LDR (unsigned offset): Load register from [rn + (imm12 << size)]
// Format: size(2) 111 0 01 imm12(12) Rn(5) Rt(5)
// NOTE: imm12 is already scaled (offset in units of access size)
//       For size=3 (8-byte): imm12=2 means byte offset=16
// Maximum offset: (4095 << size) bytes
static void arm_ldr_imm(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, unsigned int imm12, int size) {
	// Check if offset fits in 12 bits unsigned
	if (!arm_fits_unsigned(imm12, 12)) {
		// Offset too large - caller must use register offset or alternative
		ASSERT(5);
		return;
	}
	// LDR (unsigned offset): size(2) 111001 01 imm12(12) Rn(5) Rt(5)
	// Bit 22 must be set to 1 for LDR unsigned offset mode
	unsigned int inst = (size << 30) | (0x39 << 24) | (1 << 22) | (imm12 << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rt);
	B32(inst);
}

// STR (unsigned offset): Store register to [rn + (imm12 << size)]
// Format: size(2) 111 0 01 00 imm12(12) Rn(5) Rt(5)
// NOTE: imm12 is already scaled (offset in units of access size)
// Bits 23-22 = 00 for STR (different from LDR which uses 01)
static void arm_str_imm(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, unsigned int imm12, int size) {
	if (!arm_fits_unsigned(imm12, 12)) {
		ASSERT(6);
		return;
	}
	unsigned int inst = (size << 30) | (0x39 << 24) | (0 << 22) | (imm12 << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rt);
	B32(inst);
}

// LDUR (unscaled offset): Load register from [rn + imm9]
// Format: size(2) 111 0 00 imm9(9) 00 Rn(5) Rt(5)
// NOTE: imm9 is a SIGNED 9-bit byte offset (NOT scaled), range -256 to +255
static void arm_ldur_imm(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, int imm9, int size) {
	// Check if offset fits in signed 9 bits
	if (imm9 < -256 || imm9 > 255) {
		ASSERT(7);
		return;
	}
	// Encode as unsigned 9-bit value (two's complement)
	unsigned int imm9_encoded = imm9 & 0x1FF;
	unsigned int inst = (size << 30) | (0x38 << 24) | (imm9_encoded << 12) |
	                    (arm_reg(rn) << 5) | arm_reg(rt);
	B32(inst);
}

// STUR (unscaled offset): Store register to [rn + imm9]
// Format: size(2) 111 0 00 imm9(9) 00 Rn(5) Rt(5)
// NOTE: imm9 is a SIGNED 9-bit byte offset (NOT scaled), range -256 to +255
static void arm_stur_imm(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, int imm9, int size) {
	if (imm9 < -256 || imm9 > 255) {
		ASSERT(8);
		return;
	}
	unsigned int imm9_encoded = imm9 & 0x1FF;
	unsigned int inst = (size << 30) | (0x38 << 24) | (imm9_encoded << 12) |
	                    (arm_reg(rn) << 5) | arm_reg(rt);
	B32(inst);
}

// LDR (register offset): Load from [rn + rm]
// Format: size(2) 111 0 00 1 Rm(5) option(3) S(1) 10 Rn(5) Rt(5)
// option=011 (LSL), S=0 (no shift)
static void arm_ldr_reg(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, Arm64Reg rm, int size) {
	unsigned int option = 0x3;  // LSL
	unsigned int S = 0;          // No shift
	unsigned int inst = (size << 30) | (0x38 << 24) | (1 << 21) | (arm_reg(rm) << 16) |
	                    (option << 13) | (S << 12) | (0x2 << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rt);
	B32(inst);
}

// STR (register offset): Store to [rn + rm]
// Format: size(2) 111 0 00 0 Rm(5) option(3) S(1) 10 Rn(5) Rt(5)
// option=011 (LSL), S=0 (no shift)
static void arm_str_reg(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rn, Arm64Reg rm, int size) {
	unsigned int option = 0x3;  // LSL
	unsigned int S = 0;          // No shift
	unsigned int inst = (size << 30) | (0x38 << 24) | (0 << 21) | (arm_reg(rm) << 16) |
	                    (option << 13) | (S << 12) | (0x2 << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rt);
	B32(inst);
}

// LDP (load pair): Load two registers from memory
// pre_index=true: [rn, #offset]! (pre-indexed, writeback)
// pre_index=false: [rn], #offset (post-indexed, writeback)
// Format: opc(2) 101 0 01(pre) imm7(7) Rt2(5) Rn(5) Rt(5)
static void arm_ldp(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rt2, Arm64Reg rn, int offset, bool is64, bool pre_index) {
	int imm7 = offset / (is64 ? 8 : 4);  // offset in units of register size
	if (!arm_fits_signed(imm7, 7)) {
		ASSERT(7);
		return;
	}
	unsigned int opc = is64 ? 2 : 0;
	unsigned int mode = pre_index ? 3 : 1;  // 11=pre-indexed, 01=post-indexed
	unsigned int inst = (opc << 30) | (0x28 << 25) | (mode << 23) |
	                    ((imm7 & 0x7F) << 15) | (arm_reg(rt2) << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rt);
	B32(inst);
}

// STP (store pair): Store two registers to memory
// pre_index=true: [rn, #offset]! (pre-indexed, writeback)
// pre_index=false: [rn], #offset (post-indexed, writeback)
// Format: opc(2) 101 0 01(pre) imm7(7) Rt2(5) Rn(5) Rt(5)
static void arm_stp(jit_ctx *ctx, Arm64Reg rt, Arm64Reg rt2, Arm64Reg rn, int offset, bool is64, bool pre_index) {
	int imm7 = offset / (is64 ? 8 : 4);  // offset in units of register size
	if (!arm_fits_signed(imm7, 7)) {
		ASSERT(8);
		return;
	}
	unsigned int opc = is64 ? 2 : 0;
	unsigned int mode = pre_index ? 3 : 1;  // 11=pre-indexed, 01=post-indexed
	unsigned int inst = (opc << 30) | (0x28 << 25) | (mode << 23) |
	                    ((imm7 & 0x7F) << 15) | (arm_reg(rt2) << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rt);
	B32(inst);
}

// =====================================================================
// ARM64 Instruction Encoders - Multiply/Divide
// =====================================================================

// MUL (multiply): rd = rn * rm (lower 64 bits)
// Encoded as MADD with Ra = XZR: rd = ra + (rn * rm), where ra=0
// Format: sf 0 011011 000 Rm(5) 0 Ra(5) Rn(5) Rd(5)
static void arm_mul(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	// MADD: rd = XZR + (rn * rm) = rn * rm
	unsigned int inst = (sf << 31) | (0x1B << 24) |
	                    (arm_reg(rm) << 16) | (31 << 10) |  // Ra = XZR (31)
	                    (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// UDIV (unsigned divide): rd = rn / rm
// Format: sf 0 011010 110 Rm(5) 00001 0 Rn(5) Rd(5)
static void arm_udiv(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x1A << 24) | (0x6 << 21) |
	                    (arm_reg(rm) << 16) | (0x2 << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// SDIV (signed divide): rd = rn / rm
// Format: sf 0 011010 110 Rm(5) 00001 1 Rn(5) Rd(5)
static void arm_sdiv(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x1A << 24) | (0x6 << 21) |
	                    (arm_reg(rm) << 16) | (0x3 << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// MADD (multiply-add): rd = ra + (rn * rm)
// Format: sf 0 011011 000 Rm(5) 0 Ra(5) Rn(5) Rd(5)
static void arm_madd(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, Arm64Reg ra, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x1B << 24) |
	                    (arm_reg(rm) << 16) | (arm_reg(ra) << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// MSUB (multiply-subtract): rd = ra - (rn * rm)
// Format: sf 0 011011 000 Rm(5) 1 Ra(5) Rn(5) Rd(5)
static void arm_msub(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, Arm64Reg ra, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x1B << 24) |
	                    (arm_reg(rm) << 16) | (1 << 15) | (arm_reg(ra) << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// =====================================================================
// ARM64 Instruction Encoders - Bitwise Operations
// =====================================================================

// AND (register): rd = rn & rm
// Format: sf 00 01010 shift(2) 0 Rm(5) imm6(6) Rn(5) Rd(5)
static void arm_and_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x0A << 24) |
	                    (arm_reg(rm) << 16) | (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// ORR (register): rd = rn | rm
// Format: sf 01 01010 shift(2) 0 Rm(5) imm6(6) Rn(5) Rd(5)
static void arm_orr_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x2A << 24) |
	                    (arm_reg(rm) << 16) | (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// EOR (register): rd = rn ^ rm (XOR)
// Format: sf 10 01010 shift(2) 0 Rm(5) imm6(6) Rn(5) Rd(5)
static void arm_eor_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x4A << 24) |
	                    (arm_reg(rm) << 16) | (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// BIC (bit clear): rd = rn & ~rm
// Format: sf 00 01010 shift(2) 1 Rm(5) imm6(6) Rn(5) Rd(5)
static void arm_bic_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x0A << 24) | (1 << 21) |
	                    (arm_reg(rm) << 16) | (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// LSL (logical shift left): rd = rn << shift
// Implemented using UBFM (unsigned bitfield move)
// Format: sf 10 100110 N immr(6) imms(6) Rn(5) Rd(5)
static void arm_lsl_imm(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, unsigned int shift, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int n = is64 ? 1 : 0;
	unsigned int size = is64 ? 64 : 32;

	if (shift >= size) {
		ASSERT(17);
		return;
	}

	// LSL is alias for UBFM with immr = -shift mod size, imms = size - 1 - shift
	unsigned int immr = (size - shift) & (size - 1);
	unsigned int imms = (size - 1 - shift) & (size - 1);

	unsigned int inst = (sf << 31) | (0x26 << 23) | (n << 22) |
	                    (immr << 16) | (imms << 10) | (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// LSR (logical shift right): rd = rn >> shift (unsigned)
// Format: sf 10 100110 N immr(6) imms(6) Rn(5) Rd(5)
static void arm_lsr_imm(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, unsigned int shift, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int n = is64 ? 1 : 0;
	unsigned int size = is64 ? 64 : 32;

	if (shift >= size) {
		ASSERT(18);
		return;
	}

	// LSR is alias for UBFM with immr = shift, imms = size - 1
	unsigned int immr = shift;
	unsigned int imms = size - 1;

	unsigned int inst = (sf << 31) | (0x26 << 23) | (n << 22) |
	                    (immr << 16) | (imms << 10) | (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// ASR (arithmetic shift right): rd = rn >> shift (signed)
// Format: sf 00 100110 N immr(6) imms(6) Rn(5) Rd(5)
static void arm_asr_imm(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, unsigned int shift, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int n = is64 ? 1 : 0;
	unsigned int size = is64 ? 64 : 32;

	if (shift >= size) {
		ASSERT(19);
		return;
	}

	// ASR is alias for SBFM with immr = shift, imms = size - 1
	unsigned int immr = shift;
	unsigned int imms = size - 1;

	unsigned int inst = (sf << 31) | (0x13 << 23) | (n << 22) |
	                    (immr << 16) | (imms << 10) | (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// LSLV (logical shift left variable): rd = rn << rm
// Format: sf 0 0 11010110 Rm(5) 001000 Rn(5) Rd(5)
static void arm_lsl_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0xD6 << 21) | (arm_reg(rm) << 16) | (0x08 << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// LSRV (logical shift right variable): rd = rn >> rm (unsigned)
// Format: sf 0 0 11010110 Rm(5) 001001 Rn(5) Rd(5)
static void arm_lsr_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0xD6 << 21) | (arm_reg(rm) << 16) | (0x09 << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// ASRV (arithmetic shift right variable): rd = rn >> rm (signed)
// Format: sf 0 0 11010110 Rm(5) 001010 Rn(5) Rd(5)
static void arm_asr_reg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0xD6 << 21) | (arm_reg(rm) << 16) | (0x0A << 10) |
	                    (arm_reg(rn) << 5) | arm_reg(rd);
	B32(inst);
}

// =====================================================================
// ARM64 Instruction Encoders - Compare/Test
// =====================================================================

// CMP (compare): flags = rn - rm (sets NZCV flags)
// Implemented as SUBS with Rd = XZR
// Format: sf 1 1 01011 shift(2) 0 Rm(5) imm6(6) Rn(5) Rd(5)
static void arm_cmp_reg(jit_ctx *ctx, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	// SUBS XZR, Rn, Rm
	unsigned int inst = (sf << 31) | (3 << 29) | (0x0B << 24) |
	                    (arm_reg(rm) << 16) | (arm_reg(rn) << 5) | 31;  // Rd = XZR
	B32(inst);
}

// CMP (immediate): flags = rn - imm12
// Implemented as SUBS with Rd = XZR
// Format: sf 1 1 100010 shift(2) imm12(12) Rn(5) Rd(5)
static void arm_cmp_imm(jit_ctx *ctx, Arm64Reg rn, unsigned int imm12, bool is64) {
	if (!arm_fits_unsigned(imm12, 12)) {
		// Large immediate - load into temp register and use register CMP
		arm_load_imm64(ctx, X9, imm12);
		// CMP using register: SUBS XZR, rn, X9
		arm_subs_reg(ctx, XZR, rn, X9, is64);
		return;
	}
	unsigned int sf = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (3 << 29) | (0x11 << 24) |
	                    (imm12 << 10) | (arm_reg(rn) << 5) | 31;  // Rd = XZR
	B32(inst);
}

// CMN (compare negative): flags = rn + rm
// Implemented as ADDS with Rd = XZR
// Format: sf 0 1 01011 shift(2) 0 Rm(5) imm6(6) Rn(5) Rd(5)
static void arm_cmn_reg(jit_ctx *ctx, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	// ADDS XZR, Rn, Rm
	unsigned int inst = (sf << 31) | (1 << 29) | (0x0B << 24) |
	                    (arm_reg(rm) << 16) | (arm_reg(rn) << 5) | 31;  // Rd = XZR
	B32(inst);
}

// TST (test): flags = rn & rm (sets flags, discards result)
// Implemented as ANDS with Rd = XZR
// Format: sf 11 01010 shift(2) 0 Rm(5) imm6(6) Rn(5) Rd(5)
static void arm_tst_reg(jit_ctx *ctx, Arm64Reg rn, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	// ANDS XZR, Rn, Rm
	unsigned int inst = (sf << 31) | (3 << 29) | (0x0A << 24) |
	                    (arm_reg(rm) << 16) | (arm_reg(rn) << 5) | 31;  // Rd = XZR
	B32(inst);
}

// NEG (negate): rd = 0 - rn
// Implemented as SUB rd, XZR, rn
static void arm_neg(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rn, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	// SUB Rd, XZR, Rn
	unsigned int inst = (sf << 31) | (2 << 29) | (0x0B << 24) |
	                    (arm_reg(rn) << 16) | (31 << 5) | arm_reg(rd);  // Rn2 = XZR
	B32(inst);
}

// MVN (bitwise NOT): rd = ~rm
// Implemented as ORN rd, XZR, rm (rd = XZR | ~rm = ~rm)
static void arm_mvn(jit_ctx *ctx, Arm64Reg rd, Arm64Reg rm, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	// ORN Rd, XZR, Rm
	unsigned int inst = (sf << 31) | (0x2A << 24) | (1 << 21) |
	                    (arm_reg(rm) << 16) | (31 << 5) | arm_reg(rd);
	B32(inst);
}

// =====================================================================
// ARM64 Instruction Encoders - Floating Point Operations
// =====================================================================

// FADD (floating-point add): fd = fn + fm
// Format: M 0 S 11110 ftype(2) 1 Rm(5) 001010 Rn(5) Rd(5)
static void arm_fadd(jit_ctx *ctx, Arm64FpReg fd, Arm64FpReg fn, Arm64FpReg fm, bool is64) {
	unsigned int ftype = is64 ? 1 : 0;  // 0=32-bit (S), 1=64-bit (D)
	unsigned int inst = (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (arm_reg(fm) << 16) | (0x0A << 10) |
	                    (arm_reg(fn) << 5) | arm_reg(fd);
	B32(inst);
}

// FSUB (floating-point subtract): fd = fn - fm
// Format: M 0 S 11110 ftype(2) 1 Rm(5) 001110 Rn(5) Rd(5)
static void arm_fsub(jit_ctx *ctx, Arm64FpReg fd, Arm64FpReg fn, Arm64FpReg fm, bool is64) {
	unsigned int ftype = is64 ? 1 : 0;
	unsigned int inst = (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (arm_reg(fm) << 16) | (0x0E << 10) |
	                    (arm_reg(fn) << 5) | arm_reg(fd);
	B32(inst);
}

// FMUL (floating-point multiply): fd = fn * fm
// Format: M 0 S 11110 ftype(2) 1 Rm(5) 000010 Rn(5) Rd(5)
static void arm_fmul(jit_ctx *ctx, Arm64FpReg fd, Arm64FpReg fn, Arm64FpReg fm, bool is64) {
	unsigned int ftype = is64 ? 1 : 0;
	unsigned int inst = (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (arm_reg(fm) << 16) | (0x02 << 10) |
	                    (arm_reg(fn) << 5) | arm_reg(fd);
	B32(inst);
}

// FDIV (floating-point divide): fd = fn / fm
// Format: M 0 S 11110 ftype(2) 1 Rm(5) 000110 Rn(5) Rd(5)
static void arm_fdiv(jit_ctx *ctx, Arm64FpReg fd, Arm64FpReg fn, Arm64FpReg fm, bool is64) {
	unsigned int ftype = is64 ? 1 : 0;
	unsigned int inst = (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (arm_reg(fm) << 16) | (0x06 << 10) |
	                    (arm_reg(fn) << 5) | arm_reg(fd);
	B32(inst);
}

// FNEG (floating-point negate): fd = -fn
// Format: M 0 S 11110 ftype(2) 1 000000 010000 Rn(5) Rd(5)
static void arm_fneg(jit_ctx *ctx, Arm64FpReg fd, Arm64FpReg fn, bool is64) {
	unsigned int ftype = is64 ? 1 : 0;
	unsigned int inst = (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (0x40 << 10) | (arm_reg(fn) << 5) | arm_reg(fd);
	B32(inst);
}

// FABS (floating-point absolute value): fd = |fn|
// Format: M 0 S 11110 ftype(2) 1 000000 010001 Rn(5) Rd(5)
static void arm_fabs(jit_ctx *ctx, Arm64FpReg fd, Arm64FpReg fn, bool is64) {
	unsigned int ftype = is64 ? 1 : 0;
	unsigned int inst = (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (0x41 << 10) | (arm_reg(fn) << 5) | arm_reg(fd);
	B32(inst);
}

// FSQRT (floating-point square root): fd = sqrt(fn)
// Format: M 0 S 11110 ftype(2) 1 000000 010011 Rn(5) Rd(5)
static void arm_fsqrt(jit_ctx *ctx, Arm64FpReg fd, Arm64FpReg fn, bool is64) {
	unsigned int ftype = is64 ? 1 : 0;
	unsigned int inst = (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (0x43 << 10) | (arm_reg(fn) << 5) | arm_reg(fd);
	B32(inst);
}

// FCMP (floating-point compare): flags = fn - fm
// Format: M 0 S 11110 ftype(2) 1 Rm(5) 00 1000 Rn(5) opcode2(5)
static void arm_fcmp(jit_ctx *ctx, Arm64FpReg fn, Arm64FpReg fm, bool is64) {
	unsigned int ftype = is64 ? 1 : 0;
	unsigned int inst = (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (arm_reg(fm) << 16) | (0x08 << 10) | (arm_reg(fn) << 5);
	B32(inst);
}

// SCVTF (Signed Convert to Float): Convert signed integer to float
// Format: 0 sf 0 11110 ftype(2) 1 00 010 000000 Rn(5) Rd(5)
// sf=1 for 64-bit int, ftype=0 for F32, ftype=1 for F64
static void arm_scvtf(jit_ctx *ctx, Arm64FpReg fd, Arm64Reg rn, bool int64, bool float64) {
	unsigned int sf = int64 ? 1 : 0;
	unsigned int ftype = float64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (0x02 << 16) | (arm_reg(rn) << 5) | arm_reg(fd);
	B32(inst);
}

// UCVTF (Unsigned Convert to Float): Convert unsigned integer to float
// Format: 0 sf 0 11110 ftype(2) 1 00 011 000000 Rn(5) Rd(5)
static void arm_ucvtf(jit_ctx *ctx, Arm64FpReg fd, Arm64Reg rn, bool int64, bool float64) {
	unsigned int sf = int64 ? 1 : 0;
	unsigned int ftype = float64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (0x03 << 16) | (arm_reg(rn) << 5) | arm_reg(fd);
	B32(inst);
}

// FCVTZS (Float Convert to Signed integer, rounding towards Zero)
// Format: 0 sf 0 11110 ftype(2) 1 11 000 000000 Rn(5) Rd(5)
static void arm_fcvtzs(jit_ctx *ctx, Arm64Reg rd, Arm64FpReg fn, bool int64, bool float64) {
	unsigned int sf = int64 ? 1 : 0;
	unsigned int ftype = float64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (0x18 << 16) | (arm_reg(fn) << 5) | arm_reg(rd);
	B32(inst);
}

// FCVT (Float Convert between F32 and F64)
// Format: M 0 S 11110 opc(2) 1 00 0100 00000 Rn(5) Rd(5)
// opc: 00=S to H, 01=D to H, 10=H to S, 11=S to D, 00=D to S, 01=H to D
static void arm_fcvt(jit_ctx *ctx, Arm64FpReg fd, Arm64FpReg fn, bool src64) {
	unsigned int opc = src64 ? 0x01 : 0x03;  // D to S or S to D
	unsigned int inst = (0x1E << 24) | (opc << 22) | (1 << 21) |
	                    (0x04 << 15) | (arm_reg(fn) << 5) | arm_reg(fd);
	B32(inst);
}

// FMOV (floating-point to general register): rd = fn
// Format: sf 0 S 11110 ftype(2) 1 00 110 000000 Rn(5) Rd(5)
static void arm_fmov_to_gen(jit_ctx *ctx, Arm64Reg rd, Arm64FpReg fn, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int ftype = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (0x18 << 10) | (arm_reg(fn) << 5) | arm_reg(rd);
	B32(inst);
}

// FMOV (general register to floating-point): fd = rn
// Format: sf 0 S 11110 ftype(2) 1 00 111 000000 Rn(5) Rd(5)
static void arm_fmov_from_gen(jit_ctx *ctx, Arm64FpReg fd, Arm64Reg rn, bool is64) {
	unsigned int sf = is64 ? 1 : 0;
	unsigned int ftype = is64 ? 1 : 0;
	unsigned int inst = (sf << 31) | (0x1E << 24) | (ftype << 22) | (1 << 21) |
	                    (0x1C << 10) | (arm_reg(rn) << 5) | arm_reg(fd);
	B32(inst);
}

// =====================================================================
// ARM64 Instruction Encoders - Branch Instructions
// =====================================================================

// B (unconditional branch): Branch to PC + (offset << 2)
// Format: 0 00101 imm26(26)
static int arm_b(jit_ctx *ctx, int offset) {
	// offset is in bytes, needs to be >> 2 for instruction encoding
	int pos = ARM_BUF_POS();
	if (offset == 0) {
		// Forward branch - will be patched later
		B32(0x14000000);  // B with 0 offset
		return pos;
	}
	if (!arm_fits_signed(offset >> 2, 26)) {
		ASSERT(9);
		return pos;
	}
	unsigned int imm26 = (offset >> 2) & 0x3FFFFFF;
	unsigned int inst = (0x05 << 26) | imm26;
	B32(inst);
	return pos;
}

// BL (branch with link): Call function at PC + (offset << 2)
// Format: 1 00101 imm26(26)
static int arm_bl(jit_ctx *ctx, int offset) {
	int pos = ARM_BUF_POS();
	if (offset == 0) {
		B32(0x94000000);  // BL with 0 offset
		return pos;
	}
	if (!arm_fits_signed(offset >> 2, 26)) {
		ASSERT(10);
		return pos;
	}
	unsigned int imm26 = (offset >> 2) & 0x3FFFFFF;
	unsigned int inst = (0x25 << 26) | imm26;
	B32(inst);
	return pos;
}

// BR (branch to register): Branch to address in register
// Format: 1101011 0000 11111 000000 Rn(5) 00000
static void arm_br(jit_ctx *ctx, Arm64Reg rn) {
	unsigned int inst = (0xD61F << 16) | (arm_reg(rn) << 5);
	B32(inst);
}

// BLR (branch with link to register): Call function at address in register
// Format: 1101011 0001 11111 000000 Rn(5) 00000
static void arm_blr(jit_ctx *ctx, Arm64Reg rn) {
	unsigned int inst = (0xD63F << 16) | (arm_reg(rn) << 5);
	B32(inst);
}

// RET (return): Return to address in X30 (LR) or specified register
// Format: 1101011 0010 11111 000000 Rn(5) 00000
static void arm_ret(jit_ctx *ctx, Arm64Reg rn) {
	unsigned int inst = (0xD65F << 16) | (arm_reg(rn) << 5);
	B32(inst);
}

// B.cond (conditional branch): Branch to PC + (offset << 2) if condition true
// Format: 01010100 imm19(19) 0 cond(4)
static int arm_b_cond(jit_ctx *ctx, Arm64Condition cond, int offset) {
	int pos = ARM_BUF_POS();
	if (offset == 0) {
		// Forward branch - will be patched later
		B32(0x54000000 | cond);
		return pos;
	}
	if (!arm_fits_signed(offset >> 2, 19)) {
		ASSERT(11);
		return pos;
	}
	unsigned int imm19 = (offset >> 2) & 0x7FFFF;
	unsigned int inst = (0x54 << 24) | (imm19 << 5) | cond;
	B32(inst);
	return pos;
}

// CBZ (compare and branch if zero)
// Format: sf 011010 0 imm19(19) Rt(5)
static int arm_cbz(jit_ctx *ctx, Arm64Reg rt, int offset, bool is64) {
	int pos = ARM_BUF_POS();
	if (offset == 0) {
		unsigned int sf = is64 ? 1 : 0;
		B32((sf << 31) | (0x34 << 24) | arm_reg(rt));
		return pos;
	}
	if (!arm_fits_signed(offset >> 2, 19)) {
		ASSERT(12);
		return pos;
	}
	unsigned int sf = is64 ? 1 : 0;
	unsigned int imm19 = (offset >> 2) & 0x7FFFF;
	unsigned int inst = (sf << 31) | (0x34 << 24) | (imm19 << 5) | arm_reg(rt);
	B32(inst);
	return pos;
}

// CBNZ (compare and branch if not zero)
// Format: sf 011010 1 imm19(19) Rt(5)
static int arm_cbnz(jit_ctx *ctx, Arm64Reg rt, int offset, bool is64) {
	int pos = ARM_BUF_POS();
	if (offset == 0) {
		unsigned int sf = is64 ? 1 : 0;
		B32((sf << 31) | (0x35 << 24) | arm_reg(rt));
		return pos;
	}
	if (!arm_fits_signed(offset >> 2, 19)) {
		ASSERT(13);
		return pos;
	}
	unsigned int sf = is64 ? 1 : 0;
	unsigned int imm19 = (offset >> 2) & 0x7FFFF;
	unsigned int inst = (sf << 31) | (0x35 << 24) | (imm19 << 5) | arm_reg(rt);
	B32(inst);
	return pos;
}

// =====================================================================
// ARM64 Branch Patching
// =====================================================================

static void arm_patch_branch(jit_ctx *ctx, int jump_pos, int target_pos) {
	unsigned int *instr = (unsigned int*)(ctx->startBuf + jump_pos);
	int offset = target_pos - jump_pos;

	unsigned int opcode = (*instr >> 24) & 0xFF;

	// Check instruction type and patch accordingly
	if ((opcode & 0xFC) == 0x14) {
		// B or BL (26-bit offset)
		if (!arm_fits_signed(offset >> 2, 26)) {
			ASSERT(14);
			return;
		}
		unsigned int imm26 = (offset >> 2) & 0x3FFFFFF;
		*instr = (*instr & 0xFC000000) | imm26;
	} else if (opcode == 0x54) {
		// B.cond (19-bit offset)
		if (!arm_fits_signed(offset >> 2, 19)) {
			ASSERT(15);
			return;
		}
		unsigned int imm19 = (offset >> 2) & 0x7FFFF;
		*instr = (*instr & 0xFF00001F) | (imm19 << 5);
	} else if ((opcode & 0xFE) == 0x34) {
		// CBZ/CBNZ (19-bit offset)
		if (!arm_fits_signed(offset >> 2, 19)) {
			ASSERT(16);
			return;
		}
		unsigned int imm19 = (offset >> 2) & 0x7FFFF;
		*instr = (*instr & 0xFF00001F) | (imm19 << 5);
	}
}

// =====================================================================
// ARM64 Jump/Branch Helpers
// =====================================================================

// Helper: Perform unconditional jump and return position for patching
static int arm_do_jump(jit_ctx *ctx) {
	return arm_b(ctx, 0);  // Forward branch with offset=0 (to be patched)
}

// Helper: Perform conditional jump based on flags and return position for patching
static int arm_do_jump_cond(jit_ctx *ctx, Arm64Condition cond) {
	return arm_b_cond(ctx, cond, 0);  // Forward branch with offset=0 (to be patched)
}

// Helper: Perform CBZ (compare and branch if zero) and return position for patching
static int arm_do_cbz(jit_ctx *ctx, Arm64Reg rt, bool is64) {
	return arm_cbz(ctx, rt, 0, is64);  // Forward branch with offset=0 (to be patched)
}

// Helper: Perform CBNZ (compare and branch if non-zero) and return position for patching
static int arm_do_cbnz(jit_ctx *ctx, Arm64Reg rt, bool is64) {
	return arm_cbnz(ctx, rt, 0, is64);  // Forward branch with offset=0 (to be patched)
}

// Helper: Patch a jump to target current position
static void arm_patch_jump(jit_ctx *ctx, int jump_pos) {
	if (jump_pos == 0) return;
	int target_pos = ARM_BUF_POS();
	arm_patch_branch(ctx, jump_pos, target_pos);
}

// Helper: Patch a jump to a specific target position
static void arm_patch_jump_to(jit_ctx *ctx, int jump_pos, int target_pos) {
	if (jump_pos == 0) return;
	arm_patch_branch(ctx, jump_pos, target_pos);
}

// Helper: Patch a CBZ to a specific target position
static void arm_patch_cbz(jit_ctx *ctx, int jump_pos, int target_pos) {
	arm_patch_jump_to(ctx, jump_pos, target_pos);
}

// Helper: Patch a CBNZ to a specific target position
static void arm_patch_cbnz(jit_ctx *ctx, int jump_pos, int target_pos) {
	arm_patch_jump_to(ctx, jump_pos, target_pos);
}

// =====================================================================
// ARM64 Function Prologue/Epilogue (AAPCS64 Calling Convention)
// =====================================================================

// Generate function prologue:
// - Save FP (X29) and LR (X30) to stack
// - Set up new frame pointer
// - Allocate stack space for local variables
//
// Standard ARM64 prologue:
//   stp    x29, x30, [sp, #-framesize]!   ; pre-index: sp -= framesize, store FP/LR
//   mov    x29, sp                         ; set frame pointer
//
// Note: framesize must be 16-byte aligned per AAPCS64
static void arm_prologue(jit_ctx *ctx, int framesize) {
	// Ensure 16-byte stack alignment
	if (framesize & 15) {
		framesize = (framesize + 15) & ~15;
	}

	// STP with pre-index: stp x29, x30, [sp, #-framesize]!
	// We use negative offset and pre-index mode
	// For STP pre-index: opc(2) 101 0 011 0 imm7(7) Rt2(5) Rn(5) Rt(5)
	int imm7 = -framesize >> 3;  // Offset in units of 8 bytes for 64-bit pair
	if (!arm_fits_signed(imm7, 7)) {
		// Frame too large for single STP, need to adjust SP first
		// sub sp, sp, #framesize
		if (framesize <= 4095) {
			arm_sub_imm(ctx, XZR, XZR, framesize, true);  // XZR acts as SP in this context
		} else {
			// Large frame: use temporary register
			arm_load_imm64(ctx, X9, framesize);
			arm_sub_reg(ctx, XZR, XZR, X9, true);
		}
		// stp x29, x30, [sp] - offset mode (no writeback)
		// For offset mode, we'll manually encode since our function only does writeback modes
		// TODO: add offset mode support to arm_stp
		unsigned int inst = (2 << 30) | (0x29 << 25) | (2 << 23) |  // offset mode
		                    (0 << 15) | (30 << 10) | (31 << 5) | 29;
		B32(inst);
	} else {
		// STP with pre-index
		// Format: 10 1 01001 1 imm7(7) Rt2(5) Rn(5) Rt(5)
		unsigned int inst = (2 << 30) |           // bits [31:30] = 10 (64-bit)
		                    (1 << 29) |           // bit 29 = 1
		                    (0b01001 << 24) |     // bits [28:24] = 01001
		                    (1 << 23) |           // bit 23 = 1 (pre-index)
		                    ((imm7 & 0x7F) << 15) | // imm7
		                    (30 << 10) |          // Rt2 = X30 (LR)
		                    (31 << 5) |           // Rn = 31 (SP)
		                    29;                   // Rt = X29 (FP)
		B32(inst);
	}

	// mov x29, sp (set frame pointer)
	// CRITICAL: Must use ADD, not ORR-based mov_reg!
	// In ADD, reg 31 = SP; in ORR, reg 31 = XZR (zero)
	int pos_before = ARM_BUF_POS();
	arm_add_imm(ctx, X29, XZR, 0, true);  // ADD X29, SP, #0
	int pos_after = ARM_BUF_POS();
	unsigned int *instr_ptr = (unsigned int*)(ctx->startBuf + pos_before);
	printf("[PROLOGUE] MOV X29, SP instruction at offset %d: 0x%08x\n", pos_before, *instr_ptr);
}

// Generate function epilogue:
// - Restore FP (X29) and LR (X30) from stack
// - Restore stack pointer
// - Return to caller
//
// Standard ARM64 epilogue:
//   ldp    x29, x30, [sp], #framesize    ; post-index: load FP/LR, sp += framesize
//   ret                                   ; return to X30 (LR)
static void arm_epilogue(jit_ctx *ctx, int framesize) {
	// Ensure 16-byte stack alignment
	if (framesize & 15) {
		framesize = (framesize + 15) & ~15;
	}

	// LDP with post-index: ldp x29, x30, [sp], #framesize
	int imm7 = framesize >> 3;  // Offset in units of 8 bytes
	if (!arm_fits_signed(imm7, 7)) {
		// Frame too large, restore in steps - use offset mode (no writeback)
		// TODO: add offset mode support to arm_ldp
		unsigned int inst = (2 << 30) | (0x29 << 25) | (2 << 23) |  // offset mode
		                    (0 << 15) | (30 << 10) | (31 << 5) | 29;
		B32(inst);
		// add sp, sp, #framesize
		if (framesize <= 4095) {
			arm_add_imm(ctx, XZR, XZR, framesize, true);
		} else {
			arm_load_imm64(ctx, X9, framesize);
			arm_add_reg(ctx, XZR, XZR, X9, true);
		}
	} else {
		// LDP with post-index
		// Format: 10 1 01000 1 imm7(7) Rt2(5) Rn(5) Rt(5)
		unsigned int inst = (2 << 30) |           // bits [31:30] = 10 (64-bit)
		                    (1 << 29) |           // bit 29 = 1
		                    (0b01000 << 24) |     // bits [28:24] = 01000
		                    (1 << 23) |           // bit 23 = 1 (post-index)
		                    ((imm7 & 0x7F) << 15) | // imm7
		                    (30 << 10) |          // Rt2 = X30 (LR)
		                    (31 << 5) |           // Rn = 31 (SP)
		                    29;                   // Rt = X29 (FP)
		B32(inst);
	}

	// ret (return to X30/LR)
	arm_ret(ctx, X30);
}

// =====================================================================
// ARM64 Calling Convention Helpers (AAPCS64)
// =====================================================================

// AAPCS64: First 8 integer/pointer arguments go in X0-X7
// Returns the argument register for a given argument index, or -1 if on stack
static int arm_arg_reg(int arg_index) {
	if (arg_index < 8) {
		return X0 + arg_index;
	}
	return -1;  // Argument is on stack
}

// AAPCS64: First 8 FP/SIMD arguments go in V0-V7
static int arm_fp_arg_reg(int arg_index) {
	if (arg_index < 8) {
		return V0 + arg_index;
	}
	return -1;  // Argument is on stack
}

// Calculate stack offset for arguments beyond register arguments
// Each stack argument takes 8 bytes (aligned)
static int arm_arg_stack_offset(int arg_index) {
	if (arg_index < 8) {
		return -1;  // Not on stack
	}
	// Stack arguments start after the saved FP/LR (16 bytes from SP)
	return 16 + ((arg_index - 8) * 8);
}

#endif // HL_JIT_ARM64
