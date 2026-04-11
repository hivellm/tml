# F05: Derive Macros & Auto-Serialization

**Priority**: High
**Impact**: 80+ lines of manual discriminant encoding
**Complexity**: High (requires attribute system + code generation)

## Problem

Enum serialization requires manually writing out every variant with an explicit
discriminant number:

```tml
func write_binary_op(w: BinaryWriter, op: BinaryOp) {
    when op {
        OpAdd  => w.write_u8(0),
        OpSub  => w.write_u8(1),
        OpMul  => w.write_u8(2),
        // ... 25 more variants
    }
}
```

This is mechanical, error-prone (off-by-one in discriminants), and must be
kept in sync with the corresponding reader.

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `compiler-tml/src/ast/ast_writer.tml` | 108-152 | 28-variant BinaryOp encoding |
| `compiler-tml/src/ast/ast_writer.tml` | 154-170 | 8-variant UnaryOp encoding |
| `compiler-tml/src/ast/ast_writer.tml` | 172-185 | 6-variant LiteralKind encoding |
| `compiler-tml/src/ast/ast_writer.tml` | throughout | ~15 write functions for AST enums |

## Proposal

### Phase 1: Enum discriminant attributes

```tml
@repr(u8)
pub enum BinaryOp {
    OpAdd,    // = 0 (implicit)
    OpSub,    // = 1
    OpMul,    // = 2
    // ...
}

// Then: w.write_u8(op as U8)
```

### Phase 2: Derive attributes

```tml
@derive(Serialize, Deserialize)
pub enum BinaryOp { ... }

// Auto-generates write/read methods
```

### Phase 3: Derive for structs

```tml
@derive(Display, PartialEq, Duplicate)
pub type FuncParam {
    name: Str,
    ty: TypePath,
}
```

## C++ Compiler Changes

1. **Parser**: Parse `@attr` attributes on type/enum/func declarations
2. **Attribute registry**: Built-in attributes (`repr`, `derive`) + validation
3. **Codegen for @repr**: Store discriminant layout info; allow `as U8` cast on enums
4. **Derive expansion**: Generate impl blocks at compile time for known derives
5. **Long-term**: User-defined procedural macros (very complex, defer)
