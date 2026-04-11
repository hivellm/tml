# Risk Matrix: TML Compiler Self-Hosting

**Date**: 2026-04-05  
**Status**: Current — 18 risks assessed  
**Scope**: All identified risks for the TML compiler self-hosting effort (Phase 6)  

---

## Section 1: Risk Scoring Methodology

### Probability Tiers

| Tier     | Range    | Meaning                                                              |
|----------|----------|----------------------------------------------------------------------|
| Low      | 10–30%   | Unlikely but possible; would require multiple things to go wrong     |
| Medium   | 30–60%   | Roughly even odds; depends on how carefully the risk is managed      |
| High     | 60–90%   | Expected to occur unless actively mitigated; baseline assumption     |

### Impact Tiers

| Tier     | Duration       | Meaning                                                              |
|----------|----------------|----------------------------------------------------------------------|
| Low      | Days           | Setback absorbed within normal work; schedule impact minimal         |
| Medium   | Weeks          | Noticeable delay; may require rescheduling downstream tasks          |
| High     | Months         | Significant delay; may require architectural decisions               |
| Critical | Project-threat | Could cause indefinite delay or permanent abandonment of self-hosting|

### Risk Score

Score = Probability weight × Impact weight on a 1–9 scale.

| Score | Classification | Response                                          |
|-------|---------------|---------------------------------------------------|
| 1–2   | Accept        | Monitor; no immediate action required              |
| 3–4   | Monitor       | Watch indicators; have contingency plan ready      |
| 5–6   | Mitigate      | Active mitigation required; track weekly           |
| 7–9   | Critical Path | Blocker-level attention; may delay Phase 6 start   |

Numeric mapping used internally:

- Low probability = 1, Medium = 2, High = 3
- Low impact = 1, Medium = 2, High = 3, Critical = 4 (capped at 3 for scoring scale)

Score = P × I, max 9. Anything 6+ is on the critical path.

---

## Section 2: Complete Risk Register

### R-001: Type Checker Semantic Drift During Porting

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Semantic Analysis                                                   |
| Probability  | High (70%)                                                                     |
| Impact       | High (months)                                                                  |
| Score        | **9** — Critical Path                                                          |
| Description  | The TML type checker spans 21,179 lines across 17 C++ files. It implements     |
|              | Hindley-Milner inference, behavior/trait resolution, generics, closures, async |
|              | type checking, and 4-phase registration. Subtle invariants exist only in the   |
|              | implementation. When ported to TML, the ported version will handle the common  |
|              | cases correctly but diverge on edge cases: ambiguous type inference results,   |
|              | specific generic constraint interactions, `when`-expression exhaustiveness     |
|              | edge cases, and behavior impl lookup order.                                    |
| Mitigation   | (1) Document all type checker invariants before porting. (2) Build differential|
|              | test harness: run both compilers on same input, compare error output. (3) Add  |
|              | 200+ new edge-case tests specifically targeting type checker invariants.       |
|              | (4) Port the type checker last among semantic phases — do Lexer→Parser→HIR    |
|              | first to build confidence. (5) Budget 6 months for the type checker alone.    |
| Contingency  | If drift is found late: maintain a "hard rule" list of known divergences that  |
|              | are acceptable (where the TML compiler is strictly more correct than the C++   |
|              | version). Keep the C++ compiler as oracle for regression testing indefinitely. |
| Owner        | Lead developer                                                                 |
| Review       | Monthly during porting; weekly during active type checker work                 |

---

### R-002: Codegen IR Output Not Identical to C++ Compiler

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Code Generation                                                    |
| Probability  | High (80%)                                                                     |
| Impact       | High (months)                                                                  |
| Score        | **9** — Critical Path                                                          |
| Description  | The LLVM codegen layer (76,336 lines across 107 files) is the largest          |
|              | subsystem. It handles struct layouts, ABI conventions, sret, calling           |
|              | conventions, generic instantiation, method dispatch, intrinsics, async         |
|              | lowering, SIMD, derive macros, and debug info. The probability that a          |
|              | faithful TML port produces bit-identical LLVM IR on first attempt is           |
|              | essentially zero. The risk is not that the IR is wrong in obvious ways (these  |
|              | will be caught by tests) but that it is subtly wrong: identical for most       |
|              | programs but wrong for specific calling conventions, struct alignment, or ABI  |
|              | edge cases that only surface in specific programs.                             |
| Mitigation   | (1) Define "acceptable divergence": the TML codegen need not produce           |
|              | IDENTICAL IR, only semantically equivalent IR that LLVM can optimize to equal  |
|              | machine code. (2) Use the Rust-as-reference IR methodology (from CLAUDE.md).  |
|              | (3) Build a differential execution harness: compile the same program with both |
|              | compilers, run both, compare outputs. (4) Start with the MIR codegen path     |
|              | (1,622 lines) which generates textual LLVM IR — much smaller than the full    |
|              | LLVM codegen.                                                                  |
| Contingency  | Accept that some IR differences are permanent improvements (the TML codegen    |
|              | produces better IR than the C++ codegen). The test is "does the program        |
|              | produce correct output" not "is the IR byte-identical."                        |
| Owner        | Lead developer                                                                 |
| Review       | Monthly; defer to weekly when active codegen porting begins                    |

---

### R-003: Dual-Path MIR Doubles Porting Scope

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Architecture                                                       |
| Probability  | Medium (50%)                                                                   |
| Impact       | High (months)                                                                  |
| Score        | **6** — Mitigate                                                               |
| Description  | TML currently has two MIR builders: HIR→MIR (legacy, hir_mir_builder.cpp,     |
|              | ~12K lines) and THIR→MIR (new, thir_mir_builder.cpp, ~3K lines). Both paths   |
|              | are active. Porting the self-hosted compiler requires deciding: port both, or  |
|              | port one and delete the other. Porting both doubles the MIR porting scope and  |
|              | doubles the surface area for divergence bugs. Deleting the legacy path before  |
|              | porting risks removing functionality that only the legacy path provides.       |
| Mitigation   | (1) Audit which test cases use the legacy path vs the THIR path. (2) Before    |
|              | Phase 6 begins, attempt to route all test cases through the THIR path. Fix     |
|              | failures. (3) If THIR path achieves 100% parity, delete the HIR→MIR path in   |
|              | the C++ compiler before porting. This reduces porting scope by ~12K lines.    |
| Contingency  | If legacy path cannot be eliminated: port only the THIR→MIR path. Accept that  |
|              | programs requiring the legacy path will require the C++ compiler to compile    |
|              | them. Document these programs explicitly.                                      |
| Owner        | Lead developer                                                                 |
| Review       | Before Phase 6 start — decision must be made then                             |

