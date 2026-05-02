# Proposal: phase24d_heap-borrow-drop-structref

## Why

Phase24c fixed the typedef-as-func-param case via a localized
`Heap[T]::into_raw()` workaround in `base_to_ctype`'s Typedef arm.
The same Heap-borrow-drop bug class still affects:

1. `base_to_ctype` StructRef/UnionRef/EnumRef arms — `return
   CType::Struct(env.structs.get(tag))` etc. The returned `Heap`
   flows into the CType variant payload; whenever the resulting
   CType eventually drops, its Heap payload's drop frees the
   allocation that `env.structs` still references. Subsequent
   `struct Foo` lookups crash.
2. `essential.c` self-compile
   (`./build/debug/cc_driver.exe compiler/runtime/core/essential.c
   -I compiler/runtime/include/c-stdlib --emit=ast`) segfaults at
   exit 139 — likely hits the StructRef pattern above plus other
   unfixed constructs.
3. Any `HashMap[K, Heap[V]]` usage anywhere in the codebase has the
   same hazard if `get` is called and the result lives past the
   bucket entry's lifetime.

The Typedef arm fix used `into_raw()` to null the local's `ptr`
and skip drop. That works because the value flows out as a CType
(plain by-value, no nested Heap). For StructRef/UnionRef/EnumRef
the `Heap` itself is moved into the returned `CType::Struct(h)`
variant — `into_raw()` would break the variant's heap-pointer.

The structural fix needs one of:

(a) **Rc-style `Heap[T]`** — refcount in the allocation header.
    `mem_free` is gated by `refcount == 0`. `HashMap.get` cloning
    increments. Local drop decrements.

(b) **Non-owning `HashMap.get` for Heap-valued maps** — return
    `*T` (raw pointer) instead of `Heap[T]`, and require callers
    to wrap into a non-owning view before use. Or return `ref
    Heap[T]` (borrowed reference).

(c) **HashMap that owns + lends** — separate ownership from
    access. `get` returns a borrow that disallows drop. Needs
    language-level borrow tracking that TML doesn't currently
    enforce.

Option (a) is the most invasive but the cleanest semantic. Option
(b) is more surgical: define a `HashMap.get_borrowed` or change
the existing `get` signature for Heap-valued maps. Option (c) is
out of scope without lifetime annotations.

## What Changes

1. **Audit** all `HashMap[K, Heap[V]].get(...)` call sites in the
   codebase. Identify which ones move the Heap into a longer-lived
   structure (vs the Typedef-arm pattern which only reads).
2. **Apply localized `into_raw()` workarounds** to the immediate
   blockers in `compiler-tml/src/cc/types.tml`:
   - `StructRef` arm: reconstruct the Heap from the bucket's
     pointer before the move so the local's drop is a no-op.
   - Same for `UnionRef` and `EnumRef`.
3. **Decide on the long-term fix** (a/b/c above) in a design doc.
   Proposal: option (b) — add `HashMap.get_ref(key) -> ref V` (or
   `*V`) for use cases that don't transfer ownership. Migrate
   callers gradually.
4. **Verify essential.c self-compile** progresses past the
   StructRef crash class. Document the next gap as a separate
   task entry.

## Impact

- Affected specs: language design — `Heap[T]` ownership semantics
  + `HashMap.get` return-type conventions. Not a breaking change
  if option (b) is purely additive (`get_ref` alongside `get`).
- Affected code:
  - `compiler-tml/src/cc/types.tml` — StructRef / UnionRef /
    EnumRef arms.
  - `lib/std/src/collections/hashmap.tml` — possibly add
    `get_ref` or `get_borrowed`.
  - Any other `HashMap[K, Heap[V]]` users that hit the same
    hazard.
- Breaking change: NO if additive; YES if `get` semantics change.
- User benefit: unblocks `tml cc essential.c` and the broader
  self-compile path (phase24 Phase 4). Removes a class of latent
  use-after-free bugs across the codebase.
