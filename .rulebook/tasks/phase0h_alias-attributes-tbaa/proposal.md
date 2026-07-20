# Proposal: phase0h_alias-attributes-tbaa

## Why
Zero alias attributes and zero TBAA across the shipping codegen block LLVM -O2
from recovering aggregate/collection code — the concrete reason the ≤2×-Rust
goal fails at release on real programs (analysis L-062, L-063).

## What Changes
A shared attribute/metadata emit helper; noalias/readonly/dereferenceable on
reference parameters; a minimal TBAA tree on memory ops; insertvalue+sret enum
construction.

## Impact
- Affected specs: none (IR-level)
- Affected code: compiler/src/codegen/llvm/decl/{func,impl}.cpp, call-site emitters, one new shared helper
- Breaking change: NO
- User benefit: release builds of aggregate/collection-heavy code approach Rust parity
