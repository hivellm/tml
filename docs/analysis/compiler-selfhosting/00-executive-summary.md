# TML Compiler Self-Hosting: Executive Summary

**Date**: 2026-04-05
**Author**: TML Project Team
**Status**: Analysis Complete
**Companion Documents**: 01-compiler-inventory.md, 02-stdlib-readiness.md, 03-language-gaps.md, 06-prior-art.md, 07-risk-matrix.md

---

## 1. Verdict

**Self-hosting is feasible. Confidence: 85%. Recommended start: after Phase 7 complete.**

The TML compiler can be rewritten in TML. This is not a moonshot — it is a well-understood software engineering project with a clear path, bounded scope, and manageable risk. The primary technical blocker (language maturity) will be resolved by the end of Phase 7. The secondary technical blocker (LLVM integration) is already solved: the backend accepts IR as a text string, so a TML-written compiler needs only to emit a string of LLVM IR text — it does not need to call LLVM's C++ builder API.

The 85% confidence reflects three genuine uncertainties:
- Bootstrapping logistics (how to compile a self-hosting compiler for the first time)
- Performance of the TML-written compiler relative to the C++ version
- Unknown language gaps that only surface during a real porting attempt

None of these are blockers. All have well-understood mitigations from prior art (Rust, Go, Zig).

---

## 2. Compiler Size

### Layer 1: Must Port — Core Pipeline

These subsystems transform TML source code into LLVM IR text. They contain the language's semantic rules and must be rewritten in TML for true self-hosting.

| Subsystem | Source LOC | Header LOC | Total LOC | Files | Est. TML LOC |
|-----------|-----------|-----------|-----------|-------|-------------|
| Lexer | 2,830 | 869 | 3,699 | 12 | 2,400 |
| Parser | 6,327 | 3,499 | 9,826 | 19 | 6,400 |
| Type Checker | 21,179 | 2,137 | 23,316 | 47 | 15,100 |
| Borrow Checker | 4,971 | 1,629 | 6,600 | 11 | 4,300 |
| HIR Builder | 10,555 | 4,652 | 15,207 | 28 | 9,900 |
| THIR Lowerer | 1,873 | 1,169 | 3,042 | 9 | 2,000 |
| MIR Builder + Passes | 31,719 | 8,474 | 40,193 | 137 | 26,100 |
| MIR Codegen (IR text gen) | 76,336 | 2,634 | 78,970 | 136 | 51,100 |
| Query System | 2,126 | 969 | 3,095 | 15 | 2,000 |
| **Layer 1 Total** | **157,916** | **26,032** | **183,948** | **414** | **119,300** |

### Layer 2: Should Port — Tooling

These subsystems implement developer-facing tools. They can be ported after the core pipeline works. Their absence does not prevent self-hosting — they improve the developer experience.

| Subsystem | Source LOC | Header LOC | Total LOC | Files | Est. TML LOC |
|-----------|-----------|-----------|-----------|-------|-------------|
| CLI (build, run, check) | 26,791 | 0 | 26,791 | 55 | 17,400 |
| Testing Framework | 9,968 | 1,013 | 10,981 | 22 | 7,100 |
| Formatter | 1,181 | 146 | 1,327 | 7 | 860 |
| Plugin/Launcher | 916 | 282 | 1,198 | 11 | 780 |
| **Layer 2 Total** | **38,856** | **1,441** | **40,297** | **95** | **26,140** |

### Layer 3: Keep in C — Runtime

These files implement the OS interface layer. They call Windows/Linux syscalls, use LLVM/OpenSSL/BCrypt via C FFI, and manage low-level memory. The TML compiler will continue to call these as `@extern("c")` FFI — they are not candidates for rewriting.

| File Group | LOC | Rationale |
|-----------|-----|-----------|
| core/essential.c (I/O, panic, test harness) | 1,344 | OS syscalls — must stay C |
| memory/mem.c + mem_track.c + pool.c + str_free.c | 1,225 | malloc/free wrappers — OS interface |
| crypto/ (6 files) | 4,356 | OpenSSL/BCrypt FFI — not rewritable |
| net/ (iocp.c, tls.c, net.c, poll.c, dns.c) | 2,974 | IOCP/Winsock — OS interface |
| concurrency/ (sync.c, async.c) | 1,603 | OS mutex/event primitives |
| diagnostics/ (inspector.c, backtrace.c, log.c, console.c) | 2,964 | OS interface |
| os/ (os.c, os_process.c) | 1,766 | OS interface |
| collections/ (buffer_simd.c) | 249 | SIMD intrinsics — keep for perf |
| **Layer 3 Total** | **18,650** | Keep — OS interface |

