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
│        Core Language (IR)               │  RFC-0001
│     types, effects, ownership           │
├─────────────────────────────────────────┤
│      Target Backends (LLVM/WASM)        │
└─────────────────────────────────────────┘
```

## RFC Index

| RFC | Title | Status | Summary |
|-----|-------|--------|---------|
| [RFC-0001](./RFC-0001-CORE.md) | Core Language | Draft | Types, effects, ownership, IR |
| [RFC-0002](./RFC-0002-SYNTAX.md) | Surface Syntax | Draft | Human syntax + desugaring rules |
| [RFC-0003](./RFC-0003-CONTRACTS.md) | Contracts | Draft | pre/post/forall/exists, static vs runtime |
| [RFC-0004](./RFC-0004-ERRORS.md) | Error Handling | Draft | Outcome, ! operator, error propagation |
| [RFC-0005](./RFC-0005-MODULES.md) | Modules & Caps | Draft | Module system, capabilities, imports |
| [RFC-0006](./RFC-0006-OO.md) | OO Sugar | Draft | class/state/self syntactic sugar |

## Tooling Specifications

| Spec | Purpose | Format |
|------|---------|--------|
| `grammar/tml.peg` | Compiler parser | PEG grammar |
| `grammar/tree-sitter-tml/` | Editor support | Tree-sitter |
| `ir/tml-ir.schema.json` | IR interchange | JSON Schema |
| `ir/tml-ir.proto` | IR binary format | Protobuf |

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
2. **RFC-0004** (Errors) - Critical for any real code
3. **RFC-0005** (Modules) - Needed for stdlib
4. **RFC-0002** (Syntax) - Can evolve as sugar
5. **RFC-0003** (Contracts) - Can be added incrementally
6. **RFC-0006** (OO) - Pure sugar, lowest priority