---

### R-004: Bootstrap Cycle Breakage

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Operational — Infrastructure                                                   |
| Probability  | Medium (40%)                                                                   |
| Impact       | High (months)                                                                  |
| Score        | **6** — Mitigate                                                               |
| Description  | The bootstrap cycle is: C++ compiler compiles TML compiler source → TML        |
|              | compiler compiles itself → verify outputs match. A bootstrap cycle breaks when:|
|              | (a) The TML compiler source uses a TML feature the C++ compiler doesn't        |
|              | support. (b) The TML compiler produces incorrect code that prevents itself     |
|              | from compiling. (c) A language change makes the TML compiler source            |
|              | incompatible with the Stage 0 (C++) compiler.                                 |
|              | When the bootstrap cycle breaks, development stops until it is repaired.       |
| Mitigation   | (1) Never use TML language features in the compiler source that are not         |
|              | supported by the C++ Stage 0. Document the "compiler-safe subset" of TML.     |
|              | (2) Run the bootstrap verification (Stage 2 equivalence check) after every     |
|              | significant change. (3) Tag C++ compiler releases as bootstrap anchors.        |
|              | Any compiler source compatible with a tagged C++ release can always be         |
|              | bootstrapped from that release.                                                |
| Contingency  | Maintain a git tag `bootstrap-stable` pointing to the last known working        |
|              | bootstrap configuration. If the cycle breaks, revert to this tag while fixing. |
| Owner        | Lead developer                                                                 |
| Review       | After every Stage 1/Stage 2 verification run                                   |

---

### R-005: Performance Regression (TML Compiler Slower Than C++)

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Performance                                                        |
| Probability  | High (85%)                                                                     |
| Impact       | Medium (weeks–months)                                                          |
| Score        | **6** — Mitigate                                                               |
| Description  | The current C++ compiler builds in ~100 seconds for the full TML stdlib.       |
|              | The self-hosted TML compiler will initially be slower because: (1) The port    |
|              | is faithful, not optimal — C++ template metaprogramming is faster than TML    |
|              | generics for certain patterns. (2) The C++ compiler uses custom allocators     |
|              | and memory pools for AST nodes that are not directly portable. (3) The TML     |
|              | compiler will use HashMap and List from the stdlib where the C++ compiler uses |
|              | unordered_map and vector with custom allocators. The expected initial slowdown  |
|              | is 1.5–2x (150–200 seconds).                                                  |
| Mitigation   | (1) Accept the slowdown as a known cost during the porting phase. (2) After    |
|              | the port is functional and correct, dedicate a separate optimization phase.    |
|              | (3) Profile the TML compiler with `mcp__tml__profile` to identify hotspots.  |
|              | (4) The Cranelift backend (Phase 6.3) will reduce DEBUG build time even if    |
|              | release compile time remains slow.                                             |
| Contingency  | If performance does not recover within 12 months of the port being functional, |
|              | switch to arena allocators for compiler-internal data structures. TML's core   |
|              | has Arena[T] — use it for all AST nodes, HIR nodes, and MIR nodes.            |
| Owner        | Lead developer                                                                 |
| Review       | After port is functional; monthly during optimization phase                    |

---

### R-006: LLVM Version Upgrade Breaks C++ Shim

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | External — Dependencies                                                        |
| Probability  | Medium (35%)                                                                   |
| Impact       | Medium (weeks)                                                                 |
| Score        | **4** — Monitor                                                                |
| Description  | The self-hosted TML compiler will call into C++ LLVM API code via a thin shim  |
|              | (the kept C++ files: llvm_backend.cpp, lld_linker.cpp). When LLVM releases a  |
|              | new version that changes the C API (this happens every 6–12 months), the shim  |
|              | will need updating. During the self-hosting effort, this is a distraction from |
|              | the porting work. Post-self-hosting, this is the normal maintenance burden.    |
| Mitigation   | (1) Pin the LLVM version during the self-hosting effort. Do not upgrade LLVM   |
|              | during Phase 6 unless there is a specific security or correctness need.        |
|              | (2) Use the LLVM C API (llvm-c/Core.h) rather than the C++ API where possible |
|              | in the shim — the C API has a stronger stability guarantee.                    |
| Contingency  | If LLVM upgrade is forced: update the shim before resuming self-hosting work.  |
| Owner        | Lead developer                                                                 |
| Review       | When LLVM releases a new major version                                         |

---

### R-007: Single Developer Bus Factor

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Organizational — Personnel                                                     |
| Probability  | Low (25%) for extended absence; Medium (50%) for partial availability          |
| Impact       | Critical (project-threatening)                                                 |
| Score        | **6** — Mitigate                                                               |
| Description  | TML is a single-developer project. The self-hosting effort is estimated at     |
|              | 24–30 months. A developer unavailability event (illness, life change, loss of  |
|              | interest) at any point halts all progress. There is no one who can continue    |
|              | the work because the institutional knowledge is entirely in one person's head. |
|              | This is the most severe organizational risk for a single-developer project.    |
| Mitigation   | (1) Document aggressively. Every design decision, every invariant, every        |
|              | "quirk" of the type checker should be in writing in docs/analyses/. (2) Write  |
|              | the compiler so that it can be understood from source — avoid clever tricks    |
|              | that only make sense with specific context. (3) The prior art documents (06)   |
|              | and this risk matrix (07) are themselves part of the mitigation — they allow   |
|              | a future developer to understand where the project was.                        |
| Contingency  | The C++ compiler remains functional regardless. Any partial self-hosting work  |
|              | is additive. If the project must be handed off, the existing analysis documents|
|              | provide the necessary context.                                                 |
| Owner        | N/A — inherent to project structure                                            |
| Review       | N/A                                                                            |

---

### R-008: Feature Parity Gap During Transition

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Completeness                                                       |
| Probability  | High (75%)                                                                     |
| Impact       | Medium (weeks per gap)                                                         |
| Score        | **6** — Mitigate                                                               |
| Description  | During the multi-year porting effort, the C++ compiler may gain new TML        |
|              | language features (syntax changes, new type system features, new builtin types)|
|              | that the in-progress TML compiler has not yet been updated to support. The TML |
|              | compiler source will use these new features (as they are idiomatic TML),       |
|              | but the older-vintage self-hosted compiler will not understand them.           |
|              | This creates a "feature gap" that must be managed.                             |
| Mitigation   | (1) During Phase 6, maintain a "feature freeze" for the TML language. New      |
|              | syntax and type system additions should be deferred to after self-hosting.     |
|              | (2) If new features are added to C++, ensure they are also added to the TML   |
|              | compiler's in-progress port before the Stage 1 verification step.             |
| Contingency  | If gap becomes large: roll back the new C++ features to their previous behavior|
|              | until the TML port catches up. This is possible because the C++ compiler       |
|              | remains in a git repository with full history.                                 |
| Owner        | Lead developer                                                                 |
| Review       | Monthly                                                                        |

