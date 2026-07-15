# Proposal: phase24n_cc-preproc-aliasing-sweep

## Why

phase24m diagnosed and partially fixed the residual `essential.c × 5 = 0/5` SIGSEGV that survived phase24l's typedef-arm `Shared.get_clone` migration. The crash was localised to **stage1b — `pp_sweep_in_file`** in `compiler-tml/src/cc/preproc/`. Trace logs showed corrupted Str data inside `current_file` template-literal output, confirming use-after-free of HashMap-aliased values inside the C preprocessor.

phase24m landed deep-clone fixes at 15+ aliasing sites in `cc/preproc/`:
- `directives.tml::expand_macro_at` MacroDef lookup wraps in `dup_macro_def`.
- `directives.tml::pp_sweep_in_file` body construction deep-clones via `dup_pp_token`.
- `directives.tml::pp_handle_directive` #include path: `path.duplicate()` at every consumer to avoid use-after-move (Str moves into IncludeFrame on `pp_push_include`).
- `directives.tml::expand_macro_at` FunctionLike push, `drop_first_ident`, `pp_handle_directive` included-list push.
- `macros.tml::expand_macros` 4 push sites; `add_blue_paint`, `union_blue_sets`, `paste_tokens` Str field deep-clones.
- `macros.tml::substitute_func_macro` deep-clone before `out.pop()`.
- `conditionals.tml::sub_list` token push.

These fixes raised `c_essential_repro.c` from 25/30 baseline to 28-29/30 (phase24l ship preserved) AND preserved every other gate (sig_alone 10/10, int_p 30/30, typedef sig_t 30/30, lib/core unchanged). They did NOT close `essential.c × 5 = 0/5`.

The residual crash continues to live in `pp_sweep_in_file` — likely in additional aliasing sites in `cc/preproc/` not yet catalogued OR in a deeper structural issue:

(A) **Untouched aliasing sites in `cc/preproc/`** — phase24m cataloged the obvious ones via grep, but the macro expansion paths (especially recursive includes, blue-paint propagation, function-like macro substitution with `__VA_ARGS__` and `##` token pasting) have nested loops with `.get(i)` and `push(t)` patterns that may still alias.

(B) **Str ownership semantics across function calls** — phase24m discovered that `pp_push_include(pp, path)` MOVES the Str field into IncludeFrame; subsequent uses of the local `path` are use-after-move. The fix (`path.duplicate()` at every consumer) addresses one site, but ANY function in cc/preproc that takes a `Str` parameter and stores it in a List/HashMap/struct moves the pointer. All callers that re-use the local must duplicate.

(C) **The same aliasing class in c_lexer / c_parser / c_lower** — once stage1b passes, the residual crash may shift to stage1c or stage2/3 (which have similar `Shared[CDeclarator]`, `Shared[CExpr]`, `Shared[CStmt]` aliasing patterns). phase24l already fixed the typedef arm of base_to_ctype; the parser side has analogous patterns (the phase24f token-text dangling fix touched some).

(D) **Codegen-level structural fix** — TML codegen could detect `pp.defines.get(name)` patterns where V contains droppable fields (`List`, `Str`, `Heap`, `Shared`) and automatically emit a deep-clone via the type's `Duplicate` impl (or fall back to bitwise+forget). Wide blast radius but eliminates the entire class of bug across the codebase.

## What Changes

Three candidate paths, in order of structural preference:

(a) **Continue the deep-clone migration** — sweep every `.get(i)`, `.push(t)`, `.set(k, v)` site in `compiler-tml/src/cc/` and `compiler-tml/src/cc/preproc/` where the slot type contains droppable fields. Add helpers like `dup_macro_def` for every value-type stored in a HashMap. Wide-grain text-level fix; preserves baselines incrementally.

(b) **Codegen-level deep-clone emission** — at TML codegen, when emitting a `HashMap[K, V]::get` / `List[V]::get` for a V whose Duplicate impl walks nested handles, emit:
1. Bitwise read of the slot.
2. Call `V::duplicate()` on the bitwise result.
3. Skip drop on the bitwise alias (it never had ownership of the bumped inner refs).
4. Return the duplicated result.

The fix lives in `compiler/src/codegen/llvm/expr/method.cpp` (HashMap.get / List.get monomorphizations) and runtime drop-glue emission. Wide blast radius but most transparent semantically — eliminates the entire bug class without TML-side changes.

