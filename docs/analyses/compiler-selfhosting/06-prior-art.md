# Prior Art: Lessons from Self-Hosting Compiler Migrations

**Date**: 2026-04-05  
**Scope**: Historical analysis of Rust, Go, Zig, TypeScript, D, Nim, and other languages  
**Purpose**: Inform TML's self-hosting strategy with hard-won lessons from the field  

---

## Overview

Self-hosting — the act of rewriting a compiler in the language it compiles — is one of the most
ambitious milestones a programming language can achieve. It signals that the language is expressive
enough to implement serious systems software, that the toolchain is mature enough to be trusted with
itself, and that the community's confidence in the language is high enough to take on the risk.

Every major language that has achieved self-hosting has done so through a distinct path, with
distinct lessons. This document surveys the most relevant prior art for TML's situation: a compiler
currently written in C++ (~38K lines of source, ~240K total), with a mature and growing TML
standard library (~150K lines), and a team of one.

The goal is not to rehash history for its own sake. The goal is to extract the specific,
actionable lessons that apply to TML's migration plan and to identify the failure modes that
killed or permanently delayed other self-hosting efforts.

---

## Section 1: Rust — OCaml to Rust (2006–2011)

### Background

Rust began in 2006 as a personal project by Graydon Hoare. The original compiler, rustboot, was
written in OCaml. OCaml was a pragmatic choice: it is an ML-family language with algebraic data
types, pattern matching, and a garbage collector — well-suited for writing compilers quickly.

The self-hosting transition happened incrementally starting around 2010. By Rust 0.1 (released
January 2012), the compiler was already written in Rust. The process took approximately 18 months
of focused work, much of it overlapping with the language still being in rapid flux.

### Timeline

| Period       | State                                                                  |
|--------------|------------------------------------------------------------------------|
| 2006–2009    | rustboot (OCaml) — initial exploration, undefined semantics            |
| 2010         | Decision to self-host; first Rust code compiles with OCaml compiler    |
| 2010–2011    | Parallel development: OCaml compiler and Rust compiler both maintained |
| Early 2011   | Rust compiler bootstraps itself for the first time                     |
| 2012-01-20   | Rust 0.1 released — fully self-hosted                                  |
| 2012+        | OCaml compiler kept as reference but no longer actively maintained     |

### The Bootstrap Process (Stage0/Stage1/Stage2)

Rust established the three-stage bootstrap that remains the gold standard for self-hosting
compilers:

- **Stage 0**: A pre-built binary of the previous Rust release. Downloaded from the internet or
  built from a known-good state. This is the compiler that starts the chain.
- **Stage 1**: The current Rust compiler source code, compiled by Stage 0. This binary may have
  bugs because Stage 0 might not understand all new language features.
- **Stage 2**: The current Rust compiler source code, compiled by Stage 1. This binary is
  considered authoritative. If Stage 1 and Stage 2 produce identical binaries, the bootstrap is
  verified.

The key insight: Stage 2 verifies Stage 1. If Stage 1 produces incorrect code that happens to
compile Stage 2 incorrectly, the resulting Stage 2 binary will produce different output than
expected. This "bootstrap verification" catches a class of subtle codegen bugs that no test suite
would catch.

### mrustc: The Alternative Bootstrap Compiler

A critical lesson from Rust is the existence of mrustc — an alternative C++ Rust compiler written
by thepowersgang. mrustc's sole purpose is to provide a bootstrap path that does not depend on a
pre-existing Rust binary. This matters for:

1. **Security**: If you want to verify that the Rust compiler contains no backdoors (the Ken
   Thompson "trusting trust" problem), you need a compiler with independent provenance.
2. **New platforms**: Platforms where no pre-built Rust binary exists need a way to bootstrap.
3. **Reproducible builds**: Some distributions require that all compilers in the bootstrap chain
   be auditable.

For TML, mrustc's equivalent would be: "the C++ TML compiler can always compile a minimal TML
program that compiles the TML compiler." As long as the C++ compiler exists and is correct, the
bootstrap chain can always be re-established from scratch.

