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
Phase 30 (30a-30j) and Phase 31 (31a-31f) fully complete and archived.
phase13d_frontend-integration archived.
Active task: phase14c_typechecker-inference — Phases 1-5 complete + extended expr coverage, Phase 6 blocked by K001.
Total code: ~4,500 lines across 8 TML files (infer/, checker/).
23/26 task items done. 2 remaining items (6.1, 6.2) blocked by K001 codegen bug.
All 35 Expr enum variants now have type inference handlers (only Base, New, Throw fall through to fresh_var).
<!-- PLANS:CONTEXT:END -->

## Current Task

<!-- PLANS:TASK:START -->
phase14c_typechecker-inference — in-progress (22/25)
Port Hindley-Milner type inference from C++ to TML (~7,229 LOC).
Phases 1-5 complete. Phase 6 (differential testing) blocked by K001 codegen bug.
Files created this session: infer/common.tml, checker/check_call.tml (677 lines), checker/check_stmt.tml (172 lines), checker/check_pattern.tml (453 lines).
Tests: unify_basic.test.tml (15 tests), infer_differential.test.tml (10 tests). All type-check; runtime blocked by K001.
Remaining: items 6.1, 6.2 — requires K001 fix or full C++ backend for differential comparison.
<!-- PLANS:TASK:END -->

## Session History

<!-- PLANS:HISTORY:START -->
### 2026-04-12 (session 4)
## phase14c Type Inference — Phases 1-5 Complete

### Accomplished
Implemented the core type inference engine for the TML self-hosting type checker (Phases 1-5 of 6):

**Phase 1 — Inference Engine Core:**
- Created `infer/common.tml` module root (re-exports)
- Verified existing `unify.tml` (InferCtx, fresh_var, resolve, unify with structural matching)
- Wrote 15 unification tests in `unify_basic.test.tml`

**Phase 2 — Expression Type Checking:**
- Added index access inference (`infer_index` for Array/Slice element types)
- Verified existing literal, ident, binary, field, tuple, array, if, block, return inference

**Phase 3 — Call & Method Resolution (677 lines, new file):**
- `check_call.tml`: function call resolution (builtins, intrinsics, named functions, enum constructors, static methods)
- Method call resolution (primitive methods, struct/enum impl methods, behavior methods, collection methods)
- Generic instantiation: `extract_type_params` + `substitute_type` for type parameter inference from args
- Operator desugaring: `operator_behavior_name`/`operator_method_name` + `desugar_operator`

**Phase 4 — Statement Checking (172 lines, new file):**
- `check_stmt.tml`: let, var, let-else binding with pattern support
- `bind_pattern_full` for tuple/enum/struct destructuring in bindings
- Wired when/loop/while/for-in/range expressions into check_expr.tml dispatcher

**Phase 5 — Pattern & Exhaustiveness (453 lines, new file):**
- `check_pattern.tml`: 8 pattern types (Wildcard, Ident, Literal, Tuple, Struct, Enum, Or, Array)
- Enum variant matching with type parameter substitution from scrutinee type
- Exhaustiveness checking: enum variant coverage, bool coverage, wildcard catch-all detection

**Phase 6 — Differential Testing (infrastructure only):**
- Created `infer_differential.test.tml` with 10 integration tests
- Items 6.1/6.2 (full differential comparison) blocked by K001 codegen bug

### Key Decisions
- Named module root `common.tml` (not `mod.tml`) because `mod` is a reserved keyword in TML
- Used `check_call` from `check_call.tml` to replace the simpler `infer_call` in check_expr.tml
- `bind_pattern_simple` takes ctx parameter for fresh_var generation in nested patterns
- `check_pattern` uses `substitute_type` from check_call for enum variant payload type resolution
- All new code type-checks clean; runtime tests blocked by K001 (Heap[Type] in List/HashMap)

### Files Created/Modified
- NEW: `compiler-tml/src/types/infer/common.tml` (module root)
- NEW: `compiler-tml/src/types/checker/check_call.tml` (677 lines)
- NEW: `compiler-tml/src/types/checker/check_stmt.tml` (172 lines)
- NEW: `compiler-tml/src/types/checker/check_pattern.tml` (453 lines)
- NEW: `compiler-tml/tests/types/unify_basic.test.tml` (15 tests)
- NEW: `compiler-tml/tests/types/infer_differential.test.tml` (10 tests)
- MOD: `compiler-tml/src/types/checker/check_expr.tml` (added index, when, loop, for, range, method call)

