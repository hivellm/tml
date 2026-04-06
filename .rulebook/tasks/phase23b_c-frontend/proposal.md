# Proposal: C17 Frontend — Parser, Type Checker, C→MIR Lowering

## Why

TML's C runtime (`compiler/runtime/core/essential.c`, `compiler/runtime/memory/mem.c`) is currently
compiled by Clang or GCC. This means building TML requires an external C compiler — a dependency
that contradicts the project's goal of full toolchain independence. Phase 23b eliminates that
dependency by giving TML the ability to compile its own C source files.

Beyond self-sufficiency, a C frontend enables TML to interoperate with the vast C ecosystem: any
project that calls C libraries or wraps C code can use `tml cc` to compile the C parts, using the
same build tool and ABI-aware type checker as the TML parts.

## What Changes

A C17 frontend is added under `compiler-tml/src/cc/`, consisting of a lexer, parser (declarations,
expressions, statements), type checker, and a C→MIR lowering pass. The MIR produced by the C
frontend is identical in structure to the MIR produced by the TML frontend and feeds into the same
backend pipeline (MIR passes → codegen → linker), so no backend changes are needed.

### C Lexer (~1,200 LOC)

C17 token types, all literal forms (integer, float, char, string with all prefix variants),
all operators, plus trigraph removal and line splicing as the first two translation phases.

### C Parser (~5,000 LOC)

Three modules cover the full C17 grammar:

- **Declarations** (~2,000 LOC): declaration specifiers, variable declarations with initializers,
  function declarations (ANSI and K&R), struct/union/enum, typedef (including the typedef-name
  ambiguity that requires the type environment during parsing)
- **Expressions** (~1,500 LOC): all 15 precedence levels via precedence climbing, pointer/memory
  operators, sizeof/alignof, casts, compound literals, _Generic
- **Statements** (~1,500 LOC): all C control flow including goto/labels, C99 for-init declarations,
  C11 _Static_assert, GCC __attribute__ parsed and ignored for compatibility

### C Type Checker (~3,000 LOC)

C's type system is subtle in several ways that require careful implementation:

- **Integer promotions** (C11 §6.3.1.1): char/short → int in arithmetic; signed vs unsigned mixing
- **Usual arithmetic conversions** (§6.3.1.8): common type for binary operations
- **Implicit conversions**: lvalue-to-rvalue, array decay, function decay, null pointer
- **Pointer arithmetic**: `ptr + n` advances by `n * sizeof(*ptr)`; `ptr1 - ptr2` yields elements
- **Struct/union layout**: field offsets and padding per System V AMD64 ABI and Windows x64 ABI

### C → MIR Lowering (~4,000 LOC)

The lowering pass translates C AST nodes to `mir::Module`:

- C types → MIR types (`i8`/`i16`/`i32`/`i64`, `f32`/`f64`, `ptr`)
- Struct field access → GEP with computed byte offset
- Array subscript → GEP with stride = sizeof(element)
- Switch → `switch_int` with per-case arms
- Bit fields → extract/insert via shifts and masks
- Designated initializers → alloca + stores
- Integer promotions and implicit conversions → explicit MIR cast instructions

## Key Decisions

**C17 target, not C11 or C23**: C17 (ISO/IEC 9899:2018) makes no normative changes to C11 — it
only fixes defects. Targeting C17 means the same implementation satisfies C11 and C17. C23 features
(_BitInt, typeof, typeof_unqual, nullptr) are explicitly out of scope.

**VLAs (Variable-Length Arrays) — stack allocation only**: C99 VLAs are a common source of bugs
and stack overflows. The implementation supports them (required for POSIX headers) but only as
`alloca`-based stack allocations; no heap fallback, no out-of-bounds detection.

**_Generic — parsed but lowered as constant-folded**: C11 _Generic is required by many
`<tgmath.h>` and `<stddef.h>` macros. Type selection happens at type-check time and the result is
a constant-folded single expression in MIR.

**Reuses TML's MIR pipeline**: The C frontend produces `mir::Module` identical to what the TML
frontend produces. No new backend, no new codegen, no new linker changes. This is the key design
decision that keeps the implementation tractable.

**Prior art**: chibicc (~5K LOC, C11, no optimizations), TCC (~15K LOC, C99, fast codegen),
cparser (~15K LOC, C99/C11, full type system). TML's implementation is closest to cparser in
scope but targets MIR instead of a custom IR.

## Risk

**High**: The typedef-name ambiguity (an identifier that was typedef'd must be recognized as a
type name during subsequent parsing) requires the type environment to be consulted during parsing —
a classic C parsing complication. Integer promotion rules and struct layout differences between
System V AMD64 and Windows x64 ABIs are the most likely sources of subtle bugs.

**Mitigation**: The primary correctness test (compiling `essential.c` and `mem.c` and running the
full TML test suite against the result) provides a strong end-to-end validation.

## Success Criteria

1. `tml cc compiler/runtime/core/essential.c` compiles without errors
2. `tml cc compiler/runtime/memory/mem.c` compiles without errors
3. Binaries linked with TML-compiled runtime objects pass the full TML test suite
4. Output is ABI-compatible with GCC/Clang for the System V AMD64 and Windows x64 calling
   conventions (verified by calling TML-compiled C from TML-compiled TML)

## Dependencies

- **Requires**: phase23a (C preprocessor produces `List[PpToken]` consumed by C parser);
  phase15c (MIR builder as compilation target — C frontend emits into `mir::Module`)
- **Blocks**: phase23c (C++ subset frontend extends the C parser and type checker)

## Estimated Size

~16,200 LOC TML across:
- `compiler-tml/src/cc/lexer.tml` (~1,200 LOC)
- `compiler-tml/src/cc/parser.tml` (~2,000 LOC — declarations + infrastructure)
- `compiler-tml/src/cc/parse_expr.tml` (~1,500 LOC — expressions)
- `compiler-tml/src/cc/parse_stmt.tml` (~1,500 LOC — statements)
- `compiler-tml/src/cc/types.tml` (~3,000 LOC — type checker, ABI layout, promotions)
- `compiler-tml/src/cc/lower.tml` (~4,000 LOC — C AST → MIR lowering)
- `compiler-tml/src/cc/ast.tml` (~3,000 LOC — C AST node definitions)