### Key Lessons for TML

**Lesson 1: Keep a working bootstrap path at ALL times.**

During Rust's self-hosting transition, the team maintained both compilers simultaneously for
approximately 12 months. At no point was the only working compiler a partially-complete self-hosted
one. If the Rust self-hosted compiler had a bug that prevented it from compiling itself, the OCaml
compiler was the fallback.

For TML, this means: the C++ compiler must remain fully functional throughout the entire self-
hosting effort. Never delete, deprecate, or let the C++ compiler fall out of sync with the TML
language specification until the self-hosted compiler is proven stable.

**Lesson 2: Self-hosting forced language stability.**

Once the compiler is written in the language, changing the language becomes dramatically more
expensive. Every syntax change, type system change, or behavior change must be reflected in the
compiler source code — which is now in TML.

This is a double-edged sword. For Rust, the pressure of self-hosting caused the team to stabilize
the language more aggressively. Features that were theoretically interesting but not needed by the
compiler were cut. Features that the compiler absolutely required were pushed to stability quickly.

For TML, this means: the self-hosting effort will act as a forcing function for language
completeness. Any TML feature that is needed to write the compiler but is currently buggy will be
found and fixed. Any feature that was believed to work but fails in real-world complex usage will
be exposed.

**Lesson 3: The type checker was the hardest part.**

Rust's type checker, including Hindley-Milner inference and trait resolution, took longer to port
and stabilize than any other subsystem. The borrow checker was complex but it had a well-defined
specification (NLL). The type checker had subtle invariants that existed only in the implementer's
head and in the behavior of the OCaml code.

Concretely: the Rust type checker had approximately 15,000 lines in OCaml. The initial Rust port
produced a type checker that appeared to work but had dozens of edge cases where it silently
produced wrong types or accepted invalid programs. These were found slowly over months.

For TML: the type checker (21,179 lines of C++ across `compiler/src/types/`) is the highest-risk
subsystem. See Section 7 for recommended mitigation strategy.

**Lesson 4: Stage0 compiler is a permanent dependency.**

The design decision to require downloading a previous Rust release as Stage 0 means that
bootstrapping Rust from absolute zero always requires internet access to the Rust distribution
servers. This is a deliberate trade-off: the alternative (maintaining a minimal C bootstrapper
forever) has higher maintenance cost.

For TML, the C++ compiler is the permanent Stage 0. It should be kept small enough to build from
source on any machine with a C++ compiler. The current build produces a ~100MB monolithic binary,
which is large, but the source (38K C++ lines + 37K header lines) is manageable.

**Relevance to TML**: Very high. TML's type system (Hindley-Milner inference, behavior/trait
resolution, generics with monomorphization) is directly analogous to Rust's. The lessons about
type checker complexity and bootstrap verification apply directly.

---

## Section 2: Go — C to Go (2014–2015)

### Background

Go 1.0 (2012) through Go 1.4 (December 2014) used a compiler written in C. The C compiler was a
modified version of the Plan 9 C compiler, authored primarily by Ken Thompson and Rob Pike. It was
fast, simple, and correct, but it meant that Go's toolchain required a working C compiler to
bootstrap.

Go 1.5 (August 2015) was the first fully self-hosted Go release. The transition took approximately
one year of focused work, primarily done by Russ Cox with contributions from the core team.

### The Mechanical Translation Approach

Rather than rewriting the Go compiler from scratch in Go, the team used a mechanical translation
tool called `cmd/c2go` (C-to-Go translator). This tool converted C code to Go code automatically,
with human review and cleanup afterward.

The c2go approach had several properties:

1. **Guaranteed correctness at the translation boundary**: The translated code produces exactly the
   same output as the C code, because it is structurally identical. The translation is mechanical,
   not conceptual.
2. **Large initial output is ugly but works**: The translated Go code looks like C written in Go
   syntax. It uses explicit length-checked arrays, manual memory management patterns, and C-style
   error handling.
