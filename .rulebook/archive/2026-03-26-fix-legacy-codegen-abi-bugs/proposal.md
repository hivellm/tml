# Proposal: Fix Legacy Codegen ABI Bugs

## Why

The legacy codegen (compiler/src/codegen/llvm/) has multiple pre-existing bugs that prevent non-trivial TML programs from running correctly on Windows x64. These bugs were discovered while implementing the documentation audit script (`scripts/audit_docs.tml`) — the script type-checks and compiles correctly, but segfaults at runtime due to incorrect LLVM IR generation.

These bugs block:
- The TML documentation audit script (dogfooding the language)
- Any TML program using nested generic structs (e.g., `List[FileReport]` where `FileReport` contains `List[Violation]`)
- Programs with functions that take `Str` params and return structs
- Template literal interpolation in loops with mixed types

## What Changes

Fix three categories of codegen bugs in the legacy LLVM IR generator:

### Bug 1: Nested Generic Struct Copy (SEGFAULT)
- **Reproduction**: `List[FileReport].get(0)` where `FileReport` contains `List[Violation]`
- **Symptom**: Segfault at runtime — inner `List[Violation]` is shallow-copied, pointer becomes dangling
- **Root cause**: `List.get()` copies struct by value without deep-copying inner heap-allocated fields
- **Files**: `compiler/src/codegen/llvm/` (method dispatch for generic get), possibly `compiler/runtime/collections/`
- **Fix**: Either use sret for large structs or ensure proper deep copy semantics

### Bug 2: Struct Return + Str Param ABI Mismatch (SEGFAULT)
- **Reproduction**: `func scan(filepath: Str) -> FileReport { ... }` — any function with Str param returning a struct
- **Symptom**: Segfault at runtime, works fine if param is removed or function returns void
- **Root cause**: Windows x64 calling convention conflict — struct return via register + ptr param creates ABI mismatch. The same function without the Str param works.
- **Evidence**: `func scan() -> List[Violation]` works; `func scan(s: Str) -> List[Violation]` segfaults
- **Files**: `compiler/src/codegen/llvm/expr/call_user.cpp`, `compiler/src/codegen/llvm/llvm_ir_gen.cpp`
- **Fix**: Use sret convention for struct-returning functions when params are present

### Bug 3: Template Literal Type Confusion in Loops (IR ERROR)
- **Reproduction**: `` println(`{i}: {trimmed}`) `` inside a loop where `i: I64` and `trimmed: Str`
- **Symptom**: LLVM IR verification error — `%struct.Text` passed to `print_i32` or i64 passed to `str::trim`
- **Root cause**: Template literal codegen conflates SSA values from different types when multiple interpolations exist in the same expression inside a loop
- **Files**: `compiler/src/codegen/llvm/expr/` (template literal / string interpolation codegen)
- **Fix**: Ensure each interpolated value gets its own SSA temporary with correct type

## Impact
- Affected specs: None (bug fixes, no spec changes)
- Affected code: `compiler/src/codegen/llvm/` (legacy codegen)
- Breaking change: NO
- User benefit: TML programs with generic collections, file I/O, and template literals work correctly. Unblocks `scripts/audit_docs.tml` and similar non-trivial TML applications.

## Success Criteria
- `scripts/audit_docs.tml` runs to completion without segfault
- Test: `func(Str) -> Struct` pattern works (no segfault)
- Test: `List[A]` where A contains `List[B]` — nested access works
- Test: Template literals with mixed I64 + Str in loops compile and run correctly
- No regressions in existing test suite
