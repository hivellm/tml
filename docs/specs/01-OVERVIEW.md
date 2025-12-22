# TML v1.0 — Overview

## 1. Design Philosophy: LLM-First, Human-Friendly

> **TML is designed for LLM code generation, with ergonomics inspired by Rust and C# for human developers.**

### 1.1 The Problem

Existing programming languages were designed for **humans**:
- Syntax optimized for manual reading and writing
- Ambiguities resolved by context (which humans understand)
- Syntactic sugar for typing convenience
- Cryptic symbols and abbreviations

LLMs need something different:
- **Deterministic parsing** — no ambiguities
- **Unique tokens** — each symbol with one meaning
- **Explicit structure** — no contextual inferences
- **Stable IDs** — references that survive refactoring
- **Self-documenting syntax** — words over symbols

### 1.2 The Solution: Best of Both Worlds

TML takes the **rigor needed for LLM generation** and combines it with the **ergonomics that humans expect** from modern languages:

| From Rust | From C# | TML Innovation |
|-----------|---------|----------------|
| Ownership model | Clean generics `List[T]` | `and`/`or`/`not` keywords |
| Pattern matching | Method syntax `.len()` | `to`/`through` for ranges |
| `ref`/`mut` semantics | Properties and indexers | `Maybe[T]`/`Outcome[T,E]` |
| Zero-cost abstractions | LINQ-style chains | `@directives` over macros |
| Traits → `behavior` | `async`/`await` | Stable IDs `@abc123` |

**For LLMs:** Deterministic LL(1) grammar, unique token meanings, self-documenting syntax.

**For Humans:** Familiar Rust/C# patterns, readable keywords, method chaining, type inference.

## 2. The Solution: TML

**TML (To Machine Language)** is a language where:

| Principle | Implementation |
|-----------|----------------|
| Words over symbols | `and`, `or`, `not`, `ref`, `to` instead of `&&`, `\|\|`, `!`, `&`, `..` |
| One token = one meaning | `<` is always comparison, `[` is always generic/array |
| LL(1) parsing | Lookahead of 1 token determines the production |
| Explicit > implicit | `return` mandatory, `then` mandatory |
| Stable IDs | Functions and types have immutable `@id` |
| Self-documenting types | `Maybe[T]`, `Outcome[T, E]`, `Shared[T]` |
| Natural language directives | `@when(os: linux)`, `@auto(debug)` |
| No macros | Code is code, no meta-programming |

## 3. TML Design Principles

### 3.1 What Makes TML Different

| Aspect | Other Languages | TML | Reason |
|--------|-----------------|-----|--------|
| Generics | `Vec<T>` | `List[T]` | `<>` conflicts with comparison |
| References | `&T`, `&mut T` | `ref T`, `mut ref T` | Words, not symbols |
| Closures | `\|x\| x+1` | `do(x) x+1` | `\|` is bitwise OR |
| Logical | `&&` `\|\|` `!` | `and` `or` `not` | Natural language |
| Ranges | `0..10`, `0..=10` | `0 to 10`, `0 through 10` | Reads like English |
| Optional | `Option<T>`, `Some/None` | `Maybe[T]`, `Just/Nothing` | Self-documenting |
| Errors | `Result<T,E>`, `Ok/Err` | `Outcome[T,E]`, `Ok/Err` | Self-documenting type name |
| Heap alloc | `Box<T>` | `Heap[T]` | Describes where it lives |
| Ref counted | `Rc<T>`, `Arc<T>` | `Shared[T]`, `Sync[T]` | Describes behavior |
| Unsafe | `unsafe { }` | `lowlevel { }` | Neutral term |
| Annotations | `#[cfg(...)]` | `@when(...)` | Universal, readable |
| Derive | `#[derive(...)]` | `@auto(...)` | Clear purpose |
| Interfaces | `trait` | `behavior` | Describes what it defines |
| Clone | `.clone()` | `.duplicate()` | Clear action |

### 3.2 Side-by-Side Examples

