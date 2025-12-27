# TML Compiler - Missing Features Report

**Date**: 2025-12-22
**Compiler Version**: Bootstrap/Development
**Status**: Feature-Complete for Basic Programs, Missing Advanced Features

## Summary

The TML compiler successfully compiles basic programs but lacks several advanced features required for the full standard library and test framework. This document catalogs missing features discovered during package reorganization and testing.

## ✅ Recently Implemented (2025-12-22)

### Range Expressions
**Status**: ✅ Fully Implemented (2025-12-22)

**Implementation**:
- Parser: `parse_range_expr()` in `parser_expr.cpp` - already existed
- Type Checker: `check_range()` in `checker.cpp` - newly added
- Codegen LLVM: Range expressions generate I64 slices for iteration
- AST: `RangeExpr` struct already defined

**Example**:
```tml
// Exclusive range (0 to n-1)
for i in 0 to 10 {
    println("{}", i)  // prints 0..9
}

// Inclusive range (1 to n)
for i in 1 through 10 {
    println("{}", i)  // prints 1..10
}
```

**Tests**: All for-loop tests ✅ PASSING

**Features**:
- ✅ Exclusive ranges with `to` keyword
- ✅ Inclusive ranges with `through` keyword
- ✅ Type checking for integer range bounds
- ✅ Integration with for-loops
- ✅ Returns `SliceType<I64>` for iteration

### Path Expression Function Calls
**Status**: ✅ Fully Implemented (2025-12-22)

**Implementation**:
- Type Checker: Added path function lookup in `check_path()` - `checker.cpp:1029`
- Supports namespaced function calls like `Instant::now()`, `Duration::as_millis_f64()`
- Works with builtin time API

**Example**:
```tml
let start: I64 = Instant::now()
// ... do work ...
let elapsed: I64 = Instant::elapsed(start)
let ms: F64 = Duration::as_millis_f64(elapsed)
println("Took {:.3} ms", ms)
```

**Tests**: Benchmark tests ✅ PASSING

**Features**:
- ✅ Path expressions in call positions
- ✅ Two-segment paths (Type::function)
- ✅ Builtin time API (Instant, Duration)
- ✅ Type checking for namespaced functions

### Format Specifiers in println
**Status**: ✅ Fully Implemented (pre-existing)

**Implementation**:
- Codegen: `gen_format_print()` in `llvm_ir_gen_types.cpp` - already existed
- Supports `{:.N}` precision format for floating-point numbers
- Automatically handles type conversion for precision formatting

**Example**:
```tml
let pi: F64 = 3.14159265359
println("Pi to 2 decimals: {:.2}", pi)  // "3.14"
println("Pi to 5 decimals: {:.5}", pi)  // "3.14159"

let ms: F64 = 0.266
println("Time: {:.3} ms", ms)  // "0.266 ms"
```

**Tests**: Benchmark output ✅ PASSING

**Features**:
- ✅ Precision specifiers: `{:.1}`, `{:.2}`, `{:.3}`, etc.
- ✅ Automatic float/double handling
- ✅ Works with F32 and F64 types
- ✅ Also works with integers (converts to double for display)

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

### If-Let Pattern Matching
**Status**: ✅ Fully Implemented (2025-12-22)

**Implementation**:
- Parser: `parse_if_let_expr()` in `parser_expr.cpp` - already existed
- Type Checker: `check_if_let()` in `checker.cpp` - newly added
- Codegen LLVM: `gen_if_let()` in `llvm_ir_gen_control.cpp` - newly added
- AST: `IfLetExpr` struct already defined

**Example**:
```tml
if let Just(value) = maybe_thing {
    use_value(value)
} else {
    handle_nothing()
}
```

**Tests**: `packages/compiler/tests/test_if_let.tml` ✅ PASSING

**Features**:
- ✅ Pattern binding in if conditions
- ✅ Enum variant matching
- ✅ Variable binding from pattern payloads
- ✅ Optional else branches
- ✅ Works with Maybe[T], Outcome[T,E], and custom enums

### If-Then-Else Expression Syntax
**Status**: ✅ Fully Implemented (2025-12-22)

**Implementation**:
- Parser: Already supported `then` keyword parsing
- Codegen LLVM: Added phi node support for expression values in `gen_if()`

**Example**:
```tml
let x = if condition then 42 else 0
let y = if flag then "yes" else "no"
```

**Tests**: `packages/compiler/tests/test_if_then_else.tml` ✅ PASSING

**Note**: Both syntaxes now work:
- `if cond then expr else expr` (expression form with `then`)
- `if cond { block } else { block }` (block form)

