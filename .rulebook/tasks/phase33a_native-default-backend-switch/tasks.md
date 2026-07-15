## 1. Implementation
- [ ] 1.1 Add host triple detection in `cmd_build.cpp` and `cmd_run.cpp`: call the existing `target_triple()` helper at option-parse time and store it; treat `backend` field as empty-string meaning "auto"
- [ ] 1.2 Implement auto-detect resolution: in the backend selection branch, if `backend` is empty and the host triple begins with `x86_64` and contains `windows`, resolve to `"native"`; otherwise resolve to `"llvm"`
- [ ] 1.3 Update `--backend` help string in both `cmd_build.cpp` and `cmd_run.cpp` to read: `"codegen backend: native (default on x86-64 Windows) or llvm (explicit override)"`
- [ ] 1.4 Update `docs/native-backend.md` with a "Default Backend" section explaining the auto-detect logic and the `--backend=llvm` override; update `docs/compiler.md` CLI reference table to reflect the new default
- [ ] 1.5 Add CLI integration test in the compiler test suite: build a minimal `.tml` file on Windows x86-64 without `--backend`, assert exit code 0 and that the output object file exists and was produced by the native backend (verify via a native-backend marker in the object or via `tml build --print-backend` output)

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
