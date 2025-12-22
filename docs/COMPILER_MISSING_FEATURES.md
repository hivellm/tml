# TML Compiler - Missing Features Report

**Date**: 2025-12-22
**Compiler Version**: Bootstrap/Development
**Status**: Feature-Complete for Basic Programs, Missing Advanced Features

## Summary

The TML compiler successfully compiles basic programs but lacks several advanced features required for the full standard library and test framework. This document catalogs missing features discovered during package reorganization and testing.

## ✅ Recently Implemented (2025-12-22)

### Const Declarations
**Status**: ✅ Fully Implemented

**Implementation**:
- Parser: `parse_const_decl()` in `parser_decl.cpp`
- Type Checker: `check_const_decl()` in `checker.cpp`
- Codegen LLVM: Inline substitution via `global_constants_` map
- Codegen C: Generates `#define` directives

**Example**:
```tml
const MY_CONST: I64 = 42
const PI: F64 = 3.14159
```

**Tests**: `packages/compiler/tests/tml/test_const.tml` ✅ PASSING

### Panic() Builtin
**Status**: ✅ Implemented (C backend fully working, LLVM needs linking)

**Implementation**:
- Type System: Added to `env_builtins.cpp` with signature `panic(msg: Str) -> Never`
- Codegen C: `tml_panic()` in `tml_core.c` (prints to stderr, calls exit(1))
- Codegen LLVM: Emits `call void @tml_panic(ptr msg)` + `unreachable`
- Runtime: Added to `tml_runtime.h` and `tml_core.c`

**Example**:
```tml
func validate(x: I64) {
    if x < 0 {
        panic("Value must be non-negative")
    }
}
```

**Tests**: `packages/compiler/tests/tml/test_panic.tml` ✅ PASSING (C backend)

**Issue**: LLVM backend requires runtime linking configuration (symbol `tml_panic` not found during link)

## ❌ Missing Features (High Priority)

### 1. Pattern Binding in When Expressions
**Status**: ❌ Not Implemented (Major Feature)

**Description**: The compiler doesn't support binding variables in pattern matching arms with enum constructors.

**Current Behavior**:
```tml
// ❌ DOESN'T WORK - Type checker reports "Undefined variable: v"
when maybe_value {
    Just(v) => use_value(v),  // 'v' is unbound
    Nothing => default_value(),
}
```

**Expected Behavior**:
```tml
// ✅ SHOULD WORK
when maybe_value {
    Just(v) => use_value(v),  // 'v' bound to inner value
    Nothing => default_value(),
}
```

**Impact**:
- Cannot unwrap `Maybe[T]` values properly
- Cannot extract `Outcome[T, E]` success/error values
- Test framework assertions (`assert_some`, `assert_ok`, etc.) unusable
- Makes Option/Result types severely limited

**Affected Files**:
- `packages/test/src/assertions/mod.tml` (4 functions commented out)
- `packages/std/src/option.tml` (limited functionality)
- `packages/std/src/result.tml` (limited functionality)

**Why This Is Complex**:
Pattern binding requires significant infrastructure changes:

1. **TypeEnv Enum Registry**: Currently missing
   - No way to lookup enum variant definitions
   - No way to extract payload types from variants
   - EnumPattern handling exists in parser, but not in type checker

2. **Type Checker Modifications**:
   - `bind_pattern()` needs EnumPattern case (currently only handles IdentPattern, TuplePattern)
   - Needs to resolve variant name to enum type
   - Needs to extract payload types and recursively bind inner patterns
   - Needs to verify variant belongs to scrutinee type

3. **Scope Management**:
   - Pattern-bound variables must be added to arm scope
   - Needs correct lifetime tracking

4. **Codegen Support**:
   - LLVM IR: Generate pattern destructuring code
   - C backend: Generate switch/if statements for variants
   - Extract payload values from tagged union

**Implementation Estimate**:
This is a **Phase 1 feature** requiring 100-200 lines of code across:
- `include/tml/types/env.hpp` (add enum registry)
- `src/types/env_*.cpp` (enum registration/lookup)
- `src/types/checker.cpp` (EnumPattern binding)
- `src/codegen/*.cpp` (pattern destructuring)

**Current Workaround**:
Use simple patterns without binding, or avoid pattern matching entirely for enums with payloads.

**Test File**: `packages/compiler/tests/tml/test_pattern_binding.tml` (currently fails)

### 2. If-Let Pattern Matching
**Status**: ❌ Not Implemented

**Description**: `if let` syntax for conditional pattern matching.

**Current Behavior**:
```tml
// ❌ DOESN'T WORK - Parser error "Expected expression"
if let Just(value) = maybe_thing {
    use_value(value)
}
```

