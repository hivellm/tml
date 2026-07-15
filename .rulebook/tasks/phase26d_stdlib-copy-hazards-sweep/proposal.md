# Proposal: phase26d_stdlib-copy-hazards-sweep

## Why

The 08-memory-copy-audit found the copy-instead-of-move class is systemic, but a
subset of the damage is fixable at the LIBRARY level right now, independent of the
ADR-009 codegen model fix: two move-outs that double-free unconditionally
(`Arc::try_unwrap`, `AnyValue::into_inner` — the source is never forgotten/nulled),
`Sync[T]::get` copying with no safe alternative, and ~40 pass-by-value MUST-BORROW
sites (the phase24b class, still alive) where an owning aggregate is passed by value
to a read-only entry point and freed by the callee's drop-glue while the caller still
holds it. These are correctness AND performance wins that need no compiler change.

## What Changes

Null/forget the source in the two broken move-outs; port `get_ref`/`get_clone` from
`Shared` to `Sync`; migrate the pass-by-value params (BigInt operators, str::join,
HTTP/2 Buffer accumulators, HashMap::extend_from, console::table, File::write_bytes,
events/reactive) to `ref`/`mut ref` — a one-token, idiom-matching change the codebase
already uses in sibling methods.

## Impact

- Affected specs: `core::alloc::Sync` API docs.
- Affected code: `lib/std/src/sync/arc.tml`, `lib/core/src/types/any.tml`,
  `lib/core/src/alloc/sync.tml`, `lib/std/src/bigint.tml`, `lib/core/src/str/convert.tml`,
  `lib/std/src/http/h2/*`, `lib/std/src/collections/hashmap.tml`, `lib/std/src/console.tml`,
  `lib/std/src/file/file.tml`, `lib/std/src/events/*` + call sites.
- Breaking change: NO for correct callers (ref is source-compatible for reads);
  callers relying on the accidental consume/free would change behavior (intended).
- User benefit: removes 2 guaranteed double-frees + a class of UAFs, and stops
  copying/freeing whole aggregates on every arithmetic/join/append call.

## Source

- docs/analysis/tml-table-analysis/08-memory-copy-audit.md (F-017, F-018, F-020).
- Runs in parallel with phase26b (model fix); model-dependent findings (F-015/016/019/022)
  are owned by phase26b step 4; the borrow-accessor gap (F-021) is phase26e.