---

### R-009: Test Coverage Insufficient to Catch Regressions

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Quality Assurance                                                  |
| Probability  | Medium (45%)                                                                   |
| Impact       | High (months of hidden bugs)                                                   |
| Score        | **6** — Mitigate                                                               |
| Description  | TML currently has 1,682 test files passing 1,700+ tests at 93.2% coverage.    |
|              | This coverage measures how much of the TML STDLIB is tested. It does not       |
|              | measure how many COMPILER behaviors are tested. The compiler (38K C++ lines)   |
|              | likely has thousands of behaviors that are not covered by any test in the      |
|              | existing test suite. When these behaviors are ported incorrectly to TML, the   |
|              | bugs will not be caught until they surface in real programs.                   |
|              | Concretely: the type checker's generic inference has dozens of edge cases that |
|              | are not tested by any existing .test.tml file.                                 |
| Mitigation   | (1) Before Phase 6, write 500+ new test cases specifically targeting compiler  |
|              | edge cases. Focus on: ambiguous type inference, complex generic constraints,   |
|              | borrow checker edge cases, ABI boundary conditions, pattern match              |
|              | exhaustiveness. (2) Add regression tests for every codegen bug that has been   |
|              | fixed in the C++ compiler's history (check git log). (3) Use property-based   |
|              | testing (TML has a PBT framework) for type inference stability.                |
| Contingency  | If regressions are found post-port: add the failing case as a new test, fix    |
|              | the TML compiler, and document the invariant that was missing.                 |
| Owner        | Lead developer                                                                 |
| Review       | Before Phase 6; quarterly during porting                                       |

---

### R-010: Memory Safety Bugs in Self-Hosted Compiler

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Correctness                                                        |
| Probability  | Medium (40%)                                                                   |
| Impact       | Medium (weeks per bug)                                                         |
| Score        | **4** — Monitor                                                                |
| Description  | The C++ compiler may have memory bugs (use-after-free, double-free, buffer     |
|              | overflow) that have never been triggered in practice because the specific       |
|              | input sequences required are rare. When the compiler is ported to TML, the    |
|              | borrow checker will reject many patterns that are nominally valid C++ but      |
|              | memory-unsafe. This will force the TML port to find safe alternatives. However:|
|              | TML also has `lowlevel` blocks where the borrow checker does not apply. If the |
|              | port uses `lowlevel` to work around borrow checker friction, bugs can remain.  |
| Mitigation   | (1) Avoid `lowlevel` blocks in the self-hosted compiler unless absolutely       |
|              | necessary (LLVM FFI calls only). (2) Use TML's owned types (Heap[T], List[T]) |
|              | exclusively for data structures — no raw pointers for owned data. (3) Run     |
|              | `mcp__tml__debug` with `check_leaks=true` on the TML compiler binary           |
|              | regularly during development.                                                  |
| Contingency  | If a memory bug is found in the TML compiler: the borrow checker will usually  |
|              | help identify the exact location. Unlike the C++ version, TML bugs produce     |
|              | clear borrow checker errors rather than mysterious crashes.                    |
| Owner        | Lead developer                                                                 |
| Review       | Run leak checks monthly                                                        |

---

### R-011: Build Time Regression for the TML Compiler Itself

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Developer Experience                                               |
| Probability  | High (70%)                                                                     |
| Impact       | Low (days — ongoing friction)                                                  |
| Score        | **3** — Monitor                                                                |
| Description  | The C++ TML compiler currently builds in ~100 seconds (I/O bound on NVMe).    |
|              | The self-hosted TML compiler will initially be larger (more LOC) and may take  |
|              | 150–200 seconds to compile itself. During active development of the self-      |
|              | hosted compiler, this build time is experienced on every iteration. A 2x       |
|              | slowdown in the edit-compile-test loop costs significant developer time daily. |
| Mitigation   | (1) Incremental compilation is already implemented — rely on it aggressively.  |
|              | (2) The Cranelift backend (Phase 6.3) will provide faster debug builds.        |
|              | (3) Split the self-hosted compiler into multiple compilation units (one per    |
|              | subsystem) to maximize incremental compilation cache hits.                     |
| Contingency  | If build time exceeds 5 minutes during development: profile with Tracy (already|
|              | integrated), identify the bottleneck, and optimize that specific compilation   |
|              | unit.                                                                          |
| Owner        | Lead developer                                                                 |
| Review       | Monthly during porting                                                         |

---

### R-012: Cross-Platform Support (Windows / Linux / macOS)

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Platform                                                           |
| Probability  | High (80%) for Linux gaps; Medium (60%) for macOS gaps                         |
| Impact       | Medium (weeks per platform)                                                    |
| Score        | **6** — Mitigate                                                               |
| Description  | TML currently targets Windows x86-64 exclusively. Phase 6.1 adds Linux x86-64 |
|              | and ARM64 cross-compilation. The self-hosted compiler must produce correct     |
|              | code on all three platforms. Platform-specific issues include: (a) Windows vs  |
|              | Linux calling conventions (x64 ABI vs System V ABI). (b) MSVC vs GNU C        |
|              | runtime differences. (c) macOS dylib vs Windows DLL differences. (d) Windows  |
|              | path separator handling (already mitigated by `path.join()` convention).       |
|              | (e) Endianness (not an issue for x86-64/ARM64 but matters if RISC-V is added).|
| Mitigation   | (1) Complete Phase 6.1 (cross-compilation) before the self-hosted compiler     |
|              | must produce cross-platform output. (2) Use Zig CC (already integrated) for   |
|              | cross-compilation from Windows. (3) Add Linux/macOS CI before Phase 6 — catch |
|              | platform issues early. (4) Conditional compilation (`#if LINUX / #if WINDOWS`) |
|              | already exists — use it in the self-hosted compiler where needed.              |
| Contingency  | If Linux/macOS support is significantly delayed: defer cross-platform to        |
|              | Phase 6 post-stability. Self-host on Windows first, then expand.              |
| Owner        | Lead developer                                                                 |
| Review       | When Phase 6.1 begins                                                          |

---

