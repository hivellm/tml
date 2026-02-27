# Proposal: implement-name-mangling

## Why

The TML compiler uses a naive mangling scheme: `@tml_<module_path>_<func_name>` where the module path uses `_` as a separator. This causes symbol collisions in several scenarios:

1. **Short-name collision**: `core::str::to_lowercase(Str)` and `core::char::methods::to_lowercase(Char)` both register `functions_["to_lowercase"]` — the last one wins and overwrites the first, producing calls with wrong types in the LLVM IR. Confirmed bug: `'%t4123' defined with type 'ptr' but expected 'i32'`.

2. **Path ambiguity**: `core_str_func` could be `core::str::func` OR `core::str_func` — underscores in module names cause ambiguous parsing.

3. **No parameter encoding**: Two functions with the same name but different parameter types collide — makes future overloading impossible and causes mismatches in suite mode.

4. **Fragile suite prefix**: The current `s17_` prefix only disambiguates test-local functions; it does not protect shared library symbols across suites.

**How established languages solve this:**

| Language | Scheme | Collisions |
|----------|--------|------------|
| C++ (Itanium ABI) | `_ZN<len><seg>...<len><seg>E<args>` | Zero — hierarchical path + parameter types |
| Rust | `_ZN<path>h<blake2b_hash>E` | Zero — per-instantiation hash |
| Go | `pkg/path.Type.Method` (full import path) | Minimal — globally unique import paths |

## What Changes

### Phase 1: Immediate Bug Fixes (linkage + short-name registration)
- Functions local to a test file use LLVM `internal` linkage — they never enter the global namespace and cannot collide.
- Remove short-name (`func.name`) registration in `functions_` for library functions — only qualified names are registered.
- This eliminates the `core::str` vs `core::char::methods` collision with zero scheme change.

### Phase 2: Hierarchical Path Encoding
Replace the flat scheme with Itanium-inspired length-prefixed encoding:

```
Before:  @tml_core_str_to_lowercase
After:   @tml_N4core3str12to_lowercaseE
```

- `N...E` — nested name delimiter
- `<len><seg>` — each segment preceded by its decimal length
- No ambiguity even when module names contain underscores

**`--emit-ir` debug annotations**: When emitting LLVM IR, add a comment before each `define` with the original TML qualified name and signature for human readability:

```llvm
; core::str::to_lowercase(Str) -> Str
define ptr @tml_N4core3str12to_lowercaseE_s(ptr %s) {

; core::char::methods::to_lowercase(Char) -> Char
define i32 @tml_N4core4char7methods12to_lowercaseE_c(i32 %c) {
```

This makes IR debugging significantly easier — the mangled symbol is immediately cross-referenced to its TML origin.

### Phase 3: Parameter Type Encoding
Append type codes after the path:

```
to_lowercase(Char) -> Char   →  @tml_N4core4char7methods12to_lowercaseE_c
to_lowercase(Str) -> Str     →  @tml_N4core3str12to_lowercaseE_s
repeat(Str, I32) -> Str      →  @tml_N4core3str6repeatE_si
```

### Phase 5: `@no_mangle` Decorator
A new decorator that opts a function out of mangling entirely — the LLVM symbol is the exact function name with no prefix, no type encoding, no hash:

```tml
@no_mangle
pub func add(a: I32, b: I32) -> I32 { return a + b }
// → LLVM symbol: @add  (not @tml_N...E)
```

Use cases:
- **C FFI exports**: exporting TML functions to be called by C code by exact name
- **Embedded/OS targets**: entry points (`main`, `_start`, interrupt handlers) that the linker requires at a fixed name
- **Plugin APIs**: shared library exports with stable ABI names

Constraints:
- Cannot be applied to generic functions (mangling is required to distinguish instantiations)
- Forces `external` linkage (the function is globally visible by name)
- Conflicts between two `@no_mangle` functions with the same name produce a compiler error

### Phase 4: Hash for Generic Instantiations (Blake2b-truncated)
For generic instantiations, append a 64-bit hash of the type substitution map:

```
List[I32].contains   →  @tml_N4core4list8containsE_h7f8a9b1c
List[Str].contains   →  @tml_N4core4list8containsE_h2e4f6a8b
```

## Impact

- **Affected specs**: `docs/08-IR.md`, `docs/12-ERRORS.md`
- **Affected code**:
  - `compiler/src/codegen/llvm/decl/func.cpp` — function name generation
  - `compiler/src/codegen/llvm/core/llvm_utils.cpp` — `get_suite_prefix()`
  - `compiler/src/codegen/llvm/core/` — all method/class name construction
  - `compiler/src/codegen/llvm/expr/call.cpp` — call site resolution
  - `compiler/src/codegen/llvm/expr/method_impl.cpp` — method call lookup
  - `compiler/include/codegen/` — mangling types and interfaces
- **Breaking change**: YES — all LLVM symbol names change (no impact on user-visible API, internal only)
- **User benefit**: Eliminates suite mode bugs, enables future overloading, makes IR human-readable (symbol names preserve module structure)

## Success Criteria

- `tml test --coverage --no-cache` passes 100% with no `max_per_suite=1` workaround
- Functions in different modules with the same short name never collide in LLVM IR
- The new scheme is unambiguously reversible (a `tml demangle` tool can reconstruct the original path)
- All 1097+ tests pass in suite mode (8 per DLL) with no IR errors
