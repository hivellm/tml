# phase0d: JIT CLI Integration — 14/14 complete

## 1. `tml run --jit` Flag
- [x] 1.1 Added `bool jit = false` to `RunOptions` struct in `cmd_build.hpp`
- [x] 1.2 Added `--jit` flag parsing in dispatcher.cpp (run command section)
- [x] 1.3 In `run_run_ex()`: JIT branch after `compile_via_queries()`, gated by `#if TML_HAS_JIT`
- [x] 1.4 JIT path: `JitEngine::create()` → `addModule(compile.llvm_ir)` → `executeMain(opts.args)`
- [x] 1.5 Compiled path: unchanged (LLVMBackend → LLD → subprocess)

## 2. `tml script` Command
- [x] 2.1 Added `script` subcommand in dispatcher.cpp (before `run` block)
- [x] 2.2 Accepts positional file argument: `tml script hello.tml`
- [x] 2.3 Supports `--no-cache` and `--backtrace` flags
- [x] 2.4 Shebang support: not needed yet (Phase 1 scope)

## 3. Error Handling
- [x] 3.1 JIT not available: prints "JIT mode requires LLVM ORC libraries. Rebuild with: scripts\build.bat --jit"
- [x] 3.2 JIT compilation errors: displayed via TML_LOG_ERROR with J-prefixed error codes
- [x] 3.3 Runtime crashes: JIT runs in-process, so crashes show native stack traces

## 4. End-to-End Validation
- [x] 4.1 `tml run hello.tml --jit` → "Hello from JIT!" ✓ exit code 0
- [x] 4.2 `tml script hello.tml` → same result ✓
- [x] 4.3 Complex program with template literals, conditionals, strings: all work ✓

## Known Limitation (Phase 1 scope)
- Programs using `use std::collections::*` fail because stdlib module IR isn't loaded into JIT
- Core language features (variables, strings, template literals, control flow) work perfectly
- Fix: Phase 1 will load dependent library IR modules into the JIT

## Files Modified
- `compiler/src/cli/commands/cmd_build.hpp` — added `bool jit` to RunOptions
- `compiler/src/cli/dispatcher.cpp` — `--jit` flag, `tml script` command
- `compiler/src/cli/builder/builder_run.cpp` — JIT execution branch in run_run_ex()
- `compiler/src/backend/jit_engine.cpp` — data layout override, absoluteSymbols for C lib
- `compiler/runtime/core/essential.c` — TML_EXPORT on print/println/panic/print_*
- `compiler/runtime/memory/mem.c` — TML_EXPORT on mem_alloc/free/realloc/copy/move/set/zero/compare/eq
- `scripts/build.bat` — `--jit` flag → `-DTML_USE_JIT=ON`

## Binary Size Impact
- With JIT OFF: no change (default)
- With JIT ON: jit_engine.cpp.obj = 1.4MB, linked into tml_codegen_x86.dll
