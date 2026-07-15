# 03 — Codegen Stability: Invalid IR, Hangs, Crashes, Non-Determinism

Independent of the memory model (file 02), the backend itself fails on valid programs. The
two problem classes compound: memory-model fixes keep getting reverted because they trip
codegen bugs (phase24l Attempt 3 → K001 in unrelated files).

## F-005 — Codegen emits invalid LLVM IR (K001) that LLVM itself rejects, persistently

**Confidence: High. Impact: High.**

K001 = "Failed to parse LLVM IR" (`compiler/src/backend/llvm_backend.cpp:219,441`) — i.e.
the backend produces IR that fails LLVM's own parser/verifier. `.rulebook/PLANS.md:56-60`
lists K001 as a **standing** failure across:

- `c_preprocessor`
- `hir_types`
- `infer_differential`
- `std/collections` (**btreeset / btreemap / arraylist**)
- `c_frontend` (`Maybe[Heap[CBlockItem]]`)

Concrete instances root-caused and fixed one-by-one over the release history:
`List[Str]::push` mangling/typing mismatch (CHANGELOG 0.3.46), `TemplateLiteral.as_str()`
inference (0.3.38), struct forward-ref (0.3.20–0.3.25).

That the **collection suites** are among the persistent K001 failures is directly relevant to
any real application — those are the exact types a database's store is built from.

## F-006 — Compiler hangs (X002) and crashes (X003) on valid programs

**Confidence: High. Impact: High.**

`PLANS.md` standing failures include:

- `builtins_imports` — X002 timeout
- `slice_split_pred` — X002 timeout
- `let_patterns` — X002 timeout
- `other/closure_codegen` — X003 crash / X002 timeout

A compiler that non-terminates or crashes on `let_patterns` and closures — **core language
features** — cannot be trusted for a large codebase. Every hour of application development
risks tripping one of these with no workaround other than restructuring unrelated code.

## F-007 — Generic monomorphization and method-dispatch mangling are fragile

**Confidence: High. Impact: High.**

Documented dispatch/monomorphization defects from the release history:

- `List[Str]::push` — impl-level type-param re-inference minting a bogus `push__I64` symbol
  (0.3.46).
- `Heap[T]::new` — struct-by-value ABI crash for queued generic impls; fixed with a
  "struct→ptr ABI fixup" (0.3.39, 0.3.x).
- "Small-enum-method dispatch mismatch" and "`List[Shared[T]]` duplicate return-type mistype"
  (0.3.51 patch notes).

Generic collections of user-defined structs — the bread and butter of application code — are
exactly where these surface. This is also why memory-model fixes keep bouncing: phase24l
Attempt 3 changed `Shared.get`'s signature and immediately broke monomorphizations of four
unrelated generic users with K001.

## F-008 — Crashes are non-deterministic (heap-layout dependent), making them undebuggable for app authors

**Confidence: High. Impact: Critical (for adoption).**

Determinism is measured in fractions across the phase24 repro fixtures:

- `sig_alone.c`: went 0% → 60% → 10/10 across phases (fixed only after multiple iterations).
- `c_essential_repro.c`: sits at **28/30**.
- `essential.c`: **0/5** — never passes.
- phase24l §2.2 notes the repro "is sensitive to Windows heap layout."

An application developer cannot ship — or even bisect — a bug that appears in 2 of every 30
runs and moves with allocator state. This is the property that most directly turns "the
language works in a demo" into "the app is abandoned": UzDB's spike phase died here.

## What "fixed" must mean

For files 02 and 03 together, the only honest definition of done is statistical:
**N consecutive clean runs under an adversarial allocator** (guard pages / ASan-style
poisoning / allocation shuffling), not "it passed once on my machine." See file 06,
Phases A2/C for the concrete gates.