**Expected Behavior**:
```tml
// ✅ SHOULD WORK
if let Just(value) = maybe_thing {
    use_value(value)
} else {
    handle_nothing()
}
```

**Impact**:
- Test runner cannot check optional config values
- Cannot safely unwrap optional values in conditional contexts

**Affected Files**:
- `packages/test/src/runner/mod.tml` (entire module simplified)

**Implementation Needs**:
- Parser: Support `if let pattern = expr` syntax
- Type checker: Type checking for pattern binding
- Codegen: Generate conditional + destructuring code

### 3. Generic Where Clauses
**Status**: ❌ Not Implemented

**Description**: Type constraints on generic parameters using `where` clauses.

**Current Behavior**:
```tml
// ❌ DOESN'T WORK - Parser error
pub func assert_eq[T](left: T, right: T) where T: Eq {
    // ...
}
```

**Workaround**:
```tml
// ✅ WORKS - No constraints (runtime errors possible)
pub func assert_eq[T](left: T, right: T) {
    // ...
}
```

**Impact**:
- No compile-time enforcement of trait bounds
- Generic functions can't guarantee required operations exist
- Runtime errors instead of compile-time errors

**Affected Files**:
- `packages/test/src/assertions/mod.tml` (all generic functions)
- `packages/std/src/collections/` (all collection types)

**Implementation Needs**:
- Parser: Support `where T: Trait` syntax
- Type checker: Constraint solving system
- Error messages: Clear constraint violation messages

### 4. Function Types
**Status**: ❌ Not Implemented

**Description**: First-class function types as type aliases.

**Current Behavior**:
```tml
// ❌ DOESN'T WORK - Parser error
pub type TestFn = func() -> ()
pub type Predicate[T] = func(T) -> Bool
```

**Workaround**:
```tml
// No workaround - feature unavailable
// Functions cannot be stored in data structures yet
```

**Impact**:
- Cannot store test functions in arrays/structs
- Test framework cannot dynamically register tests
- Higher-order functions limited

**Affected Files**:
- `packages/test/src/types.tml` (TestFn commented out)
- `packages/test/src/runner/mod.tml` (test registry impossible)

**Implementation Needs**:
- Parser: Support `func(Args) -> Return` type syntax
- Type system: Function type representation
- Codegen: Function pointer handling

## ❌ Missing Features (Medium Priority)

### 5. If-Then-Else Expression Syntax
**Status**: ❌ Not Implemented

**Description**: Alternative `if-then-else` syntax from spec.

**Current Behavior**:
```tml
// ❌ DOESN'T WORK
let x = if condition then 1 else 0

// ✅ WORKS
let x = if condition { 1 } else { 0 }
```

**Impact**:
- Minor - brace syntax works fine
- More verbose than spec'd syntax

**Implementation Needs**:
- Parser: Support `then` keyword in if expressions
- No type checker changes needed
- No codegen changes needed

### 6. Named Tuple/Enum Variant Fields
**Status**: ❌ Not Implemented

**Description**: Named fields in tuple variants.

**Current Behavior**:
```tml
// ❌ DOESN'T WORK - Parser error "Expected ',', found ':'"
pub type BenchMode {
    Auto,
    Fixed(count: I64),  // Named field
}

// ✅ WORKS - Positional only
pub type BenchMode {
    Auto,
    Fixed(I64),  // Positional
}
```

**Impact**:
- Less self-documenting code
- Harder to remember field order
- Affects documentation quality

**Affected Files**:
- `packages/test/src/bench/mod.tml` (simplified)

**Implementation Needs**:
- Parser: Support named fields in enum variants
- Type checker: Field name resolution
- Codegen: No changes (erased at runtime)

### 7. Use Statement Groups
**Status**: ❌ Not Implemented

**Description**: Multi-item use statements with braces.

**Current Behavior**:
```tml
// ❌ DOESN'T WORK - Parser error "Expected identifier, found 'newline'"
pub use assertions::{
    assert,
    assert_eq,
    assert_ne,
}

// ✅ WORKS - One per line
pub use assertions
```

**Impact**:
- More verbose re-exports
- More lines of code

**Affected Files**:
- `packages/test/src/mod.tml` (re-exports simplified)

**Implementation Needs**:
- Parser: Support `use path::{item1, item2}` syntax
- Module system: Batch re-export handling

## ❌ Missing Features (Low Priority)

### 8. String Interpolation
**Status**: ❌ Not Implemented

**Description**: Embedded expressions in strings.

