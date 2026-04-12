# Proposal: Fix Codegen Blockers — K001 + Build Parser + tml cv

## Why

Three bugs block the compiler-tml self-hosting pipeline from running at runtime:

1. **K001 — enum insertvalue type mismatch**: The MIR codegen emits `insertvalue %struct.Type undef, i64 %t, 0` but field 0 of the LLVM struct is `i32` (the discriminant). This happens for the 16-variant `Type` enum which has recursive `Heap[Type]` fields. Root cause: the `mir_type_to_llvm()` mapping generates a struct whose field 0 is `i64` for some enum layouts, but the EnumInitInst hardcodes `i32` discriminant at line 358 of `instructions.cpp`. Without this fix, none of the 27 compiler-tml test files can compile to executables — they all type-check fine but crash at LLVM IR parsing.

2. **Build parser divergence**: `tml build` rejects `List[Heap[Type]]::new(2)` with a parse error at the `::` after the closing `]`, while `tml check` and the test runner accept it. Root cause: the `build` command uses the C++ parser path which doesn't handle generic-type + static-method syntax (`T[A]::method()`). The `check` and `test` commands use the TML parser (phase13d switchover) which handles it correctly.

3. **`tml cv` subprocess type-check**: The `_popen` call in `cmd_coverage.cpp` fails to run `tml.exe check` on Windows due to path escaping issues, causing 0/84 pass on the type-check phase. The module-to-test mapping works correctly regardless.

## What Changes

### Bug 1: K001 enum insertvalue mismatch

**Root cause location**: `compiler/src/codegen/mir/instructions.cpp:358`

The codegen always emits `i32` for the enum discriminant field, but the LLVM struct type may have been generated with a different layout. Two sub-issues:

- **Sub-issue A — struct layout**: `emit_enum_def()` generates the enum struct as `{ i32, [max_payload x i8] }` but for enums with 16+ variants or recursive `Heap[T]` fields, the type mapping can produce `{ i64, ... }` due to alignment padding or type alias resolution.
- **Sub-issue B — discriminant width**: The `@repr(U8)` directive should produce `i8` discriminant, but the EnumInitInst always uses `i32`. Need to read the actual discriminant type from the MIR enum definition.

**Fix**: In `emit_enum_init`, read the actual discriminant type from the LLVM struct type definition instead of hardcoding `i32`. Use `get_element_type(enum_type, 0)` to get the correct width.

### Bug 2: Build parser generic static method

**Root cause location**: `compiler/src/parser/` (C++ parser path)

The C++ parser handles `Type::method()` and `func[T]()` but not `Type[A]::method()` — the `[A]` is parsed as an index expression, leaving `::method()` dangling.

**Fix**: In the C++ parser's primary expression handler, when encountering `Ident [ ... ] ::`, parse as generic-type + static-method call rather than index expression.

### Bug 3: `tml cv` subprocess

**Root cause location**: `compiler/src/cli/commands/cmd_coverage.cpp:156`

The `_popen` command string has unescaped paths on Windows. Also, using subprocess for type-check is slow (spawns 111 processes).

**Fix**: Replace `_popen` subprocess with direct call to the query system (`QueryEngine::check()`) from within the same process. This is faster and avoids path escaping issues entirely.

## Impact

- Affected code: `compiler/src/codegen/mir/instructions.cpp`, `compiler/src/parser/`, `compiler/src/cli/commands/cmd_coverage.cpp`
- Affected specs: None (these are implementation bugs, not spec changes)
- Breaking change: NO — all fixes are correctness improvements
- User benefit: compiler-tml tests run at runtime (not just type-check), `tml build` works with generic static methods, `tml cv` shows accurate type-check results
