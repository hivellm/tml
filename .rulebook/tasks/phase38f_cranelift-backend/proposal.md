# Proposal: Self-Hosting Preparation + Cranelift Completion

## Status: PROPOSED (0% — Future work, depends on stable compiler infrastructure)

## Summary

Two related tracks: (1) completing the Cranelift backend to 80%+ test pass rate, enabling fast debug builds without LLVM; (2) beginning self-hosting preparation by rewriting the TML lexer and parser in TML itself, establishing the Stage 1 bootstrap path toward a fully self-hosted compiler.

## Motivation

**Cranelift**: LLVM at `-O0` is the debug-build bottleneck. The compiler links ~100MB of LLVM; compile times for large programs are measured in seconds even for trivial changes. Cranelift is a lightweight code generator designed for JIT and fast debug compilation — 3x faster than LLVM at `-O0` is the target. A working Cranelift backend dramatically improves the inner loop for TML developers.

**Self-hosting**: A language compiler written in itself is the gold standard for language maturity. It validates the language's expressiveness, serves as the largest real-world test of the compiler, and enables the language to be used for compiler development. The bootstrap plan follows Rust's: Stage 0 (C++ compiler) builds Stage 1 (partial TML compiler), which eventually builds Stage 2 (full TML compiler). The lexer and parser are the right starting point — they have well-defined inputs/outputs and can be cross-validated against the C++ reference implementation.

## Design

**Cranelift track**: The Cranelift backend already exists in the compiler (`compiler/src/backend/cranelift_backend.cpp`). The task is to run the full test suite against it, identify failing tests, fix codegen deficiencies, and reach 80%+ pass rate. Success criterion: `tml test --backend=cranelift` passes 80%+ of the current test suite. Benchmark: `tml build hello_world.tml --backend=cranelift` must be 3x faster than `--backend=llvm -O0`.

**Self-hosting track**: The bootstrap plan has three stages:
- Stage 0: current C++ compiler (builds Stage 1)
- Stage 1: TML lexer + parser written in TML, compiled by Stage 0
- Stage 2: full TML compiler written in TML, compiled by Stage 1

The TML lexer and parser are rewritten in `compiler-tml/src/lexer.tml` and `compiler-tml/src/parser.tml`. Cross-validation compares the token stream and AST from the TML implementation against the C++ reference on the full test corpus. The TML implementation must produce byte-identical output; any difference is a bug in the TML implementation.

Performance target: the TML lexer/parser must be within 2x of the C++ implementation speed. If it is slower, the bottleneck must be identified (likely string allocation patterns) and optimized before declaring Stage 1 complete.

## What Changes

- Cranelift track: fixes to `compiler/src/backend/cranelift_backend.cpp` (no new files)
- Self-hosting track:
  - New: `compiler-tml/` directory
  - New: `compiler-tml/src/lexer.tml` — TML lexer producing identical tokens to C++ lexer
  - New: `compiler-tml/src/parser.tml` — TML Pratt parser producing identical AST to C++ parser
  - New: `compiler-tml/tests/` — cross-validation tests
  - New: `compiler-tml/tml.toml` — manifest for the TML-hosted compiler component

## Dependencies

- Depends on: stable compiler infrastructure (`phase6-02-self-hosting-compiler` groundwork)
- Depends on: Cranelift backend existing in compiler (already present)
- Cranelift track depends on: all Phase 1-5 tasks being stable (can't fix codegen against a moving target)
- Self-hosting track depends on: `phase1-03-core-ffi-types` and `phase2-05-bigint` (lexer needs proper string handling)
- Enables: `phase6-02-self-hosting-compiler` — full self-hosting (lexer + parser are the prerequisite)
- Enables: Cranelift → faster developer iteration for all TML development

## Risks

- Cranelift backend may have fundamental gaps (e.g., no support for certain LLVM IR patterns TML emits); reaching 80% may require significant backend work or changes to TML codegen output
- The TML parser is a Pratt parser with a recursive descent declaration layer — this is approximately 3000-4000 lines of C++; the TML rewrite will be the largest single TML program written to date and will stress-test the language
- Cross-validation requires the C++ compiler to still be buildable at the time the TML version is written; the C++ compiler must not be modified in ways that change AST output while Stage 1 is being written
- Bootstrapping is logically circular — the TML compiler compiling itself requires a pre-existing TML compiler to compile the first version; the build system must explicitly manage Stage 0 vs Stage 1 binaries
