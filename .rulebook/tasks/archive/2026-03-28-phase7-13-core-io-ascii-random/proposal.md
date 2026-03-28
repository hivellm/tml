# Proposal: Core I/O Traits, ASCII Expansion, Random Trait

## Why

Three module-level gaps: (1) core has no Read/Write traits — they're scattered across std::stream and std::file; (2) ascii/ module is minimal — Rust has AsciiChar and is_ascii_* methods; (3) core has no Random trait — Rust core defines the trait, std provides the impl.

## What Changes

1. Create `core::io` with Read, Write, BufRead behaviors. 2. Expand `core::ascii` with AsciiChar and classification methods. 3. Add Random trait to core (impl stays in std::random).

## Impact
- Affected specs: core::io (new), core::ascii, core::random (new trait)
- Affected code: `lib/core/src/`, `lib/std/src/stream/` (re-export from core)
- Breaking change: NO (additive)
- User benefit: Unified I/O traits, ASCII utilities, trait-based random generation
