# Appendix A - Keywords

The following list contains keywords reserved for current or future use
by TML. These cannot be used as identifiers.

## Currently Used Keywords (57 reserved words)

### Declarations

| Keyword | Description |
|---------|-------------|
| `func` | Function declaration |
| `type` | Type/struct declaration |
| `behavior` | Define a behavior (trait) |
| `impl` | Implementation block |
| `mod` | Module declaration |
| `use` | Import declaration |
| `pub` | Public visibility |
| `let` | Variable declaration |
| `const` | Constant declaration |
| `decorator` | Function decorator |
| `where` | Generic constraints |

### Control Flow

| Keyword | Description |
|---------|-------------|
| `if` | Conditional branch |
| `then` | Used after conditions |
| `else` | Alternative branch |
| `when` | Pattern matching |
| `loop` | Unified loop keyword |
| `while` | While loop condition |
| `for` | For loop |
| `in` | Used in for loops |
| `to` | Exclusive range end |
| `through` | Inclusive range end |
| `break` | Exit a loop |
| `continue` | Skip to next loop iteration |
| `return` | Return from function |
| `do` | Closure/lambda syntax |

### Logical Operators

| Keyword | Description |
|---------|-------------|
| `and` | Logical AND (short-circuit) |
| `or` | Logical OR (short-circuit) |
| `not` | Logical negation |

### Types and References

| Keyword | Description |
|---------|-------------|
| `this` | Current instance |
| `This` | Current type (in impl blocks) |
| `as` | Type casting |
| `dyn` | Dynamic dispatch |
| `mut` | Mutable modifier |
| `ref` | Reference type |
| `lowlevel` | Unsafe/low-level code block |
| `life` | Lifetime annotation (reserved) |

### Modules

| Keyword | Description |
|---------|-------------|
| `crate` | Reference to current crate |
| `super` | Parent module |
| `with` | Context expression |

### Async (reserved for future)

| Keyword | Description |
|---------|-------------|
| `async` | Asynchronous function marker |
| `await` | Wait for async result |
| `quote` | Macro quoting |

### OOP Keywords (reserved for future C#-style OOP)

| Keyword | Description |
|---------|-------------|
| `class` | Class declaration |
| `interface` | Interface declaration |
| `extends` | Inheritance |
| `implements` | Interface implementation |
| `override` | Method override |
| `virtual` | Virtual method |
| `abstract` | Abstract class/method |
| `sealed` | Non-inheritable class |
| `namespace` | Namespace declaration |
| `base` | Base class reference |
| `protected` | Protected visibility |
| `private` | Private visibility |
| `static` | Static member |
| `new` | Constructor |
| `prop` | Property declaration |

## Literals

> **Note:** These are lexed as literals, not as keywords.

| Token | Description |
|-------|-------------|
| `true` | Boolean true literal |
| `false` | Boolean false literal |
| `null` | Null pointer literal (`Ptr[Unit]`) |

## Compile-Time Constants

| Constant | Type | Description |
|----------|------|-------------|
| `__FILE__` | `Str` | Current source file path |
| `__DIRNAME__` | `Str` | Directory of current source file |
| `__LINE__` | `I64` | Current line number |

## Naming Conventions

While not keywords, TML has naming conventions:

| Pattern | Use |
|---------|-----|
| `snake_case` | Variables, functions |
| `PascalCase` | Types, behaviors |
| `SCREAMING_CASE` | Constants |
| `_prefix` | Unused variables |
