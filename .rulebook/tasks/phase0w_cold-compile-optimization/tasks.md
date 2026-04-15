## 1. Diagnosis
- [ ] 1.1 Profile cold `tml check` with high-resolution timers — measure DLL load, meta parse, type-check, total
- [ ] 1.2 Profile cold `tml build` — measure codegen DLL load time separately from compiler DLL
- [ ] 1.3 Identify which DLL static constructors run at load time (LLVM global init, etc.)

## 2. Implementation
- [ ] 2.1 Delay-load `tml_codegen_x86.dll` — load only when codegen is needed, not for `check`/`lint`/`fmt`
- [ ] 2.2 Pre-generate .tml.meta cache during build — add step to `scripts/build.bat` that runs meta generation
- [ ] 2.3 Optimize DLL initialization — move LLVM context creation from DLL load to first use

## 3. Benchmark Gate
- [ ] 3.1 GATE: Cold `tml check hello.tml` must complete in under 2 seconds
- [ ] 3.2 GATE: Cold `tml build hello.tml` must complete in under 5 seconds

## 4. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 4.1 Update or create documentation covering the implementation
- [ ] 4.2 Write tests covering the new behavior
- [ ] 4.3 Run tests and confirm they pass
