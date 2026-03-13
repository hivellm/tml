# Proposal: Coverage Root-Cause Fixes

## Context

Coverage at 95.02% (5213/5486). Gap of 273 functions. The previous report inflated "missing tests" and prioritized by volume. This task corrects the approach: **root-cause first, repro mínima obrigatória, compiler bugs before infrastructure before tests.**

## Diagnosis

The 273 uncovered functions break down into 5 real categories:

| Category | Functions | % of gap | Example |
|----------|-----------|----------|---------|
| Codegen/ABI bugs | ~52 | 19% | Maybe[I32], Outcome mismatch, invalid GEP |
| Runtime crashes | ~71 | 26% | alloc exit 99, future_fuse, capture |
| Infra/link issues | ~52 | 19% | OpenSSL coverage link, TLS, threads |
| Tests not written | ~80 | 29% | array/ascii, collections/buffer |
| Genuinely untestable | ~18 | 7% | NeverError::unreachable, platform-specific |

**Key insight**: codegen/ABI bugs tend to share root causes. Fixing Maybe[I32] lowering may also fix Outcome mismatch. Fixing GEP projection may also fix iter/adapters. Attack representation bugs first.

## Phases

### Phase 0: Diagnostic Hygiene (LOW effort)
- Mark genuinely untestable functions with `@no_coverage`
- Fix coverage tracker key collisions (multiple `From` impls sharing "from" name)
- Separate report into 4 clean buckets: compiler / runtime / infra / tests-missing
- **Result**: Reliable baseline, stop chasing ghosts

### Phase 1: Codegen Representation Bugs (HIGH priority, ~52 functions)

Four bugs that likely share root causes in lowering/ABI:

**BUG-MAYBE**: `Maybe[I32]` emits `i32` instead of `%struct.Maybe__I32 = type { i32, i32 }`
- Affects: std_lowlevel suite (compile error)
- Root: enum/optional lowering in codegen
- Files: `compiler/src/codegen/llvm/decl/enum.cpp`, `compiler/src/codegen/llvm/core/types.cpp`

**BUG-OUTCOME**: `Outcome[Bytes, ZlibError]` type mismatch at runtime
- Affects: std/zlib/zlib_zstd
- Root: generic enum monomorphization / layout
- Files: `compiler/src/codegen/llvm/core/generic.cpp`

**BUG-GEP**: Invalid GEP on scalar value (should be pointer)
- Affects: core/intrinsics/intrinsics_array_ops, iter_range_sizehint
- Root: place vs value confusion in field projection
- Files: `compiler/src/codegen/llvm/expr/`

**BUG-CLOSURE**: `%struct.I32__Fn` unsized alloca for closure-typed struct fields
- Affects: cell/lazy
- Root: Fn type layout in struct context
- Files: `compiler/src/codegen/llvm/decl/llvm_struct_decl.cpp`

**Methodology**: For EACH bug:
1. Create `.sandbox/repro_<name>.tml` — minimal reproduction
2. Emit IR: `mcp__tml__emit-ir` — identify exact mismatch
3. Write equivalent `.sandbox/repro_<name>.rs` — compare with Rust IR
4. Fix in compiler C++
5. Rebuild + verify original test passes

### Phase 2: Coverage-Only Runtime Failures (MEDIUM priority, ~71 functions)

These suites pass in normal mode but crash in coverage mode. The bug is in the test harness/coverage runtime, NOT the modules themselves.

**CRASH-ALLOC**: 5 alloc suites exit code 99
- Hypothesis: global allocator conflicts with coverage harness init
- Verify: run alloc tests without coverage → if they pass, it's harness

**CRASH-ONCE**: once_lock_get_or_init fails only in coverage mode
- Hypothesis: coverage instrumentation breaks atomics/init ordering

**CRASH-CAPTURE**: other/capture crashes
- Hypothesis: panic/output hooks interact with coverage hooks

**CRASH-FUTURE**: future_fuse, async_lazy_future
- Hypothesis: async executor + coverage instrumentation

**Methodology**: For EACH crash:
1. Run without coverage → confirm pass
2. Run with coverage → get exact crash/exit code
3. Check if coverage runtime init order is the cause
4. Fix in `compiler/src/testing/` or `lib/test/runtime/`

### Phase 3: Link/Infrastructure Issues (MEDIUM priority, ~52 functions)

**LINK-OPENSSL**: std_hash link fails in coverage mode
- Action: Compare link lines (normal vs coverage), find missing/reordered lib
- Files: `compiler/src/backend/lld_linker.cpp`, `compiler/src/testing/testing_compile.cpp`

**LINK-CRYPTO**: cipher_aes, cipher_authtag, cipher_enum fail
- Likely same root cause as LINK-OPENSSL

**INFRA-TLS**: net_tls, sys_socket_options, tcp_timeout
- Need real network stack in test environment
- May be deferrable (mark with @no_coverage or create test doubles)

**INFRA-THREAD**: thread/scope
- Needs real thread support in test harness

### Phase 4: Associated Type Constraints (LOW priority, ~30 functions)

**TY-ASSOC**: `where I::Item = ref T` not resolved before codegen
- Affects: iter/adapters (cloned, copied, flatten, intersperse, peekable)
- This is a type system limitation, not a quick fix
- Files: `compiler/src/types/checker/`, `compiler/src/codegen/llvm/core/generic.cpp`

### Phase 5: Write Missing Tests (LOW priority, only after Phases 1-3)

Only for modules that compile, link, and run correctly but lack test coverage:
- array/ascii (9 functions)
- collections/buffer (67 uncovered of 80)
- alloc/layout gaps
- fmt/rt, json gaps
- pool gaps

**Gate**: Each module must pass a smoke test (single trivial test) BEFORE writing full suite.

## Impact
- Coverage: 95.02% → estimated 98.7% realistic ceiling
- Breaking changes: None
- Affected code: compiler codegen, test harness, build scripts