**Generic function:**
```tml
func first[T: Duplicate](items: ref List[T]) -> Maybe[T] {
    return items.first().duplicate()
}
```

**Struct with methods:**
```tml
type Point { x: F64, y: F64 }

extend Point {
    func new(x: F64, y: F64) -> This {
        return This { x: x, y: y }
    }

    func distance(this, other: ref Point) -> F64 {
        let dx: F64 = this.x - other.x
        let dy: F64 = this.y - other.y
        return (dx**2 + dy**2).sqrt()
    }
}
```

**Pattern matching:**
```tml
when value {
    Just(x) -> if x > 0 then x * 2 else x,
    Nothing -> 0,
}
```

**Error handling:**
```tml
let file: Outcome[File, Error] = File.open("data.txt")!
let content: String = file.read_string()!
let parsed: Outcome[I32, Error] = content.parse[I32]()! else 0
```

**Closures:**
```tml
let nums: List[I32] = items
    .filter(do(x) x > 0)
    .map(do(x) x * 2)
    .collect()
```

**References:**
```tml
// Immutable reference
func length(s: ref String) -> U64 {
    return s.len()
}

// Mutable reference
func append(s: mut ref String, suffix: String) {
    s.push(suffix)
}
```

**Behaviors (interfaces):**
```tml
behavior Printable {
    func to_text(this) -> String
}

extend Point with Printable {
    func to_text(this) -> String {
        return "(" + this.x.to_string() + ", " + this.y.to_string() + ")"
    }
}
```

**Directives:**
```tml
@when(os: linux)
func linux_specific() { ... }

@auto(debug, duplicate, equal)
type Config { name: String, value: I32 }

@test
func test_example() { assert(true) }

@lowlevel
func raw_access(p: ptr U8) -> U8 {
    return p.read()
}
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

    func query(sql: String) -> Outcome[Rows, Error]
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
| `behavior` | Behavior (interface) |
| `extend` | Type extension |
| `let` | Immutable binding |
| `var` | Mutable binding |
| `const` | Constant |
| `if` | Conditional |
| `when` | Pattern match |
| `loop` | Iteration |
| `return` | Return |
| `@` | Directive |

## 6. No Macros

Macros break deterministic parsing. TML uses directives instead:

```tml
// TML - normal functions and directives
print("x = " + x.to_string())
List.of(1, 2, 3)

@auto(debug)     // compiler processes this
type MyType { value: I32 }
```

Directives like `@auto(debug)` are processed by the compiler, not arbitrary code transformations.

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

// type annotations mandatory on all variables
let x: I32 = 42           // not: let x: I32 = 42
var count: I32 = 0        // not: var count: I32 = 0
const PI: F64 = 3.14159   // not: const PI = 3.14159
```

### 7.2 One Way to Do It

```tml
// One form of loop
loop item in items { }
loop i in 0 to 10 { }
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
// + is defined via behavior Addable explicitly
```

### 7.4 Self-Documenting

```tml
// Types explain themselves
Maybe[User]           // Maybe there's a user
Outcome[Data, Error]  // Outcome is success or failure
Shared[Cache]         // Shared reference-counted data
Heap[LargeData]       // Data allocated on heap

// Constructors explain themselves
Just(user)            // Just this user
Nothing               // Nothing here
Ok(data)              // Successful outcome
Err(error)            // Failed outcome
```

## 8. Use Cases

### 8.1 Code Generation by LLM
- Deterministic parsing allows immediate validation
- Stable IDs allow surgical patches
- No ambiguity reduces generation errors
- Self-documenting syntax reduces hallucinations

### 8.2 Code Analysis by LLM
- Explicit caps/effects allow reasoning about behavior
- Formalizable contracts
- Canonical IR allows semantic diff
- Natural language keywords improve comprehension

### 8.3 Automatic Refactoring
- IDs survive renames
- Transformations preserve semantics via IR
- Patches applicable without context

---

*Next: [02-LEXICAL.md](./02-LEXICAL.md) — Lexical Specification*
