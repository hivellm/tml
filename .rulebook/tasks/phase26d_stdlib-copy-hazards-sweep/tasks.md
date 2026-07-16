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
- [ ] 1.4 BigInt operator cluster (`lib/std/src/bigint.tml`): `other: BigInt` → `other: ref BigInt` at add:265, sub:290, mul:300, div:351, rem:357, divmod:363, gcd:566, mod_pow:614 (exp+modulus), mod_inverse:638, bitand:824, bitor:841, bitxor:862. Bodies already borrow internally — verify no `.duplicate()` needed
- [ ] 1.5 `str::join`/`concat_all` (`lib/core/src/str/convert.tml:130,200`): `parts: List[Str]` → `ref List[Str]`
- [ ] 1.6 HTTP/2 Buffer family (`lib/std/src/http/h2/`): `h2_buf_append`/`h2_conn_append_buf` (server.tml:169, connection.tml:776) and the `payload: Buffer` handlers → `ref`/`mut ref` as appropriate (accumulator `dst` is `mut ref`, read-only `src`/`payload` is `ref`)
- [ ] 1.7 Remaining sites: `HashMap::extend_from` (hashmap.tml:527), `console::table` (console.tml:337), `File::write_bytes`/`write_all_bytes` (file.tml:232,294), events/reactive `List` params — migrate to `ref`, update all call sites

> **Wave 1 (F-017/018/023) COMPLETE — v0.3.56.** Wave 2 = the F-020 `ref` migration
> (1.4–1.7 below), a broad mechanical change left for a follow-up run.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [x] 2.1 Update or create documentation covering the implementation — hazard docstrings on `Sync::get`, doc comments on the new accessors + try_unwrap weak logic; patch notes `docs/patches/v0.3.56.md`
- [x] 2.2 Write tests covering the new behavior — `shared_try_unwrap_weak`, `sync_try_unwrap_weak`, `sync_get_accessors` (new); `any_into_inner` unblocked with 3 real tests
- [x] 2.3 Run tests and confirm they pass — alloc 44/44, any 11/11, arc 5/5, determinism gate 18/18 at floor (wave-2 items 1.4–1.7 remain)