### Layer 4: Keep Permanently — LLVM/LLD Backend

These subsystems are intentional wrappers around third-party C++ libraries (LLVM 18, LLD). The TML-written compiler will call them through the same interface (IR text in, .obj file out).

| Subsystem | Source LOC | Header LOC | Total LOC | Keep Reason |
|-----------|-----------|-----------|-----------|-------------|
| LLVM Backend (llvm_backend.cpp) | 978 | 290 | 1,268 | LLVM C API wrapper |
| LLD Linker (lld_linker.cpp) | 504 | 196 | 700 | LLD C++ wrapper |
| JIT Engine (jit_engine.cpp) | 111 | 55 | 166 | LLVM JIT wrapper |
| **Layer 4 Total** | **1,593** | **541** | **2,134** | Permanent C++ |

### Grand Total

| Layer | LOC | Status |
|-------|-----|--------|
| Core Pipeline (rewrite in TML) | 183,948 | Port |
| Tooling (port after core) | 40,297 | Port |
| C Runtime (keep) | 18,650 | Keep |
| LLVM/LLD Backend (keep) | 2,134 | Keep Permanently |
| **Total compiler C++** | **245,029** | |
| **TML standard library** | **141,450** | Already in TML |

---

## 3. Key Strengths

### 3.1 IR-as-Text LLVM Interface

The most important architectural decision already made: the LLVM backend accepts IR as plain text via `LLVMParseIRInContext`. This appears in `compiler/src/backend/llvm_backend.cpp` at lines 155 and 363. A TML-written codegen stage only needs to produce a `Text` value containing valid LLVM IR — it does not need to use LLVM's C++ builder API (which would require extensive FFI declarations or a custom C bridge). This cuts the hardest part of any LLVM-based self-hosting project from "very hard" to "straightforward."

### 3.2 Mature Standard Library

The TML stdlib is production-ready and covers everything a compiler needs:

- **Data structures**: `HashMap[K,V]`, `List[T]`, `BTreeMap[K,V]`, `HashSet[T]`, `Deque[T]`, `BinaryHeap[T]`, `Trie`, `IntervalTree` — all exist
- **String handling**: `Str`, `Text`, `Buffer`, SIMD-accelerated string ops, Unicode, regex — all exist
- **File I/O**: `File`, `BufReader`, `BufWriter`, directory traversal, `DirEntry`, metadata — all exist
- **Process management**: `Process::spawn`, environment variables, `env::args` — all exist
- **Formatting**: `Display`, `Debug` behaviors, template literals, `Text::format` — all exist
- **Error handling**: `Outcome[T,E]`, typed errors, error chains — all exist

The stdlib has 535 source files (199 core + 336 std), 141,450 lines of TML, and 1,682 test files achieving 93.2% coverage. A compiler written in TML starts with a mature platform.

### 3.3 Phase 4 Migration Methodology Proven

Phase 4 already migrated ~5,210 lines of C runtime code to pure TML, reaching "0 migration candidates remaining." This project demonstrated that the incremental porting methodology works: compile TML alongside C with `@extern("c")` stubs, port subsystem by subsystem, run tests at each step. The same methodology applies to porting the C++ compiler code.

### 3.4 Language Features Sufficient

The language already has everything needed to write a compiler:

- `Heap[T]` for boxed allocation, `Shared[T]` for reference-counted nodes
- Full generics with monomorphization — AST nodes can be generic
- Closures, iterators, pattern matching (`when`) — essential for traversals
- `@extern("c")` FFI for LLVM and OS calls
- `lowlevel` blocks where needed for performance-critical inner loops
- Behavior system for visitor/transformer patterns (AST visitors, pass pipelines)
- Enums with data (`Maybe[T]`, `Outcome[T,E]`, custom variants) — essential for IR representation

### 3.5 Test Suite as Regression Net

1,700+ tests across 200+ test files provide strong regression coverage. When porting a subsystem, the full test suite immediately reveals regressions. The testing framework uses the NDJSON subprocess model, which means the test runner itself does not depend on compiler internals — it works as long as compiled executables run correctly.

---

## 4. Key Risks

