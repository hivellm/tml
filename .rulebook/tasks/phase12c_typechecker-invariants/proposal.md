# Proposal: Document Type Checker Invariants

**Task**: phase12c_typechecker-invariants
**Status**: Planned
**Priority**: P0
**Estimated effort**: 3–4 weeks
**Risk**: High — undocumented invariants are the #1 risk for self-hosting

## Problem

The TML type checker is 21,000+ LOC of C++ across 38 source files. It encodes a large body of
implicit knowledge: the exact order in which types must be registered, how `pub use` re-exports
are resolved before `impl` blocks are processed, which `TypeEnv` fields are valid at which phase,
how generic method instantiation interacts with behavior coherence checks, and dozens of other
ordering and consistency invariants. None of this is documented anywhere outside the code itself.

Porting the type checker to TML during self-hosting Phase 2 is the most complex and highest-risk
task in the entire self-hosting roadmap. Every compiler subsystem that follows (HIR, MIR, codegen)
consumes the `TypeEnv` produced by the type checker. If the TML-written type checker violates
any invariant — even one that the C++ implementation relies on silently — it will produce a
subtly wrong `TypeEnv` that corrupts all downstream output. These bugs are notoriously hard to
diagnose because they manifest far from their source.

The only reliable defense is to enumerate all invariants before porting begins, so the TML
implementation can be built against a written specification rather than discovered by running
tests until something breaks.

## Proposed Solution

A systematic read-and-document pass over all 38 type checker source files, producing a single
specification document at `docs/specs/typechecker-invariants.md`. The document is structured
in six sections matching the logical phases of type checking:

**Section 1 — Type Registration**: What `Type` variants exist, how they compare for equality,
what fields are present at each phase, and the ordering dependencies between registering structs,
enums, behaviors, and builtin types. Source: `checker/core.cpp`, `type.cpp`, `type.hpp`, `env.hpp`.

**Section 2 — Module Resolution**: The load sequence for multi-file modules, how imports are
resolved before declarations are processed, what `pub use` semantics guarantee about re-export
visibility, and what invariants hold in `TypeEnv` after module loading completes. Source:
`env_module_loading.cpp`, `env_module_load_decls.cpp`, `env_module_load.cpp`.

**Section 3 — Impl Processing**: How behavior impls are registered, the method resolution order
when multiple behaviors provide the same name, coherence rules (which impl wins for which type),
and what `extend` guarantees. Source: `checker/core_oop.cpp`, `checker/decl_struct.cpp`,
`env_lookups.cpp`.

**Section 4 — Body Checking and Inference**: The Hindley-Milner inference rules as implemented
(not the formal HM — the actual algorithm in the code), how call resolution handles overloading,
the method dispatch algorithm when the receiver type is generic, and how generic instantiation
produces monomorphic `TypeEnv` entries. Source: `checker/expr.cpp`, `expr_call.cpp`,
`expr_call_method.cpp`, `expr_call_method_types.cpp`.

**Section 5 — Cross-Cutting Invariants**: Ordering dependencies between all phases, what global
state exists in `TypeEnv` and when it is safe to read, how error recovery is handled (partial
`TypeEnv` after errors), and scope chain rules. Source: remaining 8 checker files,
`builtins/` (10 files), `env_core.cpp`, `env_scope.cpp`, `env_definitions.cpp`,
`env_module_support.cpp`.

**Section 6 — Self-Hosting Contract**: A synthetic section that distills the invariants the
TML-written type checker MUST preserve to produce a `TypeEnv` that downstream C++ stages
(HIR lowering, MIR building, codegen) accept without modification. This section is the primary
deliverable for Phase 2 implementers.

## Key Decisions

**Output format: a single Markdown file, not per-file notes.** Individual file annotations get
lost. The deliverable must be a coherent document that a Phase 2 implementer can read linearly,
understand the full type checker contract, and use as the specification for their TML port.

**Read all 38 files before writing any section.** Inter-file dependencies mean that reading
`checker/core.cpp` in isolation will miss invariants that only become visible when
`env_module_load_decls.cpp` is also read. The five-phase document structure is the output
organization, not the reading order.

**Document the implementation, not the theory.** The type checker may deviate from textbook
Hindley-Milner in ways that matter for correctness. The document must describe what the code
actually does, not what it theoretically should do.

**No code changes.** This is a read-only task. If a bug is discovered during the audit, it is
noted in the document as a known deviation, not fixed. Fixing bugs during documentation
conflates two concerns and risks introducing regressions.

**Section 6 is the primary deliverable.** Sections 1–5 are input to Section 6. If time is short,
the earlier sections can be in note form, but Section 6 — the self-hosting contract — must be
a complete, precise, unambiguous enumeration of invariants.

## Files to Create/Modify

**Created**:
- `docs/specs/typechecker-invariants.md` — the full invariant document (~50–100 pages)

**Read (not modified)**:
- `compiler/src/types/checker/core.cpp` (1,412 LOC)
- `compiler/src/types/type.cpp` (965 LOC)
- `compiler/include/types/type.hpp` (427 LOC)
- `compiler/include/types/env.hpp` (807 LOC)
- `compiler/src/types/env_module_loading.cpp` (875 LOC)
- `compiler/src/types/env_module_load_decls.cpp` (1,253 LOC)
- `compiler/src/types/env_module_load.cpp` (508 LOC)
- `compiler/src/types/checker/core_oop.cpp` (1,067 LOC)
- `compiler/src/types/checker/decl_struct.cpp` (1,207 LOC)
- `compiler/src/types/env_lookups.cpp` (1,265 LOC)
- `compiler/src/types/checker/expr.cpp` (652 LOC)
- `compiler/src/types/checker/expr_call.cpp` (802 LOC)
- `compiler/src/types/checker/expr_call_method.cpp` (1,363 LOC)
- `compiler/src/types/checker/expr_call_method_types.cpp` (668 LOC)
- 24 additional files across `checker/`, `builtins/`, `env_*.cpp`

## Success Criteria

- All 38 type checker source files are explicitly cited in the document
- Section 6 enumerates at minimum 20 distinct invariants with concrete "must hold" statements
- Each invariant is traceable to a specific file and line range in the C++ source
- A Phase 2 implementer can read Section 6 alone and know what their TML type checker must
  guarantee without reading any C++ source
- No C++ source files are modified during this task

## Dependencies

**Blocks**: Era 1 Phase 2 — type checker porting. Without this document, Phase 2 implementers
have no specification to build against and no way to verify correctness beyond running tests.

**Depends on**: Nothing. This is a read-only audit task that can start immediately and run
in parallel with all other phase12 work.
