# Use @auto for common behavior derives, @repr for enum discriminants

## Rule

- Use `@auto(duplicate, equal)` instead of manual `impl Duplicate`/`impl PartialEq` for simple data types
- Use `@repr(U8)` on enums with few variants to control discriminant size
- Use `@packed` on structs used for binary protocols or wire formats

## Examples

```tml
@auto(duplicate, equal, debug)
type Point { x: I32, y: I32 }

@repr(U8)
enum Color { Red, Green, Blue }

@packed
type PacketHeader { magic: I32, version: I8, flags: I32 }
```

## @auto name mapping

| @auto | @derive equivalent |
|-------|-------------------|
| `duplicate` | `Duplicate` |
| `equal` | `PartialEq` |
| `debug` | `Debug` |
| `display` | `Display` |
| `hash` | `Hash` |
| `default` | `Default` |
| `serialize` | `Serialize` |
| `deserialize` | `Deserialize` |