### Generic Where Clauses (Fully Implemented)
**Status**: ✅ Fully Implemented (2025-12-24)

**Implementation**:
- Lexer: Added `where` keyword (TokenKind::KwWhere)
- Parser: `parse_where_clause()` in `parser_decl.cpp` - fully implemented
- AST: WhereClause struct defined
- Type Checker: Parses, stores, and **enforces constraints at call sites**
- Behavior tracking: TypeEnv tracks which types implement which behaviors

**Example**:
```tml
func equals[T](a: T, b: T) -> Bool where T: Eq {
    return true  // T must implement Eq
}

// Works - I32 implements Eq
let result: Bool = equals(10, 20)

// Error - MyType doesn't implement Eq
type MyType { value: I32 }
equals(x, x)  // Error: Type 'MyType' does not implement behavior 'Eq'
```

**Tests**: `packages/compiler/tests/tml/test_constraints.tml` ✅ PASSING

**What Works**:
- ✅ Single trait bounds: `where T: Trait`
- ✅ Multiple trait bounds: `where T: Trait1 + Trait2`
- ✅ Multiple type parameters: `where T: Trait1, U: Trait2`
- ✅ Parsing and AST storage
- ✅ Constraint enforcement at call sites
- ✅ Behavior implementation tracking for builtin types
- ✅ Error messages for constraint violations

**Builtin Types**: I8-I128, U8-U128 implement Eq, Ord, Numeric, Hash, Display, Debug, Default, Duplicate.
F32, F64, Bool, Char, Str also implement appropriate behaviors.

### Closures (Syntax Only)
**Status**: ⚠️ Partially Implemented (2025-12-22)

**Implementation**:
- Parser: `parse_closure_expr()` in `parser_expr.cpp` - fully working
- Type Checker: `check_closure()` in `checker.cpp` - fully working
- Codegen: `gen_closure()` in `llvm_ir_gen_expr.cpp` - basic implementation
- AST: ClosureExpr struct fully defined

**Example**:
```tml
// ✅ WORKS - Closure syntax parsing
let add_one: func(I32) -> I32 = do(x: I32) -> I32 x + 1

// ✅ WORKS - Closure with block body
let add_two: func(I32) -> I32 = do(x: I32) -> I32 {
    return x + 2
}

// ✅ WORKS - Multiple parameters
let add: func(I32, I32) -> I32 = do(a: I32, b: I32) -> I32 a + b
```

**Tests**: `packages/compiler/tests/test_closures_simple.tml` ✅ PASSING (parsing/type checking)

**What Works**:
- ✅ Closure syntax: `do(params) expr`
- ✅ Closure syntax with block: `do(params) { body }`
- ✅ Type checking for closures
- ✅ Parameter binding in closure body
- ✅ Codegen generates helper functions