### R-013: Incremental Compilation in the TML Compiler Port

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Infrastructure                                                     |
| Probability  | Medium (55%)                                                                   |
| Impact       | Medium (months to implement correctly)                                         |
| Score        | **4** — Monitor                                                                |
| Description  | The C++ compiler has a query-based demand-driven incremental compilation system|
|              | (2,126 lines in `compiler/src/query/`). This system uses fingerprinting        |
|              | (CRC32C hashes of inputs and outputs), a persistent cache (`.incr-cache/`),   |
|              | and RED/YELLOW/GREEN coloring. Porting this system to TML is non-trivial:     |
|              | (a) The fingerprint system uses CRC32C which requires a hardware intrinsic.   |
|              | TML has CRC32 in its crypto module — verify it matches. (b) The persistent    |
|              | cache uses binary serialization — TML needs a binary serializer for all AST   |
|              | and HIR node types. (c) The memoization system requires function-level         |
|              | granularity which interacts with TML's ownership model.                        |
| Mitigation   | (1) Port the query system as its own phase (Stage 12 in the migration table   |
|              | from document 06). (2) Initially implement the TML compiler without            |
|              | incremental compilation — accept full recompilation on every change.           |
|              | (3) Add incremental compilation after the basic pipeline is working.           |
| Contingency  | The TML compiler without incremental compilation is still a valid self-hosted  |
|              | compiler. Incremental compilation is a performance feature, not a correctness  |
|              | requirement. Accept full recompilation for the first 12 months post-port.     |
| Owner        | Lead developer                                                                 |
| Review       | When query system porting begins                                               |

---

### R-014: Error Message Quality Regression

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Developer Experience                                               |
| Probability  | High (65%)                                                                     |
| Impact       | Low (days per message; medium for systemic regression)                         |
| Score        | **3** — Monitor                                                                |
| Description  | The C++ compiler has 15 dedicated error explanation modules (lexer_errors.cpp, |
|              | type_errors.cpp, borrow_errors.cpp, etc.) plus a rich diagnostic system with  |
|              | span tracking, hint suggestions, and error codes (T001, L001, etc.).           |
|              | When ported, the initial TML compiler may produce less helpful error messages  |
|              | because: (a) Span tracking requires accurate source location propagation through|
|              | all AST nodes. (b) Error suggestions require knowledge of the specific failure |
|              | case. (c) The explain system (cmd_explain.cpp) uses hardcoded knowledge that   |
|              | must be ported.                                                                |
| Mitigation   | (1) Treat error messages as a first-class feature. (2) Port the span tracking  |
|              | system early — it threads through every AST node and getting it wrong late is  |
|              | expensive. (3) Write tests for error messages (compile-fail tests that check   |
|              | that specific errors appear for specific invalid programs).                    |
| Contingency  | If error messages are poor in the initial port: accept this as a known          |
|              | regression. The self-hosted compiler's errors can be improved iteratively.    |
| Owner        | Lead developer                                                                 |
| Review       | After initial type checker port                                                 |

---

### R-015: String Interning Performance

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Performance                                                        |
| Probability  | Medium (50%)                                                                   |
| Impact       | Medium (weeks to implement; ongoing performance impact)                        |
| Score        | **4** — Monitor                                                                |
| Description  | The C++ compiler uses string interning extensively for identifiers, type names,|
|              | and symbol table keys. An interned string is a deduplicated string stored in a |
|              | shared table; comparisons are O(1) pointer comparisons rather than O(n) byte   |
|              | comparisons. The TML stdlib currently has no `Interner` type.                  |
|              | Without interning, the self-hosted compiler will use `Str` comparisons         |
|              | throughout the symbol table. For small programs this is acceptable. For large  |
|              | programs (the full TML stdlib, ~150K lines), the O(n) string comparisons in   |
|              | type checking could cause 5–10x slowdowns in the symbol table lookups.        |
| Mitigation   | (1) Implement `Interner[T]` in TML stdlib BEFORE starting the self-hosted      |
|              | compiler type checker port. An interner is ~200 lines of TML using HashMap.   |
|              | (2) Use `InternedStr` (a newtype over I64 index) throughout the self-hosted   |
|              | compiler's symbol table instead of raw `Str`.                                  |
| Contingency  | If interning is not implemented: use `HashMap[Str, T]` and accept the          |
|              | performance cost. Profile first to confirm it is actually a bottleneck.        |
| Owner        | Lead developer                                                                 |
| Review       | Before type checker porting begins                                             |

---

### R-016: AST Serialization Overhead in Hybrid Pipeline

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Architecture                                                       |
| Probability  | Low (25%)                                                                      |
| Impact       | Medium (weeks)                                                                 |
| Score        | **2** — Accept                                                                 |
| Description  | During the transition period, the pipeline will be hybrid: some stages in the  |
|              | C++ compiler, some in the TML compiler. For example: lexer and parser in TML, |
|              | type checker and codegen still in C++. To pass data between TML-compiled and  |
|              | C++-compiled stages, a serialization boundary is required. The current HIR     |
|              | serializer (binary_writer.cpp, text_writer.cpp) exists for this purpose, but  |
|              | it adds latency and complexity. If the serialized AST for a large file is 10MB,|
|              | the IPC cost may be measurable.                                                |
| Mitigation   | (1) Use the existing HIR binary serializer — it already works. (2) During      |
|              | hybrid operation, measure the serialization overhead with Tracy. (3) If        |
|              | overhead is >10% of total compile time, switch to shared memory or a memory-   |
|              | mapped file for the serialization boundary.                                    |
| Contingency  | If serialization is prohibitively slow: reorder the migration to avoid having  |
|              | the serialization boundary in a hot path. For example: port the full pipeline  |
|              | up to HIR completely before connecting to the C++ backend.                    |
| Owner        | Lead developer                                                                 |
| Review       | When first hybrid pipeline is operational                                      |

---

### R-017: Type Checker Undocumented Invariants

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Technical — Knowledge                                                          |
| Probability  | High (90%)                                                                     |
| Impact       | High (months of debugging)                                                     |
| Score        | **9** — Critical Path                                                          |
| Description  | The type checker (21,179 lines) was built incrementally over months and years. |
|              | Many of its invariants are implicit in the code structure, not documented.     |
|              | Examples of invariants that may not be documented:                             |
|              | (a) The 4-phase registration order (register → imports → impls → bodies)      |
|              | must be respected or certain lookup patterns will fail silently.               |
|              | (b) The `env_` type environment is mutated in place and the mutation order     |
|              | matters for certain cross-module lookups.                                      |
|              | (c) `type_implements()` has a known false-positive bug (see MEMORY.md) that   |
|              | is worked around in generate_default_method with a safe_types whitelist — this |
|              | workaround must be preserved in the TML port.                                  |
|              | (d) The builtins_cache.cpp `builtins_cache` global is populated during a       |
|              | specific phase; accessing it before that phase produces incorrect results.     |
|              | Every undocumented invariant is a potential source of divergence.              |
| Mitigation   | (1) Conduct a dedicated "invariant documentation sprint" before Phase 6 begins.|
|              | Read every file in compiler/src/types/ and document every assumption. (2) Tag  |
|              | known quirks with `// INVARIANT:` comments in the C++ code. (3) Port the     |
|              | documented invariants into TML doc comments on the ported functions.           |
| Contingency  | If an undocumented invariant causes divergence: use the C++ compiler's output  |
|              | as oracle, write a failing test, trace through the C++ code to find the        |
|              | invariant, document it, and fix the TML port.                                  |
| Owner        | Lead developer                                                                 |
| Review       | During invariant documentation sprint; ongoing during type checker porting     |

