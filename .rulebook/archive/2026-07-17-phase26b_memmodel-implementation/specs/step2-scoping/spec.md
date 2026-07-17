# phase26b Step 2 Scoping — PREMISE CORRECTION (decision-critical)

Collected 2026-07-15 by read-only compiler scoping + direct code verification.

## The headline: ADR-009 B3's premise is refuted by the code

**Claimed premise (ADR-009, phase26a spike):** "the test framework already compiles
stdlib/generic programs through the query/MIR pipeline, so Step 2 is a routing change
— point `tml build`/`run` at the same pipeline."

**Actual ground truth (verified):** `tml build` (`build.cpp:409-413`) and the test
framework (`testing_compile.cpp:650` → `provide_codegen_unit`, `query_core.cpp:931`)
use the **identical** AST-vs-MIR gate:
`if (force_mir || (!has_tml_imports_needing_codegen && !has_local_generics))` → MIR,
else AST (`query_core.cpp:1003`). Any program importing a code-bearing stdlib module
(`has_tml_imports_needing_codegen`, `build.cpp:321-329`) OR using local/library
generics (`has_local_generics`, `:331-407`) routes to the **AST `LLVMIRGen` path** —
in BOTH `tml build` and the test framework. "Query pipeline" ≠ "MIR codegen"; they are
orthogonal, and `provide_codegen_unit` picks AST for exactly the programs B3 wanted on
MIR.

**Proof:** the corpus canaries `compiler/tests/determinism/f002_hashmap.test.tml:16`
and `f002_list.test.tml:9-10` carry `Shared[…]` fields → trip `has_local_generics` →
AST path, inside the test framework. The 12k tests passing does NOT validate MIR for
these programs; it validates the AST-path fixes (`ptr_read_clone` in `intrinsics.cpp`,
the `drop.cpp:460-471` container-field-drop leak special-case, and the phase26b Step-1
`shared.tml` counter fix).

**My phase26a error:** I inferred "test framework uses MIR" from `testing_compile.cpp`
using "the query pipeline" and conflated query-pipeline with MIR-codegen. They are not
the same. Both tests and users run the AST path for real (stdlib/generic) programs.

## Consequence for effort: Step 2 is a codegen project, not routing

Reuse : Build ≈ **5% : 95%**. The routing/flag change is ~30 lines. The missing MIR
functionality is ~8,000 LOC of AST-only machinery with NO MIR equivalent:

| Missing in MIR | AST-only impl (LOC) |
|---|---|
| Imported pure-TML function-body emission (`emit_module_pure_tml_functions`, `generate.cpp:890`) | ~4,000 (`runtime_modules*.cpp`, `generate_library_only.cpp`) |
| Generic monomorphization worklist (`generate_pending_instantiations`, `generate.cpp:941`) | ~2,250 (`generic_instantiate*.cpp`) — HIR only records names, `hir_builder.cpp:883-884` |
| Generic call/struct dispatch | ~1,970 (`call_generic_*.cpp`, `generic.cpp`) |
| `@derive(Reflect/Default/FromJson/ToJson)`, unions, generic-enum construct/destructure | AST-only |

`query_core.cpp:762-846` even ADDS names to the AST-only set (Arc/Rc/Shared/Box/Vec/
BTreeMap/… + derives + unions) with comments stating MIR "would produce undefined
symbols" / "does not run [derive emitters]" / "Unions are only supported in the AST
codegen path."

## What this means for the ADR-009 decision

B3 was chosen to AVOID implementing drop-flags twice by unifying onto the
"already-tested MIR path." That path is **not already-tested** for real programs and
unifying costs ~8,000 LOC. So B3 = (8,000 LOC MIR migration) + (drop-flag work) — more
total work, not less. The premise that justified B3 over B1 no longer holds.

**The AST path is what 100% of real programs AND all tests actually run.** It already
has partial move tracking (`consumed_vars_`, `drop.cpp:97-99`, ~30 live sites) and the
drop machinery (`drop.cpp`). Fixing drop-flag elaboration THERE fixes reality directly,
with no migration prerequisite. MIR unification becomes a separate, later, optional
cleanup (it aligns with the frozen self-hosting/native-backend era, phases 30–33).

→ **Escalated to user for re-decision** (the B3 call was made on the refuted premise).

## Full plan (if B3 is kept) and risk register: see the step2-pipeline-scope agent
report captured in the session; key file:line index reproduced in the risk section of
this spec's companion notes.
