## 1. Diagnosis
- [ ] 1.1 Run `mcp__tml__debug(file="benchmarks/profile_tml/encoding_bench.tml", check_leaks=true)` — confirm 200K leaks, note allocation sizes
- [ ] 1.2 Read `lib/std/src/encoding/base64.tml` — identify every `Text::new`, `Buffer::alloc`, `List::new` call that is not returned and not freed
- [ ] 1.3 Read `lib/std/src/encoding/hex.tml` — same audit
- [ ] 1.4 Read `lib/std/src/encoding/base32.tml` — same audit

## 2. Implementation
- [ ] 2.1 In `base64.tml`: add explicit `mem::free()` calls for all scratch/intermediate buffers before each return site — ensure the returned value is NOT freed (caller owns it)
- [ ] 2.2 In `hex.tml`: same fix — free all scratch buffers, preserve returned value ownership
- [ ] 2.3 In `base32.tml`: same fix — free all scratch buffers, preserve returned value ownership
- [ ] 2.4 Run `mcp__tml__debug(check_leaks=true)` after each file fix — confirm that file's leak count drops to zero before moving to the next

## 3. Benchmark Gate
- [ ] 3.1 Run `mcp__tml__debug(file="benchmarks/profile_tml/encoding_bench.tml", check_leaks=true)` — must report 0 leaks
- [ ] 3.2 Run encoding benchmark `--stage=parser:cpp` — capture base64/hex/base32 ns/op
- [ ] 3.3 Compare vs Rust baseline from `docs/analysis/benchmark/07-encoding.md`
- [ ] 3.4 GATE: Zero memory leaks after 100K iterations. Performance must not regress from pre-fix baseline. Do NOT proceed if leak count is not 0.

## 4. Validation
- [ ] 4.1 Run `tml test --suite=core` — no regressions
- [ ] 4.2 Run `tml test --suite=compiler` — no regressions
- [ ] 4.3 Run the encoding benchmark 1M iterations — heap growth must remain flat

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update CHANGELOG.md with `fix(encoding): free scratch buffers in base64/hex/base32 — eliminate 200K leaks per benchmark run`
- [ ] 5.2 Write leak-check test: encode 10K strings in a loop, run with `check_leaks=true`, assert 0 leaks
- [ ] 5.3 Run tests and confirm they pass
