# TML Syntax Quick Reference

## Keywords (35+)

### Declarations
- `module` - Module declaration
- `import` - Import statement
- `use` - Use declaration
- `public` / `private` / `pub` - Visibility modifiers
- `func` - Function declaration
- `type` - Type/struct/enum declaration
- `behavior` - Trait/interface declaration
- `extend` / `impl` - Implementation block
- `const` - Constant declaration
- `let` - Immutable variable
- `var` - Mutable variable
- `where` - Generic constraints

### Control Flow
- `if` / `then` / `else` - Conditional
- `when` - Pattern matching
- `loop` - Infinite loop
- `while` - While loop
- `for` / `in` - For loop
- `break` - Exit loop
- `continue` - Skip iteration
- `return` - Return from function
- `catch` - Error handling

### Logical Operators (Words)
- `and` - Logical AND (&&)
- `or` - Logical OR (||)
- `not` - Logical NOT (!)

### Special
- `this` - Current instance
- `This` - Current type
- `ref` - Reference type
- `mut` - Mutable modifier
- `lowlevel` - Unsafe code
- `async` / `await` - Async operations
- `do` - Closure/lambda
- `to` / `through` - Range operators
- `with` - Context
- `as` - Type casting

## Types

### Primitives
- **Bool** - Boolean
- **I8, I16, I32, I64, I128** - Signed integers
- **U8, U16, U32, U64, U128** - Unsigned integers
- **F32, F64** - Floating point
- **String** - String type
- **Char** - Character type
- **Unit** - Empty type

### Collections
- **List[T]** - Dynamic array
- **Map[K, V]** - Key-value map
- **HashMap[K, V]** - Hash map
- **Set[T]** - Set
- **Vec[T]** - Vector
- **Buffer** - Byte buffer

### Result Types
- **Maybe[T]** - Optional value (Just(T) | Nothing)
- **Outcome[T, E]** - Result type (Ok(T) | Err(E))

### Smart Pointers
- **Heap[T]** - Heap allocation (Box)
- **Shared[T]** - Shared reference (Rc)
- **Sync[T]** - Thread-safe shared (Arc)

## Operators

### Arithmetic
- `+` Addition
- `-` Subtraction/Negation
- `*` Multiplication
- `/` Division
- `%` Modulo
- `**` Exponentiation

### Comparison
- `==` Equal
- `!=` Not equal
- `<` Less than
- `>` Greater than
- `<=` Less or equal
- `>=` Greater or equal

### Bitwise
- `&` Bitwise AND
- `|` Bitwise OR
- `^` Bitwise XOR
- `~` Bitwise NOT
- `<<` Left shift
- `>>` Right shift

### Assignment
- `=` Assignment
- `+=` `-=` `*=` `/=` `%=` Compound assignment
- `&=` `|=` `^=` `<<=` `>>=` Bitwise assignment

### Other
- `->` Return type arrow
- `!` Error propagation
- `?` `:` Ternary conditional
- `::` Path separator
- `.` Member access
- `@` Directive prefix

## Literals

### Integers
```tml
42                  // Decimal
1_000_000          // With underscores
0xFF_AA_BB         // Hexadecimal
0b1010_1010        // Binary
0o755              // Octal
42i32              // With type suffix
255u8              // Unsigned
```

### Floats
```tml
3.14159            // Decimal
2.5e10             // Scientific notation
1.0e-5             // Negative exponent
3.14f32            // With type suffix
```

### Strings
```tml
"hello"                    // Regular string
"line1\nline2"            // With escapes
r"C:\path\to\file"        // Raw string
r#"Can use "quotes""#     // Raw with delimiter
"""
Multi-line
string
"""                        // Multi-line
b"bytes"                  // Byte string
```

### Characters
```tml
'a'                // ASCII
'\n'               // Escape
'\u{1F600}'        // Unicode
```

### Booleans
```tml
true
false
```

## Directives

### Testing
- `@test` - Test function
- `@test(should_fail)` - Expected to fail
- `@benchmark` - Benchmark function

### Conditional Compilation
- `@when(os: linux)` - OS-specific
- `@when(arch: x86_64)` - Architecture-specific
- `@when(feature: async)` - Feature flag
- `@unless(debug)` - Inverse condition

### Auto-Generation
- `@auto(debug)` - Generate Debug
- `@auto(duplicate)` - Generate Clone
- `@auto(equal)` - Generate Eq
- `@auto(order)` - Generate Ord
- `@auto(hash)` - Generate Hash

### Optimization
- `@hint(inline)` - Inline suggestion
- `@hint(inline: always)` - Force inline
- `@hint(cold)` - Unlikely path
- `@hint(hot)` - Hot path

### Other
- `@deprecated("msg")` - Deprecation
- `@lowlevel` - Unsafe block
- `@export` - Public API marker
- `@doc("text")` - Documentation

## Comments

```tml
// Line comment

/* Block comment */

/// Doc comment for following item
/// Supports **markdown**

//! Module documentation

// @ai:context: Context for LLMs
// @ai:intent: Purpose/intent
// @ai:invariant: Assumptions
// @ai:warning: Warnings
// @ai:example: Usage examples
```

## Common Patterns

### Function
```tml
func add(a: I32, b: I32) -> I32 {
    return a + b
}
```

### Struct
```tml
type Point {
    x: F64,
    y: F64,
}
```

### Enum
```tml
type Maybe[T] {
    Just(T),
    Nothing,
}
```

### Pattern Matching
```tml
when value {
    Just(x) -> use(x),
    Nothing -> default(),
}
```

### Closure
```tml
let double = do(x) x * 2
list.map(do(x) x * 2)
```

### Error Propagation
```tml
let file = File.open(path)!
let data = file.read()!
```

### If Expression
```tml
if condition then value1 else value2
```

### Ternary
```tml
let max = a > b ? a : b
```

### For Loop
```tml
for i in 0 to 10 {
    println(i)
}

for item in list {
    process(item)
}
```

### Behavior (Trait)
```tml
behavior Drawable {
    func draw(this) -> String;
}

extend Shape with Drawable {
    func draw(this) -> String {
        // Implementation
    }
}
```