| Risk | Severity | Probability | Mitigation |
|------|---------|-------------|-----------|
| Bootstrapping complexity (chicken-and-egg: TML compiler needs TML compiler to compile it) | High | Certain | Keep C++ compiler as stage0; compile TML compiler with C++ compiler; then compile TML compiler with itself (same pattern as Rust, Go, Zig) |
| Performance regression (TML compiler slower than C++ version) | Medium | Medium (50%) | Profile incrementally; TML generates efficient LLVM IR; most compiler time is in LLVM itself (not front-end); accept 2–3x slower initially |
| Type system complexity (Hindley-Milner + generics is hard to port) | High | Low (25%) | Type checker (23K LOC) is the highest-risk subsystem; port last; use extensive test coverage; consider keeping C++ type checker longer |
| Unknown language gaps only surfacing during port | High | Medium (40%) | Phase 3 of language-gaps.md identified known gaps; real porting will reveal more; budget 20% overhead for gap fixing |
| MIR codegen complexity (76K LOC, most complex subsystem) | High | Low (20%) | Port incrementally by IR feature; LLVM IR text generation is well-understood; existing test suite provides per-construct coverage |
| LLVM/LLD version coupling (IR text format changes between LLVM versions) | Low | Low (15%) | Pin LLVM version during self-hosting port; upgrade after stabilization |
| Concurrent development (Phase 7 still active, changes to compiler during port) | Medium | High (70%) | Branch for self-hosting work; merge Phase 7 changes periodically; freeze API after Phase 7 complete |
| Build time regression (TML-written compiler slower to compile itself) | Low | Medium (40%) | Use incremental compilation; Cranelift backend for debug builds (Phase 6.3); accept slower first bootstrap |

---

## 5. Recommended Approach

The recommended approach follows the proven bootstrap pattern used by Rust (2011), Go (2015), and Zig (2022): build a TML compiler in TML using an existing C++ compiler as the bootstrap, then use the TML-compiled result to compile itself. The work is divided into six phases executed sequentially.

### Phase SH-1: Foundation (Pre-requisite)
Complete Phase 7 (Rust parity). Specifically: close remaining language gaps identified in 03-language-gaps.md, implement the `@derive` macro system for compiler AST boilerplate, ensure `HashMap` and `BTreeMap` are fully generic and production-stable. Duration: parallel with Phase 7 completion.

### Phase SH-2: Lexer + Parser in TML
Port the lexer (3,700 LOC) and parser (9,800 LOC) to TML. These are the simplest subsystems to port — they have no external dependencies except string handling, which the TML stdlib covers fully. The TML version is compiled by the C++ compiler and its output is compared against the C++ version on a large corpus of TML source files. Target: 100% output equivalence on the standard library and test suite.

### Phase SH-3: Type Checker in TML
Port the type checker (23,300 LOC). This is the highest-complexity subsystem. The TML type representation (`types/type.hpp`, `struct Type`) maps naturally to a TML enum with variants. The 4-phase check sequence (register → imports → impls → bodies) maps to four functions. Hindley-Milner inference maps to a constraint-solving loop with `List[Constraint]`. Duration estimate: 6–8 months, the longest single phase.

### Phase SH-4: HIR + THIR + MIR in TML
Port the HIR builder (15,200 LOC), THIR lowerer (3,000 LOC), MIR builder (12,300 LOC, excluding passes), and the 52 MIR optimization passes (19,400 LOC). The passes are the most independent units — each pass is a `mir::Module -> mir::Module` transform and can be ported one at a time.

### Phase SH-5: MIR Codegen (IR Text Generation) in TML
Port the MIR codegen (4,080 LOC) and the legacy LLVM IR generator (72,256 LOC, eventually replaced by MIR path). The MIR codegen path generates text, which is pure string concatenation — the most natural thing to do in TML with template literals and `Text`. Target: byte-for-byte equivalent LLVM IR output on the standard library.

### Phase SH-6: Bootstrap and Tooling
Perform the first bootstrap: use the TML-written compiler (compiled by C++ compiler) to compile itself. Verify output. Then use the self-compiled TML compiler to compile itself a second time and verify bit-for-bit equivalence (the classic bootstrap validation). Port CLI, testing framework, and formatter as the final step.

---

## 6. Timeline

These estimates assume one experienced developer working full-time. A team of two would roughly halve elapsed time.

