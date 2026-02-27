# Proposal: Compiler Fixes for Core Library Blockers

## Why

During implementation of the core library (`complete-core-library-gaps`), several compiler bugs and missing features were identified that block full completion. These issues prevent:

- Importing `core::num` module (I8/I16 codegen bugs)
- Testing iterator consumer methods (default method dispatch)
- Using format behaviors (nested string return corruption)
- Implementing `collect`/`partition` (parameterized bounds)
- Runtime type identification (TypeId intrinsic)
- Generic higher-order functions (Fn trait auto-impl)

**Current state:**
- Core library is 98% complete (148/151 tasks)
- Remaining 2% blocked by compiler limitations
- 939 tests pass, but some features untestable

**Impact of not fixing:**
- `core::num` cannot be fully imported
- Iterator consumer methods (`count`, `fold`, etc.) return wrong types
- Format behaviors (`Binary`, `Hex`) untestable
- Generic collection functions impossible to implement
- `Any` behavior incomplete without `TypeId::of`

## What Changes

### 1. I8/I16 Codegen Fixes (P0)
- Fix negation to produce correct LLVM type (i8/i16, not i32)
- Fix arithmetic/comparison to maintain small integer types
- Enables: `core::num` import, `I8::MIN`, `I16::MIN`

### 2. String Return Calling Convention (P1)
- Fix stack corruption when returning Str through multiple call frames
- Enables: Format behavior tests, nested string operations

### 3. Default Behavior Method Dispatch (P1)
- Fix method dispatch to return declared type (not Unit)
- Enables: Iterator consumer tests (`count`, `last`, `nth`, etc.)

### 4. Parser: Parameterized Behavior Bounds (P2)
- Support `C: FromIterator[T]` syntax in where clauses
- Enables: `collect`, `partition`, generic collection functions

### 5. TypeId Compiler Intrinsic (P2)
- Generate unique type IDs at compile time
- Enables: `TypeId::of[T]()`, `Any` behavior, runtime type checking

### 6. Fn Trait Auto-Implementation (P3)
- Auto-implement `Fn`/`FnMut`/`FnOnce` for closures
- Enables: Generic HOF with `F: Fn` bounds, closure trait objects

## Impact

### Files Affected
- `compiler/src/codegen/expr/unary.cpp` - I8/I16 negation
- `compiler/src/codegen/expr/binary.cpp` - I8/I16 operations
- `compiler/src/codegen/llvm_ir_gen_stmt.cpp` - String returns
- `compiler/src/codegen/expr/method.cpp` - Default method dispatch
- `compiler/src/parser/parser_generics.cpp` - Parameterized bounds
- `compiler/src/codegen/llvm_ir_gen_builtins.cpp` - TypeId intrinsic
- `compiler/src/codegen/expr/closure.cpp` - Fn trait auto-impl

### Breaking Changes
- None for user code
- Internal codegen changes only

### User Benefits
- Full `core::num` module usability
- All iterator methods work correctly
- Format behaviors testable
- Generic collection functions possible
- Runtime type identification available
- More flexible closure usage