---

### R-018: Scope Creep — Improving During Porting Instead of Faithful Port

| Attribute    | Value                                                                          |
|--------------|--------------------------------------------------------------------------------|
| Category     | Process — Discipline                                                           |
| Probability  | High (75%)                                                                     |
| Impact       | Medium (months of delay)                                                       |
| Score        | **6** — Mitigate                                                               |
| Description  | While porting the C++ compiler to TML, the developer will constantly notice    |
|              | opportunities to improve the design: better data structures, cleaner API       |
|              | boundaries, architectural simplifications. Acting on these improvements during  |
|              | the port makes verification harder (which differences are improvements vs bugs?)|
|              | and extends the timeline. The TypeScript-to-Go team explicitly identified this |
|              | as their biggest risk. The Nim team fell into this trap, causing a 6-month     |
|              | delay in their self-hosting.                                                   |
| Mitigation   | (1) Adopt a hard rule: "faithful port first." Write it on the wall. No new     |
|              | features, no architectural improvements, no "while I'm here" cleanups during  |
|              | Phase 6. (2) Keep a "improvements for Phase 7" list. Every improvement idea   |
|              | goes on the list, not into the code. (3) Define the completion criterion:     |
|              | "The TML compiler passes all tests that the C++ compiler passes." Not          |
|              | "produces better IR" or "uses better data structures."                         |
| Contingency  | If scope creep has already begun: stop, assess how much divergence has been    |
|              | introduced, write tests to cover the divergent behavior, then continue.        |
| Owner        | Lead developer                                                                 |
| Review       | Monthly self-assessment                                                        |

---

## Section 3: Risk Heat Map

The following grid plots all 18 risks by probability and impact. Risks in the upper-right corner
are the highest priority.

```
          LOW IMPACT      MEDIUM IMPACT     HIGH IMPACT      CRITICAL IMPACT
          (days)          (weeks)           (months)         (project-threat)
HIGH      R-011           R-005, R-008      R-001, R-002
PROB.     R-014           R-012, R-018      R-017
(60-90%)

MED       R-013           R-003, R-004      R-009
PROB.     R-015, R-016    R-013             
(30-60%)                  R-015

LOW       R-006           R-010             R-007
PROB.     R-016           
(10-30%)
```

Visual summary: Risks R-001, R-002, and R-017 are in the highest-priority cell (High probability
+ High impact). These three risks alone represent a combined 6–12 months of potential delay if
they materialize simultaneously.

### Risk Score Summary

| ID    | Risk Name                                     | Score | Classification |
|-------|-----------------------------------------------|-------|----------------|
| R-001 | Type checker semantic drift                   | 9     | Critical Path  |
| R-002 | Codegen IR not identical                      | 9     | Critical Path  |
| R-017 | Type checker undocumented invariants          | 9     | Critical Path  |
| R-003 | Dual-path MIR doubles scope                   | 6     | Mitigate       |
| R-004 | Bootstrap cycle breakage                      | 6     | Mitigate       |
| R-005 | Performance regression                        | 6     | Mitigate       |
| R-007 | Single developer bus factor                   | 6     | Mitigate       |
| R-008 | Feature parity gap during transition          | 6     | Mitigate       |
| R-009 | Test coverage insufficient                    | 6     | Mitigate       |
| R-012 | Cross-platform support gaps                   | 6     | Mitigate       |
| R-018 | Scope creep during porting                    | 6     | Mitigate       |
| R-006 | LLVM upgrade breaks C++ shim                  | 4     | Monitor        |
| R-010 | Memory safety bugs in self-hosted compiler    | 4     | Monitor        |
| R-013 | Incremental compilation porting complexity    | 4     | Monitor        |
| R-015 | String interning performance                  | 4     | Monitor        |
| R-011 | Build time regression for compiler itself     | 3     | Monitor        |
| R-014 | Error message quality regression              | 3     | Monitor        |
| R-016 | AST serialization overhead in hybrid pipeline | 2     | Accept         |

---

## Section 4: Top 5 Risks — Deep Dive

### Risk R-001: Type Checker Semantic Drift (Score: 9)

**Detailed Description**

The type checker is TML's most complex subsystem. It implements:
- Hindley-Milner type inference (bidirectional, with backtracking)
- Behavior (trait) resolution with specialization
- Generic type parameter substitution
- Closure capture type inference
- Async/await type transformation (Future[T] wrapping)
- 4-phase registration (register declarations → resolve imports → check impls → check bodies)
- Cross-module type lookup with `env_module_load.cpp` and `env_lookups.cpp`
- The `type_implements()` false positive bug (MEMORY.md) and its safe_types workaround

The probability of drift is high because: (1) The type checker is the compiler component with the
most implicit state (the `env_` type environment is a large mutable global). (2) The 4-phase
ordering is not checked by any assertion — it is a convention. Violating it produces wrong results
silently. (3) The behavior resolution logic has at least one known false positive that required a
workaround. There are likely more unknown issues that have never been triggered.

**Root Cause Analysis**

Drift occurs when the TML port makes one of these mistakes:
- Evaluation order differs: TML evaluates expressions in declaration order; if the C++ code relied
  on evaluation order for side effects (populating the env_), the TML port must preserve that order
- Map iteration order differs: C++ `unordered_map` has non-deterministic iteration order; the C++
  code must not rely on map iteration order, but if it does accidentally, TML's HashMap (also
  unordered) will produce different results
- Mutation timing differs: the type environment is mutated during type checking; if a method is
  called slightly earlier in the TML port than in the C++ original, environment state may differ
- Pattern matching completeness: C++ uses if/else chains for type checking; TML uses `when`
  expressions; if a case is missed in the `when`, it silently falls through to Nothing

**Early Warning Signs**

