# 06 — Runtime, Panic Model & Concurrency

**Findings:** L-100..L-108 · **Method:** code audit + measured probes (binary sizes, spawn timings; `.sandbox/`, cleaned up) · **Builds on:** F-007 (`../architecture-performance-review/02-test-speed-architecture.md`) — this dive re-baselines its premise and scopes the C1 gate precisely.

## Summary

TML's panic model is **not** unwinding: codegen emits `call @panic` + `unreachable` (zero `invoke`/`landingpad` in the whole backend), and the runtime either `exit(1)`s or `longjmp`s to a **process-global** `jmp_buf` — no drops run, ever, during a panic. Crucially, a mature in-process panic *and hardware-crash* catching machinery already exists and is used per-test today (setjmp + Windows VEH + `RtlRestoreContext` for corrupted-stack recovery); the only thing making it single-threaded is that every piece of its state is a plain C static — the C1 gate is a **state-locality problem, not a missing mechanism**. Separately, a severe correctness cliff: the entire `std::sync::atomic` family (and therefore `std` Arc) is **fake** — plain non-atomic loads/stores with the `Ordering` argument ignored — and the only working `thread::spawn` paths don't enforce `Send`. Finally, F-007's premise is stale: test EXEs are 0.3–0.7 MB *statically linked* binaries importing only 3 system DLLs, and spawn+exec measures ~11–30 ms, not "~100 MB DLLs / ~500 ms per spawn".

---

### L-100 — Panic is noreturn + global-state longjmp; no unwinding, no drops, poisoned locks

**Impact:** High · **Confidence:** High · **Layer:** design

Evidence: `compiler/src/codegen/llvm/builtins/io.cpp:219-225` (`call void @panic(ptr)` then `unreachable`); `compiler/src/codegen/llvm/core/runtime.cpp:117` (`declare … @panic(ptr) noreturn`); grep for `invoke|landingpad` across `compiler/src/codegen/llvm/` hits only 2 comments. Runtime: `compiler/runtime/core/essential.c:501-550` — hook → if `tml_catching_panic` (static, essential.c:145) `longjmp(tml_panic_jmp_buf)` (static, :142) else `exit(1)`. Drops are emitted only on structured scope exit (`compiler/src/codegen/llvm/core/drop.cpp:1291` `emit_scope_drops`), so a caught panic skips every destructor: heap live at panic-time leaks, and a held `MutexGuard` never unlocks — directly contradicting `lib/std/src/sync/mutex.tml:117-120` ("if a thread panics while holding the lock, the mutex remains usable"; it stays locked forever under longjmp-catch).

**Why it conflicts:** the zero-cost side is actually *good* for perf (no EH tables, no invoke overhead — Rust with `panic=abort` semantics). The conflict is that the *recovery* half (test harness, `core::runtime::panic::catch_unwind_fn`, `lib/core/src/runtime/panic.tml:126`) is exposed as if it were Rust's `catch_unwind` while giving Go-recover-without-defers semantics. Fine for a fail-fast test harness, unsound as a public language feature.

**Recommendation:** document catch-as-leaky officially; keep panic=abort semantics for production binaries; do NOT invest in full LLVM EH now (see Verdict).

---

### L-101 — In-process per-test panic/crash isolation already exists; thread-level isolation is blocked only by ~15 process-global statics + harness policy

**Impact:** Very High (this is the C1 gate) · **Confidence:** High · **Layer:** implementation

Evidence: `tml_run_test_catch_impl` `compiler/runtime/core/essential.c:1383-1480` — per-test setjmp, VEH install, and severe-crash recovery via `RtlCaptureContext`/`RtlRestoreContext` (essential.c:1396, 1190-1193); VEH handler with crash-severity classification `essential.c:1018-1200, 284-300`. Already used in-process by the generated test main (`compiler/src/codegen/llvm/core/generate_entry.cpp:678-688`) and the NDJSON suite dispatcher (`compiler/src/testing/testing_dispatcher_gen.cpp:393`). Zero `_Thread_local`/`__declspec(thread)` anywhere in the runtime (grep confirmed): `tml_panic_jmp_buf`, `tml_catching_panic`, `tml_panic_msg`, backtrace buffers, `tml_recovery_context`, crash-context buffers are all shared statics. The watchdog timeout uses `TerminateProcess` (essential.c:1277), and output suppression is global (essential.c:108-135).