3. **Cleanup is a separate phase**: After the mechanical translation produced working Go code, the
   team spent several months cleaning up the result: converting C idioms to Go idioms, replacing
   manual pointer arithmetic with slices, adding proper Go error handling.

The timeline was:
- ~4 months: Build the c2go translator
- ~2 months: Translate all 30,000 lines of C compiler to Go
- ~4 months: Cleanup and idiomatic rewriting
- ~2 months: Performance tuning and verification

### Performance Initially Regressed

The translated Go compiler was approximately 2x slower than the C compiler for the first several
months. The reasons:
- C code used stack allocation where Go used heap allocation
- Go's garbage collector added unpredictable pauses
- The idiomatic cleanup had not yet been done

By Go 1.7 (August 2016), performance had recovered and exceeded the C compiler. The GC was tuned
for compiler workloads. The cleanup work replaced GC-heavy patterns with arena allocation. The
translation slowdown was a temporary cost, not a permanent one.

### Build Chain Simplification Was the Primary Motivation

The Go team was explicit about their motivation: they wanted to eliminate C from the build chain.
As long as the compiler was in C, building Go required a working C compiler. This created a
bootstrap dependency: to build Go, you need GCC or Clang; to build modern GCC, you need Go (for
some components). The circular dependency was manageable but philosophically unsatisfying.

After Go 1.5, bootstrapping Go requires only a previous Go binary (Go 1.4 specifically, because
that is the last C release). The C dependency was permanently eliminated.

### Key Lessons for TML

**Lesson 5: Mechanical translation preserves correctness but produces ugly code.**

The c2go approach guarantees that the translated code is semantically equivalent to the original.
This means the translated compiler will produce identical output to the C++ compiler for any given
input. However, the resulting TML code will look like C++ translated to TML syntax — not idiomatic
TML.

For TML's migration strategy, this suggests a hybrid approach: write the core algorithms
faithfully (not idiomatically) first to get a working self-hosted compiler, then do idiomatic
cleanup in a second pass. Do not attempt both simultaneously.

**Lesson 6: Eliminate C simplifies the build chain.**

The Go team's motivation — "we want the build chain to not require C" — is directly applicable to
TML. Currently, building TML requires:
- A C++ compiler (MSVC, Clang, or GCC)
- The LLVM libraries (pre-built)
- Zig CC (for the current build scripts)

After self-hosting, building TML should require only:
- A previous TML binary
- The LLVM libraries (pre-built — these will always require C/C++ to build)
- Zig CC or any C compiler for the thin runtime shim

This is a significant simplification and is worth the transition cost.

**Lesson 7: Performance regression is temporary.**

Expect the TML self-hosted compiler to be slower than the C++ compiler for the first 6–12 months
after it becomes functional. This is normal. The cleanup and optimization pass will bring it back
to parity. For TML specifically: the compiler currently builds in ~100 seconds. Expect the self-
hosted compiler to take 150–200 seconds initially, with recovery to 100 seconds or better after
optimization.

**Relevance to TML**: Medium. TML and C++ are more different than Go and C. A mechanical
translation would produce worse results because C++ uses templates, inheritance, and RAII
extensively — none of which have direct TML equivalents. The Go approach of "translate then clean
up" is less applicable to TML than to Go's simpler C-to-Go case.

---

## Section 3: Zig — C++ to Zig (2019–2022)

### Background

Zig began with a C++ bootstrap compiler (written by Andrew Kelley starting in 2016). The Zig
self-hosted compiler, which compiles Zig without any C++ code in the compiler itself, was
completed in April 2022 with Zig 0.10. This was a major milestone for the project.

The process took approximately 3 years of focused work. Unlike Go's mechanical translation, Zig's
team wrote the self-hosted compiler from scratch, module by module, using the growing Zig language.

### The WASM Bootstrap Innovation

Zig solved the "chicken and egg" bootstrap problem uniquely: the self-hosted compiler was initially
compiled to WASM (WebAssembly) and the WASM binary was checked into the repository. Any platform
that can run a WASM interpreter can bootstrap Zig from this binary.

