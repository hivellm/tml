<!-- PLANS:START -->
# Project Plans & Session Context

This file is a **persistent session scratchpad** maintained by AI agents.
It provides continuity across sessions without relying on conversation history.

## How to Use

At the **start of each session**: Read this file to understand current context.
During the **session**: Update with decisions, discoveries, and progress.
At **session end**: Write a summary to the Session History section.

## Active Context

<!-- PLANS:CONTEXT:START -->
## ⚡ STRATEGIC PIVOT (2026-07-15) — Stabilization ERA 0 (READ FIRST)

User decision after the state-of-language analysis
(`docs/analysis/tml-table-analysis/` — motivated by the abandoned UzDB app):

1. **`feat/self-hosting-compiler` merges to main AS-IS** — self-hosting is
   **paused by strategic decision, not finished** (stated in README). A new
   branch off main hosts the stabilization work.
2. **Task board reorganized** (`TASKS-INDEX.md`): new ERA 0 tasks
   phase25a/25b (determinism harness + LLVM verifier CI) →
   phase26a/26b/26c (ADR-009 memory-model decision → implementation →
   band-aid revert) → phase27a/27b (K001 / X002-X003 root-cause) →
   phase28a/28b (UzDB-core acceptance soak + async-file hardening).
3. **Re-sequenced (2026-07-15, user request)**: one continuous future timeline —
   registry → phase29a/b (was 38a/b, FIRST to unfreeze), native/self-hosting →
   phase30–33 (was 31–34), C++ frontend → phase34a (was 23c), features 35–37
   unchanged, toolchain → phase38a–d (was 38c–f), 39a unchanged. Renamed
   proposals note their old ids; analysis docs cite pre-resequence ids.
4. **Archived (superseded)**: phase0z, phase24i–n band-aid line — per-site fixes
   proven non-convergent (phase24l Attempt log); the class closes at the root in
   phase26. Repros live in the phase25a corpus; dirs in archive/2026-07-15-*.
