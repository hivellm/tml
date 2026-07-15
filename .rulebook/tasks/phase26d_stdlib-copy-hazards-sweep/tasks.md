# phase26d — Stdlib Copy-Hazard Sweep (library-level, model-independent)

> The subset of the 08-memory-copy-audit findings (F-017/F-018/F-020) that are
> fixable at the LIBRARY level WITHOUT the ADR-009 model fix. Safe wins; runs in
> parallel with phase26b. Model-dependent findings (F-015/F-016/F-019/F-022) belong
> to phase26b step 4; the borrow-accessor gap (F-021) is a separate future task.
> Every change gated by: determinism corpus + affected suites + K002 verifier.

## 1. Implementation

### F-017 — broken move-outs that double-free unconditionally
- [ ] 1.1 `Arc::try_unwrap` (`lib/std/src/sync/arc.tml:283-318`): forget/null the source so `Arc::drop` cannot run on the freed allocation after the value is moved out. Regression: `tml run` probe that try_unwraps a unique `Arc[Shared[I64]]` and checks no double-free/leak
- [ ] 1.2 `AnyValue::into_inner` (`lib/core/src/types/any.tml:400-412`): null `this.data` after `dealloc` on BOTH match and mismatch paths so `AnyValue::drop` is a no-op. Regression test

### F-018 — Sync has no safe read accessor
- [ ] 1.3 Port `get_ref(this) -> ref T` and `get_clone(this) -> T where T: Duplicate` from `Shared` (`lib/core/src/alloc/shared.tml:177,207`) to `Sync[T]` (`lib/core/src/alloc/sync.tml`); document `get`'s copy hazard the same way `Shared::get` is documented. IR-verify `get_ref` returns a true borrow and `get_clone` has no dropping temp

### F-023 — try_unwrap frees ignoring weak refs
- [ ] 1.3b `Shared::try_unwrap` (`shared.tml:285-295`) and `Sync::try_unwrap` (`sync.tml:260`): `is_unique()` checks only `strong_count==1` and the free ignores `weak_count`, dangling any outstanding `SharedWeak`/`SyncWeak`. Fix: on strong→0 move the value out + decrement strong, and `mem_free` only when `weak_count` also reaches 0 (mirror `decrement_count`). Regression: downgrade → try_unwrap → weak.upgrade()/drop must not touch freed memory

### F-020 — pass-by-value MUST-BORROW → `ref` migration (one-token, idiom-matching)
- [ ] 1.4 BigInt operator cluster (`lib/std/src/bigint.tml`): `other: BigInt` → `other: ref BigInt` at add:265, sub:290, mul:300, div:351, rem:357, divmod:363, gcd:566, mod_pow:614 (exp+modulus), mod_inverse:638, bitand:824, bitor:841, bitxor:862. Bodies already borrow internally — verify no `.duplicate()` needed
- [ ] 1.5 `str::join`/`concat_all` (`lib/core/src/str/convert.tml:130,200`): `parts: List[Str]` → `ref List[Str]`
- [ ] 1.6 HTTP/2 Buffer family (`lib/std/src/http/h2/`): `h2_buf_append`/`h2_conn_append_buf` (server.tml:169, connection.tml:776) and the `payload: Buffer` handlers → `ref`/`mut ref` as appropriate (accumulator `dst` is `mut ref`, read-only `src`/`payload` is `ref`)
- [ ] 1.7 Remaining sites: `HashMap::extend_from` (hashmap.tml:527), `console::table` (console.tml:337), `File::write_bytes`/`write_all_bytes` (file.tml:232,294), events/reactive `List` params — migrate to `ref`, update all call sites

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
