# TML Language Gaps — Verified Proposal (2026-03-19)

## Methodology

Each gap was tested with real TML code that was compiled and executed.
No assumptions — every claim is backed by an actual compilation result.

## Verified Results

| # | Feature | Parser | Typechecker | Codegen | Runtime | Verdict |
|---|---------|:------:|:-----------:|:-------:|:-------:|---------|
| 1 | Bool struct + fn ptr | ✅ | ✅ | ✅ | ❌ SEGFAULT | **Codegen bug** |
| 2 | dyn Behavior dispatch | ✅ | ✅ | ❌ Invalid IR | — | **Codegen bug** |
| 3 | async func + .await | ✅ | ✅ | ❌ Type mismatch | — | **Codegen bug** |
| 4 | Template literals | ✅ | ✅ | ✅ | ✅ Works! | **Not a gap** |

## Key Finding

All 3 real gaps are **codegen bugs**, not missing features. The parser and type checker
handle all of them correctly. The problem is in LLVM IR generation:

1. **Bool/i1**: struct layout mismatch when Bool (i1) fields are in structs passed through fn ptrs
2. **dyn**: vtable dispatch emits undefined value references (`%v1` not defined)
3. **async**: state machine lowering has i64/i32 type confusion

## Priority

1. **Bool/i1** (HIGH) — smallest fix, biggest immediate impact (unblocks HTTP middleware)
2. **dyn** (HIGH) — enables error chaining, plugin systems, polymorphic dispatch
3. **async** (MEDIUM) — larger fix, manual async works as workaround

## What Was NOT a Gap

Template literals work perfectly: `` `Hello, {name}!` `` → "Hello, World!"
Returns `Text` type. Should be used in HTTP response building instead of lowlevel.

Additionally, ~15 types/APIs exist in core/std that the HTTP code doesn't use:
Buffer, Text, HashMap, List, Slice, TcpStream, Outcome, Mutex, str::*, etc.
The HTTP module should be rewritten to use these before any compiler fixes.
