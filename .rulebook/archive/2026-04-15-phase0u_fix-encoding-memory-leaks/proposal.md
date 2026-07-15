# Proposal: phase0u_fix-encoding-memory-leaks

## Why
The encoding benchmark (`benchmarks/profile_tml/encoding_bench.tml`) reports 200,000 memory leaks totaling 2.8 MB after 100,000 iterations — approximately one 28-byte allocation leaked per encode/decode call. Rust has zero leaks for equivalent operations. The leaks come from TML's base64, hex, and base32 encoding functions in `lib/std/src/encoding/` allocating intermediate `Text` or `Buffer` values that are never freed (no RAII/Drop yet). This makes the encoding module unusable in long-running servers or loops — every call grows the heap. The fix must free all intermediate allocations at the encoding function boundary, without waiting for the Drop implementation (phase0v). See `docs/analysis/benchmark/07-encoding.md` for the leak report details.

## What Changes
Each encoding function in `lib/std/src/encoding/base64.tml`, `hex.tml`, and `base32.tml` will be audited for allocations. Every `Text::new()`, `Buffer::alloc()`, or `List::new()` created internally must be explicitly freed before the function returns (or errors). Where the function returns an owned value, the callee retains ownership and the caller is responsible — this is the current correct contract. The bug is functions that allocate scratch buffers and forget to free them. Each function will be instrumented with `mcp__tml__debug(check_leaks=true)` to confirm zero leaks after the fix.

## Impact
- Affected specs: std/encoding/base64, std/encoding/hex, std/encoding/base32
- Affected code: `lib/std/src/encoding/base64.tml`, `lib/std/src/encoding/hex.tml`, `lib/std/src/encoding/base32.tml`
- Breaking change: NO (same API; internal allocations are implementation details)
- User benefit: Encoding functions become safe to use in loops and servers — no unbounded heap growth. Prerequisite for any production use of the encoding module.