**What's Missing**:
- ❌ Environment capture (closures don't capture outer variables)
- ❌ Function pointer passing at runtime
- ❌ Storing closures in data structures
- ❌ Higher-order functions with closures as arguments

**Impact**: Syntax is available and closures are generated as inline helper functions, but they don't capture their environment and can't be passed around as first-class values yet. This is a foundation for future full closure support.

### Function Types (Syntax Only)
**Status**: ⚠️ Partially Implemented (2025-12-22)

**Implementation**:
- Parser: Added `func(Args) -> Return` syntax in `parser_type.cpp` - fully working
- Type System: FuncType already existed in type.hpp
- Type Aliases: Can declare function type aliases
- Type Checker: Function types can be used in signatures
- Codegen: ❌ Function pointer passing not yet supported

**Example**:
```tml
// ✅ WORKS - Type aliases
type UnaryOp = func(I32) -> I32
type BinaryOp = func(I32, I32) -> I32
type Predicate = func(I32) -> Bool
type Action = func()

// ✅ WORKS - Function signatures with function types
func apply_twice(f: UnaryOp, x: I32) -> I32 {
    return f(f(x))
}
```

**Tests**: `packages/compiler/tests/test_function_types.tml` ✅ PASSING (parsing/type checking)

**What Works**:
- ✅ Function type syntax: `func(T, U) -> R`
- ✅ Type aliases for function types
- ✅ Function parameters with function types
- ✅ Generic function types: `func[T](T) -> T`
- ✅ Type checking

**What's Missing**:
- ❌ Actually passing functions as values at runtime
- ❌ Function pointer codegen in LLVM backend
- ❌ Storing functions in structs/arrays

**Impact**: Syntax is available for writing higher-order functions, but runtime support is limited. Code using function types will type-check but may fail at code generation when trying to pass functions as values.

## ❌ Missing Features (High Priority)

### 1. Pattern Binding in When Expressions
**Status**: ✅ Implemented (With Limitations) - See "Recently Implemented" section

**Description**: Pattern binding in when expressions is now implemented! Variables can be bound in pattern matching arms with enum constructors.

**What Works**:
```tml
// ✅ WORKS - Basic pattern binding
type Maybe[T] {
    Just(T),
    Nothing,
}

func test() {
    let x: Maybe[I64] = Just(42)
    when x {
        Just(v) => println(v),  // v is bound to 42
        Nothing => println(0),
    }
}
```

**What Works**:
- ✅ Single-payload enum variants
- ✅ Multiple enum variants with payloads
- ✅ Nested when expressions
- ✅ Type checking for pattern variables

**Known Limitations**:
- ⚠️ Mixed variants (some with payload, some without) may have codegen issues in LLVM backend
- ❌ Multiple payloads per variant not fully tested
- C backend works better than LLVM backend for complex patterns

**Impact**: Now GREATLY REDUCED
- ✅ Can unwrap `Maybe[T]` values
- ✅ Can extract `Outcome[T, E]` success/error values
- ⚠️ Test framework assertions may work with simple patterns
- ✅ Option/Result types are now usable!

**Affected Files**:
- `packages/test/src/assertions/mod.tml` (may need review)
- `packages/std/src/option.tml` (should work now)
- `packages/std/src/result.tml` (should work now)

**Implementation Status**:
✅ **IMPLEMENTED** (2025-12-22)

Infrastructure that was added:
1. **TypeEnv Enum Registry**: ✅ Complete
   - `env_.all_enums()` returns all enum definitions
   - `env_.lookup_enum()` finds enum by name
   - Variant lookup works correctly

2. **Type Checker Modifications**: ✅ Complete
   - `bind_pattern()` handles EnumPattern in `src/types/checker.cpp:707`
   - Resolves variant name to enum type
   - Extracts payload types and recursively binds inner patterns
   - Verifies variant belongs to scrutinee type

3. **Scope Management**: ✅ Complete
   - Pattern-bound variables are added to arm scope
   - Correct lifetime tracking via push/pop scope

4. **Codegen Support**: ⚠️ Partial
   - LLVM IR: Pattern destructuring works for simple cases
   - C backend: Should work (not tested)
   - Tag matching works correctly
   - Payload extraction works for single payloads

**Test Files**:
- ✅ `packages/compiler/tests/tml/test_pattern_binding.tml` - PASSING
- ✅ `packages/compiler/tests/tml/test_enum_variant_simple.tml` - PASSING
- ✅ `packages/compiler/tests/tml/test_enum_two_variants.tml` - PASSING
- ⚠️ `packages/compiler/tests/tml/test_enum_mixed_variants.tml` - Issues with mixed variants

### 2. If-Let Pattern Matching
**Status**: ✅ Fully Implemented (2025-12-22) - See "Recently Implemented" section

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
**Status**: ✅ Fully Implemented (2025-12-24)

**Description**: Type constraints on generic parameters using `where` clauses.

**Current Behavior**:
```tml
// ✅ WORKS - Constraint enforced at call site
func equals[T](a: T, b: T) -> Bool where T: Eq {
    return true
}

// Works - I32 implements Eq
equals(10, 20)

// Error - MyType doesn't implement Eq
type MyType { value: I32 }
let x: MyType = MyType { value: 1 }
equals(x, x)  // Compile error: Type 'MyType' does not implement behavior 'Eq'
```

**Implementation**:
- ✅ Parser: `where T: Trait` syntax
- ✅ Type checker: Constraint enforcement at call sites
- ✅ Error messages: Clear constraint violation messages
- ✅ Behavior tracking: Builtin types implement common behaviors

**Note**: Custom types must implement behaviors to satisfy constraints. Currently, only builtin types (I32, Bool, Str, etc.) have behavior implementations registered.

### 4. Function Types
**Status**: ⚠️ Partially Implemented (2025-12-22) - See "Recently Implemented" section

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
**Status**: ✅ Fully Implemented (2025-12-22) - See "Recently Implemented" section

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
**Status**: ⚠️ Partially Implemented (2025-12-22) - See "Recently Implemented" section

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
- ✅ Range expressions (to/through)
- ✅ Path expression function calls
- ✅ Format specifiers in println
- ✅ Pattern binding in when/if-let
- ✅ If-then-else syntax
- ⚠️ Where clauses (syntax only)
- ⚠️ Function types (syntax only)
- ⚠️ Closures (syntax only, no capture)

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
