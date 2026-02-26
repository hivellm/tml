# Proposal: fix-ref-slice-expression

## Why

The expression `ref buf[0 to n]` (taking a reference to an array slice) failed with error C026: "Can only take reference of variables". This blocked the TCP/UDP request benchmark (`benchmarks/profile_tml/tcp_udp_request_bench.tml:240`) and any code that passes array slices by reference to functions expecting `ref [T]`.

## What Changes

### 1. AST Codegen: IndexExpr in Ref handler (unary.cpp)

The `gen_unary()` Ref handler previously only supported:
- `ref variable` → return alloca pointer
- `ref literal` → alloca + store + return pointer
- `ref struct.field` → GEP to field

Added two new cases for `IndexExpr`:

**`ref arr[start to end]`** (RangeExpr index → fat pointer slice):
- Get array base pointer from locals_
- Evaluate start and end expressions
- GEP to `arr[start]` for data pointer
- Compute `length = end - start`
- Allocate `{ ptr, i64 }` fat pointer on stack
- Store data_ptr (field 0) and length (field 1)
- Return pointer to fat pointer

**`ref arr[i]`** (simple index → element pointer):
- Get array pointer from locals_
- Evaluate index expression
- GEP to `arr[0][i]`
- Return element pointer

### 2. MIR Codegen: Missing struct type declarations (mir_codegen.cpp)

`StructInitInst` instructions referencing imported structs (like `Range` from for-loop iteration) generated `%struct.Range` in the IR without declaring the type. Added:
- `used_struct_types_` member variable to track struct names → field types
- First-pass collection in both `generate()` and `generate_cgu()`
- Type declaration emission in `emit_type_defs()` for undeclared structs

### 3. Type Checker: RefType unwrapping for slice operations

**3.1 `check_index()` — ref [T] indexing (expr.cpp:1071-1076)**
- `check_index()` resolved `ArrayType` and `SliceType` but not `RefType` wrapping them
- Added RefType unwrapping before the ArrayType/SliceType checks
- Now `data[0]` on `ref [T]` correctly returns `T` instead of `()`

**3.2 SliceType method resolution — ref [T].len() (expr_call_method.cpp:1236-1243)**
- The SliceType method handler (`is<SliceType>()`) didn't match `ref [T]` (which is `RefType { inner: SliceType }`)
- Added RefType unwrapping before the SliceType check
- Now `.len()`, `.is_empty()`, `.get()`, `.iter()`, etc. work on `ref [T]`

### 4. MIR Codegen: Bounds-check guard for non-integer index types (instructions.cpp)

- MIR bounds-check emitted `icmp slt %struct.Range %v4, 0` — comparing a struct to an integer
- Added `is_integer_idx` guard: only emit `icmp` bounds checks when the index type starts with `i` and doesn't contain `%` (i.e., is an LLVM integer type, not a struct)
- Same guard applied to the `@llvm.assume` hint path
- Struct-typed indices (from for-loop Range iteration) now skip bounds checks instead of generating invalid IR

## Impact

- Affected specs: None (implementation-only fix)
- Affected code:
  - `compiler/src/codegen/llvm/expr/unary.cpp` — IndexExpr in Ref handler
  - `compiler/include/codegen/mir_codegen.hpp` — used_struct_types_ member
  - `compiler/src/codegen/mir_codegen.cpp` — struct type collection + emission
  - `compiler/src/types/checker/expr.cpp` — RefType unwrapping in check_index()
  - `compiler/src/types/checker/expr_call_method.cpp` — RefType unwrapping for SliceType methods
  - `compiler/src/codegen/mir/instructions.cpp` — integer type guard for bounds checks
- Breaking change: NO
- User benefit: `ref array[start to end]` and `ref array[index]` expressions now work correctly, enabling array slice passing to functions. `.len()` and indexing on `ref [T]` now type-check correctly.