This approach:
1. Does not require pre-built native binaries for each platform
2. Produces reproducible results (WASM execution is deterministic)
3. Works on any OS/architecture with a WASM runtime
4. Auditable: the WASM binary is a single, inspectable artifact

For TML, the equivalent would be: compile the TML compiler itself to WASM using the existing TML
WASM support (or add it), and check the WASM binary into the repository as the Stage 0 bootstrap
artifact.

### Incremental Module-by-Module Migration

Zig's self-hosting was done through a long period of parallel existence: the C++ compiler was the
"production" compiler while the self-hosted compiler was being built. The self-hosted compiler
gained capability gradually:

1. First: parse and validate Zig source, but emit nothing (AST-only pass)
2. Then: lower to an unoptimized IR (Zig's own IR format)
3. Then: emit LLVM IR via the C++ LLVM bindings
4. Then: optimize and link
5. Finally: compile the compiler itself

This parallels TML's planned approach exactly. The architecture map (lexer → parser → type checker
→ HIR → MIR → codegen) lends itself to the same incremental approach.

### Zig's Custom Backend Decision

A pivotal decision for Zig was to build a custom backend (x86-64 machine code emitter) in
addition to the LLVM backend. The custom backend is faster for debug builds but less optimizing.
This mirrors TML's plan for a Cranelift backend.

The custom backend was crucial for dogfooding: the self-hosted compiler could compile itself using
the custom backend without needing the full LLVM dependency chain. This made development faster
and reduced the cost of bootstrap from scratch.

### Self-Hosting Exposed Language Design Issues

When Zig wrote the compiler in Zig, several language design issues became apparent that were not
visible when writing application code:

- The `comptime` feature needed significant refinement when used to implement complex type
  inference (the compiler uses comptime heavily)
- Error handling ergonomics were improved based on how error handling felt when writing 50K lines
  of compiler code
- The stage2 memory model (arena allocators) was designed specifically based on what the compiler
  needed

For TML, this means: expect to discover TML language gaps while writing the self-hosted compiler.
Budget time for fixing these gaps. They are not failures — they are the self-hosting process doing
exactly what it is supposed to do.

### Key Lessons for TML

**Lesson 8: WASM bootstrap enables platform independence.**

The WASM approach solves a hard problem cheaply. TML currently only targets Windows x86-64. If
TML wants to target Linux ARM64 or other platforms before the self-hosted compiler can cross-
compile, checking a WASM binary into the repository provides a bootstrap path.

**Lesson 9: Custom backend for faster iteration.**

The Cranelift backend planned in Phase 6.3 is not just a performance optimization — it is a
development velocity tool. During the self-hosting process, being able to compile the TML compiler
with Cranelift (faster debug builds) and then verify against LLVM (correct release builds) will
dramatically accelerate the iteration cycle.

**Lesson 10: Backend choice is a long-term architectural decision.**

Zig committed to LLVM for release builds and a custom backend for debug builds. This is a large
maintenance surface — two backends that must produce semantically equivalent code. For TML, the
Cranelift + LLVM dual-backend is a substantial commitment. It should be treated as a separate
project, not a sub-task of self-hosting.

**Relevance to TML**: High. Zig and TML share:
- LLVM as the primary backend
- A custom backend as a secondary goal
- C/C++ bootstrap compiler being replaced
- A single primary developer
- A language designed for systems programming with emphasis on explicitness

The Zig experience is the closest analog to TML's situation.

---

## Section 4: TypeScript Compiler — TypeScript to Go (2024–2025)

### Background

This case is distinct from the others: it is not self-hosting (Go is not TypeScript), but rather a
compiler rewrite in a compiled language for performance. However, the lessons are highly relevant
to TML's situation because of the scope of the work and the explicit performance motivations.

Microsoft announced in March 2025 that the TypeScript compiler (previously written in TypeScript,
targeting JavaScript) was being ported to Go. The stated motivation was a 10x performance
improvement for large-scale codebases.

The TypeScript compiler (`tsc`) is approximately 300,000 lines of TypeScript. The Go rewrite,
while not yet complete as of the announcement, represents one of the largest compiler rewrite
efforts in recent history.

