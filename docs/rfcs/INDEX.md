# TML RFCs (Request for Comments)

This directory contains the normative specifications for TML (To Machine Language).

## Design Philosophy

TML uses a **layered architecture**:

```
┌─────────────────────────────────────────┐
│     Surface Syntax (Human View)         │  RFC-0002
│     class, state, @decorator            │
├─────────────────────────────────────────┤
│          Desugaring Pass                │  RFC-0002
├─────────────────────────────────────────┤
│        Core Language (IR)               │  RFC-0001, RFC-0007
│     types, effects, ownership           │
│     SSA form, stable IDs                │
├─────────────────────────────────────────┤
│    Query-Based Pipeline (8 stages)      │  RFC-0012, RFC-0013
│    Incremental compilation (Red-Green)  │
├─────────────────────────────────────────┤
│    Embedded LLVM + LLD (in-process)     │
│    Target Backends (LLVM/Cranelift)     │
└─────────────────────────────────────────┘
```

## RFC Index

| RFC | Title | Status | Summary |
|-----|-------|--------|---------|
| [RFC-0001](./RFC-0001-CORE.md) | Core Language | **Active** | Types, effects, ownership, IR, concurrency |
| [RFC-0002](./RFC-0002-SYNTAX.md) | Surface Syntax | **Active** | Human syntax + desugaring rules |
| [RFC-0003](./RFC-0003-CONTRACTS.md) | Contracts | Draft | pre/post/forall/exists, static vs runtime |
| [RFC-0004](./RFC-0004-ERRORS.md) | Error Handling | **Active** | Outcome, ! operator, error propagation |
| [RFC-0005](./RFC-0005-MODULES.md) | Modules & Imports | **Active** | Module system, imports, visibility |
| [RFC-0006](./RFC-0006-OO.md) | OO Sugar | Superseded | Replaced by RFC-0014 |
| [RFC-0007](./RFC-0007-IR.md) | Intermediate Representation | Active | IR format, normalization, stable IDs, serialization |
| [RFC-0008](./RFC-0008-GENERICS.md) | Generics | **Active** | Monomorphization, generic structs/enums |
| [RFC-0010](./RFC-0010-TESTING.md) | Testing | Active | Test framework, @test decorator |
| [RFC-0011](./RFC-0011-FFI.md) | FFI | **Active** | @extern and @link for C/C++ interop |
| [RFC-0012](./RFC-0012-MIR.md) | Mid-level IR | **Active** | SSA-form MIR for optimization |
| [RFC-0013](./RFC-0013-HIR.md) | High-level IR | **Active** | Type-resolved AST representation |
| [RFC-0014](./RFC-0014-OOP-CLASSES.md) | C#-Style OOP | **Active** | Classes, interfaces, inheritance, polymorphism |
| [RFC-0015](./RFC-0015-JSON.md) | Native JSON | **Active** | JSON parsing, serialization, JSON-RPC 2.0, schema validation |

## Tooling Specifications

| Spec | Purpose | Format | RFC |
|------|---------|--------|-----|
| `grammar/tml.peg` | Compiler parser | PEG grammar | RFC-0002 |
| `grammar/tree-sitter-tml/` | Editor support | Tree-sitter | RFC-0002 |
| `ir/tml-ir.schema.json` | IR interchange | JSON Schema | RFC-0007 |
| `ir/tml-ir.proto` | IR binary format | Protobuf | RFC-0007 |

## RFC Structure

Each RFC follows this template:

```markdown
# RFC-XXXX: Title

## Status
Draft | Active | Final | Superseded

## Summary
One paragraph overview.

## Motivation
Why this feature exists. What problem does it solve?

## Specification
Precise, normative language. MUST/SHOULD/MAY per RFC 2119.

## Examples
Concrete code examples showing usage.

## Desugaring (if applicable)
How surface syntax maps to core IR.

## Compatibility
Interaction with other RFCs. Breaking changes.

## Alternatives Rejected
Other designs considered and why they were not chosen.

## References
Related work, inspiration, prior art.
```

## Versioning

- **Draft**: Under active development, may change
- **Active**: Stable, implemented, may have minor revisions
- **Final**: Frozen, will not change
- **Superseded**: Replaced by another RFC

## Implementation Priority

1. **RFC-0001** (Core) - Foundation, must be solid first
   - ✅ Types, ownership, generics
   - ✅ **Concurrency primitives** (v0.6.0) - Atomics, fences, spinlocks
2. **RFC-0007** (IR) - ✅ Implemented in v0.1.0 - Canonical format
3. **RFC-0008** (Generics) - ✅ Implemented in v0.4.0 - Monomorphization
4. **RFC-0010** (Testing) - ✅ Implemented - @test decorator
5. **RFC-0004** (Errors) - ✅ Implemented - Outcome[T,E], Maybe[T], `!` operator
6. **RFC-0005** (Modules) - ✅ Basic implementation - `use` declarations, `mod` paths
7. **RFC-0002** (Syntax) - ✅ Implemented - `impl`, `when`, closures, let-else, `?.`
8. **RFC-0003** (Contracts) - Can be added incrementally
9. **RFC-0014** (OOP) - ✅ Implemented - Classes, interfaces, inheritance, vtables
   - ✅ Class/interface declarations
   - ✅ Virtual methods and polymorphism
   - ✅ Vtable generation and dispatch
   - ✅ Namespace support
   - 📋 OOP optimizations (planned)
10. **RFC-0015** (JSON) - ✅ Implemented in v0.6.0 - Native JSON for MCP
    - ✅ Parser with integer precision preservation
    - ✅ Serializer with streaming output
    - ✅ Fluent builder API
    - ✅ JSON-RPC 2.0 support
    - ✅ Schema validation

## Compiler Infrastructure (v0.7.0)

The following infrastructure is implemented in the compiler:

- ✅ **Embedded LLVM Backend** — ~55 LLVM static libraries linked into compiler; in-process IR→obj compilation (no clang subprocess)
- ✅ **Embedded LLD Linker** — In-process COFF/ELF/Mach-O linking (no external linker needed)
- ✅ **Query System** — Demand-driven compilation with 8 memoized stages (analogous to rustc's TyCtxt)
- ✅ **Red-Green Incremental Compilation** — 128-bit fingerprints + dependency edges persisted to disk; GREEN path skips pipeline; no-op rebuild < 100ms
- ✅ **No intermediate .ll files** — LLVM IR stays in-memory (written to disk only with `--emit-ir`)
- ✅ **Test suite**: 3,632 tests passing in ~17 seconds (down from ~15 minutes before embedded LLVM)