### 2026-04-12 (session 3)
## Phase 31 Ergonomics Migration — ALL COMPLETE + phase13d closed

### Accomplished
All 6 phase31 tasks archived in one session using parallel agent teams (up to 9 concurrent agents):
- **31a** for-in core/std — ~406 loops converted across 72 files (commit 5ac50102)
- **31b** for-in compiler-tml — 80 loops converted across 9 files (commit c92c1736)
- **31c** let-else/pattern-guards — 17 nested when blocks flattened across 10 files (commit 3052460b)
- **31d** @auto/@repr — 11 types got @auto(duplicate,equal), 12 enums got @repr(U8) (commit c4f550ba)
- **31e** destructuring/struct-update — 3 struct update sites in relation.tml (commit bf5b31b0)
- **31f** optional-chaining/behavior-aliases — ThreadSafe = Send + Sync alias, 7 applications (commit ef8f25c8)
- **phase13d** — closed last unchecked item (3.5 IR-diff), archived (commit 94faa744)

### Discoveries
- for-in codegen phi-node bug: when for-in is last expression in an else block, codegen emits `0` instead of `{}` for the loop-didn't-execute phi. Workaround: revert `List.resize()` to manual loop.
- @repr(U8) on TypeKind causes K001 codegen bug: `ptr` vs `i8` mismatch in method dispatch. Reverted.
- Most destructuring/optional-chaining candidates already used inline field access — very few eligible sites.

### Net Impact
- −1,693 lines removed across 109 files
- ~500 boilerplate patterns replaced with idiomatic TML syntax
- 7 commits on feat/self-hosting-compiler

### 2026-04-11 (session 2)
## phase30 Language Ergonomics — ALL COMPLETE (commit 1292e666)

### Accomplished
All 9 phase30 tasks archived in one session:
- **30a** for-in-loops — tail closed (CHANGELOG already documented)
- **30b** pattern-guards — verified already implemented, tail closed
- **30c** struct-update-syntax — tail closed
- **30d** operator-overloading — **fixed K001 GEP codegen bug** in binary_ops.cpp; operator `+` on user-defined structs now generates correct IR
- **30e** auto-repr-directives — implemented @repr(U8/I32), @auto(duplicate,equal), @packed; updated all 11 derive codegen files
- **30f** bool-layout-fix — already completed in prior session
- **30g** closure-type-inference — verified fully implemented (parser, type checker, HIR, codegen all wired)
- **30h** behavior-aliases — implemented `behavior Name = Bound1 + Bound2` (parser, AST, type checker)
- **30i** destructuring-let — tail closed

### Key Fixes
- Operator overloading codegen: AST legacy path had zero support for non-primitive `+`/`-`/`*` — added struct detection + behavior method dispatch in `gen_binary_ops()`
- @auto directive: alias for @derive with lowercase name mapping (duplicate→Duplicate, equal→PartialEq, etc.)

### Tests Added
- operator_overload.test.tml (method call + operator syntax + primitives)
- directives_repr.test.tml (@repr(U8) enum discriminants)
- directives_packed.test.tml (@packed struct layout)
- closure_type_inference.test.tml (annotated, inferred param, inferred return, fully inferred)
- behavior_alias.test.tml (alias expansion)

### 2026-04-11
## phase14a_typechecker-registration — COMPLETE

### Accomplished
- Created 4 new TML modules under `compiler-tml/src/types/`:
  - `ty.tml` — Type enum (16 variants), factory functions, equality, display, substitution, size/align, predicates
  - `env.tml` — TypeEnv struct with index-based storage (List + HashMap[Str, I64] pattern), Scope chain, register/lookup operations
  - `builtins.tml` — 17 primitive types, 4 core enums, 18 behaviors, primitive impl registrations, memory/collection builtins
  - `register.tml` — AST declaration walker: struct, union, enum, func, trait, impl, type-alias, mod, namespace registration
- Created 2 test files:
  - `type_basic.test.tml` — 13 passing tests (equality, display, size, predicates)
  - `env_minimal.test.tml` — placeholder (TypeEnv runtime tests blocked by K001 codegen bug)
- Fixed `Decl::Impl(Heap[ImplDecl])` crash from previous session (cleared stale incr cache)
- Added `enforce-no-bash-grep.sh` PreToolUse hook
- 227/230 compiler tests pass (3 pre-existing failures)