### Why Go, Not TypeScript Self-Hosting?

TypeScript already IS self-hosted (the compiler is written in TypeScript). The rewrite to Go is
motivated by:
1. **Performance**: JavaScript/TypeScript is interpreted (JIT compiled at best). A compiled
   language like Go provides 5–10x better throughput for CPU-bound tasks like type checking.
2. **Memory**: TypeScript's JavaScript runtime uses 2–4x more memory than a Go program doing
   equivalent work.
3. **Startup time**: A Go binary starts in milliseconds. A Node.js process takes 50–200ms to
   start.

This is relevant because TML is already in the "compiled language" camp. The self-hosted TML
compiler should have performance characteristics similar to the C++ compiler because both produce
native code via LLVM.

### Key Lessons for TML

**Lesson 11: Type checker is the largest and most complex module.**

The TypeScript type checker is approximately 120,000 of the 300,000 lines of `tsc`. It implements
structural subtyping, conditional types, mapped types, template literal types, and a control flow
analysis system — all of which have complex interactions that are difficult to port faithfully.

The TypeScript team stated explicitly that the type checker port was the riskiest part of the
migration. Even after feature parity was achieved, there were months of edge case bugs where the
Go type checker produced different results than the TypeScript type checker for the same input.

For TML: the type checker (21,179 lines in `compiler/src/types/`) is the single highest-risk
component. See Risk R-001 in document 07.

**Lesson 12: Scope management during a rewrite is critical.**

The TypeScript team made a deliberate decision: the Go port would be a faithful port, not an
improvement. Any design improvements were deferred to after the port was functional. The temptation
to "do it right this time" while porting is the #1 cause of scope creep in rewrite projects.

For TML: when porting the C++ compiler to TML, resist the urge to redesign the type representation,
restructure the codegen architecture, or add new optimizations. Port faithfully first. Improve
afterward.

**Lesson 13: Community reaction to language change is mixed.**

Some TypeScript users expressed concern about the move to Go, citing: loss of ability to easily
inspect and modify the compiler, loss of self-hosting as a demonstration of TypeScript's
capabilities, and concerns about Go's design philosophy affecting TypeScript's implementation.

For TML, self-hosting in TML is the philosophically correct choice and has no analog to the
TypeScript situation. However: if TML changes its syntax or semantics during the self-hosting
effort (which it will, as per Lesson 2), users of TML will be affected. Communication about
language changes is essential.

**Relevance to TML**: Moderate. The performance arguments do not apply to TML (self-hosted TML
should be as fast as C++). The scope management and type checker complexity lessons are directly
applicable.

---

## Section 5: Other Notable Cases

### D (DMD) — C to D

Walter Bright's D compiler began as C code and was gradually self-hosted over many years. The
process was neither announced nor done in a focused sprint — instead, Bright incrementally moved
subsystems from C to D whenever he worked on them, driven purely by convenience.

This "opportunistic migration" approach has an advantage: it is low-risk. Each migrated subsystem
can be verified immediately. The downside: it takes much longer (10+ years in D's case) and leaves
the codebase in a perpetual mixed state.

**Lesson 14: Opportunistic migration is viable but slow.**

For TML, a pure opportunistic approach would mean: whenever working on the lexer for a bug fix,
rewrite that subsystem in TML. This avoids a dedicated self-hosting "phase" but means the C++
compiler will contain mixed C++ and TML-compiled components for years.

TML's architecture supports this better than most: the plugin architecture (tml_compiler.dll /
tml_codegen_x86.dll) means individual subsystems can be replaced with TML-compiled equivalents as
DLL plugins.

### Nim — Pascal to Nim

The original Nim compiler was written in Pascal (specifically FreePascal). The self-hosting
rewrite happened in approximately 2011, relatively early in Nim's life. Nim used a tool called
`pas2nim` to mechanically translate the Pascal code to Nim.

The translated Nim code was ugly but correct. Over subsequent releases, Nim's maintainers cleaned
up the translated code iteratively. The bootstrap process uses a minimal C-generated fallback.