- Differential testing shows the TML type checker accepts programs the C++ rejects (or vice versa)
- Test programs that use complex generics (e.g., `List[HashMap[Str, List[T]]]`) produce different
  error messages between compilers
- The type checker passes 95% of tests but has a consistent failure rate on a specific category
  (e.g., all failures involve closures with inferred return types)
- Bootstrap verification fails: Stage 2 produces different output than Stage 1

**Mitigation Plan (Specific Actions)**

1. Write `docs/analyses/compiler-selfhosting/08-type-checker-invariants.md` (target: 50+ pages)
   documenting every invariant in the type checker before porting begins. Estimated effort: 3 weeks.
2. Add 300 new test cases targeting type checker edge cases:
   - Ambiguous inference (at least 30 cases)
   - Generic constraint combinations (at least 50 cases)
   - Closure inference with complex capture types (at least 30 cases)
   - Cross-module behavior dispatch (at least 40 cases)
   - Error cases: invalid programs that should produce specific errors (at least 150 cases)
3. Build the differential testing harness before starting type checker port:
   ```
   test_differential(input.tml) -> compare (C++ errors, TML errors)
   ```
4. Port the type checker in sub-phases, not as a single unit:
   - Phase 1: Expression type inference (checker/expr.cpp — 850 lines)
   - Phase 2: Declaration registration (checker/core.cpp — 1,412 lines)
   - Phase 3: Method call resolution (checker/expr_call_method.cpp — 1,363 lines)
   - Phase 4: Generic instantiation (checker/resolve.cpp)
   - Phase 5: Integration and cross-phase interactions
5. After each sub-phase: run the full test suite differential comparison.

**Contingency if Mitigation Fails**

If the type checker produces divergent results after 3 months of debugging: accept the divergences
where the TML port is provably more correct (e.g., if it rejects programs that the C++ compiler
incorrectly accepts). For divergences where the C++ compiler is more correct: document them
explicitly as known limitations of the TML compiler v1.0, with planned fixes in v1.1.

**Owner and Cadence**: Lead developer; weekly review during type checker porting.

---

### Risk R-002: Codegen IR Not Identical to C++ Compiler (Score: 9)

**Detailed Description**

The LLVM codegen is 76,336 lines across 107 files. It handles:
- 15 builtin modules (assert, async, atomic, intrinsics, SIMD, io, math, mem, string, sync, time)
- 4 control flow patterns (if, loop, return, when)
- 12 core generation modules (class codegen, debug info, drop, dyn, generate, generic, llvm types)
- 6 declaration types (enum, func, impl, struct decl)
- 11 derive macros (Debug, Default, Deserialize, Display, Duplicate, FromStr, Hash, PartialEq,
  PartialOrd, Reflect, Serialize)
- 30+ expression types (await, binary, call, cast, closure, collections, method dispatch, struct,
  tuple, unary)

Generating bit-identical LLVM IR is not required. What is required is that the emitted LLVM IR
is semantically equivalent and that LLVM can optimize it to equivalent machine code.

The risk is subtle IR differences that produce subtly wrong programs:
- ABI mismatches (sret convention, calling convention, argument passing)
- Struct layout differences (padding, field ordering)
- Integer overflow behavior (nsw/nuw flags)
- Missing or wrong null checks
- Incorrect lifetime information (affects LLVM optimization quality)

**Root Cause Analysis**

The C++ codegen has years of accumulated fixes for ABI edge cases. For example (from MEMORY.md):
- Bool/i1 struct field layout fix (IncomingMessage.is_complete → I64)
- Unit `{}` vs `void` call mismatch fix
- RawPtr single-field struct cast (extractvalue/insertvalue)
- sret convention for struct returns

Each of these fixes represents an ABI edge case. When ported to TML, these fixes must be ported
faithfully — and any new TML-specific ABI edge cases must be handled.

**Early Warning Signs**

- Programs that compile and link but produce wrong output at runtime
- Segfaults or memory corruption in programs compiled by the TML compiler
- Tests that pass when compiled by the C++ compiler but crash when compiled by the TML compiler
- The bootstrap verification failing with a segfault (not a wrong output, but a crash)

**Mitigation Plan (Specific Actions)**

1. Build the differential execution harness:
   - Take every test in `lib/core/tests/` and `lib/std/tests/`
   - Compile with C++ compiler → run → capture output
   - Compile with TML compiler → run → capture output
   - Compare outputs
   This harness provides end-to-end correctness verification regardless of IR differences.
2. Start with the MIR codegen path (1,622 lines in mir_codegen.cpp) rather than the full LLVM
   codegen (76K lines). The MIR path generates textual LLVM IR — it is much smaller and more
   portable.
3. For each ABI issue documented in MEMORY.md, write a test specifically targeting that ABI
   pattern before porting. Confirm the test passes with the C++ compiler. Then port and verify
   the test still passes with the TML compiler.
4. Maintain a "known IR differences" document. Any IR difference that is verified to not cause
   different program behavior can be accepted and documented.

**Contingency if Mitigation Fails**

If the TML codegen produces systematically wrong ABI for a specific pattern: revert to generating
LLVM IR via the textual MIR path for that pattern (add a code path that falls back to the text
emitter for problematic cases). This creates a hybrid codegen that is correct even if not optimal.

**Owner and Cadence**: Lead developer; daily during active codegen porting.

---

### Risk R-017: Type Checker Undocumented Invariants (Score: 9)

**Detailed Description**

This risk is distinct from R-001 (semantic drift) in that R-017 specifically concerns implicit
knowledge that is not in any test or document — it lives only in the implementer's memory.

Concrete examples of undocumented invariants discovered during research for this document:

**Invariant 1**: The `type_implements()` function has a known false positive where it returns
`true` for types that do not actually implement a behavior. This was worked around in
`generate_default_method` using a `safe_types` whitelist that allows only primitive types. If the
TML port calls `type_implements()` without the whitelist check, it will generate incorrect default
method implementations for user-defined types.

**Invariant 2**: The `env_module_load.cpp` private_imports resolution is not recursive. If a
module re-exports a private import, the re-exported symbol is not visible to the codegen. This is
a known bug (from MEMORY.md: "Bug 1: private_imports not recursive"). The TML port must preserve
this behavior (the bug) for compatibility, then fix it separately.

**Invariant 3**: `pub use` re-exports are invisible to the codegen. This is another known bug.
The TML port must reproduce this behavior until it is explicitly fixed.

**Invariant 4**: The `builtins_cache.cpp` global is populated in a specific phase. If any
function reads from this cache before the population phase completes, it gets empty results. This
is not asserted anywhere.

These are only the invariants that are known. There are certainly more.

