# TML Standard Library: String Interning

> `std::intern` — Memory-efficient string deduplication with global interning cache.

## Overview

The intern package provides string interning capabilities. Interned strings are deduplicated in a global cache, reducing memory overhead when the same strings appear many times in a program. Each unique string is stored once, and references are identity-based rather than equality-based.

## Import

```tml
use std::intern
use std::intern::{Interner, intern, lookup}
```

---

## Core Types

### Interner — Global String Cache

```tml
pub type Interner {
    // internal: HashMap[Str, I64] mapping string content to unique IDs
}

extend Interner {
    /// Creates an empty interner
    pub func new() -> Interner

    /// Interns a string, returning its unique ID
    pub func intern(mut this, s: Str) -> I64

    /// Looks up a string by its ID
    pub func lookup(ref this, id: I64) -> Maybe[Str]

    /// Checks if an ID exists
    pub func has_id(ref this, id: I64) -> Bool

    /// Returns the number of interned strings
    pub func len(ref this) -> I64

    /// Clears all interned strings
    pub func clear(mut this)
}
```

---

## Global Interning

Use module-level functions to work with the global interner:

```tml
/// Interns a string globally
pub func intern(s: Str) -> I64

/// Looks up a string in the global interner
pub func lookup(id: I64) -> Maybe[Str]
```

### Usage

```tml
use std::intern::{intern, lookup}

func main() {
    // Intern strings
    let id1 = intern("hello")
    let id2 = intern("world")
    let id3 = intern("hello")  // same as id1

    // id1 == id3 (same string content)
    when lookup(id1) {
        Just(s) => println(s),  // prints "hello"
        Nothing => println("not found"),
    }
}
```

---

## Use Cases

### Efficient Symbol Storage

When the same identifier appears many times, intern once and use the ID:

```tml
use std::intern::{intern, lookup}

type Event {
    event_type: I64,  // interned string ID
    timestamp: I64,
    data: Str,
}

func main() {
    let click_id = intern("click")
    let hover_id = intern("hover")
    let scroll_id = intern("scroll")

    var events = List[Event]::new()
    events.push(Event {
        event_type: click_id,
        timestamp: 0,
        data: "button clicked",
    })

    events.push(Event {
        event_type: click_id,  // reuse interned ID
        timestamp: 100,
        data: "another click",
    })

    // Only 3 strings stored ("click", "hover", "scroll")
    // instead of storing "click" twice
}
```

### Symbol Comparison

Interned IDs allow identity-based comparison instead of string equality:

```tml
use std::intern::{intern, lookup}

func event_matches(event_id: I64, expected_type: Str) -> Bool {
    let expected_id = intern(expected_type)
    return event_id == expected_id  // fast I64 comparison
}

func main() {
    let click_id = intern("click")
    println(event_matches(click_id, "click"))  // => true
    println(event_matches(click_id, "hover"))  // => false
}
```

### Parsing and ASTs

Intern identifiers and keywords when building ASTs:

```tml
use std::intern::{intern, lookup}

type AstNode {
    node_type: I64,  // interned "FunctionDef", "VarDecl", etc.
    name: I64,       // interned identifier
    children: List[AstNode],
}

func main() {
    let func_def = intern("FunctionDef")
    let var_decl = intern("VarDecl")
    let add_func = intern("add")

    var root = AstNode {
        node_type: func_def,
        name: add_func,
        children: List[AstNode]::new(),
    }

    // Each node uses interned IDs, reducing memory
}
```

---

## Memory Efficiency

Interning is most beneficial when:
- The same strings appear many times (>10 occurrences)
- Strings are large (>20 bytes)
- Memory usage is a concern

Example: Storing 1,000 event types with 10 variants each:

| Approach | Memory Used |
|----------|------------|
| Store each string separately | ~150 KB (10 * 15 bytes avg * 1,000 copies) |
| Intern strings | ~150 bytes (10 * 15 bytes) + 8KB (1,000 IDs) |
| **Savings** | **140 KB (93%)** |

---

## Implementation Notes

- **Thread-safe**: The global interner uses a Mutex internally
- **Deterministic IDs**: The same string always gets the same ID in a session
- **IDs not stable across sessions**: Restart the program, get new IDs
- **No eviction**: Once interned, a string stays until the interner is cleared

---

## See Also

- [std::collections](./10-COLLECTIONS.md) — HashMap (used internally)
- [std::sync](./13-SYNC.md) — Mutex (thread safety)
- Core String API — Use `Str` type with `contains()`, `find()`, etc.
