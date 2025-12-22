# RFC-0007: Intermediate Representation (IR)

## Status
Active - **Implemented in v0.1.0**

## Summary

This RFC defines the canonical Intermediate Representation (IR) format for TML. The IR serves as the normalized, stable format for code analysis, transformation, and interchange between compilation phases and external tools. Unlike the surface syntax (RFC-0002), the IR is minimal, deterministic, and optimized for machine processing.

## Motivation

LLM-generated code requires a stable, analyzable representation for:

1. **Semantic Diffing** - Compare code changes by meaning, not cosmetics
2. **Surgical Patching** - Modify code via stable IDs without full regeneration
3. **Automatic Merging** - Combine patches from multiple sources safely
4. **Cross-Tool Interchange** - Enable analysis tools, formatters, and optimizers
5. **Deterministic Compilation** - Same IR always produces same output

The IR provides these guarantees by being:
- **Normalized** - Multiple syntactically different sources → same IR
- **Stable** - Content-addressable IDs resist renames and moves
- **Minimal** - No syntactic sugar, only semantic content
- **Versioned** - Schema evolution with compatibility guarantees

## Specification

### 1. IR Format

The IR has three serialization formats:

| Format | Use Case | File Extension |
|--------|----------|----------------|
| **S-Expression** | Human-readable debugging | `.ir.lisp` |
| **JSON** | Tool interchange, validation | `.ir.json` |
| **Protobuf** | Binary storage, network transfer | `.ir.pb` |

All formats represent the same semantic information. JSON and Protobuf MUST validate against their respective schemas.

### 2. Normalization Rules

#### 2.1 Field Ordering
Struct fields are sorted alphabetically in IR, regardless of source order:

```tml
// Source
type Point { y: F64, x: F64 }

// IR (S-Expression)
(type Point @id001
  (fields
    (field x F64)  ; alphabetically first
    (field y F64)))
```

#### 2.2 Whitespace and Comments
Whitespace and comments are discarded. Source location is preserved in `span` metadata.

#### 2.3 Implicit vs. Explicit
All implicit information is made explicit:
- Type annotations always present (inferred types are resolved)
- Effect sets explicit (even if empty: `effects: []`)
- Visibility explicit (default `private` written out)

### 3. Stable IDs

Every named item (function, type, field, parameter) has a content-addressable ID:

```
ID = base58(sha256(canonical_representation))[:8]
```

**Properties:**
- **Deterministic** - Same content → same ID
- **Collision-resistant** - 58^8 ≈ 128 billion combinations
- **Rename-stable** - ID doesn't change when name changes

**Example:**
```tml
func add@a1b2c3d4(x: I32, y: I32) -> I32 {
    return x + y
}
```

The ID `a1b2c3d4` is stable even if:
- Function is renamed: `add` → `sum`
- Parameters renamed: `x, y` → `a, b`
- File moved or module restructured

### 4. IR Node Types

#### 4.1 Module
```json
{
  "kind": "module",
  "id": "mod_abc123",
  "name": "mylib::math",
  "version": "1.0.0",
  "imports": [ ... ],
  "items": [ ... ]
}
```

#### 4.2 Function Definition
```json
{
  "kind": "func_def",
  "id": "fn_abc123",
  "name": "add",
  "visibility": "pub",
  "type_params": [],
  "params": [
    { "name": "a", "type": { "kind": "primitive", "name": "I32" } },
    { "name": "b", "type": { "kind": "primitive", "name": "I32" } }
  ],
  "return_type": { "kind": "primitive", "name": "I32" },
  "effects": [],
  "contracts": [],
  "decorators": [],
  "body": { ... },
  "span": { "file": "math.tml", "start": 10, "end": 35 }
}
```

#### 4.3 Type Definition
```json
{
  "kind": "type_def",
  "id": "ty_xyz789",
  "name": "Point",
  "visibility": "pub",
  "type_params": [],
  "body": {
    "kind": "struct",
    "fields": [
      { "name": "x", "type": { "kind": "primitive", "name": "F64" }, "visibility": "pub" },
      { "name": "y", "type": { "kind": "primitive", "name": "F64" }, "visibility": "pub" }
    ]
  }
}
```