5. Progress: **phase25a DONE (v0.3.53)**, **phase25b DONE (v0.3.54)**,
   **phase26a DONE — ADR-009 Option B3 ACCEPTED by user 2026-07-15** (rulebook
   decision #10). Key spike findings: (a) F-013 bleed REAL on user path
   (`tml run` probe: nested count 2→1→−1, silent UAF); (b) tests compile via
   query/MIR pipeline, user builds via AST-legacy fallback — **the 12k-test
   suite validates a path real programs never run** (testing_compile.cpp:69-74
   vs build.cpp:413).
6. **ADR-009 REVISED 2026-07-15 → B1-on-AST** (rulebook decision #11, supersedes
   the same-day B3). Step-2 scoping refuted B3's premise: query_core.cpp:931
   proves the test framework uses the SAME AST gate as build.cpp — tests AND
   users run the AST path for stdlib/generic programs; MIR is unused there. B3's
   "unify onto the already-tested MIR path" would be an ~8,000-LOC MIR codegen
   project, not routing. So: **fix the AST-legacy path directly** (what everyone
   runs); defer MIR unification to phases 30-33 (frozen).
7. **phase26b PROGRESS (B1-on-AST)**: Step 1 DONE (v0.3.55 F-013). Step 2.1-2.3
   DONE (v0.3.57 — borrow facts exported, def-span join proven 100%; DISCOVERY:
   move_value is DEAD CODE — checker never tracked moves, root of F-015).
   **User decision #12 (2026-07-16)**: close concrete bugs first; Step 2.4 +
   3.4 + Step 4 deferred to phase26f milestone. Step 3 DONE (v0.3.58 — 13
   F-016 read-out sites → balanced clones/borrow-pred retain, Arc::make_mut
   deep-clone, F-022 destroy releases elements; 4 new corpus canaries).
8. **phase26d COMPLETE + ARCHIVED (2026-07-16)**: wave 1 (v0.3.56 —
   F-017/018/023) + wave 2 (v0.3.59 — F-020 ref migration, ~120 edits:
   BigInt/str::join/extend_from/File/events/HTTP-2/controller; consuming
   constructors kept by-value; console::table reverted → pre-existing K001).
9. **ALL CONCRETE MEMORY BUGS CLOSED** (F-013/016/017/018/020/022/023,
   v0.3.55-59). Remaining: F-015 moves (phase26f, IN PROGRESS — agent on
   items 1.1/1.2: activate move_value behind TML_STRICT_MOVES, measure blast
   radius), F-021 borrow accessors (26e, after 26f), band-aid revert (26c).
10. **10 pre-existing failures newly verified-at-HEAD + catalogued** in
   scripts/known-failures.txt during v0.3.58/59 verification (9 K001 + 1
   X002) — direct input for phase27a/27b root-causing (6 fresh K001
   specimens now listed in phase27a tasks.md).

Historical context below describes the pre-pivot phase24 grind.

---

Last completed (2026-05-01): phase24b_cc-typedef-name-resolution
items 1–5 done. Fix selected option (a) from proposal — type-system
entry points in `compiler-tml/src/cc/types.tml` and callers in
`compiler-tml/src/cc/lower.tml` take `env: ref CTypeEnv` instead of
by-value `CTypeEnv`. Eliminates the value-pass + drop-glue path that
freed the caller's HashMap buckets on `base_to_ctype` callee exit.
Regression `test_phase24b_base_to_ctype_typedef_repeat` lands in
`c_frontend.test.tml`. Compiler suite: 299/318 (baseline preserved).
phase24c/24d/24e all shipped:
- 24c (v0.3.41): Typedef arm Heap-borrow-drop fix via `into_raw`.
- 24d (v0.3.42): same pattern in StructRef/UnionRef/EnumRef arms.
- 24e (v0.3.43): replaced `into_raw`+`from_raw` aliasing hack with
  proper deep-clone via `impl Duplicate for CType` + `@auto(duplicate)`
  on CFuncType / CArrayType / CAggregateField / CAggregate /
  CEnumValue / CEnumType. The env's bucket and consumer's CType are
  fully decoupled — no double-free hazard regardless of drop order.

cc_driver now parses every typedef/struct/union/enum-as-param
shape we tested (`typed_simple`, `typed_two`, `typed_ptr`,
`struct_ref`, `union_ref`, `enum_ref`, `test_no_inc`). All exit 0
with `cc_driver: parsed`. 5/5 regression tests pass.

phase24f shipped (v0.3.44, partial): `declarator_name_heap` had
the same Heap-borrow-drop pattern at the parser level. Replaced
the recursive `declarator_name → declarator_name_heap →
declarator_name` loop with `declarator_name_value_leak` using
`into_raw()` at every Heap layer. `sig_alone.c` improved from
0% → 60% deterministic; full determinism requires structural
Heap[T] refcounting.

Filed `phase24g_heap-rc-or-borrow-language-fix` as the structural
fix: upgrade `Heap[T]` from unique-owning to refcounted-shared in
`lib/core/src/alloc/heap.tml`. Closes the entire Heap-borrow-drop
bug class in one shot; surgical phase24c/24d/24e/24f patches can
then be reverted in favor of the simpler refcount semantics.
`essential.c` self-compile remains gated on phase24g.
Branch: feat/self-hosting-compiler. Version: 0.3.40.
coverage_cli.exe now builds and runs end-to-end — `--format=all` on the
golden LCOV fixtures materialises LCOV + JSON + Cobertura XML + HTML
SPA and exits 0.
Pre-existing failures (unchanged, pre-session): c_preprocessor K001,
hir_types K001, infer_differential K001, core/any T056, std/collections
K001 (btreeset/btreemap/arraylist), builtins_imports X002 timeout,
slice_split_pred X002 timeout, let_patterns X002 timeout,
other/closure_codegen X003/X002, c_frontend K001 (Maybe[Heap[CBlockItem]]).
<!-- PLANS:CONTEXT:END -->

## Current Task

<!-- PLANS:TASK:START -->
phase24l shipped (partial): added `Shared.get_clone(this) -> T where
T: Duplicate` to `lib/core/src/alloc/shared.tml` (new opt-in method,
preserves existing `.get()` bitwise semantics). Migrated ONLY the
typedef arm in `compiler-tml/src/cc/types.tml::base_to_ctype` to use
it. Closes `c_essential_repro.c` minimal trigger (28/30 vs 29/30
baseline; within noise band). Preserves sig_alone 10/10, `int (*p);`
30/30, `typedef void (*sig_t)(int);` 30/30, compiler suite 290/295.

essential.c x 5 = 0/5 NOT met. Per task spec (phase24l 3.1), VERSION
bump deferred to phase24m. ADR #9 records the hybrid (a)+(b) decision
with full attempt log. phase24m filed as the residual essential.c
follow-up — covers options (a)+audit, (b) compiler-codegen, or (c)
HashMap.get specialization.

Phase24l attempts that REGRESSED and were reverted:
- Broader (b) migration to ~40 call sites in cc/types.tml + lower.tml
  + parser.tml: minimal repro fell to 25/30.
- Pure (a): change `Shared.get` to `where T: Duplicate { value.duplicate() }`:
  REGRESSED lib/core (cache_aligned_box, cache, cache_soavec_set,
  future_fuse fail K001).

Active follow-up: `phase24c_cc-driver-runtime-link` to restore
`tml build cc_driver.tml` linking `lib/std/runtime/file.c`. Once
that lands, phase24b items 4.1 / 4.2 (end-to-end `tml cc`
verification) become runnable in CI.

Old fix-candidate analysis preserved here for context (superseded
by option (a) ship): proposal.md candidates were
(a) `env: ref CTypeEnv` everywhere — SHIPPED;
(b) codegen elides field-level drops on a value-passed local
when the field is a single pointer; (c) explicit borrow semantics
for struct value parameters.

Next active task: phase24b_cc-typedef-name-resolution Phase 3
(codegen / type-system fix). Secondary option: phase0w Phase 10
(delete C++ HTML generator — destructive, awaits user OK).
<!-- PLANS:TASK:END -->

## Session History

<!-- PLANS:HISTORY:START -->
### 2026-07-19
phase26c_memmodel-bandaid-revert COMPLETED + ARCHIVED (v0.3.80, commit f379019f). (1) Inventory: headline phase24 band-aids (into_raw/from_raw chains, declarator_name_value_leak) were already deleted in v0.3.45; live in-scope remnants were API-surface only — Shared.get still the old unsound bitwise copy + hazard docstring, Shared.get_ref on the mis-typing ptr_as_ref, historical narrative comments in sync/heap/hashmap/list. Frozen compiler-tml mass (180 .duplicate(), CollectedArgs, 10 manual impl Duplicate) enumerated for record, untouched. (2) API decision: Shared.get folded to the sound ptr_read_clone pattern (Sync.get-identical), get_ref onto ref (*this.ptr).value; get_clone/get_ref KEPT as documented API; ptr_read_clone intrinsic KEPT as the canonical mechanism; all dead-bug-class hazard docstrings removed. (3) New regression tests (shared_get_sound.test.tml) EXPOSED a real codegen hole: ptr_read_clone[T] was silently bitwise for user aggregates with handle fields but no explicit/derived Duplicate — fixed via structural clone-glue synthesis symmetric with drop-glue (gen_structural_duplicate in derive/duplicate.cpp, drop-glue predicate in intrinsics.cpp, suppress_field_drops fix in stmt_let). The standalone-pass/suite-fail appearance was a HARNESS FALSE-PASS (standalone dispatcher emits test_pass on panicking bodies) — filed phase44a_test-dispatcher-false-pass. (4) Gates on rebuilt compiler: determinism adversarial ×100 = 28/28 at floors; std/collections consolidated 96/96 — btreemap/btreeset/arraylist K001-FREE (era-0 Phase B gate CLOSED; phase27a's in-flight Class 2 fix at d39333dd satisfied the dependency); core/alloc 44/45 (documented floating flaky); str/hash/json/borrow zero divergence. Learning captured in rulebook memory ("why per-site band-aids never converged"). NEXT: phase27a formally still pending (Class 2 partially landed at HEAD; known-failures.txt lines for collections now stale and should be curated when phase27a closes); phase26e (borrow accessors) and phase44a queued.

### 2026-05-04 — phase24k_essential-cleanup-segv DIAGNOSED (filed phase24l)

### Accomplished

Bisected the residual `essential.c × 5 = 0/5` SIGSEGV down to a 3-line
minimal reproducer:

```c
typedef void (*sig_t)(int);
sig_t f(int sig, sig_t handler);
int main(void) { return 0; }
```

Saved as `compiler-tml/tests/native/c_essential_repro.c` with full
trigger-pattern documentation in the file header.

### Root cause precisely identified

`Shared.get(this) -> T` (`lib/core/src/alloc/shared.tml:126`) returns
the inner T by bitwise copy via `(*this.ptr).value`. When T contains
nested `Shared[...]` fields (e.g., the recursive `CType` enum), those
nested Shareds are aliased into the returned value WITHOUT bumping
their refcounts. When the returned value drops, its drop-glue
decrements those refcounts to 0, freeing the env's stored
sub-allocations. Subsequent typedef lookups operate on the resulting
dangling pointers.

Crash trace pinpointed the failure to recursive `lower_type` calls
on the SECOND typedef use in a single function declaration. First
use of `sig_t` (return type) succeeds; its CType drop frees the
env's `Shared[CFuncType]`. Second use (param) returns CType wrapping
the now-dangling pointer; `lower_type`'s `when t {` discriminant
read crashes.

### Fix attempts (both reverted)

1. `let v = ...get(); return v.duplicate()` in `base_to_ctype` typedef
   arm. Result: minimal repro near baseline; essential.c REGRESSED
   from 5/5 → 30/30 crashes.
2. `expand_typedef_value(h: Shared[CType]) -> CType` helper that
   manually walks the variant calling `inner.duplicate()` per Shared
   field. Result: minimal repro fixed (0/30); but essential_top50.c
   regressed 2/10 → 10/10. essential.c stayed 5/5.

Per fail-twice rule + T0 (no "blocked"), filed
`phase24l_shared-get-aliasing-deep-fix` for the structural fix.
Three options proposed: (a) make `Shared.get()` deep-clone for
`T: Duplicate`, (b) audit-and-fix every `.get()` call site in
compiler-tml/src/cc, (c) compiler-codegen-level fix.

### Verified preserved baselines

- minimal repro × 30: 0/30 crashes
- sig_alone.c × 10: 10/10 OK
- `int (*p);` × 30: 30/30 OK
- typedef sig_t × 30: 30/30 OK
- essential.c × 5: 0/5 (residual SIGSEGV — gate NOT met)

### Files touched

- NEW: `compiler-tml/tests/native/c_essential_repro.c` (regression fixture)
- NEW: `.rulebook/tasks/phase24l_shared-get-aliasing-deep-fix/` (follow-up)
- UPD: `.rulebook/tasks/phase24k_essential-cleanup-segv/tasks.md` (status)
- (no compiler-tml/src/cc/ changes — both attempted fixes were reverted)

### Next session starter

Active follow-up: `phase24l_shared-get-aliasing-deep-fix`. Start at
item 1.1 — choose between options (a/b/c) for the structural fix
to `Shared.get()` aliasing. Phase24k stays open until phase24l
ships and essential.c × 5 = 5/5.

NO VERSION bump (no behavior shipped).

### 2026-05-01 (session 2) — phase0x_heap-decl-codegen-crash COMPLETE

### Accomplished

Fixed the codegen ABI mismatch that crashed `cp_parse_translation_unit`
on `int x;` after the dangling-Str fix in commit 7e70a7bc7 exposed it.

**Bisect path** (sentinels at every `= call` emit site):
1. `gen_call_user_function` (call_user.cpp) — not invoked.
2. `gen_call_generic_struct_method` (call_generic_struct.cpp) — not invoked.
3. `gen_call` top of dispatch (call.cpp) — not invoked!
4. `gen_method_call` (method.cpp) — fires for `method=new`.
5. `gen_method_static_dispatch` (method_static_dispatch.cpp) — fires
   for `method=new, receiver_seg0=Heap`. The actual emitter.

So the call to `Heap[T]::new(value)` is parsed as a `MethodCallExpr`
with PathExpr receiver, NOT a CallExpr — explains why the previous
audit of CallExpr handlers turned up nothing.

**Diagnostic via emit_line in the fixup site** showed
`functions_.find("Heap__MinDecl_new")` returned `end()` at
arg-processing time. Generic instantiations are queued at
method_static_dispatch.cpp:953-956 and the FuncInfo registers later
when `gen_impl_method_instantiation` runs as part of a subsequent
pass. The existing fixup at line 1010-1020 had nothing to read.

**Fix** (method_static_dispatch.cpp:1006-1029): added a fallback
that mirrors the impl.cpp:392-395 rule unconditionally when:
- the FuncInfo lookup misses, AND
- `i == 0` (first user-visible param), AND
- the resolved arg type starts with `%struct.` or `%enum.`

Scoped to the `func_sig` branch only; the other branches (1081+,
1162+) handle their own cases through `pending_generic_impls_`.

### Verification

- `compiler/tests/compiler/heap_decl_var_repro.test.tml` PASS.
- Bug #7/#8/#9 regression tests (`nested_constructor_push`,
  `large_enum_by_value_duplicate`) still pass.
- `compiler-tml/tests/native/c_parser.test.tml`:
  `test_parse_top_decl_int_x` and `test_parse_translation_unit_int_x`
  added and pass.
- `tml cc .sandbox/int_x.c --emit=ast` exits 0 with output
  `cc_driver: parsed .sandbox/int_x.c`.
- Compiler suite (`tml test --suite=compiler --no-fail-fast`): 298/317
  pass. The 19 failures are all pre-existing K001 self-host tests
  (lexer_basic, parser_basic, hir_types, infer_differential,
  c_preprocessor, parse_*, test_*) plus one X002 timeout
  (builtins_imports). Verified by re-running on `git stash` (without
  the fix) — same failure set, no regressions introduced.

### Bisect data point worth keeping

Static method calls via `Type::method(args)` syntax in TML are
parsed as `MethodCallExpr`, not `CallExpr`. The dispatch path lives
in `method.cpp::gen_method_call → gen_method_static_dispatch`, NOT
`call.cpp::gen_call`. Future ABI-mismatch debugging on static-method
calls should start at method_static_dispatch.cpp.

### Files changed

- `compiler/src/codegen/llvm/expr/method_static_dispatch.cpp`
  (+15 / −9, the actual fix).
- `compiler-tml/tests/native/c_parser.test.tml` (+34 / −3, regression
  tests).
- `compiler/tests/compiler/heap_decl_var_repro.test.tml` (already
  landed in 0e814621; no new changes).
- `VERSION`: 0.3.38 → 0.3.39.
- `CHANGELOG.md`: 0.3.39 row.
- `docs/patches/v0.3.39.md`: full release notes.
- `.rulebook/tasks/phase0x_heap-decl-codegen-crash/tasks.md`: all
  items checked.

### Next session starter

phase24 Phase 3 (cmd_cc.cpp CLI subcommand) and Phase 4
(self-compile essential.c / mem.c via `tml cc`) are now unblocked.

### 2026-05-01 — cc parser dangling-Str fix + Heap[CDecl] crash isolation

### Accomplished

Audited every `tok.text` / `tok.file` site in
`compiler-tml/src/cc/parser.tml`. `cp_dup_token` already deep-
duplicated Str fields on `cp_peek`, but five raw `parser.tokens.get(p)`
calls inside loops bypassed it: dropping the local `tok` at end of
iteration freed Str pointers still owned by `parser.tokens`. Fixed:
all five raw `.get(p)` → `cp_peek(parser.tokens, p)` (lines 287, 478,
536, 677, 960).

Added `.duplicate()` at every Str escape site where `tok.text` or
`tok.file` flows into a value outliving the local `tok`:
`c_parse_err.file`, `cp_expect_ident.name`,
`CBaseType::Typedef(tok.text)`, `tag = tag_tok.text`,
`CExpr::Ident(tok.text)`, `CDeclarator::Ident(tok.text)`,
10× `start_tok.file` across CStructDef/CUnionDef/CEnumDef/CFuncDecl/
CTypedefDef/CVarDecl, label `name = tok.text`, and
`_Static_assert msg = msg_tok.text`. Also made `declarator_name`
return an owned Str so HashMap/struct captures of the returned name
don't dangle.

Type-check clean; `c_lexer`, `c_parser`, `c_frontend` test suites all
pass after the fix (no regressions).

### Crash isolation: discovered Heap[CDecl] codegen bug

After dangling-Str fixes, the `cp_parse_translation_unit` segfault on
`int x;` moved to `decls.push(Heap[CDecl]::new(CDecl::Var(vd)))`.
Bisected via `File::append_all` instrumentation (println is buffered
and lost on SIGSEGV — direct file appends survive). Reduced to:

```tml
let vd: CVarDecl = CVarDecl {
    name: "x", specifiers: specs, declarator: CDeclarator::Ident("x"),
    init: Nothing,
    file: "t.c", line: 1, col: 1
}
decls.push(Heap[CDecl]::new(CDecl::Var(vd)))
```

Crashes with ACCESS_VIOLATION at `Heap[CDecl]::new(...)` even with
literal Strs — so it's **not** in my fix; it's a pre-existing codegen
bug in `Heap[T]::new` for `T = CDecl`-shaped values (large enum +
nested struct fields with Str payloads). The phase0v bug #7/#8/#9
regression tests pass because their synthetic types differ from the
`CVarDecl` / `CDeclSpecifiers` / `CDeclarator` shape.

Filed as `phase0x_heap-decl-codegen-crash` with bisect plan and
Rust-as-Reference IR methodology. This is the actual blocker for
phase24 Phase 4.

### Files modified this session (uncommitted)
- `compiler-tml/src/cc/parser.tml` — dangling-Str fix (~25 sites).
- `.rulebook/tasks/phase0x_heap-decl-codegen-crash/` — new follow-up.

### Next session starter

Active task: `phase0x_heap-decl-codegen-crash`. Start at item 1.1 —
land the minimal `heap_decl_var_repro.test.tml` fixture and confirm it
crashes on a clean checkout. Item 2.1 (`emit-ir` on the fixture) is
where the actual codegen bug becomes visible.

### 2026-04-18 (session 2) — phase24 Phase 2.3 + parser bug bisection

<!-- PLANS:HISTORY:END -->
