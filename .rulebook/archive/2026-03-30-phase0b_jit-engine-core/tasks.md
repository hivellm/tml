# phase0b: JIT Engine Core — 12/12 complete

## Binary Size (with --jit)
- jit_engine.cpp.obj: 1.4 MB
- tml_codegen_x86.dll: 60.1 MB (was 63.0 MB without JIT smoke test include)
- tml_compiler.dll: 76.7 MB (was 80.4 MB)
- Note: JIT symbols are compiled but not yet referenced by any code path, so linker dead-strips them from the DLL. Size will increase when Phase 0c/0d call JitEngine.

## 1. Header (`compiler/include/backend/jit_engine.hpp`)
- [x] 1.1 Define `JitEngine` class with pimpl (JitEngineImpl forward-declared)
- [x] 1.2 Define `JitResult` struct: `{ bool success; int exit_code; std::string error; }`
- [x] 1.3 Define public API: `static create()` → pair<JitResult, unique_ptr>, `addModule()`, `lookup()`, `executeMain()`, `selfTest()`
- [x] 1.4 Guard with `#if TML_HAS_JIT`, `#pragma once`

## 2. Implementation (`compiler/src/backend/jit_engine.cpp`)
- [x] 2.1 `JitEngine::create()` — LLJITBuilder().create(), host triple, error handling [J001]
- [x] 2.2 `JitEngine::addModule(ir_text)` — parseIR → ThreadSafeModule → addIRModule, error [J002/J003]
- [x] 2.3 `JitEngine::lookup(symbol_name)` — jit->lookup, error [J004]
- [x] 2.4 `JitEngine::executeMain(args)` — lookup "main", cast to int(*)(int,char**), call, error [J005]
- [x] 2.5 Error handling — `handleAllErrors` with `error_to_string()`, all paths return JitResult

## 3. Build Integration
- [x] 3.1 `jit_engine.cpp` added to CMakeLists.txt via `target_sources(tml_backend PRIVATE)` when `TML_USE_JIT`
- [x] 3.2 Build succeeds: `scripts\build.bat --jit` → [6/6] linking complete. Also added `--jit` flag to build.bat.

## 4. Unit Test
- [x] 4.1 `JitEngine::selfTest()` — creates engine, adds `define i32 @main(...) { ret i32 42 }`, verifies exit_code==42
- [x] 4.2 selfTest also tests invalid IR ("this is not valid IR $$$$") returns error without crash
