# Proposal: phase24g_heap-rc-or-borrow-language-fix

## Why

Phases 24c, 24d, 24e, and 24f have surgically patched specific
sites of the same underlying bug: `Heap[T]` is a unique-owning
pointer (`impl Drop` calls `mem_free`), but it gets used as a
shared / borrowed pointer in many contexts:

1. `HashMap[K, Heap[V]].get(k)` — bucket entry and returned local
   share the allocation; default Drop frees the bucket's
   allocation. (phase24c/24d for `base_to_ctype`'s 4 arms).
2. Returning a `CType` value with nested `Heap[CType]` payloads
   from a function consumes the inner allocation when the value
   drops, even if the caller still references the same allocation
   through another path. (phase24e via `impl Duplicate for CType`).
3. `Heap[CDeclarator]` parameters in recursive declarator-walking
   functions share their allocation with the caller's nested
   variant payload. (phase24f for `declarator_name_heap`).

Each surgical fix uses one of:

- `into_raw()` to null the local Heap's `ptr` (skipping `mem_free`
  in the Drop impl).
- `Heap::from_raw(raw)` to reconstruct a Heap from the same
  pointer for the next consumer.
- `borrowed.duplicate()` to deep-clone the entire structure so
  the consumer owns an independent allocation.

These patches are fragile, error-prone, and don't compose. Every
new site that holds a `Heap[T]` borrowed from a longer-lived
container is a latent crash. `tml cc` on `essential.c` still
crashes intermittently because the surgical patches don't cover
every potential drop site (e.g.
`let td = CTypedefDef { declarator: dp.decl, ... }` — the
struct construction copies CDeclarator with shared nested Heap
pointers, and both `dp.decl` and `td.declarator` race to drop
them).

The structural fix is to upgrade `Heap[T]` semantics at the
language level. Three options:

(a) **Refcounted `Heap[T]`** — store `refcount: U64` in the
    allocation header. `impl Drop` decrements; `mem_free` only
    runs when `refcount == 0`. Every implicit copy of `Heap[T]`
    increments the count. Equivalent to `Rc<T>` in Rust.
- Pro: zero source changes for callers; existing `Heap[T]`
  uses become safe automatically.
- Con: 8-byte overhead per allocation; refcount thread-safety
  story (`Rc` vs `Arc`) needs a decision.

(b) **`Borrow[T]` distinct from `Heap[T]`** — owning vs non-
    owning at the type level. `HashMap.get_borrowed(k) ->
    Borrow[V]` returns a non-dropping view. Existing `get` keeps
    its move semantics (and the existing crash hazard) for
    backward compatibility.
- Pro: explicit at every call site, no runtime overhead.
- Con: invasive — every `Heap[T]` consumer that should be a
  borrow must migrate to `Borrow[T]`. Doubles the API surface.

(c) **Language-level lifetime/borrow tracking** — annotations
    similar to Rust's lifetime parameters that prevent dangling
    references at compile time.
- Pro: zero runtime cost, full safety.
- Con: very large language surface change; non-trivial type
  system extension.

## What Changes

Recommendation: prototype **option (a) — refcounted `Heap[T]`** —
as the path of least resistance. Implementation outline:

1. Update `lib/core/src/alloc/heap.tml`:
   - Allocation header layout: `[refcount: U64][T payload]`.
   - `Heap::new(value)` allocates header + payload, sets
     `refcount = 1`, returns `Heap { ptr: payload_addr }`.
   - `Heap::clone()` (new explicit method, OR auto-Duplicate
     fallthrough): atomic-increment refcount, return same `ptr`.
   - `impl Drop for Heap[T]`: atomic-decrement refcount; if it
     hits 0, run user-Drop on T then `mem_free` the header.
2. Update HashMap / List / Buffer drops to use the new semantics
   (no source change required if the impls already call
   `Heap.drop`).
3. Remove the surgical `into_raw()` / `from_raw()` patches in
   `compiler-tml/src/cc/types.tml` (phase24c/24d/24e) and
   `compiler-tml/src/cc/parser.tml` (phase24f). Remove the
   `impl Duplicate for CType` deep-clone path — replaced by the
   automatic refcount.
4. Verify `./build/debug/cc_driver.exe
   compiler/runtime/core/essential.c -I
   compiler/runtime/include/c-stdlib --emit=ast` exits 0
   deterministically across 10 consecutive runs.
5. Run the full compiler test suite to confirm no regressions
   (the existing 299/318 baseline must not drop).

Bench impact: every Heap allocation gains an 8-byte header.
For a typical compile that allocates ~100k Heap objects, this
adds ~800KB peak memory. Acceptable.

Atomicity story: TML doesn't yet have multi-threaded Heap
sharing (cc_driver is single-threaded). Use non-atomic
increment/decrement initially; revisit if multi-threaded sharing
becomes a requirement (Arc-style upgrade).

## Impact

- Affected specs: language design — `Heap[T]` ownership semantics
  shift from unique-owning to refcounted-shared. Change is
  backward-compatible at the source level (callers don't see the
  refcount).
- Affected code:
  - `lib/core/src/alloc/heap.tml` — allocation header, Drop, new
    explicit `clone()` method.
  - `compiler-tml/src/cc/types.tml` — remove `into_raw()` /
    `from_raw()` / `duplicate()` workarounds added in
    phase24c/24d/24e.
  - `compiler-tml/src/cc/parser.tml` — remove
    `declarator_name_value_leak` workaround added in phase24f.
  - Possibly other Heap[T] consumers across `lib/` and
    `compiler-tml/`.
- Breaking change: NO at source level; YES at ABI level (Heap
  layout changes — every existing C-runtime consumer that touches
  Heap pointers must be reviewed).
- User benefit: closes the entire Heap-borrow-drop bug class in
  one shot. Unblocks `tml cc essential.c` and the broader
  self-compile path. Removes a class of latent use-after-free
  bugs across the compiler-tml codebase.
