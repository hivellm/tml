# Proposal: Error Codes Expansion — 261 New Codes Across All Pipeline Stages

## Why

57% of compiler error sites (270/460) have no error code. Users see raw messages like "Type mismatch: expected X, found Y" or "Failed to parse LLVM IR" without a trackable code. `tml explain` can't help. 7 pipeline stages have ZERO codes (Preprocessor, HIR, THIR, LLVM Backend, Query, Linker, Format/Lint, Testing). This makes debugging extremely difficult — users can't search for the error, can't get help, can't report it precisely.

## What Changes

Add 261 new error codes across 17 categories, bringing total from 197 to 460. Every error site in every pipeline stage gets a unique, explainable code. Also add `tml explain` entries for all new codes.

Full proposal: [docs/error-codes-proposal.md](../../docs/error-codes-proposal.md)

## Impact
- Affected specs: All compiler pipeline stages
- Affected code: `compiler/src/types/checker/*.cpp`, `compiler/src/codegen/llvm/**/*.cpp`, `compiler/src/backend/*.cpp`, `compiler/src/hir/*.cpp`, `compiler/src/mir/*.cpp`, `compiler/src/query/*.cpp`, `compiler/src/borrow/*.cpp`, `compiler/src/lexer/*.cpp`, `compiler/src/parser/*.cpp`, `compiler/src/format/*.cpp`, `compiler/src/testing/*.cpp`, `compiler/src/cli/explain/*.cpp`
- Breaking change: NO (additive — existing error messages unchanged, only adds code tags)
- User benefit: Every compiler error has a searchable code. `tml explain X001` works for all errors.
