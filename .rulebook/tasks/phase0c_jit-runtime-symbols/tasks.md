# phase0c: JIT Runtime Symbol Resolution — 0/10 complete

## 1. Static-Link C Runtime into Compiler
- [ ] 1.1 Identify all C runtime `.c` files that TML programs depend on
- [ ] 1.2 Add C runtime object compilation to `tml_backend` or `tml_compiler` CMake target
- [ ] 1.3 Verify symbols are exported (visible to `GetForCurrentProcess()` on Windows)
- [ ] 1.4 On Windows: may need `__declspec(dllexport)` or `/EXPORT` linker flags

## 2. JIT Engine Search Generator
- [ ] 2.1 Add `DynamicLibrarySearchGenerator::GetForCurrentProcess()` to JitEngine's main JITDylib
- [ ] 2.2 Add platform library search (Windows: ucrt, kernel32 via `LoadLibrary` generator)
- [ ] 2.3 Handle symbol name mangling differences (COFF prepends `_` on some targets)

## 3. Validation
- [ ] 3.1 Test: JIT a TML program that calls `print("hello")` — verify output appears
- [ ] 3.2 Test: JIT a TML program that allocates memory (List, HashMap) — verify no crash
- [ ] 3.3 Test: JIT a TML program with `lowlevel` block (ptr_read/ptr_write) — verify correct behavior