**Current Behavior**:
```tml
// ❌ DOESN'T WORK
println("Test {test.name} passed in {elapsed}ms")

// ✅ WORKS - Manual concatenation
println("Test " + test.name + " passed in " + elapsed + "ms")
```

**Impact**:
- More verbose string formatting
- Less readable output code

**Implementation Needs**:
- Parser: Support `{expr}` in string literals
- Codegen: Generate concatenation or format calls

### 9. Inline Module Declarations
**Status**: ❌ Not Implemented

**Description**: Modules declared inline within a file.

**Current Behavior**:
```tml
// ❌ DOESN'T WORK
pub mod utils {
    pub func helper() { }
}

// ✅ WORKS - Separate files only
// utils/mod.tml:
pub func helper() { }
```

**Impact**:
- Minor - file-based modules work fine
- More files needed for small modules

**Implementation Needs**:
- Parser: Support `mod name { decls }` syntax
- Module system: Inline module handling
- Scope management: Nested scopes

### 10. Closures
**Status**: ❌ Not Implemented

**Description**: Anonymous functions with environment capture.

**Current Behavior**:
```tml
// ❌ DOESN'T WORK
let add_x = do(y) x + y

// ✅ WORKS - Named functions only
func add_x(y: I64) -> I64 { x + y }
```

**Impact**:
- Cannot use functional programming patterns
- Benchmarking framework severely limited

**Implementation Needs**:
- Parser: Support `do(params) expr` syntax
- Type system: Closure type representation
- Codegen: Environment capture and function generation

## 🔧 Known Issues

### LLVM Backend Runtime Linking
**Status**: ⚠️ Partially Working

**Issue**: LLVM-generated executables fail to link with runtime library.

**Error**:
```
test_panic-ff2386.o : error LNK2019: unresolved external symbol tml_panic
```

**Cause**: Build system doesn't link `tml_runtime.c` when using LLVM backend.

**Workaround**: Use C backend for now.

**Fix Needed**: Update CMakeLists.txt or build scripts to:
1. Compile `runtime/tml_core.c` to object file
2. Link with LLVM-generated object files
3. Or: Compile runtime to static library and link

## 📊 Feature Completion Status

### Core Language Features
- ✅ Functions, types, structs, enums
- ✅ Basic pattern matching (without binding)
- ✅ Generics (without constraints)
- ✅ Loops, conditionals
- ✅ Const declarations
- ❌ Pattern binding
- ❌ If-let
- ❌ Where clauses
- ❌ Function types
- ❌ Closures

### Standard Library Support
- ✅ Basic types compile
- ✅ Module structure correct
- ⚠️ Limited functionality (pattern binding needed)
- ❌ Full collections unusable (constraints needed)

### Test Framework Support
- ✅ Package structure compiles
- ⚠️ Basic assertions work (without pattern matching)
- ❌ Advanced assertions unavailable
- ❌ Test runner incomplete
- ❌ Benchmarking unavailable

## 🎯 Recommended Implementation Priority

### Phase 1: Pattern System (HIGH)
1. Pattern binding in when expressions
2. If-let pattern matching
3. Enables: Full Maybe/Outcome usage, assertions

### Phase 2: Generics Constraints (HIGH)
1. Where clauses for type constraints
2. Trait-bound generic functions
3. Enables: Safe generic collections, full assertions

### Phase 3: First-Class Functions (MEDIUM)
1. Function types
2. Function values in data structures
3. Enables: Test registration, callbacks

### Phase 4: Syntax Improvements (MEDIUM)
1. If-then-else syntax
2. Use statement groups
3. Named enum fields
4. Improves: Ergonomics, readability

### Phase 5: Advanced Features (LOW)
1. Closures
2. String interpolation
3. Inline modules
4. Enables: Functional patterns, better formatting

### Phase 6: Build System (INFRASTRUCTURE)
1. LLVM runtime linking
2. Enables: Full LLVM backend support

## 📝 Testing Status

### Compiler Tests
- ✅ 228/244 passing (93.4%)
- ❌ 16 failing (pre-existing issues)

### Package Tests
- ✅ `packages/std/` - Structure correct, compiles with limitations
- ✅ `packages/test/` - Compiles successfully (simplified)
- ⚠️ Both packages have limited functionality due to missing features

## 🔗 Related Documentation

- [REORGANIZATION_SUMMARY.md](./REORGANIZATION_SUMMARY.md) - Package restructuring details
- [DOCUMENTATION.md](../rulebook/DOCUMENTATION.md) - Documentation requirements
- [docs/specs/](./specs/) - Language specification

---

**Last Updated**: 2025-12-22
**Next Review**: After implementing any missing feature