**Why it (doesn't) conflict:** the docs' framing ("thread-level panic isolation … doesn't exist", `single-binary-test-compilation.md:72`) overstates the gap — VEH runs *on the faulting thread*, so TLS-ifying the state makes the whole machinery (including `RtlRestoreContext` recovery) thread-correct with no new mechanism and no LLVM EH work.

**Recommendation — concrete C1 plan:** (1) `_Thread_local` on the ~15 Group-C statics in essential.c (the "Group C must stay in one TU" constraint, essential.c:36-38, is unaffected); (2) a C-side thread-pool dispatcher consuming a codegen-emitted fn-pointer table (small change in `testing_dispatcher_gen.cpp`); (3) per-test `TerminateProcess` timeout becomes suite-level deadline (Go/Rust semantics) or subprocess fallback; (4) TLS output capture for NDJSON ordering; (5) keep the existing severity policy — heap-corruption class still aborts the process. **≈ 1.5–2.5 engineer-weeks.**

---

### L-102 — `std::sync::atomic` types are not atomic; `Ordering` is decorative; std `Arc` refcount is a data race

**Impact:** Very High (correctness cliff; blocks safe parallel anything) · **Confidence:** High · **Layer:** implementation

Evidence: `lib/std/src/sync/atomic/i64.tml:25-85` and `u64.tml:25-58` — `load`/`store`/`swap`/`compare_exchange`/`fetch_add` are plain reads/writes/RMWs inside `lowlevel` blocks; the `order: Ordering` parameter is never used. `lib/std/src/sync/arc.tml:227,391,433,458` builds its strong/weak counts on these fake `AtomicU64`s → cross-thread `Arc` clone/drop is UB. Real atomics exist but only as **I32-only, seq_cst-only** builtins: `lib/core/src/sync.tml:24-57` → `compiler/src/codegen/llvm/builtins/atomic.cpp:36-141` (`load atomic … seq_cst`, `atomicrmw`, `cmpxchg` hardwired seq_cst). Meanwhile a *second* Arc, `core::alloc::sync::Sync[T]`, uses correct C FFI atomics (`lib/core/src/alloc/sync.tml:54-61` → `compiler/runtime/concurrency/sync.c:751-784`, `InterlockedExchangeAdd`/`__sync_fetch_and_add`) — two parallel Arc implementations with different soundness.

**Why it conflicts:** a Rust-class language's core promise is fearless concurrency; today the advertised atomic API is a mirage, so any C1 thread-per-test run executing TML concurrency tests is unsound *by library design*, and the `Ordering` API can't deliver its intended perf (relaxed) or correctness (acquire/release) semantics.

**Recommendation:** priority fix, small scope: extend `builtins/atomic.cpp` to i64/u64/ptr widths + map the `Ordering` argument to LLVM orderings (constant-folded at codegen), reimplement the `std::sync::atomic` type methods on those intrinsics, and collapse std `Arc` onto `core::alloc::sync::Sync` (or vice versa).

---

### L-103 — `Send`/`Sync` exist but are unenforced; the generic `thread::spawn` is a stub that always fails

**Impact:** High · **Confidence:** High · **Layer:** implementation (design intent is right)

Evidence: `lib/std/src/thread/mod.tml:760-766` — `Builder::spawn[T: Send]` returns `Err(SpawnError::Unsupported)` with comment "blocked by codegen limitations"; top-level `spawn[T: Send]` (mod.tml:806-812) therefore panics. The only working paths are `spawn_fn(f: func())` and `spawn_i64` (mod.tml:328, 363) — no `Send` bound, so `Shared[T]` (documented non-thread-safe, `lib/core/src/alloc/shared.tml:60-61`) and `Cell`-likes cross threads unchecked. Marker behaviors exist with full docs (`lib/core/src/traits/marker.tml:87,133`).

**Why it conflicts:** the safety half of the Rust model (compile-time data-race exclusion) is what *permits* aggressive concurrency without runtime cost; without enforcement, TML gets neither safety nor the perf of a trusted concurrent stdlib, and threading is effectively monomorphic (Unit/I64 returns only).

**Recommendation:** fix the codegen gaps for generic fn-pointer/closure spawn (same class of work already tracked for closures), then enforce `Send` on all spawn paths and auto-derive Send/Sync structurally. (Same conclusion as L-023 in the memory-model dive.)

---

### L-104 — Allocator is raw CRT `malloc` called straight from codegen; no per-thread caching story

**Impact:** Medium-High · **Confidence:** High · **Layer:** design

Evidence: codegen emits `call ptr @malloc` directly for closure envs (`compiler/src/codegen/llvm/expr/closure.cpp:430`), heap struct literals (`expr/llvm_struct_expr.cpp:672`), string concat (`expr/binary.cpp:1152`), class instantiation (`core/class_codegen.cpp:893`); `compiler/src/codegen/llvm/core/runtime.cpp:95,179-182` declares `malloc`/`mem_alloc` with proper `allockind`/`alloc-family` attributes (good — LLVM can elide); `compiler/runtime/memory/mem.c` is a thin malloc wrapper. Opt-in TML arenas/pools exist (`lib/std/src/alloc/{arena,pool,cache,soo}.tml`) but the default global path is Windows CRT heap (heap-lock serialized).

**Why it conflicts:** the memory-model band-aids *added copies* (F-015..F-017) so generated code is allocation-heavier than Rust's; pairing an allocation-heavy codegen with the slowest mainstream allocator compounds the gap, and under C1-style in-process parallelism the CRT heap lock becomes a shared bottleneck.

**Recommendation:** statically link mimalloc (or rpmalloc) into `tml_runtime.lib` and route `malloc/mem_alloc` there — one link-line change, no codegen change, benefits every existing binary; keep the LLVM alloc attributes.

---

### L-105 — F-007's cost premise is stale: test EXEs are slim static binaries; measured spawn ≈ 11–30 ms, not ~500 ms with ~100 MB DLLs

**Impact:** High (redirects the C1 business case) · **Confidence:** High for this tree (measured) · **Layer:** design-doc accuracy

Evidence: `/DEFAULTLIB:libcmt` static CRT (`compiler/src/backend/lld_linker.cpp:237`); OpenSSL/socket libs linked only on demand (`compiler/src/cli/builder/build.cpp:1040-1063`). Measured: hello world **300 KB**, `build/debug/*.test.exe` 0.3–0.7 MB, PE imports = `WS2_32.dll, ADVAPI32.dll, KERNEL32.dll` only; spawn+exec avg **11.5 ms** (hello, N=20), **10.9 ms** (test EXE), **29.9 ms** for a full `--run-all` suite process. The "~100MB of runtime DLLs" text survives at `compiler/src/cli/commands/cmd_test.cpp:299` and in F-007/F-011 (`../architecture-performance-review/02-test-speed-architecture.md`).

**Why it matters:** at ~176 suite processes × ~11 ms, total spawn overhead is ~2 s serial (~0.5 s at 8-wide) — the remaining structural test cost is the **~176 codegen+link cycles**, which subprocess-vs-thread does not change (the mega-binary Phase 2 of `docs/analysis/compiler-internals/single-binary-test-compilation.md:54-66` keeps subprocesses). C1's real unlock is enabling the mega-binary *endgame* (1 link, N threads, intra-suite execution parallelism), not deleting 500 ms/spawn.

**Recommendation:** re-baseline F-007 with these measurements; sequence C1 as the enabler of single-mega-binary rather than as a spawn-cost fix. Also note the per-entry fail-fast (`generate_entry.cpp:684-688` — first failure skips the rest of that EXE's tests) as a hidden serial-retry cost.

---

### L-106 — FFI `@extern("c")` is genuinely zero-cost — sound

**Impact:** Positive · **Confidence:** High · **Layer:** design

Evidence: `compiler/src/codegen/llvm/decl/func.cpp:384-451` — extern fns lower to a bare `declare` + direct `call`; calling-convention overrides (stdcall/fastcall) supported (func.cpp:414-418); bool return promoted for C ABI (func.cpp:431); per-decl link libs collected for the linker (func.cpp:449-450). No marshalling layer, no wrappers; `Str` crosses as a raw pointer with `tml_str_from_cstr` for the inbound direction.

**Why it doesn't conflict:** identical cost model to Rust `extern "C"`. Safety is trust-based (no unsafe-boundary audit), acceptable at this maturity.

**Recommendation:** keep; later add `lowlevel`-gating diagnostics rather than runtime checks.

---

### L-107 — Startup is clean (no runtime init before main); sync primitives sit on the right OS objects

**Impact:** Positive · **Confidence:** High · **Layer:** implementation

Evidence: `generate_entry.cpp:752-765` — `@main` is a 3-line wrapper directly calling `@tml_main`; no init function, statics are lazy in C. Sync: SRWLOCK / CONDITION_VARIABLE / CreateThread on Windows, pthreads on POSIX (`compiler/runtime/concurrency/sync.c:11-19,53-360`); `Mutex[T]` wraps them via FFI with a 64-byte opaque raw slot (`lib/std/src/sync/mutex.tml:82-105,181-187`). Minor costs: core `Sync[T]` pays an out-of-line FFI call per refcount op (vs Rust's inlined `lock xadd`), and `Mutex[T]` carries a redundant `state: AtomicU32` shadow (mutex.tml:142,185) plus 64 bytes for an 8-byte SRWLOCK.

**Why it doesn't conflict:** thin-wrapper design over kernel-optimized primitives is exactly right; remaining costs are shavable later (intrinsify refcount ops).

**Recommendation:** keep; when atomics are fixed (L-102), inline the `Sync[T]` refcount via the atomic builtins instead of FFI.

---

### L-108 — Portability: dual-path C runtime exists, but POSIX is second-class

**Impact:** Medium · **Confidence:** Medium-High · **Layer:** implementation

Evidence: Win32/POSIX branches throughout (`sync.c` 19 `_WIN32` blocks, `os/os.c` 40, `essential.c` 13). Gaps: POSIX per-test timeout is a literal `TODO` (`essential.c:1344-1346`); severe-crash recovery (`RtlRestoreContext`) has no POSIX analogue (signals+longjmp only, essential.c:1451-1478); async runtime is a single-threaded polling executor with no epoll/kqueue/IOCP backend (`compiler/runtime/concurrency/async.c:1-20`); Windows-only VEH quality features (stack-guarantee, `_resetstkoflw`) have no equivalents.

**Why it conflicts (mildly):** ADR-007 (Zig CC) signals cross-compilation ambition; the debt is contained in the C runtime (the panic design being setjmp-based, not SEH/Itanium-EH-based, is actually the *portable* choice), so this is tractable — but any C1 work should land the TLS/dispatcher changes in both branches to avoid widening the gap.

**Recommendation:** when doing L-101, implement the POSIX side (sigaltstack + TLS jmp_buf) in the same change; defer real async I/O backends until the concurrency foundation (L-102/L-103) is sound.

---

## Verdict

**C1 (thread-level panic isolation) is concretely feasible and cheaper than the docs imply — but its payoff must be re-based.** The mechanism (per-test setjmp + VEH + context-restore recovery, essential.c:1383-1480) already runs in-process today; nothing about it requires LLVM `invoke`/landingpads, SEH personalities, or Itanium EH. The precise gap: (1) ~15 process-global statics → `_Thread_local`; (2) a table-driven thread-pool dispatcher in the generated harness; (3) per-test watchdog → suite-level deadline; (4) TLS output capture for NDJSON; (5) keep the "severe crash aborts process" policy. **Scope: ≈ 1.5–2.5 engineer-weeks.** Caught panics will still skip drops (leak-per-failure) — same as today's process model, acceptable for a harness; full drops-on-unwind (a multi-month EH + drop-flags codegen project entangled with the move-tracking debt) is *not* required for C1.

However: measured spawn cost is ~11–30 ms per statically-linked suite EXE (not ~500 ms/100 MB — that premise is stale), so C1's value is not spawn deletion; it is **unlocking the single-mega-binary endgame** (1 link instead of ~176, intra-run execution parallelism).

**Top runtime investment ranked by unlock value:**
1. **Real atomics + Ordering + Send enforcement (L-102/L-103)** — small effort, removes a silent-UB cliff, hard prerequisite for trusting any parallel in-process execution.
2. **C1 TLS-ification + parallel dispatcher (L-101)** — gates the last structural test-speed lever.
3. **mimalloc static link (L-104)** — one-line link change that pays on every allocation the codegen already over-emits.

## Keep

- The VEH crash machinery (severity classification, `RtlRestoreContext` for corrupted stacks, watchdog design, NDJSON-safe error escaping) — genuinely sophisticated, better than most young languages (essential.c:1018-1200).
- Zero-cost `@extern` FFI (func.cpp:384-451).
- Thin OS-primitive sync layer (SRWLOCK/pthreads) with dual-platform C (sync.c).
- Startup: no runtime init before `main`, 300 KB static hello world, slim static test EXEs, in-process LLD self-contained linking (lld_linker.cpp).
- Panic-as-noreturn codegen (no EH-table/invoke tax on the hot path) — right default for a perf-first language.

## Top 3 highest-leverage recommendations

1. **Make the atomics real** (`builtins/atomic.cpp` → all widths + honor `Ordering`; rewrite `lib/std/src/sync/atomic/*.tml` on the intrinsics; unify std `Arc` with core `Sync[T]`). Days of work; converts the concurrency stack from silently-unsound to trustworthy, prerequisite for everything parallel.
2. **TLS-ify the panic/catch state and ship the in-process parallel test dispatcher** (essential.c + testing_dispatcher_gen.cpp), then pursue the mega-binary (1 link) that it unlocks — this, not spawn cost, is the remaining test-speed lever.
3. **Link a modern allocator** (mimalloc) into `tml_runtime.lib` — cheapest runtime perf win available given the codegen's allocation-heavy output; update stale F-007/`cmd_test.cpp:299` claims so future planning uses the measured 11–30 ms spawn numbers.
