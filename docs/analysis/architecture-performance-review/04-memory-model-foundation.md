# 04 — Memory Model Foundation

## Overview

TML's memory safety model couples performance and correctness: Rust-style RAII was built over raw `*T` smart pointers with no move/init state tracking, creating a double-free/use-after-free class that required 14+ phases of band-aids. Each band-aid added copies, making the system slower.

---

### F-015 — Rust-style RAII built over raw `*T` smart pointers with no move/init state surviving to codegen

**Impact: Very High**  
**Status: Mostly RESOLVED (phase26), residuals OPEN**

ADR-009 (`docs/adr/ADR-009-memory-model-soundness.md:37-44`):

> "TML ships Rust-style RAII… with no move/init tracking that survives to codegen, over smart pointers built on raw `*T`. Result: a systematic double-free / use-after-free class. Fourteen phases of per-site workarounds did not converge."

**The architectural problem:**

1. **Frontend (borrow checker)** computes init/move state (`compiler/src/borrow/checker.hpp:799-816`, path not verified)
2. **Frontend discards that state** before lowering to codegen
3. **Codegen** has no information about whether a `*T` is initialized, moved, or should be dropped
4. **Result:** double-free when drop-glue runs on a moved value; use-after-free when a reference extends past a drop

**Performance consequence:**

The band-aids applied were *added copies*:
- `into_raw` / `from_raw` chains around move points
- `.duplicate()` deep clones
- Bitwise `Shared.get` copies that violate ownership invariants

These are the opposite of zero-cost abstraction.

**Real fix (phase26b-f):**

The real solution ported the borrow checker's already-computed init-state facts into AST codegen. This eliminated most of the band-aids.

**Residuals:**

Open issues remain in F-016 and F-017, traced back to this foundation.

---

### F-016 — Clone-read and drop are asymmetric for handle-bearing aggregates

**Impact: High**  
**Status: OPEN (phase44b 1.3b/1.3c)**

`.rulebook/tasks/phase44b_collections-standalone-heap-corruption/tasks.md:156-185`

**The bug:**

`compiler/src/codegen/llvm/builtins/intrinsics.cpp:854` takes a documented bitwise-copy fallback:

```
if (!gen_structural_duplicate(type_name)) return raw;
```

This fallback runs *even when `needs_clone` was true*, violating the invariant: **clone-read and drop must be symmetric**.

**Consequence — two crash faces:**

1. `Deque::iter()` — field is a heap-owning `List[T]`. Bitwise-copies the buffer, then drop-glue frees it. Result: **use-after-free** (UAF).
2. `List::iter()` — mirror case. Duplicates but never drops. Result: **refcount leak**.

**Live status:**

This is a residual of the F-015 foundation. The root cause is that `needs_clone` is computed (correctly), but the codegen path has a fallback that ignores it.

---

### F-017 — Hand-rolled memory code whose byte counts drift from the real type

**Impact: Medium (heap corruption masked as flaky)**  
**Status: OPEN (phase44c lint proposed)**

`.rulebook/tasks/phase44c_hand-rolled-alloc-size-lint/tasks.md`

**Examples of the bug:**

- `mem_alloc(8)` for a 12-byte `SharedInner` — a `weak_count` field was added later; the literal wasn't updated
- `shrink()` delegating to `grow()` with `mem_copy(old_layout.size())` — copies 64 bytes into a 16-byte buffer

**Consequence:**

Both cases silently corrupted the heap for *whichever unrelated test allocated next*. They survived months as "machine load" or "flaky tests" because the corruption manifested in different test suites or reruns (`.rulebook/tasks/phase44b_collections-standalone-heap-corruption/tasks.md:56-114`).

**Why this happens:**

The tension between "minimize C code, prefer TML" (good) and "hand-rolled `lowlevel` blocks everywhere for performance" (pragmatic) creates a maintenance burden: byte counts must stay synchronized manually.

**Proposed solution:**

Phase44c proposes a lint that checks `size_of[T]` at allocation sites. Catching these at compile time would prevent months of nondeterministic corruption.

---

### Summary: The memory-model debt

The three issues (F-015/F-016/F-017) are symptoms of a foundational mismatch: Rust-style RAII + ownership on raw pointers, with the type system and borrow-check information *discarded* at codegen time.

- **F-015** is the root (information loss)
- **F-016** is a residual bug (needs_clone ignored)
- **F-017** is a maintenance hazard (manual byte counting)

All three couples performance (band-aids = added copies) with correctness (UAF, double-free, heap corruption). Fixing the foundation eliminates the need for band-aids and closes the crash classes.