#### 4.4 Expression Nodes
All expressions have:
- `kind` - Expression type (literal, binary_op, call, etc.)
- `type` - Resolved type (inference complete)
- `span` - Source location

**Example:**
```json
{
  "kind": "binary_op",
  "op": "add",
  "left": { "kind": "ident", "name": "a", "type": "I32" },
  "right": { "kind": "ident", "name": "b", "type": "I32" },
  "type": "I32",
  "span": { "file": "math.tml", "start": 25, "end": 30 }
}
```

### 5. Metadata

Every IR node includes:

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | Yes* | Stable content-addressable ID |
| `span` | Span | Yes | Source location (file, start, end) |
| `type` | Type | Yes** | Resolved type (expressions only) |
| `decorators` | Decorator[] | No | Applied decorators |
| `contracts` | Contract[] | No | Pre/post conditions |
| `effects` | string[] | No | Effect annotations |

\* Required for items (functions, types), not for sub-expressions
\** Required for expressions, not for statements/declarations

### 6. Versioning

IR follows semantic versioning (`major.minor.patch`):

- **Patch** (0.1.x) - Bug fixes, clarifications, no schema changes
- **Minor** (0.x.0) - Additive changes (new fields, new node kinds)
- **Major** (x.0.0) - Breaking changes (removed fields, renamed types)

Current version: **1.0.0** (frozen as of TML v0.1.0 compiler release)

Tools MUST:
1. Check `version` field before parsing
2. Reject unknown major versions
3. Ignore unknown fields in minor version bumps (forward compatibility)
4. Preserve unknown fields when round-tripping (backward compatibility)

## Examples

### Example 1: Simple Function

**TML Source:**
```tml
func factorial(n: I32) -> I32 {
    if n <= 1 then 1 else n * factorial(n - 1)
}
```

**IR (S-Expression):**
```lisp
(func factorial @fn_a1b2c3
  (vis private)
  (params (param n I32))
  (return I32)
  (effects [])
  (body
    (if
      (<= (var n) (lit 1 I32))
      (lit 1 I32)
      (* (var n) (call factorial [(- (var n) (lit 1 I32))])))))
```

**IR (JSON):**
```json
{
  "kind": "func_def",
  "id": "fn_a1b2c3",
  "name": "factorial",
  "visibility": "private",
  "params": [
    { "name": "n", "type": { "kind": "primitive", "name": "I32" } }
  ],
  "return_type": { "kind": "primitive", "name": "I32" },
  "effects": [],
  "body": {
    "kind": "if",
    "condition": {
      "kind": "binary_op",
      "op": "le",
      "left": { "kind": "ident", "name": "n" },
      "right": { "kind": "literal", "value": 1, "type": "I32" }
    },
    "then_branch": { "kind": "literal", "value": 1, "type": "I32" },
    "else_branch": {
      "kind": "binary_op",
      "op": "mul",
      "left": { "kind": "ident", "name": "n" },
      "right": {
        "kind": "call",
        "callee": { "kind": "ident", "name": "factorial" },
        "args": [{
          "kind": "binary_op",
          "op": "sub",
          "left": { "kind": "ident", "name": "n" },
          "right": { "kind": "literal", "value": 1, "type": "I32" }
        }]
      }
    }
  }
}
```

### Example 2: Generic Type with Stability Annotations

**TML Source:**
```tml
@stable(since: "v1.0")
pub type Maybe[T] = Just(T) | Nothing
```

**IR (JSON):**
```json
{
  "kind": "type_def",
  "id": "ty_xyz123",
  "name": "Maybe",
  "visibility": "pub",
  "type_params": [
    { "name": "T", "bounds": [] }
  ],
  "decorators": [
    {
      "name": "stable",
      "args": { "since": "v1.0" }
    }
  ],
  "body": {
    "kind": "sum",
    "variants": [
      {
        "name": "Just",
        "fields": [{ "kind": "param", "name": "T" }]
      },
      {
        "name": "Nothing",
        "fields": []
      }
    ]
  }
}
```

