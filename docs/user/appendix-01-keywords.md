# Appendix A - Keywords

The following list contains keywords reserved for current or future use
by TML. These cannot be used as identifiers.

## Currently Used Keywords

| Keyword | Description |
|---------|-------------|
| `and` | Logical AND operator |
| `as` | Type casting |
| `async` | Asynchronous function marker |
| `await` | Wait for async result |
| `behavior` | Define a behavior (trait) |
| `break` | Exit a loop |
| `const` | Constant declaration |
| `continue` | Skip to next loop iteration |
| `crate` | Reference to current crate |
| `decorator` | Function decorator |
| `do` | Closure/lambda syntax |
| `else` | Alternative branch |
| `false` | Boolean false literal |
| `for` | For loop |
| `func` | Function declaration |
| `if` | Conditional branch |
| `impl` | Implementation block |
| `in` | Used in for loops |
| `let` | Variable declaration |
| `loop` | Infinite loop |
| `lowlevel` | Unsafe code block |
| `mod` | Module declaration |
| `mut` | Mutable modifier |
| `not` | Logical NOT operator |
| `or` | Logical OR operator |
| `pub` | Public visibility |
| `quote` | Macro quoting |
| `ref` | Reference type |
| `return` | Return from function |
| `super` | Parent module |
| `then` | Used after conditions |
| `this` | Current instance |
| `This` | Current type |
| `through` | Inclusive range end |
| `to` | Exclusive range end |
| `true` | Boolean true literal |
| `type` | Type/struct declaration |
| `use` | Import declaration |
| `when` | Pattern matching |
| `with` | Context expression |

## Reserved for Future Use

These keywords are reserved but not yet implemented:

| Keyword | Planned Use |
|---------|-------------|
| `match` | Alternative to `when` |
| `enum` | Enumeration type |
| `trait` | Alternative to `behavior` |
| `where` | Generic constraints |
| `yield` | Generator functions |
| `macro` | Macro definitions |

## Naming Conventions

While not keywords, TML has naming conventions:

| Pattern | Use |
|---------|-----|
| `snake_case` | Variables, functions |
| `PascalCase` | Types, behaviors |
| `SCREAMING_CASE` | Constants |
| `_prefix` | Unused variables |
