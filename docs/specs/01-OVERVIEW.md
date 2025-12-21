# TML v1.0 — Overview

## 1. The Problem

All existing programming languages were designed for **humans**:
- Syntax optimized for manual reading and writing
- Ambiguities resolved by context (which humans understand)
- Syntactic sugar for typing convenience
- Error messages for human developers

LLMs are not humans. They need:
- **Deterministic parsing** — no ambiguities
- **Unique tokens** — each symbol with one meaning
- **Explicit structure** — no contextual inferences
- **Stable IDs** — references that survive refactoring

## 2. The Solution: TML

**TML (To Machine Language)** is a language where:

| Principle | Implementation |
|-----------|----------------|
| One token = one meaning | `<` is always comparison, `[` is always generic/array |
| LL(1) parsing | Lookahead of 1 token determines the production |
| Explicit > implicit | `return` mandatory, `then` mandatory |
| Stable IDs | Functions and types have immutable `@id` |
| No macros | Code is code, no meta-programming |

## 3. Comparison with Rust

TML learns from Rust but doesn't copy its syntax.

### 3.1 What TML Keeps from Rust
- Ownership and borrowing (concept)
- Result/Option for errors
- Pattern matching
- Traits (called traits)
- Expressions as values
- No null/nil

### 3.2 What TML Changes

| Aspect | Rust | TML | Reason |
|--------|------|-----|--------|
| Generics | `Vec<T>` | `List[T]` | `<>` conflicts with comparison |
| Closures | `\|x\| x+1` | `do(x) x+1` | `\|` is bitwise OR |
| Logical | `&&` `\|\|` `!` | `and` `or` `not` | Clearer |
| Functions | `fn` | `func` | More explicit |
| Self | `self` | `this` | Universal |
| Modules | `mod`/`use` | `module`/`import` | Clearer |
| Impl | `impl T for X` | `extend X with T` | Reads naturally |
| Mutable | `let mut` | `var` | Shorter |
| Lifetimes | `'a` | (inferred) | Not exposed to user |
| Match | `match x {}` | `when x {}` | No conflict with English "match" |
| Loops | `for`/`while`/`loop` | unified `loop` | One keyword |

### 3.3 Side-by-Side Examples

**Generic function:**
```rust
// Rust
fn first<T: Clone>(items: &[T]) -> Option<T> {
    items.first().cloned()
}
```
```tml
// TML
func first[T: Clone](items: &List[T]) -> Option[T] {
    return items.first().clone()
}
```

**Struct with methods:**
```rust
// Rust
struct Point { x: f64, y: f64 }

impl Point {
    fn new(x: f64, y: f64) -> Self {
        Self { x, y }
    }

    fn distance(&self, other: &Point) -> f64 {
        let dx = self.x - other.x;
        let dy = self.y - other.y;
        (dx*dx + dy*dy).sqrt()
    }
}
```
```tml
// TML
type Point { x: F64, y: F64 }

extend Point {
    func new(x: F64, y: F64) -> This {
        return This { x: x, y: y }
    }

    func distance(this, other: &Point) -> F64 {
        let dx = this.x - other.x
        let dy = this.y - other.y
        return (dx**2 + dy**2).sqrt()
    }
}
```

**Pattern matching:**
```rust
// Rust
match value {
    Some(x) if x > 0 => x * 2,
    Some(x) => x,
    None => 0,
}
```
```tml
// TML - no guards, inline logic
when value {
    Some(x) -> if x > 0 then x * 2 else x,
    None -> 0,
}
```

**Error handling:**
```rust
// Rust
let file = File::open("data.txt")?;
let content = std::fs::read_to_string(&file)?;
let parsed = content.parse::<i32>().unwrap_or(0);
```
```tml
// TML
let file = File.open("data.txt")!
let content = file.read_string()!
let parsed = content.parse[I32]()! else 0
```

**Closures:**
```rust
// Rust
let nums: Vec<i32> = items
    .iter()
    .filter(|x| x > &0)
    .map(|x| x * 2)
    .collect();
```
```tml
// TML
let nums: List[I32] = items
    .filter(do(x) x > 0)
    .map(do(x) x * 2)
    .collect()
```

## 4. Unique Features

### 4.1 Stable IDs

Each definition has a unique ID that survives renames:

```tml
func calculate@a1b2c3d4(x: I32) -> I32 {
    return x * 2
}
```

LLMs can reference `@a1b2c3d4` in patches without depending on the name.

### 4.2 Caps and Effects

Explicit declaration of what code can and does:

```tml
module Database {
    caps: [io.network, io.file]

    func query(sql: String) -> Result[Rows, Error]
    effects: [db.read]
    {
        // ...
    }
}
```

### 4.3 Contracts

Formal pre and post-conditions:

```tml
func sqrt(x: F64) -> F64
pre: x >= 0.0
post(result): result >= 0.0 and result * result == x
{
    // ...
}
```

### 4.4 Canonical IR

Code normalizes to a unique representation:

```tml
// Source
type Point { y: F64, x: F64 }

// IR (fields sorted alphabetically)
type Point { x: F64, y: F64 }
```

## 5. LL(1) Grammar

Each token uniquely determines the production:

| First Token | Production |
|-------------|------------|
| `module` | Module declaration |
| `import` | Import |
| `func` | Function |
| `type` | Type (struct or enum) |
| `trait` | Trait |
| `extend` | Type extension |
| `let` | Immutable binding |
| `var` | Mutable binding |
| `const` | Constant |
| `if` | Conditional |
| `when` | Pattern match |
| `loop` | Iteration |
| `return` | Return |

## 6. No Macros

Macros break deterministic parsing. TML doesn't have:

```rust
// Rust - macro changes syntax
println!("x = {}", x);
vec![1, 2, 3];
#[derive(Debug)]
```

```tml
// TML - normal functions
print("x = " + x.to_string())
List.of(1, 2, 3)
#[derive(Debug)]  // annotation, not macro
```

Annotations like `#[derive(Debug)]` are processed by the compiler, not arbitrary macros.

## 7. Design Philosophy

### 7.1 Always Explicit

```tml
// return mandatory
func add(a: I32, b: I32) -> I32 {
    return a + b  // not: a + b alone
}

// then mandatory
if x > 0 then positive() else negative()  // not: if x > 0 { }

// type in parameters mandatory
func greet(name: String)  // not: func greet(name)
```

### 7.2 One Way to Do It

```tml
// One form of loop
loop item in items { }
loop while condition { }
loop { }

// One form of pattern match
when x { }

// One form of function
func name() { }
```

### 7.3 No Magic

```tml
// No implicit coercion
let x: I64 = 42i32          // error: needs .to_i64()
let x: I64 = 42i32.to_i64() // ok

// No magical operator overloading
// + is defined via trait Add explicitly
```

## 8. Use Cases

### 8.1 Code Generation by LLM
- Deterministic parsing allows immediate validation
- Stable IDs allow surgical patches
- No ambiguity reduces generation errors

### 8.2 Code Analysis by LLM
- Explicit caps/effects allow reasoning about behavior
- Formalizable contracts
- Canonical IR allows semantic diff

### 8.3 Automatic Refactoring
- IDs survive renames
- Transformations preserve semantics via IR
- Patches applicable without context

---

*Next: [02-LEXICAL.md](./02-LEXICAL.md) — Lexical Specification*