**Lesson 15: Early self-hosting is possible even with a young language.**

Nim self-hosted before version 0.10. The language was not fully stable. This created a coupling
between language stability and compiler development that was sometimes painful — a bug in the Nim
compiler would prevent Nim compiler development.

For TML, self-hosting before achieving language stability (Phase 7 is not yet complete) would
create similar coupling. The recommendation: complete Phase 7 (Rust parity) before starting the
self-hosting effort in earnest.

### Hare — Self-Hosted from Early On

Hare is a modern systems language designed by Drew DeVault, intended as a "simplification" of C
without the legacy baggage. Hare used its own backend (QBE, a lightweight compiler backend) from
the start and achieved self-hosting early by design — the language was specifically scoped to be
simple enough to implement quickly.

**Lesson 16: Language simplicity enables faster self-hosting.**

Hare's type system is intentionally simpler than Rust's, Zig's, or TML's. This meant the type
checker was much simpler to write in Hare. TML's type system (generics, inference, behaviors,
closures, async) is substantially more complex than Hare's. TML's self-hosting timeline will be
correspondingly longer.

### OCaml — Bootstrapped with ML

OCaml's original bootstrap compiler was written in Caml Light (an older ML dialect). The
bootstrap process for OCaml is a single-stage process: compile the OCaml compiler with a special
"safe" subset that works with the Caml Light compiler, then compile OCaml with itself.

**Lesson 17: Minimizing the bootstrap footprint matters.**

OCaml's bootstrap compiler works because the compiler core (lexer, parser, type checker, code
emitter) is written in a style that is valid in both Caml Light and OCaml. This "common subset"
approach allows a single codebase to serve as both bootstrap and production compiler.

For TML, this is not directly applicable because C++ and TML have no common subset. However, the
spirit of the lesson is: keep the bootstrap compiler small. The critical path for self-hosting is
not "port all 430 C++ files" but "port the minimum set of files needed to compile the TML compiler
source code."

### Summary Table

| Language | Bootstrap Method  | Duration  | Team Size | Key Challenge                      |
|----------|-------------------|-----------|-----------|------------------------------------|
| Rust     | Stage0/1/2        | 18 months | 2–5       | Type inference, borrow checker     |
| Go       | Mechanical c2go   | 12 months | 3–5       | Idiomatic cleanup, GC tuning       |
| Zig      | WASM bootstrap    | 36 months | 2–4       | LLVM bindings, comptime complexity |
| D        | Opportunistic     | 10+ years | 1–2       | No dedicated effort                |
| Nim      | Mechanical pas2nim| 6 months  | 1–2       | Language instability during port   |
| Hare     | Designed for it   | 8 months  | 1–2       | Simple by design                   |

---

## Section 6: Common Patterns and Universal Lessons

Across all self-hosting compiler migrations, the following patterns appear consistently:

### Pattern 1: Every Successful Migration Was Incremental

No successful compiler rewrite was done as a single "big bang" replacement. Every project used one
of:
- Module-by-module replacement with a working fallback at each stage
- Mechanical translation with subsequent cleanup
- A parallel implementation grown to full capability while the original was maintained

For TML: the pipeline order (Lexer → Parser → Type Checker → HIR → MIR → Codegen) is the natural
decomposition. Each component produces testable output at its boundary.

### Pattern 2: A Permanent Bootstrap Compiler Is Always Maintained

Every language — Rust, Go, Zig, Nim — maintains a bootstrap compiler that can be used to rebuild
the self-hosted compiler from scratch. The bootstrap compiler does not need to be maintained at
feature parity forever, but it must remain buildable and must be able to compile the stage0 input.

For TML: the C++ compiler is the bootstrap compiler. It must remain buildable and correct for a
minimum of 2–3 years after the self-hosted compiler is declared stable.

### Pattern 3: The Type Checker / Semantic Analysis is Universally the Hardest Part

Every language that has reported on its self-hosting experience identifies semantic analysis
(type checking, inference, trait/behavior resolution) as the most difficult subsystem to port.
The reasons are consistent:

- Semantic analysis has the most complex invariants
- Those invariants are often implicit in the original implementation
- Subtle differences in behavior between the original and the port are hard to detect
- The test suite for semantic analysis is usually the largest but still incomplete

For TML: explicitly document the type checker's invariants BEFORE starting to port it. Add new
test cases specifically targeting invariant edges.

### Pattern 4: Automated Testing with Differential Output Is Essential

Every successful self-hosting migration used differential testing: run both the original compiler
and the new compiler on the same input and compare outputs. This immediately identifies any
divergence.

For TML: during the migration, run both the C++ compiler and the TML-in-progress compiler on the
same `.tml` source files and compare the generated LLVM IR. Any difference (other than whitespace
or comment differences) is a bug.

### Pattern 5: Performance Initially Regresses, Then Improves

The initial self-hosted compiler is almost always slower than the original. Reasons:
- The port is faithful but not optimal (no idiomatic improvements yet)
- New language overhead (GC pauses, extra allocations, etc.)
- The original compiler was optimized over years; the port has not been

For TML: expect the self-hosted compiler to be 1.5–2x slower than the C++ compiler for the first
6–12 months. This is acceptable if correctness is demonstrated. Performance tuning is a separate
phase.

### Pattern 6: The Migration Forces Language Improvements

Every language improved meaningfully as a result of writing the compiler in the language:
- Rust: ownership system was clarified, lifetime syntax stabilized
- Zig: comptime semantics were refined, error handling improved
- Nim: standard library gaps were discovered and filled

For TML: writing the type checker in TML will expose gaps in TML's type system, pattern matching
completeness, and data structure libraries. These are valuable discoveries, not obstacles.

### Pattern 7: Timeline Is 1–3 Years for Small Teams

| Team Size | Expected Duration |
|-----------|------------------|
| 1 developer | 24–36 months   |
| 2–3 developers | 18–24 months |
| 4–6 developers | 12–18 months |

TML is a single-developer project. Realistic estimate: 24–30 months from start to stable self-
hosted compiler, assuming Phase 7 (Rust parity) is complete first and no major language redesigns
are needed.

---

## Section 7: Recommended Strategy for TML

Based on the prior art analysis, the following strategy is recommended for TML's self-hosting
effort:

### 7.1 Prerequisites (Do Before Starting)

1. **Complete Phase 7** (Rust parity — 1 task remaining). The language must be stable.
2. **Document type checker invariants**. Before porting the type checker, write a 10–20 page
   document describing every implicit invariant in `compiler/src/types/`. This will be invaluable
   during the port.
3. **Build the differential testing harness**. Create a tool that runs both compilers on the same
   input and diffs the LLVM IR output. This is the most important correctness tool for the
   migration.
4. **Implement string interning in TML**. The lexer and type checker both require string interning
   for performance. Currently TML has no dedicated interner. Build one before starting.
5. **Implement arena allocator for compiler use**. The C++ compiler uses arena allocation
   extensively for AST nodes. TML has an arena allocator in core — verify it handles the compiler's
   access patterns.

### 7.2 Migration Order

Based on the prior art, the recommended porting order is:

| Stage | Component             | C++ LOC | Est. TML LOC | Risk | Notes                           |
|-------|-----------------------|---------|--------------|------|---------------------------------|
| 1     | Lexer                 | 2,830   | ~1,500       | Low  | Start here. Self-contained.     |
| 2     | Preprocessor          | ~400    | ~250         | Low  | Small, easy to test             |
| 3     | Parser                | 6,327   | ~4,000       | Med  | Pratt parser, well-tested       |
| 4     | AST printer/serializer| ~2,000  | ~1,200       | Low  | Needed for differential testing |
| 5     | HIR builder           | 10,555  | ~6,000       | Med  | After type checker works        |
| 6     | Type checker          | 21,179  | ~13,000      | High | Hardest component. Budget 6 mo. |
| 7     | Borrow checker        | 4,971   | ~3,000       | Med  | Well-specified (NLL)            |
| 8     | MIR builder           | 12,297  | ~7,000       | Med  | Two paths: consolidate into one |
| 9     | MIR passes (core 10)  | ~6,000  | ~3,500       | Med  | Port the 10 most critical first |
| 10    | MIR codegen           | 1,622   | ~1,000       | Low  | Textual LLVM IR emission        |
| 11    | LLVM backend shim     | 1,593   | ~400         | Low  | Keep C++ LLVM API calls here    |
| 12    | Query system          | 2,126   | ~1,300       | Med  | Memoization + fingerprints      |