**Root Cause Analysis**

Implicit invariants accumulate in any large codebase that grows incrementally without a dedicated
specification document. The type checker grew from a small prototype to 21K lines over months.
Each addition was locally correct but collectively produced a system with emergent behaviors that
were never written down.

**Mitigation Plan (Specific Actions)**

1. Allocate 4 weeks dedicated to the invariant documentation sprint before Phase 6 begins.
   Read every file in `compiler/src/types/` from top to bottom. For every function:
   - What preconditions must hold before this function is called?
   - What postconditions does this function guarantee?
   - What global state does this function read? Write?
   - Are there known bugs or workarounds in this function?
   Write these as structured comments in a new document: `08-type-checker-invariants.md`.
2. Add `// INVARIANT: description` comments to the C++ source for every discovered invariant.
   These comments are cheap to add and invaluable for the future TML porter.
3. Write regression tests for every known bug (like the `type_implements` false positive).
   Confirm that the test fails if the workaround is removed. This proves the test covers the bug.
4. During porting: for every TML function ported from C++, include the invariants from step 1
   as TML doc comments (`///`). Do not strip the documentation.

**Contingency if Mitigation Fails**

If undocumented invariants cause unexpected failures after porting: use the C++ compiler as oracle.
For each failing test, trace through the C++ type checker with added debug logging. Find the
invariant that the TML port is violating. Document it. Fix the TML port.

**Owner and Cadence**: Lead developer; continuous during invariant documentation sprint and
type checker porting.

---

### Risk R-007: Single Developer Bus Factor (Score: 6)

**Detailed Description**

The bus factor of a project is the minimum number of developers who could be hit by a bus (i.e.,
become suddenly unavailable) before the project is unable to continue. TML's bus factor is 1.

For a 24–30 month self-hosting effort, the probability of at least one significant developer
unavailability event is non-trivial. This includes: illness (planned or unplanned), life changes,
loss of motivation after a particularly difficult period, or choosing to prioritize other projects.

The impact is high because:
- The institutional knowledge of TML's type checker is complex and largely undocumented
- The self-hosting effort requires sustained, focused work — interruptions reset momentum
- There is no "fallback developer" who knows the codebase well enough to continue

**Root Cause Analysis**

This is inherent to the project structure. Single-developer projects are common in language
development (the D compiler was primarily Walter Bright, the Lua interpreter is primarily PUC-Rio
with 2–3 core developers). But single-developer projects require extra discipline to ensure
continuity.

**Mitigation Plan (Specific Actions)**

1. Maintain a `CURRENT_STATE.md` in the compiler root that is updated weekly:
   - What is the current focus?
   - What is the most recent working state (git hash)?
   - What are the 3 next tasks?
   - What invariants were discovered this week?
2. Document every session's learnings in the `.rulebook/knowledge/` directory (as per the agent
   workflow rules). These persist across sessions and reduce the cold-start cost of returning after
   an absence.
3. Keep the commit log informative. Each commit message should explain WHY, not just WHAT. A
   future developer (or the same developer returning after 6 months) should be able to understand
   the reasoning from the git log alone.
4. The 9 analysis documents in `docs/analyses/compiler-selfhosting/` are themselves part of the
   mitigation. They represent captured knowledge that does not depend on the developer's memory.

**Contingency if Mitigation Fails**

If the project must be handed off: the C++ compiler remains functional indefinitely. Any partial
self-hosting work is additive value, not wasted work. The analysis documents provide the context
for a successor to understand where the project was and why each decision was made.

**Owner and Cadence**: N/A (inherent); discipline check monthly.

---

### Risk R-009: Test Coverage Insufficient to Catch Regressions (Score: 6)

**Detailed Description**

TML has 1,682 test files and 93.2% function-level coverage of the stdlib. This is excellent
stdlib coverage but does not directly translate to compiler coverage. The question is: for every
behavior the C++ compiler exhibits, does at least one test verify that behavior?

Known gaps in compiler behavior coverage:
- Type inference with multiply-constrained type variables (ambiguous inference resolution)
- Behavior resolution order (when multiple impls could apply)
- Generic const parameter handling
- Borrow checker edge cases with nested lifetimes
- Codegen for async state machines with multiple await points
- Pattern match exhaustiveness for deeply nested enum patterns

Each of these represents a class of programs that the TML compiler must handle identically to the
C++ compiler. Without tests covering these classes, a porting error will not be caught until a
user program triggers it.

**Root Cause Analysis**

The existing tests were written primarily to verify stdlib behavior, not compiler behavior. They
were written incrementally as stdlib modules were developed. They are excellent for ensuring that
`List[T].sort()` works correctly. They are not designed to stress test the type checker's handling
of mutually recursive generic types.

**Mitigation Plan (Specific Actions)**

1. Conduct a "compiler coverage audit" before Phase 6 begins. For each major subsystem:
   - Lexer: test every token type, every escape sequence, every edge case (e.g., Unicode in strings)
   - Parser: test every grammar production, every precedence edge case
   - Type checker: test every inference rule, every error case
   - Borrow checker: test every ownership transfer, every lifetime edge case
   - Codegen: test every ABI pattern, every struct layout, every calling convention
2. Target: 500 new tests specifically for compiler behavior. Budget 2 months for writing them.
3. Write differential tests: programs that should produce the same result regardless of which
   compiler is used (C++ or TML). These are the highest-value regression tests.
4. Use the property-based testing framework (already in TML stdlib) to generate random valid TML
   programs and verify both compilers produce identical output.

**Contingency if Mitigation Fails**

Accept that some regressions will be found by users. Establish a clear bug reporting and fix
process. Track all self-hosted compiler bugs separately from C++ compiler bugs.

**Owner and Cadence**: Lead developer; compiler coverage audit before Phase 6; monthly review.

---

## Section 5: Risk-Adjusted Timeline

### Baseline Timeline (from migration strategy)

| Phase                          | Duration  | C++ LOC Ported | Risk Level |
|--------------------------------|-----------|----------------|------------|
| Prerequisites + invariant docs | 3 months  | 0              | Low        |
| Lexer + Preprocessor + Parser  | 3 months  | 9,557          | Low–Med    |
| HIR builder                    | 3 months  | 10,555         | Medium     |
| Type checker                   | 6 months  | 21,179         | High       |
| Borrow checker                 | 2 months  | 4,971          | Medium     |
| MIR builder (consolidated)     | 3 months  | 12,297         | Medium     |
| MIR passes (core 10)           | 2 months  | ~6,000         | Medium     |
| MIR codegen                    | 2 months  | 1,622          | Low        |
| Query system + CLI             | 2 months  | 4,252          | Medium     |
| Integration + verification     | 2 months  | 0              | High       |
| **Total baseline**             | **28 mo** | **70,433**     |            |

