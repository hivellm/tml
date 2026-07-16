# phase26d Wave 2 — F-020 `ref` Migration: Complete Work-List

Read-only enumeration, 2026-07-16. Implementation input for items 1.4–1.7.

## Convention verified (decides the cost)

**Explicit `ref` IS required at call sites** — verified against existing callers:
`Text::compare(ref b)`, `Buffer::compare(ref buf)`, `List::extend(ref other)`.
So every migrated signature requires editing its call sites: `func(x)` → `func(ref x)`.
Total: **~85–100 sites**.

## Clusters (signature → new signature; all bodies verified read-only unless noted)

### 1. BigInt operators — `lib/std/src/bigint.tml` (14 sigs, ~50 sites)
**All inherent methods, NOT trait impls** (only Display/PartialEq/Drop are impl'd) — no
trait-signature constraint blocks the migration.

| Line | Method | New param |
|------|--------|-----------|
| 265 add · 290 sub · 300 mul · 351 div · 357 rem · 363 divmod · 566 gcd · 638 mod_inverse · 824 bitand · 841 bitor · 862 bitxor | `other: ref BigInt` |
| 614 mod_pow | `exp: ref BigInt, modulus: ref BigInt` |

Internal call sites (bigint.tml): 292, 352, 358, 411, 422, 527, 528, 557, 559, 570,
617, 619, 625, 627, 629, 630, 640, 646, 648, 651, 661, 675, 690, 704, 720, 725, 729,
733, 747, 759, 767, 796, 802, 814, 816 (35).
Test sites (`lib/std/tests/math/bigint.test.tml`): 34, 42, 50, 51, 59, 67, 70, 78, 79,
96, 153, 164, 170, 207, 215, 223 (16). All args are locals — safe for `ref`.

### 2. str::join / concat_all — `lib/core/src/str/convert.tml` (2 sigs, 6 sites)
- :130 `join(parts: ref List[Str], separator: Str)`
- :200 `concat_all(parts: ref List[Str])`
Call sites: `tools/ir_diff/src/normalizer.tml:180`, `samples/05-strings/string-ops.tml:83`,
`lib/core/tests/str/str_coverage.test.tml:59,68,77,90`.

### 3. HTTP/2 Buffer family (~12 sigs, 17 sites)
- `server.tml:169` `h2_buf_append(dst: mut ref Buffer, src: ref Buffer)`; callers :152, :161
- `connection.tml:776` `h2_conn_append_buf(dst: mut ref Buffer, src: ref Buffer)`; caller :573
- Payload handlers (connection.tml): 471, 519, 562, 578, 590, 606, 642, 660, 683, 702 → `payload: ref Buffer`
- frame.tml: 192 `H2Frame::new`, 252 `H2Frame::data`, 313 `h2_encode_frame_raw`, 533 `h2_decode_settings` → `payload: ref Buffer`

### 4. Remaining stdlib (5 sigs, ~8 sites)
- `hashmap.tml:527` `extend_from(this, keys: ref List[K], values: ref List[V])`
- `console.tml:337` `table(items: ref List[Str])`
- `file.tml:232` `write_bytes(this, data: ref Buffer)`; `:294` `write_all_bytes(path: Str, data: ref Buffer)`
- `controller.tml:255` `register_all(app: mut ref App, controllers: ref List[...])`

### 5. Events/reactive (5 sigs, ~2-5 sites)
- `promise.tml:411,455,475,503` `promises: ref List[Promise[I32]]`
- `observable.tml:142` `list: ref List[I32]`

## Intentionally NOT migrating
1. **Subject state functions** (observable.tml `subject_*`/`behavior_subject_*`/
   `replay_subject_*`): interior-mutability state-bag pattern (mutation via `List::set`
   through the handle), not ownership transfer. Migrating would change semantics.
2. **`H2Frame.payload: Buffer` struct FIELD** (frame.tml:187): struct fields cannot be
   `ref` types — needs the borrow-accessor language surface (F-021 → phase26e).

## Execution order (dependency-safe)
1. str::join/concat_all → 2. HashMap::extend_from → 3. console::table →
4. File::write_bytes/write_all_bytes → 5. HTTP/2 family → 6. Promise/Observable →
7. BigInt (highest volume, parallelizable after signatures).

Per-cluster gate: `/check` file → affected `/test` suite → grep-verify all call sites
carry `ref` → update `///` doc examples in the changed files. Final: full-suite +
determinism gate before marking items done.
