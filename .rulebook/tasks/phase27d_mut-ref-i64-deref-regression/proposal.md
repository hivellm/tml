# Proposal: phase27d_mut-ref-i64-deref-regression

## Why
A regression in the current working tree breaks the established `mut ref I64`
implicit-deref idiom **globally** across the stdlib. Committed functions that take
a `mut ref I64` parameter and use it as a value (read = deref, `x = x + 1` = write
through the ref) now fail to type-check.

**Evidence (both files committed & clean, using the identical established idiom):**
- `lib/std/src/http/app/app.tml:459` `app_register(count: mut ref I64)` — `if count >= MAX_HANDLERS`, `count + 1`, `count = count + 1` → **7× `error[T001]: ... found mut ref I64 and I64`** (+ T056). Log: `.sandbox/p41b_appcheck.log`.
- `lib/std/src/http/server/parse.tml:552` `app_pattern_match(pc: mut ref I64)` — `if pc < MAX_PARAMS`, `pc = pc + 1` → **3× T001/assignment errors**. Log: `.sandbox/p41b_parsefix_check.log`.

Both fail identically under the current `tml_compiler.dll`, which was built with
another session's **uncommitted** changes: `compiler/src/codegen/llvm/expr/method_impl.cpp`,
`compiler/src/codegen/llvm/expr/method_static_dispatch.cpp` (and `parser.hpp`,
precedence-only). This is the "method-dispatch fallout" surfaced during phase41b
(shared stdlib object) and phase41a (~111 pre-existing compile-failing test files).

## What Changes
Root-cause and fix the `mut ref I64` (and likely `mut ref T` generally)
implicit-deref type-checking / method-dispatch regression so `app_register`-style
usage type-checks again. This belongs to the session that owns the uncommitted
`method_impl.cpp` / `method_static_dispatch.cpp` changes — this task is a
COORDINATION RECORD with the evidence, NOT a directive to work around it in the
`.tml` sources (that would corrupt the established idiom and mask in-flight work).

Related / held: the `match`-as-identifier fix in `parse.tml` (`match`→`matched`, a
genuine committed-debt reserved-word bug) is **applied in the working tree but held
uncommitted** — `parse.tml` cannot pass a clean type-check until THIS regression is
resolved. Commit the `match` fix once the deref regression clears.

## Impact
- Affected specs: none
- Affected code: `compiler/src/codegen/llvm/expr/method_impl.cpp`, `method_static_dispatch.cpp`, type-checker ref/deref handling (owner: other session)
- Breaking change: NO (restores prior behavior)
- User benefit: unblocks ~111 files + the phase41b std/http path; restores the `mut ref` idiom
