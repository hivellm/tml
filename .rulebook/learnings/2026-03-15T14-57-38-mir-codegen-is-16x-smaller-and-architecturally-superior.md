# MIR codegen is 16x smaller and architecturally superior
**Source**: manual
**Date**: 2026-03-15
**Related Task**: optimize-codegen-like-rust
**Tags**: codegen, mir, legacy, architecture, investment
MIR codegen: ~3,000 lines, operates on SSA-form MIR, carries TypePtr through pipeline. Legacy codegen: ~49,000 lines, operates on AST, encodes types as strings. MIR has mem2reg + load_store_opt passes built-in. The quality gap is clear: MIR avoids the god-class problem, string-based type encoding, and state leakage bugs by design. Investment should prioritize closing MIR gaps (library IR, debug info, coverage, async) over improving legacy. Every legacy fix is throwaway work once MIR reaches parity.