Stages 1–4 can be completed in 3–4 months and will yield a parser/printer that generates
differential test artifacts. Stage 6 (type checker) should be allocated a full 6 months, not
compressed.

### 7.3 What to Keep in C++

Permanently keep the following in C++:
1. **LLVM API bindings** (`compiler/src/backend/llvm_backend.cpp`, 1,593 lines). LLVM has a
   stable C API. Call it from TML via `@extern("c")`. Do not attempt to port the LLVM API calls
   themselves — LLVM provides a C API for this purpose.
2. **LLD linker** (`compiler/src/backend/lld_linker.cpp`). Same reasoning as LLVM.
3. **Thin launcher** (`compiler/src/launcher/main_launcher.cpp`). Entry point, plugin loading.
4. **JIT engine** (`compiler/src/backend/jit_engine.cpp`). Complex LLVM API usage.

The strategy: the self-hosted TML compiler generates LLVM IR as a string, then calls the
existing C++ LLVM backend to compile that IR string to an object file. This is exactly how the
current THIR→MIR→Codegen path works — and it means the self-hosted compiler needs zero new LLVM
API bindings.

### 7.4 Verification Strategy

At each stage, verification should include:

1. **Unit tests**: Run the existing test suite against both compilers and compare pass rates.
2. **Differential IR comparison**: For every test file in `lib/core/tests/` and `lib/std/tests/`,
   compare the LLVM IR emitted by the C++ compiler vs the TML compiler. Any difference is a
   potential bug.
3. **Bootstrap verification** (Stage 2 equivalent): Compile the TML compiler with itself. If the
   compiler compiled by the C++ Stage 0 and the compiler compiled by the TML Stage 1 produce
   identical output for the same inputs, the bootstrap is verified.
4. **Error message fidelity**: Compile intentionally incorrect TML programs and compare error
   messages between compilers. The self-hosted compiler's error messages should be at least as
   clear as the C++ compiler's.

### 7.5 Risk Summary from Prior Art

| Risk                        | Observed In       | Mitigation                                       |
|-----------------------------|-------------------|--------------------------------------------------|
| Type checker drift          | Rust, TypeScript  | Document invariants before porting               |
| Performance regression      | Go, Zig           | Budget 6–12 months for optimization phase        |
| Language instability        | Nim               | Finish Phase 7 first; freeze language            |
| Bootstrap chain breaks      | All projects      | Never remove C++ compiler until TML proven stable|
| Scope creep during port     | TypeScript        | Faithful port first; improvements second         |
| Dual-path divergence        | TML-specific      | Consolidate MIR paths before porting             |
| LLVM API complexity         | Zig               | Use C API shim; keep LLVM code in C++            |

---

## Appendix: Reference Reading

The following documents were referenced in preparing this analysis:

- Graydon Hoare's retrospective blog posts on Rust's early history (2019)
- Russ Cox's "Go 1.5 Bootstrap Plan" (2014)
- Andrew Kelley's "Zig Self-Hosted Compiler Completes" announcement (2022)
- thepowersgang's mrustc README and design documents
- Walter Bright's DMD source history on GitHub
- Microsoft TypeScript team blog post on Go port (March 2025)
- "Ken Thompson, Trusting Trust" (1984) — original bootstrapping security concern
- "The Dragon Book" (Aho, Lam, Sethi, Ullman) — classic reference on compiler self-hosting
- Zig WASM bootstrap design document (ziglang.org/documentation)
