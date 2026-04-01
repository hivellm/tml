# Proposal: Split Large Files (>1500 lines)

## Status: PROPOSED

## Why

20 project files exceed the 1500-line threshold. Large files hurt:
- **LLM context**: agents waste tokens reading irrelevant code to find the section they need
- **Compile time**: touching one function recompiles the entire translation unit
- **Merge conflicts**: multiple agents/developers editing the same file causes conflicts
- **Cognitive load**: harder to understand a 2000-line file than 3 focused 700-line files

The codegen directory already follows a good split pattern (90+ files, most under 800 lines). The remaining large files are stragglers that grew organically.

## Scope

### Compiler C++ — Codegen (5 files, highest priority)

| File | Lines | Split Strategy |
|------|-------|----------------|
| `compiler/src/codegen/llvm/builtins/intrinsics_slice_simd.cpp` | 2,123 | Split by SIMD category (arithmetic, comparison, shuffle, conversion) |
| `compiler/src/codegen/llvm/core/runtime_modules.cpp` | 1,986 | Split: `runtime_modules_decls.cpp` (FFI decls) + `runtime_modules_emit.cpp` (TML function emission) + `runtime_modules_refs.cpp` (referenced library defs/decls) |
| `compiler/include/codegen/llvm/llvm_ir_gen.hpp` | 1,873 | Group methods by category into partial headers or use forward declarations to slim down |
| `compiler/src/codegen/llvm/expr/method_impl.cpp` | 1,811 | Split: `method_impl_local.cpp` (try_gen_impl_method_call, ~1200 lines) + `method_impl_module.cpp` (try_gen_module_impl_method_call, ~600 lines) |
| `compiler/src/codegen/llvm/core/generate.cpp` | 1,793 | Split: `generate.cpp` (main generate() function) + `generate_library.cpp` (emit_string_constants, emit_referenced_library_*) |
| `compiler/src/codegen/llvm/core/generic_instantiate.cpp` | 1,738 | Split by instantiation category (struct vs function vs behavior) |
| `compiler/src/codegen/llvm/builtins/intrinsics_extended.cpp` | 1,534 | Split by intrinsic domain (string, collection, math, io) |

### Compiler C++ — Type System (2 files)

| File | Lines | Split Strategy |
|------|-------|----------------|
| `compiler/src/types/env_module_load.cpp` | 1,543 | Split: module loading vs module resolution/linking |
| `compiler/src/types/checker/expr.cpp` | 1,531 | Split: `expr_literal.cpp` (literals, idents) + `expr_compound.cpp` (binary, unary, cast, block, etc.) |

### Compiler C++ — MIR (1 file)

| File | Lines | Split Strategy |
|------|-------|----------------|
| `compiler/src/mir/thir_mir_builder.cpp` | 1,529 | Split: `thir_mir_builder.cpp` (build, declarations, types) + `thir_mir_builder_expr.cpp` (all build_* expression methods) + `thir_mir_builder_control.cpp` (if, loop, while, for, break, continue) |

### Compiler C++ — Testing (2 files)

| File | Lines | Split Strategy |
|------|-------|----------------|
| `compiler/src/testing/testing_compile.cpp` | 1,824 | Split: compile pipeline vs linking/caching |
| `compiler/src/testing/testing_coordinator.cpp` | 1,792 | Split: test discovery/grouping vs execution/reporting vs debug_layers |

### Compiler C++ — CLI (1 file)

| File | Lines | Split Strategy |
|------|-------|----------------|
| `compiler/src/cli/explain/type_errors.cpp` | 1,531 | Split by error category (inference, mismatch, missing impl, etc.) |

### Runtime C (2 files)

| File | Lines | Split Strategy |
|------|-------|----------------|
| `compiler/runtime/os/os.c` | 1,736 | Split by OS subsystem (file, process, env, time, etc.) |
| `compiler/runtime/core/essential.c` | 1,719 | Split: `essential_io.c` (print, read) + `essential_panic.c` (panic, abort) + `essential_test.c` (test harness) |

### TML Library (2 files)

| File | Lines | Split Strategy |
|------|-------|----------------|
| `lib/core/src/str.tml` | 2,017 | Split: `str.tml` (core type + basic ops) + `str_search.tml` (find, contains, split, replace) + `str_format.tml` (pad, trim, case conversion) |
| `lib/std/src/sync/atomic.tml` | 1,507 | Split: `atomic.tml` (core AtomicI32/I64) + `atomic_ext.tml` (AtomicBool, AtomicPtr, fences) |

### C++ Tests (3 files — lower priority)

| File | Lines | Split Strategy |
|------|-------|----------------|
| `compiler/tests/codegen/oop_test.cpp` | 2,722 | Split by feature area (inheritance, virtual, generics, etc.) |
| `compiler/tests/ir/mir_test.cpp` | 2,620 | Split by MIR construct (basic blocks, instructions, passes, etc.) |
| `compiler/tests/ir/hir_test.cpp` | 1,868 | Split by HIR feature (lowering, monomorphization, closures, etc.) |

## Approach

Each file split follows the same mechanical pattern:

1. **Read the file**, identify logical sections (functions, static helpers, section comments)
2. **Create new files** in the same directory, following existing naming conventions
3. **Move functions** to appropriate new files, keeping includes and namespace
4. **Update CMakeLists.txt** to add new source files
5. **Build** to verify compilation
6. **Run affected tests** to verify no regressions

Since all methods are on the same class (`LLVMIRGen::`, `TypeChecker::`, etc.), splitting is purely mechanical — move methods + their static helpers to a new `.cpp` file, keep the same header include. No API changes, no refactoring.

## Risks

- **Low risk**: These are purely mechanical splits (move code between .cpp files, same header)
- **CMake**: Each new .cpp must be added to the correct CMakeLists.txt target
- **Static helpers**: Some static functions are used by multiple methods in the same file — these stay in the original or get a shared `_helpers.cpp`

## NOT in scope

- Refactoring code logic — this is a pure file organization task
- Changing any public API or header structure (except slimming `llvm_ir_gen.hpp`)
- Changing any behavior or fixing bugs