## Implementation Status

### Compiler Support (v0.1.0)

| Component | Status | Notes |
|-----------|--------|-------|
| IR Builder | ✅ Implemented | 10 modules, full AST → IR |
| IR Emitter | ✅ Implemented | Emits SSA-form IR |
| Stable IDs | ⚠️ Partial | Uses temp IDs, content-addressing TODO |
| JSON Schema | ✅ Complete | Validates IR nodes |
| Protobuf | ✅ Complete | Binary serialization ready |
| S-Expression | ⚠️ Partial | Debug output only, no parser |

### IR Generation Pipeline

```
Source Code (.tml)
    ↓
Lexer → Tokens
    ↓
Parser → AST (Abstract Syntax Tree)
    ↓
Type Checker → TAST (Typed AST)
    ↓
Borrow Checker → Valid TAST
    ↓
IR Builder → IR Nodes (SSA form)
    ↓
IR Emitter → JSON/Protobuf/S-Expr
```

## Desugaring

Surface syntax (RFC-0002) desugars to IR:

| Surface Syntax | IR Equivalent |
|----------------|---------------|
| `class Point { state x, y }` | `type Point = { x: ..., y: ... }` |
| `@decorator` | Transformed during IR generation |
| `for i in 0 to 10` | `loop` with explicit iteration |
| `arr[i]` | `call(index, [arr, i])` |
| `arr.len()` | `call(len, [arr])` |

## Compatibility

### With RFC-0001 (Core)
IR is the machine representation of RFC-0001. All core language features have IR nodes.

### With RFC-0002 (Syntax)
Surface syntax desugars to IR during compilation. Multiple surface forms → same IR.

### With RFC-0003 (Contracts)
Contracts stored in `contracts` metadata field. Verification is IR-level analysis.

### With RFC-0004 (Errors)
`Outcome[T, E]` and `!` operator represented as IR types and expressions.

### With RFC-0005 (Modules)
Module structure preserved in IR. Imports are explicit dependencies.

## Alternatives Rejected

### 1. AST as IR
**Rejected:** AST preserves surface syntax details (whitespace, comments, syntactic sugar). IR must be normalized.

### 2. LLVM IR as Canonical Format
**Rejected:** LLVM IR is too low-level, loses semantic information needed for analysis.

### 3. Custom Binary Format
**Rejected:** Protobuf provides proven, extensible binary serialization.

### 4. Random UUIDs for IDs
**Rejected:** Content-addressable IDs are deterministic and merge-friendly.

## References

### Inspiration
- **Rust MIR** (Mid-level IR) - SSA form, borrow checking
- **Cranelift IR** - Code generation IR
- **WebAssembly** - Portable binary format
- **JSON-LD** - Schema-based JSON interchange

### Standards
- [JSON Schema Draft-07](http://json-schema.org/draft-07/schema)
- [Protocol Buffers v3](https://developers.google.com/protocol-buffers)
- [Base58 Encoding](https://en.bitcoin.it/wiki/Base58Check_encoding)

### Related RFCs
- RFC-0001 (Core Language) - Semantic foundation
- RFC-0002 (Surface Syntax) - Desugars to this IR
- RFC-0016 (Compiler Architecture) - IR in compilation pipeline

## Schema Files

- [`ir/tml-ir.schema.json`](ir/tml-ir.schema.json) - JSON Schema (909 lines)
- [`ir/tml-ir.proto`](ir/tml-ir.proto) - Protobuf schema (587 lines)
- [`ir/VERSION.md`](ir/VERSION.md) - Version history and migration guide

## Checksums (v1.0.0)

```
SHA-256:
  tml-ir.schema.json: (to be calculated on freeze)
  tml-ir.proto:       (to be calculated on freeze)
```
