## 1. Diagnose residual essential.c crash
- [ ] 1.1 Bisect via `--emit=ast` between phase24l-fixed minimal repro (28-30/30) and full essential.c (0/5). Find the smallest TU that triggers the residual exit-127 / SIGSEGV.
- [ ] 1.2 Capture stderr/stack-trace via `TML_BACKTRACE=1` or by instrumenting `compiler/runtime/core/essential.c` panic handlers; identify whether the crash is in lexer, parser, type-checker, or lowerer.
- [ ] 1.3 Determine whether the crash is a stack overflow (exit 127 with empty stderr is a strong signal on Windows) or a use-after-free reachable through a `.get()` aliasing path that the typedef-arm fix did not cover.

## 2. Implement (one of three paths from proposal.md)
- [ ] 2.1 Pick the structurally cleanest option from proposal.md based on diagnosis: (a) refine option (a) + audit-fix consumers, (b) compiler-codegen automatic deep-clone, or (c) HashMap.get specialization for Shared values.
- [ ] 2.2 Apply the chosen fix.
- [ ] 2.3 Verify minimal repro stays at >= 28/30.
- [ ] 2.4 Verify sig_alone 10/10 stays.
- [ ] 2.5 Verify `int (*p);` and `typedef void (*sig_t)(int);` 30/30 each stay.
- [ ] 2.6 Verify compiler suite stays >= 290/295.
- [ ] 2.7 Verify `lib/core` cache_aligned_box / cache / cache_soavec_set / future_fuse tests pass (these regressed under phase24l attempt 3).
- [ ] 2.8 essential.c x 5 must reach 5/5 exit 0.

## 3. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 3.1 VERSION bump to 0.3.52 (or next), CHANGELOG entry, `docs/patches/v0.3.X.md`.
- [ ] 3.2 Update `lib/core/src/alloc/shared.tml` doc-comments to describe the final semantic of `.get()` / `.get_clone()` after this task lands.
- [ ] 3.3 Add regression test in `compiler-tml/tests/native/c_frontend.test.tml` for the specific essential.c trigger reduced in 1.1.
- [ ] 3.4 Run all touched tests and confirm they pass.
- [ ] 3.5 Archive phase24m AND phase24l AND phase24k AND phase0z.
