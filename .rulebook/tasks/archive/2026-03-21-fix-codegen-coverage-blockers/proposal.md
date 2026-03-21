# Fix Codegen Coverage Blockers

## Purpose

Fix the remaining LLVM codegen bugs that block ~300 library functions from being tested, preventing coverage from exceeding 94.5%. The biggest single blocker — generic trait dispatch returning void — accounts for ~57 functions alone.

## What Changes

### Phase 1: Generic Trait Dispatch → () (HIGH priority, ~57 functions)

Constrained generic behavior impls like `impl[T: Hash] Hash for Array[T, N]` resolve the return type as `()` (void) instead of the actual type (Bool, I64, Str, Ordering, etc.). This blocks PartialEq, Hash, Display, Debug, Default, Duplicate impls across Array, Pool, Range, Poll, and other types.

**Root cause**: When instantiating generic impl methods from library modules, the return type substitution fails — the type parameter `T` in the return type is not substituted with the concrete type.

**Files**: `compiler/src/types/checker/expr_call_method.cpp`, `compiler/src/codegen/llvm/core/generic.cpp`

### Phase 2: Missing LLVM Intrinsic Declarations (~19 functions)

The codegen doesn't emit declarations for `@tml_ptr_read_unaligned`, `@tml_ptr_write_unaligned`, `@tml_ptr_read_volatile`, `@tml_ptr_write_volatile`, `@tml_memcpy`, `@tml_memmove`, `@tml_memset`. These are needed by ptr module operations.

**Files**: `compiler/src/codegen/llvm/core/runtime_decls.cpp` or equivalent

### Phase 3: Mutex[Unit] Void Zeroinitializer (~8 functions)

`Mutex[Unit]` generates `void zeroinitializer` which is invalid LLVM IR. Blocks all thread/scope functions.

**Root cause**: Unit type mapped to void in struct fields instead of `{}` or `i8`.

**Files**: `compiler/src/codegen/llvm/core/types.cpp`, `compiler/src/codegen/llvm/decl/enum.cpp`

### Phase 4: Const Generic N Cross-Module (~19 functions)

`ArrayIter[I32, 3]` imported from another module loses the const generic value N, emitting `[0 x i32]` instead of `[3 x i32]`. The local-module fix was applied but `apply_type_substitutions` for `ArrayType` doesn't substitute the size parameter.

**Files**: `compiler/src/codegen/llvm/core/types_resolve.cpp`

### Phase 5: Closure Capture Bug (~5 functions)

Closures that capture function parameters return wrong values (0 instead of captured value). Blocks `local_const` and other closure-heavy APIs.

**Files**: `compiler/src/codegen/llvm/expr/closure.cpp` or equivalent

## Impact

- **Coverage**: 94.5% → 96%+ (Phase 1 alone gets ~95.5%)
- **Affected specs**: None (codegen internals only)
- **Breaking changes**: None
- **User benefit**: More library functions actually work correctly
