# Pattern: CGValue — Typed Codegen Value Wrapper

## Context

MIR codegen previously passed values around as loose `(string reg, string type)` pairs,
leading to ABI mistakes (wrong immediate/address distinction, missing aggregates check).
CGValue encapsulates all four facts about an LLVM SSA register in one struct.

## Key Design Decision: Avoid Nested Struct with Circular Dependency

The original spec placed `SpillResult` and `LoadResult` as nested structs inside `CGValue`.
This causes a C++ incomplete-type error because those structs hold a `CGValue` by value and
the enclosing `CGValue` is not yet complete when the nested struct declarations are parsed.

**Solution:** Declare `CGValue` first (with factory methods and query methods only), then
define the result structs as top-level structs (`CGSpillResult`, `CGLoadResult`), and
expose the conversion operations as **free functions** (`cg_to_address`, `cg_to_immediate`)
to avoid a reverse dependency from the result structs back into `CGValue`.

## File Locations

- Header: `compiler/include/codegen/cg_value.hpp`
- Implementation: `compiler/src/codegen/cg_value.cpp`
- Added to: `compiler/CMakeLists.txt` (tml_codegen sources, after abi.cpp)

## TML_MODULE Macro

All `compiler/src/codegen/*.cpp` files must start with `TML_MODULE("compiler")` (not
`"codegen_x86"`). The `codegen_x86` module string is used only in `src/codegen/mir/`
helper files. Check surrounding files if unsure which string to use.

## is_aggregate Delegation

`CGValue::is_aggregate()` delegates to `codegen::is_aggregate_llvm_type(llvm_type)` from
`codegen/abi.hpp`. This keeps the aggregate classification rule in a single place.

## Counter Convention

`cg_to_address` and `cg_to_immediate` each take an `int& counter` that is incremented on
every call to ensure unique LLVM register names (`%spill.0`, `%spill.1`, `%load.0`, etc.).
The caller owns the counter and keeps it per-function.
