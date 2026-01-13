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
│      Target Backends (LLVM/WASM)        │
└─────────────────────────────────────────┘
```

## RFC Index

| RFC | Title | Status | Summary |
|-----|-------|--------|---------|
| [RFC-0001](./RFC-0001-CORE.md) | Core Language | **Active** | Types, effects, ownership, IR, concurrency |
| [RFC-0002](./RFC-0002-SYNTAX.md) | Surface Syntax | Draft | Human syntax + desugaring rules |
| [RFC-0003](./RFC-0003-CONTRACTS.md) | Contracts | Draft | pre/post/forall/exists, static vs runtime |
| [RFC-0004](./RFC-0004-ERRORS.md) | Error Handling | Draft | Outcome, ! operator, error propagation |
| [RFC-0005](./RFC-0005-MODULES.md) | Modules & Caps | Draft | Module system, capabilities, imports |
| [RFC-0006](./RFC-0006-OO.md) | OO Sugar | Superseded | Replaced by RFC-0014 |
| [RFC-0007](./RFC-0007-IR.md) | Intermediate Representation | Active | IR format, normalization, stable IDs, serialization |
| [RFC-0008](./RFC-0008-GENERICS.md) | Generics | **Active** | Monomorphization, generic structs/enums |
| [RFC-0010](./RFC-0010-TESTING.md) | Testing | Active | Test framework, @test decorator |
| [RFC-0011](./RFC-0011-FFI.md) | FFI | **Active** | @extern and @link for C/C++ interop |
| [RFC-0012](./RFC-0012-MIR.md) | Mid-level IR | **Active** | SSA-form MIR for optimization |
| [RFC-0013](./RFC-0013-HIR.md) | High-level IR | **Active** | Type-resolved AST representation |
| [RFC-0014](./RFC-0014-OOP-CLASSES.md) | C#-Style OOP | **Active** | Classes, interfaces, inheritance, polymorphism |

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
5. **RFC-0004** (Errors) - Critical for any real code
6. **RFC-0005** (Modules) - ✅ Basic implementation - `use` declarations
7. **RFC-0002** (Syntax) - Can evolve as sugar
8. **RFC-0003** (Contracts) - Can be added incrementally
9. **RFC-0014** (OOP) - ✅ Implemented - Classes, interfaces, inheritance, vtables
   - ✅ Class/interface declarations
   - ✅ Virtual methods and polymorphism
   - ✅ Vtable generation and dispatch
   - 🔄 Namespace support (in progress)
   - 📋 OOP optimizations (planned)
