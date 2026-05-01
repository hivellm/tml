# Proposal: phase24b_cc-typedef-name-resolution

## Why

After phase24a wired the C preprocessor (directive sweep, macro
expansion, `#include` resolution, bundled stdlib headers) into
`cc_driver`, `tml cc` on a C source that uses a typedef'd name as
a type still crashes silently. Concrete reproducer:

```c
typedef int int32_t;
int32_t add(int32_t a, int32_t b) { return a + b; }
```

`tml cc test.c --emit=ast` returns no output and exits non-zero.
Initial guess was a parser bug; live `File::append_all` instrumentation
shows otherwise:

  - `cp_parse_translation_unit` returns Ok with 2 decls.
  - `lower_translation_unit` walks decls; `lower_typedef` for
    `int32_t = int` completes; HashMap entry registered.
  - `lower_func_decl` for `add` enters; the parameters loop calls
    `base_to_ctype(env, p.specifiers.base_type)` which is
    `Typedef("int32_t")`.
  - Inside `base_to_ctype`'s `Typedef(name)` arm, `env.typedefs.get(name)`
    returns a `Heap[CType]`. `t.get()` returns a `CType` value.
    The crash happens on the `return` of that CType from
    `base_to_ctype` to its caller.
  - Synthetic-token equivalent calling `cp_parse_translation_unit`
    directly without going through cc_driver's full pipeline does
    NOT crash (test in `c_parser.test.tml::test_typedef_then_use_*`).
    So the parser is correct; the lowerer's CType return path is
    the trigger.

This is the same family of ABI mismatch as `phase0x_heap-decl-codegen-crash`:
returning a value-type with `Heap`-wrapped enum/struct fields by
value triggers a calling-convention mismatch on Win64. phase0x
fixed the call-site struct→ptr fixup for `Heap[T]::new(...)`
(a function ARG path); the same fix needs to land on the function
RETURN path so that returning a `CType` (or any aggregate with
`Heap[X]` fields) from a function does not corrupt memory.

## What Changes

1. **Reproduce the crash with a minimal C++ test fixture.** The
   TML-side test framework times out on large-test compilation; a
   smaller C++ fixture in `compiler/tests/compiler/` (matching the
   `heap_decl_var_repro.test.tml` pattern from phase0x but for the
   return path) should isolate the codegen bug from the lowerer.

2. **Compare LLVM IR with Rust's calling convention.** Rust returns
   16-byte enums by value via SSE registers on Win64; TML may be
   emitting a sret return without matching the call-site
   expectation. Use the Rust-as-Reference IR methodology that
   phase0x landed.

3. **Apply the codegen fix** in `compiler/src/codegen/llvm/...`
   alongside the existing phase0x patch. Likely candidates:
   `compiler/src/codegen/llvm/decl/func.cpp` (return type lowering)
   and `compiler/src/codegen/llvm/expr/return_stmt.cpp` (return
   instruction emission).

4. **Verify on the original typedef reproducer.** Once the codegen
   fix lands, `tml cc test_no_inc.c --emit=ast` should succeed.

5. **Run phase24a Phase 5**: `tml cc essential.c` should now reach
   further into the file before any next limitation surfaces.

## Impact

- Affected specs: none (codegen fix, no language change).
- Affected code:
  - `compiler/src/codegen/llvm/...` — the actual fix.
  - `compiler/tests/compiler/heap_ctype_return_repro.test.tml` —
    minimal regression for the return-path ABI mismatch.
- Breaking change: NO.
- User benefit: unblocks phase24 Phase 4 — `tml cc` becomes
  usable on real C runtime files. Without this fix any TML library
  module that returns a complex value type (CType, CDecl, MIR
  Instruction enum, ...) by value risks the same crash class.