(c) **Move semantics enforcement** — make TML's borrow-checker reject the `pp_push_include(pp, path); File::read_all(path)` pattern as a use-after-move. The user is forced to either `path.duplicate()` (current phase24m fix) or restructure to retain ownership. Compiler-side static-analysis fix that surfaces the bug class at compile time. Largest blast radius (every TML program with similar patterns may need refactoring), but produces the safest end-state.

## Impact

- Affected specs: `Shared::get_clone` / `HashMap::get` / `List::get` contract under non-trivial droppable V types.
- Affected code:
  - Option (a): `compiler-tml/src/cc/preproc/*.tml` (15+ additional sites estimated), `compiler-tml/src/cc/{lexer,parser,lower}.tml`.
  - Option (b): `compiler/src/codegen/llvm/expr/method.cpp`, `compiler/src/codegen/llvm/core/drop.cpp`.
  - Option (c): `compiler/src/borrow/checker.cpp` (NLL refinement), all TML code that re-uses moved values.
- Breaking change: Yes for option (c) borrow-check tightening. NO at API surface for (a)/(b).
- User benefit: closes the residual `essential.c × 5 = 0/5` SIGSEGV; `tml cc essential.c` self-compiles deterministically; phase0z gate closes; phase24k/l/m archive.

## Source

- phase24m_essential-c-residual-segv (this task's diagnostic predecessor; landed 15+ surgical fixes in cc/preproc/, identified `pp_sweep_in_file` as crash site, ruled out shared.get_clone aliasing as primary cause).
- phase24l_shared-get-aliasing-deep-fix (typedef-arm fix shipped; preserved by phase24m + phase24n).
- phase24k_essential-cleanup-segv (originally diagnosed the `Shared.get` aliasing class).
- phase24g_heap-rc-or-borrow-language-fix (introduced the `Shared[T]` migration whose underlying `.get()` aliasing this task closes).
- phase0z_cc-driver-essential-c-residual (ultimate gate-blocker).

## Constraints

- Cannot regress the 290/295 compiler-suite baseline.
- Cannot regress `lib/core` test suite.
- Cannot regress `c_essential_repro.c` minimal trigger (current 28-29/30 with phase24m shipped).
- Cannot regress phase24h sig_alone / int*p / sig_t typedef reproducers (30/30 each).
- Must close `essential.c × 5 = 5/5` exit 0.

## Diagnostic data preserved from phase24m

### Trace signature

Crash localised via `File::append_all` sentinels in cc_driver.tml at every pipeline stage. For essential.c, the trace consistently shows:

```
ast_pipeline_start
stage0_start
stage0_done
stage1a_done    (pp_tokenize_source completes)
[CRASH or sweep_start sweep_start sweep_end ...]
```

Some runs also show:

```
sweep_start
sweep_start
sweep_end       (inner #include sweep finishes)
[CRASH before outer sweep_end]
```

Confirms the crash is in `pp_sweep_in_file` recursion over #include directives, NOT in tokenize or parse.

### Garbage-data evidence

Earlier traces with `current_file` interpolation showed binary garbage where the resolved path string should be — direct evidence that Str pointers in cc/preproc are dangling/freed-then-reused before the trace executes. The `path.duplicate()` fix at #include consumers addresses the most prominent case but additional sites remain.

### Sites already fixed (phase24m); should NOT be re-touched

- `compiler-tml/src/cc/preproc/macros.tml` lines 28, 52, 56, 59 (push sites)
- `compiler-tml/src/cc/preproc/macros.tml` `add_blue_paint`, `union_blue_sets`, `paste_tokens`
- `compiler-tml/src/cc/preproc/macros.tml` `substitute_func_macro` `out.pop()` defense
- `compiler-tml/src/cc/preproc/directives.tml` lines 85 (body push), 175 (expanded push), 320 (included_swept push)
- `compiler-tml/src/cc/preproc/directives.tml` line 133 (`dup_macro_def(pp.defines.get(...))`)
- `compiler-tml/src/cc/preproc/directives.tml` `drop_first_ident` line 360
- `compiler-tml/src/cc/preproc/directives.tml` `pp_handle_directive` #include block: `path.duplicate()` at 4 consumer sites
- `compiler-tml/src/cc/preproc/conditionals.tml` `sub_list`
- `compiler-tml/src/cc/preproc/macros.tml` added helpers: `dup_pp_token_list`, `dup_macro_def`
- `lib/core/src/alloc/shared.tml` added `get_ref(this) -> ref T` method (additive API; not a fix but enables future ref-based patterns)