Note: Not all 38K C++ source lines need to be ported. CLI tools, docs, MCP server, and the LLVM
backend remain in C++. The ~70K lines above is the core compiler pipeline.

### Scenario 1: Best Case (no major risks materialize)

Conditions: Type checker drift is minimal (good invariant documentation pays off). Dual MIR path
is consolidated before porting begins. Performance regression is below 1.3x. No LLVM upgrade
during porting.

Timeline adjustment:
- Type checker: 4 months instead of 6 (–2 months)
- Integration: 1 month instead of 2 (–1 month)
- No major setbacks

**Best case total: 25 months**

### Scenario 2: Expected Case (some risks materialize)

Conditions: Type checker drift requires 2 months of debugging beyond the 6 allocated. Dual MIR
path takes 1 extra month to consolidate. One platform support issue (Linux) adds 1 month. Scope
creep adds 1 month.

Timeline adjustment:
- Prerequisites: +1 month (invariant documentation more extensive than expected)
- Type checker: +2 months (drift debugging)
- MIR consolidation: +1 month
- Platform: +1 month
- Scope creep: +1 month

**Expected case total: 34 months**

### Scenario 3: Worst Case (multiple major risks materialize)

Conditions: Type checker has deep undocumented invariants requiring 4 additional months. Codegen
produces ABI mismatches requiring an architectural change in the codegen layer. Developer
unavailability for 2 months during the type checker porting phase. Performance regression exceeds
2x and requires an optimization phase before the compiler can compile itself in reasonable time.

Timeline adjustment:
- Type checker: +4 months
- Codegen ABI rework: +3 months
- Developer unavailability: +2 months (momentum reset)
- Performance optimization: +3 months

**Worst case total: 40 months**

### Summary

| Scenario     | Duration  | Probability |
|--------------|-----------|-------------|
| Best case    | 25 months | 15%         |
| Expected     | 34 months | 60%         |
| Worst case   | 40 months | 25%         |
| Weighted avg | ~33 months|             |

---

## Section 6: Go/No-Go Decision Framework

### Prerequisites for Starting Phase 6

The following conditions must be met before Phase 6 (self-hosting) begins. Each condition has a
status and the action needed to reach "Ready."

| Condition                            | Status   | Required State              | Action                        |
|--------------------------------------|----------|-----------------------------|-------------------------------|
| Phase 7 (Rust parity) complete       | 95% done | 100% done                   | Complete phase7-16 (27 items) |
| Language freeze declared             | No       | Freeze in effect            | Declare freeze; document scope|
| Invariant documentation written      | No       | Docs 08 written             | 4-week sprint                 |
| Differential testing harness built   | No       | Harness running             | 2-week sprint                 |
| Compiler coverage audit complete     | No       | 500+ new tests written      | 2-month sprint                |
| String interner implemented in TML   | No       | `Interner[T]` in stdlib     | 1-week sprint                 |
| Dual MIR path consolidation planned  | No       | Plan written; risk assessed | 1-week analysis               |
| Bootstrap verification procedure     | No       | Procedure documented        | 1-week sprint                 |
| LLVM version pinned                  | No       | Version in CMakeLists.txt   | 1 day                         |
| `CURRENT_STATE.md` template created  | No       | Template in repo             | 1 day                         |

**Go Criteria**: All 10 conditions are in "Required State." Estimated time to reach Go: 4–5 months
of prerequisite work.

**No-Go Criteria** (stop self-hosting work and reassess):
- The differential testing harness reveals >5% type checker divergence after 6 months of porting
- The self-hosted compiler cannot compile itself after 18 months of porting
- Performance regression exceeds 3x and does not improve after 3 months of optimization
- A language design issue is discovered that requires significant TML changes before porting can
  continue

**Pivot Criteria** (change strategy, not abandon):
- Dual MIR path consolidation proves too risky → port only the THIR path, accept limitation
- Type checker porting too slow → consider mechanical translation tool (like cmd/c2go) for the
  type checker specifically
- LLVM API changes too frequently → switch to LLVM C API only (more stable) for the shim

### Decision Review Points

| Milestone                              | Review Question                                |
|----------------------------------------|------------------------------------------------|
| After lexer + parser port (month 6)    | Are we on track? Is differential testing clean?|
| After HIR builder port (month 9)       | Is IR output equivalent? Build time acceptable?|
| After type checker port (month 17)     | Critical milestone. Go/No-Go/Pivot decision.   |
| After MIR builder port (month 22)      | Is bootstrap cycle working?                    |
| After first self-compilation (month 27)| Is Stage 2 equivalent to Stage 1?              |

---

## Appendix A: Risk Dependencies

Some risks are causally linked. Understanding the dependency structure helps prioritize mitigation.

```
R-017 (undocumented invariants)
  └─ causes → R-001 (type checker drift)
                └─ causes → R-009 (insufficient coverage can't catch it)
                              └─ causes → R-004 (bootstrap cycle breaks)

R-003 (dual MIR path)
  └─ amplifies → R-002 (codegen IR differences — double the surface area)

R-007 (bus factor)
  └─ amplifies → R-001, R-017 (if developer unavailable, invariants lost forever)

R-018 (scope creep)
  └─ amplifies → R-008 (feature parity gap grows as new features are added during port)
```

Mitigating R-017 is the highest-leverage action: it reduces R-001, R-009, and R-004 simultaneously.

---

## Appendix B: Comparison with Zig's Risk Profile

Zig's self-hosting effort was the closest analog to TML's situation. For comparison:

| Risk                         | Zig Experience              | TML Assessment                              |
|------------------------------|-----------------------------|---------------------------------------------|
| Type checker complexity      | Severe — took 18 months     | Similar — Hindley-Milner adds complexity    |
| LLVM API stability           | Moderate — upgraded carefully| Same strategy recommended                   |
| Performance regression       | 1.5x for 8 months           | Expected similar: 1.5–2x for 6–12 months    |
| Single developer risk        | 2–3 core developers helped  | TML has 1 — higher risk                     |
| Bootstrap infrastructure     | WASM binary approach        | C++ compiler approach — simpler but heavier |
| Timeline                     | 36 months (longer than est) | Estimate 34 months expected                 |
| Language design discoveries  | comptime refined, errors improved| Expect similar: closures, inference gaps|

Zig's experience suggests: the self-hosting effort will take longer than estimated, will expose
language design gaps, and will ultimately produce a better language and compiler than existed
before. The risk is manageable but not small.