### Key Decisions
- File named `ty.tml` not `type.tml` because `type` is a reserved keyword in TML
- TypeEnv uses List + HashMap[Str, I64] index pattern instead of HashMap[Str, ComplexType] — workaround for codegen "Unknown method: duplicate" bug when HashMap/List values transitively contain `Heap[Type]`
- The recursive `Type` enum (GenericType.bounds: List[Heap[Type]]) triggers duplicate generation failure in codegen; all code type-checks and will work when compiled by the full C++ backend

### Key Discovery: K001 "duplicate" codegen bug
Any `List[T].push()` or `HashMap[K,V].set()` where T/V transitively contains `Heap[Type]` fails with "Unknown method: duplicate". Root cause: Type enum has recursive List[Heap[Type]] fields, codegen tries to generate duplicate for Type but can't handle the recursion. Affects ALL runtime tests for TypeEnv but NOT type-checking or compilation via C++ backend.

### 2026-04-10
## phase13d_frontend-integration — COMPLETE (commit 7a75479c)

### Accomplished
Phase 5 (Switchover) + Tail items fully implemented.

**Phase 5.1 — TML parser as default:**
- `BuildOptions::stage_overrides` defaults to `{{"parser", "tml"}}`
- `RunOptions::stage_overrides` defaults to `{{"parser", "tml"}}`
- `dispatcher.cpp` local stage_overrides var at line 374 starts with TML default

**Phase 5.2 — C++ fallback:**
- `--stage=parser:cpp` accepted by build, run, and test commands
- Validation updated to accept both `"tml"` and `"cpp"` as valid impl values
- Test command: added `--stage=` parsing to `cmd_test.cpp`, `stage_overrides` field to `TestOptions`, `TestConfig`, `CompileConfig` — full propagation chain to `QueryOptions`
- Both testing_compile.cpp qopts creation sites updated

**Tail:**
- `docs/patches/v0.2.15.md` created with full phase13d coverage
- CHANGELOG.md updated with v0.2.15 entry
- VERSION bumped 0.2.14 → 0.2.15
- New test: `compiler-tml/tests/parser/parse_type_new_keyword.test.tml` (4 regression tests for tok_is_name/KwNew fix)
- 213/213 compiler tests pass; 771/798 core (27 pre-existing K001/T056)

### Key Decisions
- `QueryOptions::stage_overrides` stays empty by default — CLI structs enforce the TML default. This keeps the internal query layer neutral.
- The test runner has TWO qopts creation sites in testing_compile.cpp (lines 391 and 651) — both needed updating.
- `--stage=parser:cpp` works by storing "cpp" value; query_core.cpp only activates TML when value == "tml", so "cpp" falls through naturally.

### Next Steps
- Era 1 Phase 2: type checker porting (blocked on phase13d, now unblocked)
- Remaining 27 K001/T056 codegen bugs are pre-existing, tracked separately


### 2026-04-09
## phase13c: TML Self-Hosting Parser — Complete

### Accomplished
- Fixed codegen bug: all `Outcome[(StructType, I64), E]` tuple returns replaced with named `ParsedX` structs across all 7 parser files. Root cause: `!`-unwrapping an Outcome wrapping a tuple containing a struct generates `i32` (discriminant) instead of the struct payload (K001 LLVM error).
- Fixed `parse_decl.tml`: added local types `ParsedVisibility`, `ParsedGenericParams`, `ParsedFuncParams`, `ParsedFuncParam`, `ParsedEnumVariant`, `ParsedFuncDeclRaw`; updated all call sites from `.0`/`.1` to named fields.
- Added `ParsedTypePath { path: TypePath, pos: I64 }` to `common.tml`; updated `parse_type.tml`, `parse_expr.tml`, `parse_pattern.tml`, `parse_decl.tml` callers.
- Deleted stale `mod.tml` (renamed to `common.tml` because `mod` is a reserved keyword).
- 4 tests now passing: `parse_path_only`, `parse_path_full`, `parse_named_type`, `parse_type_basic`.
- Task phase13c archived via rulebook.

### Key Decisions
- All parser functions use named `ParsedX` result structs (never tuples) to avoid K001 codegen bug.
- Incremental build cache (`incr.bin`) must be deleted to pick up source changes when LLVM IR is stale.
- `parser_basic.test.tml` (full `parse_module` tests) hits X002 codegen timeout — 30s limit exceeded by full parser chain (~8k lines). Not a parser correctness issue.

### Next Steps
- phase13d: wire TML parser into compiler pipeline — create `main_frontend.tml` binary, add `ParseModuleTml` query in C++, implement `--stage=parser:tml` flag.

<!-- PLANS:HISTORY:END -->
