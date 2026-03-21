# 8. LL(1) Grammar with Single-Token Lookahead

**Status**: proposed
**Date**: 2026-03-15

## Context

LLMs generate code token-by-token. Complex grammars requiring backtracking or multi-token lookahead increase the chance of LLM generation errors. TML is designed specifically for LLM code generation.

## Decision

TML uses an LL(1) grammar where every production is determined by a single-token lookahead. Square brackets [T] for generics (not angle brackets), 'do' keyword for closures (not |x|), keyword-based logical operators (and/or/not). No macros — decorators with quote/splice replace them. Mandatory type annotations on all declarations.

## Alternatives Considered

- PEG grammar (more expressive but requires backtracking)
- Rust-style grammar (proven but LLM-hostile with < ambiguity and | closures)
- Lisp-style S-expressions (trivially parseable but less human-readable)

## Consequences

LL(1) constraint means some constructs need more keywords (do, then, when, through) but parsing is deterministic and fast. LLMs can generate valid TML with higher accuracy than Rust. No ambiguity in tokenization.
