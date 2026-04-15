## 1. Diagnosis
- [ ] 1.1 Profile `tml check` with manual timing — identify top 5 hotspots in the type-checker
- [ ] 1.2 Map the module dependency graph — identify which modules can be type-checked in parallel
- [ ] 1.3 Measure .tml.meta file I/O time vs type resolution time

## 2. Implementation
- [ ] 2.1 Parallel module preloading: use std::async for independent modules concurrently
- [ ] 2.2 Omit type-checking for imported modules never referenced in user code
- [ ] 2.3 Memory-map .tml.meta files instead of fread for faster I/O
- [ ] 2.4 Profile release-mode compiler binary — measure type-checker speedup with -O2

## 3. Benchmark Gate
- [ ] 3.1 GATE: `tml check hello.tml` cold must complete in under 1 second
- [ ] 3.2 GATE: `tml check` must be within 3x of `cargo check` for equivalent code

## 4. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 4.1 Update or create documentation covering the implementation
- [ ] 4.2 Write tests covering the new behavior
- [ ] 4.3 Run tests and confirm they pass
