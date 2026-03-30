# phase0b: JIT Engine Core — 0/12 complete

## 1. Header (`compiler/include/backend/jit_engine.hpp`)
- [ ] 1.1 Define `JitEngine` class with forward-declared LLJIT pimpl
- [ ] 1.2 Define `JitResult` struct: `{ bool success; int exit_code; std::string error; }`
- [ ] 1.3 Define public API: `static create()`, `addModule()`, `lookup()`, `executeMain()`
- [ ] 1.4 Guard with `#if TML_HAS_JIT`

## 2. Implementation (`compiler/src/backend/jit_engine.cpp`)
- [ ] 2.1 Implement `JitEngine::create()` — initialize LLJIT with host target triple
- [ ] 2.2 Implement `JitEngine::addModule(ir_text)` — parse IR text → ThreadSafeModule → addIRModule
- [ ] 2.3 Implement `JitEngine::lookup(symbol_name)` — resolve symbol to function pointer
- [ ] 2.4 Implement `JitEngine::executeMain(args)` — lookup "main", cast to `int(*)(int,char**)`, call
- [ ] 2.5 Implement error handling — wrap LLVM Expected/Error into JitResult

## 3. Build Integration
- [ ] 3.1 Add `jit_engine.cpp` to CMakeLists.txt `tml_backend` sources
- [ ] 3.2 Verify build succeeds with new files

## 4. Unit Test
- [ ] 4.1 Hardcode a minimal IR string (define @main that returns 42), JIT it, verify exit_code == 42
- [ ] 4.2 Test error case: invalid IR text returns error, doesn't crash
