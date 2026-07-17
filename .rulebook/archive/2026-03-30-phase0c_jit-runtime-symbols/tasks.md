# phase0c: JIT Runtime Symbol Resolution — 10/10 complete

## Architecture Notes
- Runtime (`tml_runtime` static lib) linked into `tml_compiler.dll` via `tml_cli`
- JIT engine in `tml_codegen_x86.dll` via `tml_backend`
- `GetForCurrentProcess()` searches ALL loaded DLLs → finds exported symbols cross-DLL
- x86-64 COFF: global prefix is '\0' (no underscore mangling)

## Key Fix: TML_EXPORT on Runtime Functions
- `print`, `println`, `panic`, `print_i32/i64/f64/bool` in essential.c → added `TML_EXPORT`
- `mem_alloc`, `mem_free`, `mem_realloc`, etc. in mem.c → added `TML_EXPORT` + macro definition
- C library functions (`snprintf`, `memcpy`, `memset`, `memmove`, `strlen`) registered via `absoluteSymbols()`
- Data layout mismatch fix: `mod->setDataLayout(jit->getDataLayout())` after IR parse

## 1. Static-Link C Runtime into Compiler
- [x] 1.1 Identified all runtime .c files: essential.c, mem.c, collections.c, etc.
- [x] 1.2 Already linked: `tml_runtime` → `tml_cli` → `tml_compiler.dll`
- [x] 1.3 Added `TML_EXPORT` to bare-name functions (print, println, panic, print_*, mem_*)
- [x] 1.4 No `/EXPORT` flags needed — `__declspec(dllexport)` on functions handles it

## 2. JIT Engine Search Generator
- [x] 2.1 Added `DynamicLibrarySearchGenerator::GetForCurrentProcess()` in `create()`
- [x] 2.2 C library functions registered via `absoluteSymbols()` (snprintf, memcpy, etc.)
- [x] 2.3 Symbol mangling: `getGlobalPrefix()` returns '\0' on x86-64 COFF — no issues

## 3. Validation
- [x] 3.1 `tml script jit_hello.tml` → prints "Hello from JIT!" ✓
- [x] 3.2 Template literals, string ops, conditionals all work in JIT mode ✓
- [x] 3.3 Collections test reveals stdlib symbols need IR loading (known Phase 1 scope) ✓

## Files Modified
- `compiler/src/backend/jit_engine.cpp` — search generator, absoluteSymbols, data layout fix
- `compiler/runtime/core/essential.c` — added TML_EXPORT to print, println, panic, print_*
- `compiler/runtime/memory/mem.c` — added TML_EXPORT macro + exported all mem_* functions
- `compiler/tests/codegen/jit_engine_test.cpp` — gtest unit tests
