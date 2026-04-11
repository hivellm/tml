# Self-Hosting Migration Strategy

**Date**: 2026-04-05
**Status**: Planned — Phase 6 (after Phase 7 complete)
**Companion Documents**: 00-executive-summary.md, 01-compiler-inventory.md, 02-stdlib-readiness.md, 03-language-gaps.md, 06-prior-art.md, 07-risk-matrix.md

---

## Table of Contents

1. [Bootstrap Architecture](#1-bootstrap-architecture)
2. [Phase 0: Pre-Work](#2-phase-0-pre-work-3-4-months)
3. [Phase 1: Lexer and Parser](#3-phase-1-lexer-and-parser-3-4-months)
4. [Phase 2: Type Checker](#4-phase-2-type-checker-6-9-months)
5. [Phase 3: HIR, THIR, and MIR](#5-phase-3-hir-thir-and-mir-4-5-months)
6. [Phase 4: MIR Codegen](#6-phase-4-mir-codegen-4-6-months)
7. [Phase 5: Query System, CLI, and Tooling](#7-phase-5-query-system-cli-and-tooling-2-3-months)
8. [Testing Strategy](#8-testing-strategy)
9. [Timeline Summary](#9-timeline-summary)
10. [Go/No-Go Criteria](#10-gono-go-criteria)
11. [Resource Requirements](#11-resource-requirements)

---

## 1. Bootstrap Architecture

### 1.1 The Three-Stage Model

Self-hosting a compiler requires three stages. This is the same model used by Rust, Go, Zig, and every other self-hosted compiler. Each stage is a complete, working compiler. The goal is to arrive at Stage 2 and verify it produces identical output to Stage 1.

```
Stage 0: C++ Compiler (tml.exe, current)
  |
  |  compiles
  v
Stage 1: TML Compiler (tml-s1.exe)
    Written in TML, compiled by Stage 0.
    Produces valid executables.
    Used to compile Stage 2.
  |
  |  compiles
  v
Stage 2: TML Compiler (tml-s2.exe)
    Written in the same TML source as Stage 1.
    Compiled by Stage 1 instead of Stage 0.
    Must produce bit-identical output to Stage 1
    when compiling the same input.
  |
  |  verified
  v
Bootstrap Verification
    Stage 1 output == Stage 2 output (for identical input)
    If equal: self-hosting is confirmed.
    If not equal: a codegen bug exists in Stage 1.
```

Stage 0 (the C++ compiler) is never deleted. It serves as the permanent bootstrap baseline, the oracle for differential testing, and the fallback if a regression breaks Stage 1. This mirrors how GCC, Clang, and rustc all maintain their bootstrap compilers indefinitely.

### 1.2 The Hybrid Pipeline Period

The self-hosting effort does not require a "big bang" rewrite where all 158K lines of C++ are replaced at once. Instead, the pipeline is replaced one subsystem at a time using a serialization boundary.

Each subsystem produces a well-defined output:
- Lexer produces tokens
- Parser produces an AST
- Type checker produces a typed symbol table and annotated AST
- HIR builder produces HirModule
- MIR builder produces mir::Module
- Codegen produces LLVM IR text (a plain string)

At each phase boundary, the output can be serialized to a file, read by the next subsystem, and verified independently. This means a TML-written lexer can feed into the C++ parser on day one — the entire downstream pipeline does not need to change until the parser is also ready.

```
[TML Lexer] --> tokens.bin --> [C++ Parser] --> ... --> [C++ Codegen] --> .exe
```

This hybrid mode lets each phase be developed, tested, and integrated independently. The C++ compiler never goes offline. At no point is the project in a state where it cannot compile TML source code.

### 1.3 The Serialization Boundary

The MIR serializer already exists (`compiler/src/mir/serializer/`): `binary_writer.cpp`, `binary_reader.cpp`, `text_writer.cpp`, `text_reader.cpp`. This infrastructure enables the hybrid pipeline at the MIR level today, without additional infrastructure work.

For earlier phases (AST, TypeEnv, HIR), serializers must be written as part of Phase 0. These are investment pieces: they serve as both the integration glue for the hybrid pipeline and the differential testing harness for verifying correctness.

---

## 2. Phase 0: Pre-Work (3-4 months)

Phase 0 is not optional. Skipping it will make every subsequent phase harder. The goal of Phase 0 is to establish the infrastructure that makes all later phases safe, testable, and reversible.

### 2.1 Consolidate MIR Paths

**Current state**: Two MIR builder paths exist in parallel.
- `hir_mir_builder.cpp` + `builder/hir_expr.cpp` + `builder/hir_expr_control.cpp` (legacy HIR→MIR)
- `thir_mir_builder.cpp` + `thir_mir_builder_expr.cpp` + `thir_mir_builder_control.cpp` (new THIR→MIR)

**Problem**: Any fix to MIR building must be applied twice. Any new MIR instruction must be handled in two code paths. When porting MIR building to TML, porting two divergent paths doubles the work and the risk of subtle behavioral divergence.

**Action**: Retire `hir_mir_builder.cpp` entirely. Route all compilation through the THIR→MIR path. This requires verifying that the THIR path handles every code pattern the HIR path handles — run the full test suite with HIR path disabled to find gaps, fix them in the THIR path, then delete the HIR path.

**Estimated effort**: 4-6 weeks.
**Exit criterion**: Full test suite passes with `hir_mir_builder.cpp` removed from the build.

### 2.2 Build std::intern

**Problem**: The compiler extensively interns strings (type names, function names, identifiers). The C++ version uses `std::unordered_map<std::string, uint32_t>` for intern tables. The TML compiler needs an equivalent that is efficient and avoids allocating duplicate strings.

**Design**: A 200-line TML module `lib/core/src/data/intern.tml`:

```tml
type InternTable {
    strings: List[Str],
    index: HashMap[Str, I32],
}

type InternId(I32)

impl InternTable {
    func new() -> InternTable { ... }
    func intern(self: mut ref InternTable, s: Str) -> InternId { ... }
    func get(self: ref InternTable, id: InternId) -> Str { ... }
    func len(self: ref InternTable) -> I32 { ... }
}
```

The `InternId` newtype wrapper prevents accidentally passing an intern ID as a raw integer to a function that expects a different kind of ID (a common bug in C++ compiler internals where everything is `uint32_t`).

**Estimated effort**: 3-5 days including tests.
**Exit criterion**: `mcp__tml__test suite="core/intern"` passes.

### 2.3 Document Type Checker Invariants

The type checker is the highest-risk phase (R-001, score 9 in the risk matrix). Its behavior is partially documented in the spec (`docs/specs/`) but many invariants live only in comments and implementation details in `compiler/src/types/checker*.cpp`.

Before porting begins, a 50-100 page invariant document must be written covering:
- The four registration phases (register → imports → impl resolution → body checking) and what state is mutable in each
- How Hindley-Milner inference state is threaded through the checker
- How behavior dispatch resolution handles generic constraints and impl overlap
- All 23 currently-known error codes (T001–T023) and the exact conditions that trigger each
- The 15 edge cases in `when`-expression exhaustiveness checking

This document is written by reading the C++ implementation and converting its implicit invariants into explicit prose. It becomes the specification that the TML type checker must satisfy.

**Estimated effort**: 3-4 weeks.
**Exit criterion**: Document reviewed, 200+ edge-case tests written that exercise every documented invariant.

### 2.4 Build IR-Diff Testing Infrastructure

The most critical infrastructure for the codegen phase (Phase 4) is a tool that compares LLVM IR output from two compilers on the same input and reports divergences.

Naive text comparison is too strict: LLVM IR contains generated names like `%0`, `%1` that vary between compilations even when the logic is identical. The IR-diff tool must normalize:
- Register names (`%0`, `%1`) → ordinal positions
- Basic block labels (`entry`, `bb1`) → ordinal positions
- Function names with mangled generics → canonical forms
- Debug metadata (line numbers, file references) → stripped

After normalization, a semantic-level diff reports:
- Functions present in C++ output but absent from TML output (missing functions)
- Functions present in TML output but absent from C++ output (spurious functions)
- Instruction-level differences within matching functions

**Estimated effort**: 2-3 weeks. Written in TML itself (dogfooding: the TML compiler helps build the TML compiler).
**Exit criterion**: Tool correctly identifies a deliberate single-instruction difference in a test IR pair.

### 2.5 Hybrid Pipeline Framework

Write the serialization layer for AST, TypeEnv, and HIR. These are the three boundaries needed for the Phase 1 (Lexer/Parser) and Phase 2 (Type Checker) hybrid integrations.

Each serializer is a pair of TML functions:
```tml
func serialize_ast(module: ParsedModule) -> Buffer { ... }
func deserialize_ast(data: Buffer) -> ParsedModule { ... }
```

The C++ side reads/writes the same binary format via matching C++ functions in a new file `compiler/src/serial/ast_serial.cpp`.

**Estimated effort**: 4-6 weeks (AST serializer is the largest piece).
**Exit criterion**: Round-trip test passes: C++ parses a .tml file, serializes AST, TML deserializes it, reserializes it, C++ reads it back — all fields identical.

### Phase 0 Summary

| Item | Effort | Dependency | Exit Criterion |
|------|--------|------------|----------------|
| Consolidate MIR paths | 4-6 weeks | None | Test suite passes without hir_mir_builder |
| Build std::intern | 3-5 days | None | intern tests pass |
| Document type checker invariants | 3-4 weeks | None | 200+ edge-case tests pass |
| IR-diff testing infrastructure | 2-3 weeks | None | Tool detects 1-instruction diff |
| Hybrid pipeline serializers | 4-6 weeks | None | Round-trip test passes |
| **Phase 0 Total** | **~3-4 months** | | All items complete |

---

## 3. Phase 1: Lexer and Parser (3-4 months)

### 3.1 Lexer

**Current size**: 2,830 LOC C++ (source), 869 LOC headers. Total: 3,699 LOC.
**Estimated TML size**: ~2,400 LOC (35% reduction: no header/source split, TML enum variants replace C++ enum class, `HashMap[Str, TokenKind]` replaces `std::unordered_map`).

The lexer is the simplest subsystem. It has no external dependencies, no recursion, and no shared mutable state. Every required TML feature is already available: `Str`, `Char` iteration, `List[Token]`, `HashMap`, `when` for character dispatch.

**Key files to port**:

| C++ File | LOC | TML Equivalent |
|----------|-----|----------------|
| lexer_core.cpp | 538 | Main scan loop, character dispatch |
| lexer_string.cpp | 753 | String/template literal scanning |
| lexer_number.cpp | 375 | Numeric literal parsing |
| lexer_operator.cpp | 240 | Multi-char operator recognition |
| lexer_ident.cpp | 176 | Identifier scanning, keyword lookup |

**Data structures**:
```tml
type TokenKind enum {
    // 170+ variants — literals, keywords, operators, punctuation
    IntLit, FloatLit, StrLit, Ident,
    KwFunc, KwType, KwEnum, KwBehavior, KwImpl,
    // ...
}

type Token {
    kind: TokenKind,
    span: Span,
    value: TokenValue,
}

type Span { start: I32, end: I32, line: I32, col: I32 }
```

**Integration**: TML lexer produces `List[Token]`. Serialized to binary. C++ parser reads via `compiler/src/serial/token_serial.cpp`. This is the first hybrid handoff.

**Testing approach**: Run TML lexer and C++ lexer on the same 500+ .tml source files from the test suite. Token-for-token comparison. Any divergence is a bug.

**Estimated effort**: 6-8 weeks.
**Exit criterion**: Token-for-token match on all 500+ test files. Full test suite passes with TML lexer feeding C++ parser.

### 3.2 Parser

**Current size**: 6,327 LOC C++ (source), 3,499 LOC headers. Total: 9,826 LOC.
**Estimated TML size**: ~4,000 LOC (header elimination saves ~35%; Pratt operator table fits naturally in a `List` of `{prefix_fn, infix_fn, precedence}` records).

The parser is a Pratt expression parser combined with recursive descent for declarations. Both techniques translate naturally to TML. The recursive descent portion uses `Maybe[T]` returns for optional grammar elements — the same pattern TML's `Maybe` was designed for.

**Key subsystems**:

| Subsystem | Description | Complexity |
|-----------|-------------|------------|
| Pratt expression parser | `parse_expr(min_prec)` loop | Medium |
| Declaration parser | func, type, enum, behavior, impl | Medium |
| Pattern parser | destructuring, guard clauses | Medium |
| Type expression parser | generic params, ref types, function types | Medium |
| Error recovery | skip to next statement on syntax error | High |

**Error recovery** is the parser's hardest subsystem. The C++ parser uses a `synchronize()` function that skips tokens until it reaches a statement boundary (`func`, `type`, `{`, `}`). This logic must be ported faithfully — a parser that crashes on the first syntax error is unusable in practice.

**Integration**: TML parser produces `ParsedModule`. Serialized via the AST serializer built in Phase 0. C++ type checker reads via `compiler/src/serial/ast_serial.cpp`.

**Estimated effort**: 8-10 weeks.
**Exit criterion**: AST-for-AST match on all test files (after normalization of source positions). Full test suite passes with TML frontend (lexer + parser) feeding C++ type checker.

### Phase 1 Summary

| Subsystem | C++ LOC | Est. TML LOC | Effort | Risk |
|-----------|---------|--------------|--------|------|
| Lexer | 3,699 | ~2,400 | 6-8 weeks | Low |
| Parser | 9,826 | ~4,000 | 8-10 weeks | Medium |
| **Phase 1 Total** | **13,525** | **~6,400** | **3-4 months** | **Low-Medium** |

---

## 4. Phase 2: Type Checker (6-9 months)

### 4.1 Overview

**Current size**: 21,179 LOC C++ (source), 2,137 LOC headers. Total: 23,316 LOC across 47 files.
**Estimated TML size**: ~15,100 LOC.

The type checker is the highest-risk phase in the entire self-hosting effort (Risk R-001, score 9). It implements Hindley-Milner type inference, behavior/trait resolution, generic instantiation, 4-phase compilation ordering, borrow region analysis, and exhaustive `when`-expression checking. Its correctness cannot be verified by running a test suite alone — subtle divergences produce programs that compile but have wrong semantics.

The invariant document from Phase 0 is the prerequisite for starting this phase.

### 4.2 Sub-Phase Decomposition

Porting the type checker as a single unit would produce an untestable 15,000-line blob. Instead, it is ported in four sub-phases that mirror the four registration phases the C++ checker already uses:

**Sub-phase 2a: Registration (2-3 months)**
Port the first registration phase: collect all top-level type names, function names, and behavior definitions. This phase does not do inference — it only builds the top-level symbol table. Output: a `TypeEnv` with all names but no resolved types.

**Sub-phase 2b: Import Resolution (2-3 weeks)**
Port the second phase: resolve `use` declarations, link imported names to their definitions. This phase is mechanical but has edge cases around re-exports and circular imports.

**Sub-phase 2c: Inference Engine (3-4 months)**
Port the Hindley-Milner unification engine, behavior dispatch resolution, generic constraint solving, and closure type inference. This is the most complex sub-phase. Every type inference rule in the spec must produce identical results in the TML implementation.

**Sub-phase 2d: Body Checking (1-2 months)**
Port the expression-level checking: exhaustiveness checking for `when` expressions, unreachable code detection, integer overflow checking, and the 23 typed error codes.

### 4.3 Differential Testing

For every sub-phase, the TML type checker must agree with the C++ type checker on every input. The test protocol:

1. Compile a .tml file with the C++ checker. Record: the set of error codes emitted (if any), and the type assigned to every expression (via `--dump-types`).
2. Compile the same file with the TML checker. Record the same data.
3. Compare. Any difference is a bug. Fix before proceeding.

This differential testing runs on every commit during Phase 2. The test corpus includes:
- All 500+ existing .tml test files
- 200+ edge-case files written in Phase 0 specifically for type checker invariants
- Deliberately invalid .tml files that should produce specific error codes

### 4.4 Borrow Checker

**Current size**: 4,971 LOC C++ (source), 1,629 LOC headers. Total: 6,600 LOC.
**Estimated TML size**: ~4,300 LOC.

The borrow checker uses Non-Lexical Lifetime (NLL) analysis and place-based tracking. It runs after the type checker and before HIR lowering. It is ported as a sub-phase alongside Sub-phase 2c (inference), since it shares the `TypeEnv` and annotated AST.

### Phase 2 Summary

| Sub-Phase | Description | Effort | Risk |
|-----------|-------------|--------|------|
| 2a: Registration | Top-level symbol collection | 2-3 months | Medium |
| 2b: Import resolution | `use` declaration linking | 2-3 weeks | Low |
| 2c: Inference engine | HM unification, behavior dispatch | 3-4 months | Critical |
| 2d: Body checking | Exhaustiveness, error codes | 1-2 months | Medium |
| Borrow checker | NLL place-based tracking | 2-3 months | Medium |
| **Phase 2 Total** | **Type checker + borrow checker** | **6-9 months** | **Critical** |

---

## 5. Phase 3: HIR, THIR, and MIR (4-5 months)

### 5.1 HIR Builder

**Current size**: 10,555 LOC C++ (source), 4,652 LOC headers. Total: 15,207 LOC across 28 files.
**Estimated TML size**: ~9,900 LOC.

The HIR builder lowers the typed AST to a High-level Intermediate Representation (HIR). Its primary jobs:
- Resolve all types (every expression gets a concrete `TmlType`)
- Desugar syntax (for-loops → iterator calls, `var` → `let mut`, pattern matching → nested `when`)
- Monomorphize generics (each generic function/type instantiation becomes a concrete HIR function)
- Resolve field indices and variant indices

**Printable output**: `mcp__tml__emit-mir` (which shows HIR-derived MIR) serves as the observable output for differential testing. A dedicated `--dump-hir` flag can be added to the CLI for Phase 3 if needed.

**Estimated effort**: 6-8 weeks.

### 5.2 THIR Lowerer

**Current size**: 1,873 LOC C++ (source), 1,169 LOC headers. Total: 3,042 LOC across 9 files.
**Estimated TML size**: ~2,000 LOC.

The THIR lowerer is the smallest non-trivial pass. It inserts implicit coercions, resolves operator overloading (`a + b` → `a.add(b)`), and normalizes associated types. It is ported after HIR because it consumes HIR output.

**Estimated effort**: 2-3 weeks.

### 5.3 MIR Builder

**Current size**: 31,719 LOC C++ (source), 8,474 LOC headers. Total: 40,193 LOC across 137 files.
**Estimated TML size**: ~26,100 LOC.

This is the largest single subsystem to port in Phase 3. After the Phase 0 MIR consolidation, only the THIR→MIR path exists. The MIR builder converts the THIR tree to SSA-form basic blocks.

The 66 optimization passes (`compiler/src/mir/passes/*.cpp`) can be ported pass by pass. Each pass is independent — a pass that is not yet ported simply does not run, producing valid (if unoptimized) output. The passes are prioritized:

**Priority 1 — correctness-critical** (must port first):
- `mem2reg.cpp`: Promotes `alloca` variables to SSA registers. Without this, all local variables live on the stack and codegen emits hundreds of unnecessary loads and stores.
- `simplify_cfg.cpp`: Removes empty basic blocks, merges blocks with single predecessors.
- `dead_code_elimination.cpp`: Removes unreachable instructions.

**Priority 2 — performance-critical** (port before release):
- `constant_folding.cpp`, `constant_propagation.cpp`
- `inlining.cpp`
- `loop_opts.cpp`, `loop_rotate.cpp`
- `sroa.cpp` (scalar replacement of aggregates)

**Priority 3 — optimization passes** (port after bootstrap verified):
- All remaining 55 passes

**Serializer**: MIR serializers already exist in `compiler/src/mir/serializer/`. The TML MIR builder writes the same binary format, allowing the C++ codegen to run unchanged while Phase 3 stabilizes.

**Estimated effort**: 10-14 weeks (MIR builder core); passes are incremental and can continue through Phase 4.

### Phase 3 Summary

| Subsystem | C++ LOC | Est. TML LOC | Effort | Risk |
|-----------|---------|--------------|--------|------|
| HIR Builder | 15,207 | ~9,900 | 6-8 weeks | Medium |
| THIR Lowerer | 3,042 | ~2,000 | 2-3 weeks | Low |
| MIR Builder (core + P1 passes) | 40,193 | ~26,100 | 10-14 weeks | Medium |
| **Phase 3 Total** | **58,442** | **~38,000** | **4-5 months** | **Medium** |

---

## 6. Phase 4: MIR Codegen (4-6 months)

### 6.1 Overview

**Current size**: 76,336 LOC C++ (source), 2,634 LOC headers. Total: 78,970 LOC across 136 files.
**Estimated TML size**: ~51,100 LOC.

The MIR codegen is the largest single subsystem in the compiler. It converts `mir::Module` (SSA form) to LLVM IR text (a plain string). The output is a `.ll` file that is passed to the LLVM backend (`llvm_backend.cpp`) unchanged.

Because the output is a text string, the TML-written codegen does not need to call LLVM's C++ API at all. It builds the string using TML's `Text` builder and emits it to a file. The LLVM backend then compiles that file using the existing C++ wrapper — which stays in C++ permanently.

### 6.2 Structure of the Codegen

The codegen is organized into these groups, each ported as an independent unit:

| Group | Files | LOC | Description |
|-------|-------|-----|-------------|
| Core (types, functions, modules) | codegen/mir/mir_types.cpp, codegen_helpers.cpp | ~8,000 | LLVM type emission, function prologue/epilogue, module header |
| Instructions | codegen/mir/instructions.cpp | ~12,000 | emit_call_inst, binary ops, casts, comparisons |
| Method dispatch | codegen/mir/instructions_method.cpp | ~6,000 | behavior vtable calls, dyn dispatch |
| Misc instructions | codegen/mir/instructions_misc.cpp | ~4,000 | phi nodes, GEP, extractvalue, insertvalue |
| Terminators | codegen/mir/terminators.cpp | ~3,000 | br, switch, ret, unreachable |
| Calls | codegen/mir/instructions_call.cpp | ~4,000 | calling convention, sret, varargs |
| Derives | codegen/llvm/derive/ | ~8,000 | @derive(Debug), @derive(Clone), etc. |
| Builtins | codegen/llvm/builtins/ | ~6,000 | SIMD intrinsics, overflow-checked arithmetic |
| Control flow | codegen/llvm/control/ | ~5,000 | async state machines, coroutines |
| Declarations | codegen/llvm/decl/ | ~4,000 | extern declarations, global constants |
| Expressions | codegen/llvm/expr/ | ~8,000 | LLVM IR expression builders |
| Core/misc | codegen/llvm/core/ | ~8,336 | Top-level orchestration |

### 6.3 IR-Diff Verification

At this phase, the IR-diff tool from Phase 0 becomes the primary correctness gate. The process for each ported group:

1. Port the group to TML.
2. Run the IR-diff tool on all 500+ test files.
3. For each divergence: examine the difference, determine if it is semantically equivalent (same computation, different register names) or semantically different (different instruction sequence, different calling convention).
4. Fix all semantically-different divergences before moving to the next group.

A divergence is acceptable only if it represents a known optimization (TML codegen produces strictly fewer instructions than C++ codegen for the same logic). All such acceptable divergences are documented.

### 6.4 LLVM IR Correctness

The LLVM verifier (`llvm_backend.cpp` calls `llvm::verifyModule`) will catch structural errors — malformed IR, type mismatches, missing terminators. Every piece of TML-emitted IR must pass this verifier. Verifier failures are treated as bugs, not warnings.

**Estimated effort**: 16-24 weeks. This is the longest phase due to size (51K estimated TML LOC) and the precision required (IR must match or be strictly better).
**Exit criterion**: IR-diff reports zero semantically-different divergences on all test files. Full test suite passes with all-TML pipeline through codegen.

### Phase 4 Summary

| Component | C++ LOC | Est. TML LOC | Effort | Risk |
|-----------|---------|--------------|--------|------|
| MIR codegen (all groups) | 78,970 | ~51,100 | 16-24 weeks | High |
| **Phase 4 Total** | **78,970** | **~51,100** | **4-6 months** | **High** |

---

## 7. Phase 5: Query System, CLI, and Tooling (2-3 months)

### 7.1 Query System

**Current size**: 2,126 LOC C++ (source), 969 LOC headers. Total: 3,095 LOC.
**Estimated TML size**: ~2,000 LOC.

The query system wraps all pipeline phases in memoized, demand-driven queries. Each query takes a `QueryKey` (a tagged value identifying what is being computed) and returns a cached result or computes it fresh. Cross-session caching is stored in `.incr-cache/incr.bin`.

The query system has no complex logic — it is infrastructure (a `HashMap[QueryKey, QueryResult]` with fingerprinting). Porting it requires implementing the fingerprinting hash (input hash + output hash) and the cross-session binary cache format.

**Estimated effort**: 3-4 weeks.

### 7.2 CLI

**Current size**: 26,791 LOC C++ (source). Across 55 files.
**Estimated TML size**: ~17,400 LOC.

The CLI is the `tml build`, `tml run`, `tml test`, `tml check`, `tml format`, and `tml lint` command implementations. It is the outermost layer — it parses command-line arguments, invokes the pipeline, and reports results.

The CLI is not on the critical path for bootstrap. Stage 1 only needs to compile .tml files to executables. The CLI's complex test runner (`compiler/src/testing/`) and MCP server integration are non-essential for the initial bootstrap milestone.

**Priority for Phase 5**: Port `tml build` and `tml check` first (the two commands that invoke the core pipeline). Port `tml test`, `tml run`, and other commands afterward.

**Estimated effort**: 6-8 weeks (build + check); 4-6 weeks more for full CLI parity.

### 7.3 Formatter

**Current size**: 1,181 LOC C++ (source), 146 LOC headers. Total: 1,327 LOC.
**Estimated TML size**: ~860 LOC.

The formatter walks the AST and produces canonical formatted output. It is independent of the compilation pipeline — it uses the lexer and parser only. It can be ported at any time during Phase 5.

**Estimated effort**: 2-3 weeks.

### 7.4 Testing Framework

**Current size**: 9,968 LOC C++ (source), 1,013 LOC headers. Total: 10,981 LOC.
**Estimated TML size**: ~7,100 LOC.

The testing framework is the subprocess-based coordinator that discovers test files, compiles them, launches them as processes, and streams NDJSON results. The TML-side test library (`lib/test/`) is already written in TML. Only the coordinator (the C++ code that orchestrates test execution) needs porting.

**Estimated effort**: 4-5 weeks.

### Phase 5 Summary

| Subsystem | C++ LOC | Est. TML LOC | Effort | Risk |
|-----------|---------|--------------|--------|------|
| Query system | 3,095 | ~2,000 | 3-4 weeks | Low |
| CLI (build + check) | 26,791 | ~8,000 | 6-8 weeks | Low |
| CLI (full parity) | — | ~9,400 | 4-6 weeks | Low |
| Formatter | 1,327 | ~860 | 2-3 weeks | Low |
| Testing framework | 10,981 | ~7,100 | 4-5 weeks | Low |
| **Phase 5 Total** | **42,194** | **~27,360** | **2-3 months** | **Low** |

---

## 8. Testing Strategy

### 8.1 Differential Testing (Primary Gate)

Every phase uses differential testing as its primary correctness gate. The protocol is identical at every level:

1. Run the C++ compiler on a corpus of .tml files. Record the output (tokens / AST / TypeEnv / HIR / MIR / LLVM IR).
2. Run the TML-implemented subsystem on the same corpus. Record the same output.
3. Compare after normalization (remove source positions, normalize generated names to ordinal positions).
4. Any difference is a bug. No exceptions.

The test corpus for differential testing is:
- All test files in `lib/core/tests/` and `lib/std/tests/` (500+ files)
- The 200+ edge-case files written in Phase 0 for type checker invariants
- Deliberately invalid .tml files (verify identical error messages)

### 8.2 IR-Level Comparison (Codegen Gate)

For Phase 4 specifically, LLVM IR comparison is the gate. The IR-diff tool (Phase 0) normalizes and compares IR function by function. Acceptable divergences:
- Register renaming (purely cosmetic)
- Instruction reordering within a basic block that does not change semantics
- TML codegen emitting fewer instructions (strictly better — document but not a bug)

Unacceptable divergences (bugs):
- Different calling conventions (sret mismatch, argument count difference)
- Different type layouts (struct field order, enum tag width)
- Different control flow (different branch structure for same logic)
- Extra or missing function definitions

### 8.3 Regression Gate (Phase Boundary)

Before advancing to the next phase, all of the following must pass:
- Full test suite (`mcp__tml__test` with `no_cache=true`): zero new failures
- IR-diff on codegen test corpus: zero semantically-different divergences
- Bootstrap verification (Phase 4 and later): Stage 1 output == Stage 2 output

### 8.4 Bootstrap Verification Protocol

At the end of Phase 4, the bootstrap verification runs:

```
1. Take the TML compiler source (tml_compiler.tml)
2. Compile it with Stage 0 (C++ tml.exe) -> produces tml-s1.exe
3. Compile tml_compiler.tml with tml-s1.exe -> produces tml-s2.exe
4. Compile a standard test file with tml-s1.exe -> produces test_s1.exe
5. Compile the same test file with tml-s2.exe -> produces test_s2.exe
6. Compare: sha256(test_s1.exe) == sha256(test_s2.exe)
7. If equal: bootstrap confirmed. If not: codegen bug in Stage 1.
```

Executable-level comparison (not IR-level) is the final gate. Two compilers that produce the same executable from the same source are by definition equivalent.

---

## 9. Timeline Summary

| Phase | Description | Duration | Start | End | Dependencies | Risk | Milestone |
|-------|-------------|----------|-------|-----|--------------|------|-----------|
| Phase 7 | Rust parity (2 tasks remaining) | 2 months | 2026-04 | 2026-06 | None | Low | Phase 7 complete — language mature |
| Phase 0 | Pre-work: consolidate, intern, invariants, IR-diff, serializers | 3-4 months | 2026-06 | 2026-10 | Phase 7 done | Low | Infrastructure ready |
| Phase 1 | Lexer + Parser | 3-4 months | 2026-10 | 2027-02 | Phase 0 done | Low-Medium | TML frontend active |
| Phase 2 | Type checker + borrow checker | 6-9 months | 2027-02 | 2027-11 | Phase 1 done; invariant doc | Critical | TML semantic analysis active |
| Phase 3 | HIR + THIR + MIR | 4-5 months | 2027-11 | 2028-04 | Phase 2 done; MIR consolidation | Medium | TML mid-end active |
| Phase 4 | MIR Codegen | 4-6 months | 2028-04 | 2028-10 | Phase 3 done; IR-diff tool | High | Full TML pipeline — bootstrap attempt |
| Phase 5 | Query + CLI + Tooling | 2-3 months | 2028-10 | 2029-01 | Phase 4 done | Low | Full self-hosted toolchain |

**Total estimated duration**: 26-35 months from Phase 0 start.
**Bootstrap milestone** (first Stage 1 == Stage 2 verification): end of Phase 4, approximately 2028-10.
**Full self-hosted release**: end of Phase 5, approximately 2029-01.

Note: Phase 2 (type checker) is the schedule-determining task. If Phase 2 finishes faster than estimated, the entire timeline compresses. If Phase 2 encounters unexpected complexity (see R-001), the timeline extends. Monthly go/no-go reviews during Phase 2 will refine the estimate.

---

## 10. Go/No-Go Criteria

Before starting each phase, all prerequisites in the corresponding row must have status "Done".

| Phase | Prerequisite | Current Status | How to Verify |
|-------|-------------|----------------|---------------|
| Phase 0 | Phase 7 complete (language mature) | In progress (14/16 tasks) | All phase7 tasks marked done in rulebook |
| Phase 0 | Full test suite passing (1,700+ tests) | Done | `mcp__tml__test structured=true` — zero failures |
| Phase 0 | MIR serializer working | Done | binary_writer/reader exist in compiler/src/mir/serializer |
| Phase 1 | MIR paths consolidated (hir_mir_builder retired) | Not started | Test suite passes with hir_mir_builder removed from build |
| Phase 1 | std::intern module implemented | Not started | `mcp__tml__test suite="core/intern"` passes |
| Phase 1 | AST serializer round-trip test passing | Not started | Round-trip test in compiler/tests/serial/ passes |
| Phase 2 | Token-for-token lexer match on all test files | Not started | IR-diff tool reports zero token divergences |
| Phase 2 | AST-for-AST parser match on all test files | Not started | IR-diff tool reports zero AST divergences |
| Phase 2 | Type checker invariant document complete | Not started | 200+ edge-case tests pass |
| Phase 3 | Differential type checker: zero divergences on test corpus | Not started | diff tool reports zero type divergences |
| Phase 3 | Borrow checker: zero divergences | Not started | diff tool reports zero borrow errors mismatch |
| Phase 4 | HIR MIR match: MIR dump identical for all test files | Not started | mir_printer diff reports zero divergences |
| Phase 4 | All P1 MIR passes ported (mem2reg, simplify_cfg, dce) | Not started | Test suite passes with TML MIR passes |
| Phase 5 | IR-diff: zero semantically-different divergences on test corpus | Not started | IR-diff tool reports zero functional differences |
| Phase 5 | Bootstrap verification passes | Not started | sha256(test_s1.exe) == sha256(test_s2.exe) |
| Release | Full test suite passing with all-TML pipeline | Not started | `mcp__tml__test` — zero failures, no regression |

---

## 11. Resource Requirements

### 11.1 Developer Time

Self-hosting the TML compiler is a 26-35 month effort at one dedicated developer. This is consistent with prior art:
- Rust's self-hosted compiler (rustc) was written over ~3 years with 2-4 core contributors
- Go's self-hosted compiler (gc, 2015) took ~2 years of work before the Go 1.5 release
- Zig's self-hosting effort has been in progress since 2019 with a small core team

The estimated time per phase (from Phase 0 through Phase 5) totals approximately 23-31 months of implementation work. Including Phase 7 prerequisites, the full timeline from today is 26-35 months.

Phase 2 (type checker) is the only phase that cannot be parallelized — it requires the output of Phase 1 and produces the output that Phase 3 needs. Phases 1, 3, 4, and 5 have subsystems that can be parallelized if a second developer is available.

### 11.2 CI Infrastructure

Each phase requires dedicated CI jobs:

| CI Job | Phase | Frequency | Description |
|--------|-------|-----------|-------------|
| Differential token test | Phase 1+ | Every commit | C++ lexer vs TML lexer on full corpus |
| Differential AST test | Phase 1+ | Every commit | C++ parser vs TML parser on full corpus |
| Differential type test | Phase 2+ | Every commit | C++ type checker vs TML type checker |
| IR-diff codegen test | Phase 4+ | Every commit | C++ codegen vs TML codegen, function-by-function |
| Bootstrap verification | Phase 4+ | Every release candidate | Stage 1 == Stage 2 executable comparison |
| Full test suite | All phases | Every commit | Regression guard — zero new failures |

Each differential test run adds approximately 5-15 minutes to CI time (running both compilers on 500+ test files). This is acceptable. The bootstrap verification (Stage 1 → Stage 2 round-trip) adds ~30-60 minutes and runs only on release candidates.

### 11.3 Review Cadence

Given the risk profile of Phase 2 (type checker), a structured review cadence is required:

- **Monthly**: Progress review — how many type checker invariants ported, differential test divergence count, schedule variance
- **Phase boundary**: Full review before advancing — all go/no-go criteria satisfied, risk register updated, timeline re-estimated
- **Weekly during Phase 2**: Differential divergence count reported. If divergence count is not decreasing week over week, escalate immediately (R-001 mitigation)

### 11.4 Tooling Investments

Phase 0 investments that pay dividends through all subsequent phases:

| Tool | Phase 0 Effort | Benefit Duration |
|------|---------------|-----------------|
| std::intern | 3-5 days | Phase 1 through Phase 5 |
| AST serializer | 4-6 weeks | Phase 1 and Phase 2 |
| IR-diff tool | 2-3 weeks | Phase 4 (primary gate) |
| Type checker invariant document | 3-4 weeks | Phase 2 (prevents rework) |
| MIR path consolidation | 4-6 weeks | Phase 3 and Phase 4 |

Total Phase 0 tooling investment: ~3-4 months. Without this investment, Phase 2 alone would take 3-6 months longer due to undocumented invariants causing rework, and Phase 4 would lack the IR-diff tool that enables systematic correctness verification.

---

*This document reflects the state of the TML compiler as of 2026-04-05. LOC measurements are from live source: lexer 2,830, parser 6,327, types 21,179, borrow 4,971, hir 10,555, thir 1,873, mir 31,719, codegen 76,336, backend 1,593, query 2,126, cli 26,791, testing 9,968, format 1,181. See 01-compiler-inventory.md for full per-file breakdowns.*
