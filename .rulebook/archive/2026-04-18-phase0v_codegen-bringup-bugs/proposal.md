# Proposal: fix the 9 compiler bugs discovered during phase23b C-frontend bring-up

## Why

Phase 23b implemented the full C17 frontend in TML (~3,800 LOC of
`.tml`). Every module type-checks and the component-level tests pass,
but bringing up the parser and lowerer surfaced **9 concrete compiler
bugs** in the TML compiler itself — three parser / language-design
issues and six actual codegen bugs that cause the TML-compiled frontend
to crash at runtime on non-trivial inputs.

Today those 9 bugs are worked around inline in `parser.tml`, `types.tml`
and `lower.tml` — each workaround is a tax on readability, and three
of them (items #7, #8, #9) directly block phase24 (`tml cc` CLI
integration) because they prevent the TML-compiled frontend from ever
running against a real C source file.

This task fixes the root causes in the C++ compiler (`compiler/src/`)
and removes the inline workarounds so the phase23b modules can be
written in the natural, unworkarounded form.

## What Changes

Each of the 9 bugs gets a targeted fix in the C++ compiler with a
failing reproducer added to `compiler/tests/compiler/` before the fix
and a passing assertion after. The bugs are listed below in the order
they surfaced during phase23b bring-up (with note numbers from the
phase23b tasks.md bring-up sections).

### Lexer bring-up bugs (phase23b Phase 1)

1. **`base` is a reserved keyword (`KwBase`)** — local variables and
   struct field names can't be called `base`. Fix: promote `base` from
   reserved-word to contextual-keyword (only recognised in the contexts
   where it actually has semantic meaning — inheritance `impl X with Base`
   etc.). Elsewhere it should lex as a regular identifier.
2. **`return StructName { ... }` inside an `if` confuses the parser** —
   the parser reads the `{` as the trailing block of the return statement
   rather than the struct literal body. Workaround is to bind to a typed
   local and `return local`. Fix: in the parser, when `return` is
   followed by an identifier that resolves to a type name, try the
   struct-literal parse path first.
3. **`HashMap.get(k)` returns `V` directly, not `Maybe[V]`** — the
   signature documented in `docs/` says `Maybe[V]` but the actual
   implementation returns the zero-value on miss. Either fix the
   signature (and rewrite every caller) or fix the implementation.
   Recommended: fix the implementation to return `Maybe[V]` and update
   the ~20 call sites that currently rely on the zero-value-on-miss
   behaviour.

### Expression / control-flow F64 phi bugs (phase23b Phase 1)

4. **`if-expr` with F64 branches emits phi nodes containing integer `0`** —
   K001 codegen bug. When both branches of an `if` expression produce
   `F64`, the phi node is typed `double` but receives `i64 0` as an
   incoming value. Fix: the `if`-expression lowering must track the
   result type and use `double 0.0` for the implicit-zero fallback
   (or better: not emit the fallback when both branches are reachable).
5. **`for _ in 0 to n { acc = acc * 10.0 }` has the same F64-phi K001
   issue** — the accumulator's phi incoming from the loop header uses
   `i64 0` instead of `double 0.0`. Fix is the same as #4 in the
   loop-lowering path.
6. **`Str.parse_f64 -> Maybe[F64]` loops forever at runtime** — the
   let-else pattern `let Just(v) = parse_f64(s) else { ... }` causes
   an infinite loop inside stdlib (observed during the phase 23b float
   decoder bring-up). Fix: isolate the loop with a minimal reproducer,
   trace through the stdlib `parse_f64` implementation and the
   `Maybe[F64]` codegen, fix the underlying K001.

### Enum / constructor codegen bugs (phase23b Phase 2 — BLOCKERS for phase24)

7. **Enum variants holding a `Heap[T]` followed by other fields fail
   codegen on pattern-binding the non-heap fields** — e.g.
   `Func(Heap[CDeclarator], List[CParam], I64)` rejects `Func(_, ps, _)`
   with "Unknown variable: ps". The codegen appears to only wire the
   pattern bindings for the leading Heap field; subsequent fields are
   not emitted as extractable bindings. Fix: codegen for enum pattern
   matching must emit extractvalue instructions for every non-wildcard
   pattern binding, not just the leading one.
8. **Deeply-nested constructor expressions in a single statement hang
   or crash at runtime** — e.g.
   `decls.push(Heap[CDecl]::new(CDecl::Var(Heap[CVarDecl]::new(vd))))`.
   The duplicate codegen path recurses through each nested enum/struct
   payload; when a deeply-nested `Heap` wrap is interleaved with
   variant constructors the recursion appears to hit a bug that
   manifests as infinite loop or null-pointer crash. Fix: track the
   underlying recursion limit or heap boundary and short-circuit
   duplicate at each `Heap` wrap.
9. **Large enums with by-value struct payloads crash on duplicate** —
   e.g. `CDecl::Var(CVarDecl)` where `CVarDecl` contains 7 nested
   fields. Duplicate of the enum walks through the variant's entire
   struct by value and crashes. Fix: enum duplicate must match the
   active-variant's layout, not the union of all variant bytes.

## Acceptance criteria

After this task lands:

1. Every workaround in `compiler-tml/src/cc/*.tml` flagged with
   "phase 1 bring-up note #N" or "phase 2 bring-up note #N" can be
   un-applied and the component test suite still passes.
2. The phase 23b parser can run against a non-trivial C source
   (e.g. a 50-line C snippet with function, struct, control flow, and
   expressions) without crashing.
3. New regression tests in `compiler/tests/compiler/` cover each of
   the 9 bugs so they don't re-appear.
4. No existing test regresses.

## Impact

- Affected specs: unblocks phase24 (cc CLI integration), indirectly
  helps every future TML self-hosting work.
- Affected code: primarily `compiler/src/codegen/` (bugs #4–#9),
  `compiler/src/parser/` (bugs #1–#2), `lib/core/src/collections/hashmap.tml`
  + callers (bug #3). Potential cleanup of Heap-wrapping workarounds
  in `compiler-tml/src/cc/*.tml` after fixes land.
- Breaking change: partial — bug #3's fix (HashMap.get returning
  Maybe[V]) is a breaking change to the stdlib API.
- User benefit: the TML-written compiler front-end becomes runnable;
  TML idioms (pattern binding, nested constructors, let-else) work
  uniformly rather than with ad-hoc workarounds.
