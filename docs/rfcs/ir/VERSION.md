# TML IR Version 0.1.0

**Status: FROZEN**

This IR schema is frozen as of v0.1.0. Changes require a new minor version.

## Versioning Policy

- **Patch (0.1.x)**: Bug fixes, clarifications, no schema changes
- **Minor (0.x.0)**: Additive changes (new fields, new node types)
- **Major (x.0.0)**: Breaking changes (removed fields, renamed types)

## v0.1.0 (Current)

Initial frozen release. Includes:

### Node Types

**Items:**
- `func_def` - Function definitions
- `type_def` - Type definitions (struct, sum, alias)
- `const_def` - Constant definitions
- `behavior_def` - Behavior (trait) definitions
- `impl_block` - Implementation blocks
- `mod_def` - Module definitions
- `decorator_def` - Custom decorator definitions

**Types:**
- `primitive` - Bool, Char, integers, floats
- `named` - Path with generic args
- `tuple` - Tuple types
- `function` - Function types with effects
- `reference` - ref T, mut ref T
- `pointer` - *T, *mut T
- `array` - [T; N]
- `slice` - [T]
- `param` - Type parameter reference

**Expressions:**
- `literal` - All literal types
- `ident` - Identifiers and paths
- `binary_op` - Binary operations
- `unary_op` - Unary operations
- `call` - Function calls
- `field_access` - Field access
- `index` - Array/slice indexing
- `if` - Conditional expressions
- `match` - Pattern matching
- `loop` - Loop expressions
- `block` - Block expressions
- `return` - Return expressions
- `break` - Break expressions
- `continue` - Continue expressions
- `let` - Let bindings
- `assign` - Assignment
- `ref` - Reference creation
- `deref` - Dereference
- `cast` - Type cast
- `struct_init` - Struct initialization
- `tuple` - Tuple expressions
- `array` - Array expressions
- `closure` - Closure expressions
- `propagate` - Error propagation (!)
- `await` - Async await

**Patterns:**
- `wildcard` - _
- `ident` - Identifier binding
- `literal` - Literal matching
- `tuple` - Tuple destructuring
- `struct` - Struct destructuring
- `variant` - Enum variant matching
- `or` - Or patterns
- `range` - Range patterns

### Metadata

- `id` - Content-addressable stable ID
- `span` - Source location (file, start, end, line, column)
- `decorators` - Applied decorators
- `contracts` - Pre/post conditions
- `effects` - Effect annotations
- `visibility` - pub, pub(crate), etc.

## Migration Guide

### From pre-v0.1.0

No prior versions exist. This is the initial release.

## Schema Files

- `tml-ir.schema.json` - JSON Schema (for validation)
- `tml-ir.proto` - Protobuf (for binary serialization)

## Compatibility

Tools MUST:
1. Check `version` field before parsing
2. Reject unknown major versions
3. Ignore unknown fields in minor version bumps
4. Preserve unknown fields when round-tripping

## Checksums

```
tml-ir.schema.json: (calculate on freeze)
tml-ir.proto: (calculate on freeze)
```
