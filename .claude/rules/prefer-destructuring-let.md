# Use destructuring let to extract struct fields directly

## Rule

When you need multiple fields from a struct, use destructuring `let` instead of individual field accesses into separate variables.

## Anti-pattern

```tml
let name = decl.name
let generics = decl.generics
let fields = decl.fields
let span = decl.span
```

## Correct pattern

```tml
let StructDecl { name, generics, fields, span } = decl
```

## Also works with

- **Renamed bindings**: `let Point { x: px, y: py } = point`
- **Tuples**: `let (a, b, c) = get_triple()`
- **Nested**: `let Wrapper { inner: Point { x, y } } = w`
