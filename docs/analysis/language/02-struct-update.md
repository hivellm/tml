# F02: Struct Update Syntax

**Priority**: Critical
**Impact**: 50+ call sites with full struct reconstruction
**Complexity**: Low-Medium (parser + codegen)

## Problem

Modifying a single field requires reconstructing the entire struct:

```tml
pub func short(this, s: Str) -> Arg {
    return Arg {
        name: this.name,
        short_flag: s,             // only this field changes
        long_flag: this.long_flag,
        help_text: this.help_text,
        is_flag: this.is_flag,
        is_required: this.is_required,
        default_val: this.default_val,
        is_positional: this.is_positional
    }
}
```

This pattern repeats 6 times in `cli.tml` alone (short, long, help, flag, required,
default, positional), each reconstructing an 8-field struct to change one field.

## Evidence

| File | Lines | Pattern |
|------|-------|---------|
| `lib/std/src/cli.tml` | 65-132 | 6 builder methods, each copies 8 fields |
| `lib/std/src/cli.tml` | 159-172 | App::version/description, copies 4 fields |
| `lib/std/src/bigint.tml` | 50-58 | Constructor boilerplate |
| `compiler-tml/src/parser/common.tml` | throughout | ParseState reconstruction |

## Proposal

### Struct update syntax (Rust-style `..`)

```tml
pub func short(this, s: Str) -> Arg {
    return Arg { short_flag: s, ..this }
}
```

Semantics:
- `..expr` copies all unspecified fields from `expr`
- `expr` must be the same struct type
- Explicitly listed fields override copied ones
- If the type implements `Duplicate`, fields are duplicated; otherwise moved

### Alternative: `with` keyword

```tml
return this with { short_flag: s }
```

More readable but introduces a new keyword.

## C++ Compiler Changes

1. **Parser**: Allow `..expr` as the last item in struct literal field list
2. **Type checker**: Verify source expression matches target struct type
3. **Codegen**: Emit field-by-field copy for unspecified fields, use explicit values for specified ones
4. **Optimization**: For small structs, emit `insertvalue` chains; for large structs, memcpy + overwrite
