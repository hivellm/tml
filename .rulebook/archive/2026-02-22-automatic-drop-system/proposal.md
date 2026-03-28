# Proposal: automatic-drop-system

## Why

The TML compiler currently only emits drop calls for types with explicit `impl Drop`. This means heap-allocated types like `Str` (returned by string operations, encoding functions, formatting) are never freed, causing thousands of memory leaks across the test suite (33 suites, ~3,900 leaks, ~210KB lost).

The Rust compiler solves this with a `needs_drop` recursive analysis + automatic drop glue generation. TML needs an equivalent system to prevent memory leaks for ALL heap-allocated types, not just those with explicit `impl Drop`.

## What Changes

### Phase 1: Str Auto-Free (Immediate Fix)
- Add special-case handling in the drop system for `Str` type
- When a local variable of type `Str` goes out of scope, emit `call void @free(ptr %var)`
- Skip string literals (they point to global constants, not heap)
- Handle: let bindings, function returns, temporaries, reassignment

### Phase 2: `needs_drop` Recursive Analysis
- Implement `needs_drop(type)` query that recursively checks if any field needs cleanup
- Structs with fields that need drop → auto-generate field-level drops
- This matches Rust's drop glue system

### Phase 3: Temporary Value Drops
- Track temporary values (e.g., `foo().bar()` — foo()'s return) and drop them at statement end
- Track reassignment drops (`x = new_value` — drop old value first)

## Impact
- Affected specs: 06-MEMORY.md (ownership/drop semantics)
- Affected code: compiler/src/codegen/llvm/core/drop.cpp, llvm_ir_gen_stmt.cpp, types.cpp
- Breaking change: NO (adds automatic cleanup that was missing)
- User benefit: Zero memory leaks for standard TML programs without manual free() calls