| Phase | Description | Duration | Cumulative | Milestone |
|-------|-------------|----------|-----------|-----------|
| SH-1 | Foundation (complete Phase 7 + language gaps) | 3 months | 3 months | All Phase 7 items done; known gaps closed |
| SH-2 | Lexer + Parser in TML | 2 months | 5 months | TML front-end parses entire stdlib correctly |
| SH-3 | Type Checker in TML | 8 months | 13 months | TML type checker passes full test suite |
| SH-4 | HIR + THIR + MIR in TML | 6 months | 19 months | TML generates correct MIR for all test cases |
| SH-5 | MIR Codegen in TML | 4 months | 23 months | TML emits equivalent LLVM IR for stdlib |
| SH-6 | Bootstrap + Tooling | 3 months | 26 months | TML compiler compiles itself; three-stage bootstrap clean |

**Total estimate: 24–30 months** (single developer). Optimistic case (2 developers, no major language gaps): 14–18 months.

For comparison: Rust's self-hosting took ~18 months (2011–2013, team of ~5); Go's took 12 months (2014–2015, team of ~3); Zig's is still ongoing after 4+ years due to the simultaneous language+compiler coevolution challenge.

---

## 7. Investment Required

### Developer Resources
- **Minimum viable**: 1 senior compiler engineer, 24–30 months
- **Recommended**: 2 compiler engineers, 14–18 months
- **Key skills required**: Compiler theory (parsing, type inference, SSA IR), TML language expertise, LLVM IR knowledge

### Infrastructure
- **Bootstrap compiler**: The existing C++ compiler (already built)
- **Regression test suite**: Already exists (1,700+ tests) — run after each ported subsystem
- **Binary diff tooling**: Script to compare LLVM IR output between C++ and TML compiler versions
- **Three-stage build script**: C++ compiler → TML compiler stage1 → TML compiler stage2 → bit-compare stage1 and stage2
- **CI changes**: Add three-stage bootstrap build to CI pipeline (adds ~30 min to build time)

### Code Volume
- **Port**: ~183,948 LOC of C++ core pipeline → ~119,300 LOC of TML (35% reduction from language expressiveness)
- **Net result**: The C++ compiler (~245,029 LOC total) is replaced by a TML compiler (~145,440 LOC, Layer 1+2 combined) plus permanent C++ layer (~20,784 LOC, Layer 3+4)
- **Reduction**: From 245,029 LOC of C++ to ~20,784 LOC of C++ permanently maintained — an 91.5% reduction in C++ maintenance burden

---

## 8. Decision Framework

### Prerequisites (all must be true before starting SH-2)

| Prerequisite | Current Status | Action Required |
|-------------|---------------|----------------|
| Phase 7 complete (Rust parity 16/16) | 14/16 done | Complete phase7-16 (Slice + Num + Fmt) |
| `@derive` macro system works for struct boilerplate | Partial | Verify or implement for AST nodes |
| `HashMap[Str, T]` stable with 10K+ entries | Yes (verified) | None |
| `BTreeMap[K,V]` fully generic | Done (phase7-15) | None |
| Language gap list from 03-language-gaps.md closed | Partially | Address P0/P1 gaps |
| Three-stage build script exists | No | Write before SH-6 |
| IR comparison tooling exists | No | Write before SH-5 |

### Go / No-Go Decision Points

**Go decision** (recommended if all true):
- Phase 7 complete
- No P0 language gaps remaining
- At least one developer available for 12+ months
- Organization committed to maintaining C++ compiler in parallel during port

**No-go conditions**:
- Phase 5 networking goals not met (performance issues outstanding)
- Compiler test coverage below 90% (current: 93.2% — OK)
- Fewer than 2 experienced TML developers available

### Recommendation

**Start SH-1 now** (parallel with Phase 7 completion). Specifically: close the language gaps in 03-language-gaps.md that are rated P0 and P1. These gaps need to be fixed regardless of self-hosting. **Start SH-2 immediately after Phase 7 is marked complete.** The lexer and parser are the lowest-risk entry point and will reveal any remaining tooling gaps before investing in the high-complexity type checker phase.

---

*Full technical details: [01-compiler-inventory.md](01-compiler-inventory.md) | [02-stdlib-readiness.md](02-stdlib-readiness.md) | [03-language-gaps.md](03-language-gaps.md) | [07-risk-matrix.md](07-risk-matrix.md)*
