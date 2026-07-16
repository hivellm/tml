# phase26d — Stdlib Copy-Hazard Sweep (library-level, model-independent)

> The subset of the 08-memory-copy-audit findings (F-017/F-018/F-020) that are
> fixable at the LIBRARY level WITHOUT the ADR-009 model fix. Safe wins; runs in
> parallel with phase26b. Model-dependent findings (F-015/F-016/F-019/F-022) belong
> to phase26b step 4; the borrow-accessor gap (F-021) is a separate future task.
> Every change gated by: determinism corpus + affected suites + K002 verifier.

## 1. Implementation

### F-017 — broken move-outs that double-free unconditionally
- [x] 1.1 `Arc::try_unwrap`: `mut this` + `this.ptr = null` after the CAS+dealloc; added a null guard at the top of `Arc::drop` so a forgotten handle no-ops. Tests: `lib/std/tests/sync/arc*.test.tml` 5/5
- [x] 1.2 `AnyValue::into_inner`: `mut this` + `this.data = null` after `dealloc` on BOTH match and mismatch paths; unblocked the stubbed `any_into_inner.test.tml` (was a `BLOCKED` no-op) with 3 real tests (match/mismatch/i64). any suite 11/11

### F-018 — Sync has no safe read accessor
- [x] 1.3 Added `Sync::get_clone(this) -> T where T: Duplicate` (deep clone via `T::duplicate()`) and `Sync::get_ref(this) -> ref T` (borrow via `ref (*this.ptr).value`), plus the `get` hazard docstring. Tests `sync_get_accessors.test.tml` pass in the real build (get_ref borrows: `let r: ref I64 = s.get_ref(); assert_eq(*r, 42)` green). NOTE: `mcp__tml__check` FALSE-NEGATIVES on `get_ref` (reports `ptr_as_ref` undefined / `ref T` vs `*T`) — the isolated-file checker doesn't resolve the borrow-through-rawptr intrinsic that the full build has; verified working via the passing test. `Shared::get_ref` has the same check-tool false-negative (it also builds+works). The zero-copy IR-level verification of get_ref is owned by phase26e (borrow accessors)

### F-023 — try_unwrap frees ignoring weak refs
- [x] 1.3b `Shared::try_unwrap` + `Sync::try_unwrap`: on strong→0, move the value out, decrement strong + implicit weak, and `mem_free` ONLY when `weak_count` also reaches 0 (mirrors `decrement_count`); null `this.ptr`. Sync uses atomic fetch_sub. Tests: `shared_try_unwrap_weak.test.tml` + `sync_try_unwrap_weak.test.tml` (unique-ok, keeps-weak-alive w/ downgrade→upgrade==Nothing, multiple-strong-fails) — alloc suite 44/44, determinism gate 18/18

### F-020 — pass-by-value MUST-BORROW → `ref` migration (one-token, idiom-matching)
- [x] 1.4 BigInt operator cluster — all 12 single-param ops + `mod_pow` (2 params) + `mod_inverse` migrated to `ref`; 35 internal call sites updated (4 temp-arg sites restructured with local bindings: ext-Euclid `q.mul(temp_r/s)`, `modulus.abs()`); 16 test call sites updated. Type-valid in the full pipeline (parse+typecheck+borrowcheck pass; K001 during IR emission is PRE-EXISTING at HEAD — catalogued `std/math/bigint`, runtime validation gated on phase27a). No other src users exist
- [x] 1.5 `str::join`/`concat_all` → `ref List[Str]` + 6 call sites (wave 2a, v0.3.59a commit 027c87e5); ref-param pass-through verified working
- [x] 1.6 HTTP/2 Buffer family — DONE: `h2_buf_append`/`h2_conn_append_buf` → `(dst: ref, src: ref)` incl. the `ref this.outbound` field-access caller; the 10 `handle_*` payload handlers + `process_frame` → `payload: ref Buffer` (internal dispatch = ref passthrough); `h2_encode_frame_raw`/`h2_decode_settings` → `ref` + all 12 src callers (`ref frame.payload`/`ref this.payload` field-access refs work) + 37 test call sites across 7 files (regex-rewritten). SCOPE CORRECTION applied: `H2Frame::new`/`data` are consuming constructors (store payload into the struct) — correctly left by-value. `controller.tml register_all(controllers: ref ...)` migrated (no external callers). Validation: 6 of 9 h2 suites green (incl. process_frame/encode/decode ref surfaces); the 3 failing (`h2_build_response`, `h2_flow_control`, `h2_connection_streams`) are PRE-EXISTING K001 'integer constant must have integer type' verified identical at HEAD — catalogued
- [x] 1.7 Remaining sites: `HashMap::extend_from` (ref both lists) + `File::write_bytes`/`write_all_bytes` (ref Buffer) + call sites (wave 2a); events/reactive (4 promise combinators + `observable_from_list_i32`) + observable test sites (wave 2b, events 9/9 + observable 7/7 green). `console::table` attempted and REVERTED — trips a PRE-EXISTING K001 ('Cannot allocate unsized type', verified at HEAD and pre-Step3; catalogued). Subject state-bags intentionally not migrated (interior-mutability semantics)

> **Wave 1 (F-017/018/023) COMPLETE — v0.3.56. Wave 2 (F-020 ref migration)
> COMPLETE — v0.3.59** (wave 2a: 027c87e5, wave 2b: f6ffa863 + HTTP/2/controller).
> ~120 signature/call-site edits total. One deliberate exception: `console::table`
> reverted (pre-existing K001); subject state-bags excluded by design.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [x] 2.1 Update or create documentation covering the implementation — wave 1: hazard docstrings + v0.3.56 patch notes; wave 2: doc examples updated at every migrated signature, v0.3.59 patch notes, 9 newly-verified pre-existing failures catalogued in known-failures.txt
- [x] 2.2 Write tests covering the new behavior — wave 1: `shared_try_unwrap_weak`, `sync_try_unwrap_weak`, `sync_get_accessors`, `any_into_inner` unblocked; wave 2: existing suites exercise every migrated signature (call sites updated)
- [x] 2.3 Run tests and confirm they pass — wave 1: alloc 44/44, any 11/11, arc 5/5; wave 2: str_coverage/hashmap_extend/binary_io/events 9/9/observable 7/7/h2 6 suites green; determinism gate 22/22 at floor (adversarial). BigInt + 3 h2 suites' runtime validation gated on pre-existing K001s (phase27a, catalogued)
