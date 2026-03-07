# Array `mut this` Method Dispatch Bug

## Date: 2026-03-06

## Summary
`mut this` methods on arrays (`get_mut`, `first_mut`, `last_mut`, `each_mut`, `as_mut_slice`, `index_mut`, `borrow_mut`) fail with "Unknown method" because of TWO independent gaps in the compilation pipeline.

## Root Cause 1: Hardcoded Method List in Both Type Checker and Codegen

### Type Checker (`expr_call_method.cpp:1170-1250`)
The ArrayType method handling is a hardcoded if-chain that only knows about:
- `len`, `is_empty`, `get`, `first`, `last`, `map`, `eq`, `ne`, `cmp`, `as_slice`, `as_mut_slice`, `iter`, `into_iter`, `duplicate`, `hash`, `to_string`, `debug_string`

Missing: `get_mut`, `first_mut`, `last_mut`, `each_ref`, `each_mut`, `zip`, `try_map`, `index_mut`, `borrow_mut`

When method is not in the list, execution falls through to line 1369 and silently returns `make_unit()`. No error is emitted.

### Codegen (`method_array.cpp`)
Similarly hardcoded list: `len`, `is_empty`, `get`, `first`, `last`, `map`, `eq`, `ne`, `cmp`, `as_slice`, `as_mut_slice`

When method is not recognized, returns `nullopt` at line 566, causing dispatch to continue.

## Root Cause 2: ArrayType vs NamedType Representation Gap

The library defines `pub type Array[T, const N: I64]` and registers impl methods as `"Array::get_mut"`, `"Array::first_mut"`, etc. via `env_.define_func()`.

But user code `var arr: [I32; 3]` resolves to `types::ArrayType{I32, 3}` (resolve.cpp:97-109), NOT to `types::NamedType{"Array", type_args=[I32]}`.

This means:
1. `receiver_type_name` at method.cpp:530-588 is EMPTY for ArrayType (no case handles it)
2. `try_gen_impl_method_call` at method_impl.cpp:93 returns nullopt immediately (requires NamedType)
3. `try_gen_module_impl_method_call` at method_impl.cpp:749 returns nullopt immediately (requires NamedType)

## Root Cause 3: core::array Not in Essential Modules

Even if the type gap were fixed, the Array impl methods from the library would not be available during codegen because `core::array` is NOT in any of the essential module lists:
- `essential_library_modules` at runtime_modules.cpp:336
- `core_essential` at runtime_modules.cpp:525
- `core_essential_modules` at runtime_modules.cpp:669

## Affected Methods
All `mut this` methods: `get_mut`, `first_mut`, `last_mut`, `each_mut`, `as_mut_slice` (codegen has inline, but type checker missing), `index_mut`, `borrow_mut`, `as_mut`

Also missing from inline codegen (both immutable and mutable): `each_ref`, `zip`, `try_map`

## Fix Strategy (Three Options)

### Option A: Expand Hardcoded Lists (Quick Fix)
Add `get_mut`, `first_mut`, `last_mut`, etc. to both:
- `expr_call_method.cpp` (type checker)
- `method_array.cpp` (codegen inline)

Pro: Simple, no architectural change. Con: Keeps growing the hardcoded lists.

### Option B: Bridge ArrayType to NamedType for Dispatch (Proper Fix)
In method.cpp, when `receiver_type->is<ArrayType>()`:
1. Set `receiver_type_name = "Array"`
2. Convert `ArrayType{elem, size}` to equivalent `NamedType{"Array", [elem]}` for impl lookup
3. Add `core::array` to essential modules

Pro: All library-defined methods work automatically. Con: More complex, needs const generic plumbing.

### Option C: Hybrid (Recommended)
- Option A for methods that need special inline codegen (get_mut, first_mut, last_mut - they need bounds checking and GEP)
- Option B for methods that can delegate to the library (zip, try_map, each_ref, each_mut, borrow_mut)

## Key File Locations
- Type checker array methods: `compiler/src/types/checker/expr_call_method.cpp:1170-1250`
- Codegen array methods: `compiler/src/codegen/llvm/expr/method_array.cpp`
- Codegen dispatch: `compiler/src/codegen/llvm/expr/method.cpp:157` (gen_array_method call)
- receiver_type_name gap: `compiler/src/codegen/llvm/expr/method.cpp:530-588`
- Impl lookup gate: `compiler/src/codegen/llvm/expr/method_impl.cpp:93`
- Module impl lookup gate: `compiler/src/codegen/llvm/expr/method_impl.cpp:749`
- Essential modules: `compiler/src/codegen/llvm/core/runtime_modules.cpp:336,525,669`
- Type resolution: `compiler/src/types/checker/resolve.cpp:97-109` (ArrayType, not NamedType)
- Existing test confirming bug: `lib/core/tests/array/array_mut_access.test.tml`
