# Standard Library

The TML standard library (`std`) provides essential types and functions for building TML programs. It includes common data structures, I/O operations, error handling, and utilities.

## Overview

The standard library is organized into modules:

```
std/
├── option.tml          - Maybe[T] type for optional values
├── result.tml          - Outcome[T, E] for error handling
├── string.tml          - String manipulation
├── prelude.tml         - Auto-imported common types
├── collections/        - Vec, List, Map, Set
├── io/                 - Input/output operations
├── fs/                 - File system operations
├── time/               - Time and duration types
├── fmt/                - Formatting utilities
├── error/              - Error handling utilities
├── sync/               - Synchronization primitives
├── net/                - Networking
├── process/            - Process management
├── path/               - Path manipulation
└── env/                - Environment variables
```

## Core Types

### Maybe[T] - Optional Values

The `Maybe[T]` type represents an optional value that might be present (`Just`) or absent (`Nothing`):

```tml
use std::option::{Maybe, Just, Nothing}

func find_user(id: I64) -> Maybe[User] {
    if user_exists(id) {
        Just(get_user(id))
    } else {
        Nothing
    }
}

// Using Maybe
let result: Maybe[User] = find_user(42)
when result {
    Just(user) => println("Found: {}", user.name),
    Nothing => println("User not found"),
}
```

**Common Methods:**
```tml
let maybe: Maybe[I32] = Just(42)

maybe.is_some()              // true
maybe.is_none()              // false
maybe.unwrap()               // 42 (panics if Nothing)
maybe.unwrap_or(0)           // 42
maybe.unwrap_or_else(|| 0)   // 42
maybe.map(|x| x * 2)         // Just(84)
maybe.and_then(|x| Just(x + 1))  // Just(43)
```

### Outcome[T, E] - Error Handling

The `Outcome[T, E]` type represents either a success (`Ok`) or an error (`Err`):

```tml
use std::result::{Outcome, Ok, Err}

func parse_number(s: Str) -> Outcome[I64, Str] {
    if is_valid_number(s) {
        Ok(convert_to_i64(s))
    } else {
        Err("Invalid number format")
    }
}

// Using Outcome
let result: Outcome[I64, Str] = parse_number("123")
when result {
    Ok(num) => println("Parsed: {}", num),
    Err(e) => println("Error: {}", e),
}
```

**Common Methods:**
```tml
let result: Outcome[I32, Str] = Ok(42)

result.is_ok()               // true
result.is_err()              // false
result.unwrap()              // 42 (panics if Err)
result.unwrap_or(0)          // 42
result.map(|x| x * 2)        // Ok(84)
result.map_err(|e| e + "!")  // Ok(42)
```

## Collections

### Vec[T] - Dynamic Array

```tml
use std::collections::Vec

let mut numbers: Vec[I64] = Vec::new()
numbers.push(1)
numbers.push(2)
numbers.push(3)

println("Length: {}", numbers.len())        // Length: 3
println("First: {}", numbers.get(0))        // First: Just(1)
println("Last: {}", numbers.pop())          // Last: Just(3)

// Iteration
for num in numbers {
    println("{}", num)
}
```

### List[T] - Linked List

```tml
use std::collections::List

let mut list: List[I32] = List::new()
list.push_front(1)
list.push_back(2)
list.push_back(3)

println("Length: {}", list.len())
```

### Map[K, V] - Hash Map

```tml
use std::collections::Map

let mut scores: Map[Str, I32] = Map::new()
scores.insert("Alice", 100)
scores.insert("Bob", 85)

when scores.get("Alice") {
    Just(score) => println("Alice: {}", score),
    Nothing => println("Not found"),
}
```

### Set[T] - Hash Set

```tml
use std::collections::Set

let mut set: Set[I32] = Set::new()
set.insert(1)
set.insert(2)
set.insert(1)  // Duplicate ignored

println("Size: {}", set.len())  // Size: 2
println("Contains 1: {}", set.contains(1))  // true
```

## Time and Duration

The `std::time` module provides time measurement utilities:

```tml
use std::time::{Instant, Duration}

// Measure execution time
let start: Instant = Instant::now()
expensive_computation()
let elapsed: Duration = start.elapsed()

println("Took: {:.3} ms", elapsed.as_millis_f64())

// Benchmark with multiple runs
let mut total: Duration = Duration::from_micros(0)
for _ in 0 to 10 {
    let start: Instant = Instant::now()
    work()
    total += start.elapsed()
}
let avg: F64 = total.as_millis_f64() / 10.0
println("Average: {:.3} ms", avg)
```

**Duration Methods:**
```tml
let d: Duration = Duration::from_millis(1500)

d.as_secs()          // 1
d.as_millis()        // 1500
d.as_micros()        // 1500000
d.as_secs_f64()      // 1.5
d.as_millis_f64()    // 1500.0
```

## I/O Operations

### Reading and Writing

```tml
use std::io::{stdin, stdout, Read, Write}

// Read from stdin
let mut input: Str = String::new()
stdin().read_line(&mut input)
println("You typed: {}", input)

// Write to stdout
stdout().write("Hello, World!\n")
```

### File Operations

```tml
use std::fs::{File, read_to_string, write}

// Read entire file
let content: Outcome[Str, Error] = read_to_string("data.txt")

// Write to file
write("output.txt", "Hello, file!")

// Manual file handling
let file: Outcome[File, Error> = File::open("data.txt")
when file {
    Ok(f) => {
        let content: Str = f.read_to_string()
        println("{}", content)
    },
    Err(e) => println("Error: {}", e),
}
```

## String Operations

```tml
use std::string::String

let mut s: String = String::from("Hello")
s.push_str(", World!")
s.push('!')

println("{}", s)  // "Hello, World!!"

// String methods
s.len()                      // Length in bytes
s.is_empty()                 // Check if empty
s.contains("World")          // true
s.starts_with("Hello")       // true
s.ends_with("!")             // true
s.to_lowercase()             // "hello, world!!"
s.to_uppercase()             // "HELLO, WORLD!!"
s.trim()                     // Remove whitespace
s.replace("World", "TML")    // "Hello, TML!!"
s.split(",")                 // Iterator of substrings
```

## Error Handling

```tml
use std::error::Error

pub type AppError {
    IoError(Str),
    ParseError(Str),
    NotFound,
}

impl AppError {
    pub func to_string(this) -> Str {
        when this {
            IoError(msg) => "I/O Error: " + msg,
            ParseError(msg) => "Parse Error: " + msg,
            NotFound => "Resource not found",
        }
    }
}

func risky_operation() -> Outcome[I64, AppError] {
    // ... operation that might fail ...
    if failure_condition {
        return Err(AppError::NotFound)
    }
    Ok(result)
}
```

## Synchronization

```tml
use std::sync::{Mutex, Arc}

// Shared mutable state across threads
let counter: Arc[Mutex[I32]] = Arc::new(Mutex::new(0))

// Clone for each thread
let c1 = counter.clone()
let c2 = counter.clone()

thread::spawn(|| {
    let mut num = c1.lock()
    *num += 1
})

thread::spawn(|| {
    let mut num = c2.lock()
    *num += 1
})
```

## Environment and Process

```tml
use std::env
use std::process

// Environment variables
let home: Maybe[Str] = env::var("HOME")
let args: Vec[Str] = env::args()

// Process execution
let status: Outcome[ExitStatus, Error> = process::Command::new("ls")
    .arg("-la")
    .status()
```

## Path Operations

```tml
use std::path::{Path, PathBuf}

let path: PathBuf = PathBuf::from("/home/user/file.txt")
let parent: Maybe[&Path] = path.parent()
let filename: Maybe[&str] = path.file_name()
let extension: Maybe[&str] = path.extension()

// Path manipulation
path.push("subdir")
path.pop()
path.set_extension("md")
```

## Formatting

```tml
use std::fmt

// Custom formatting for types
impl fmt::Display for User {
    pub func fmt(this, f: &mut fmt::Formatter) -> Outcome[Unit, fmt::Error] {
        write!(f, "User({})", this.name)
    }
}
```

## Prelude

The prelude module contains commonly used types that are automatically imported into every TML file:

```tml
// These are always available without explicit import:
Maybe, Just, Nothing
Outcome, Ok, Err
Vec
String
```

To use other types, you need explicit imports:

```tml
use std::collections::Map
use std::fs::File
use std::time::Instant
```

## Module Organization

Standard library modules follow a hierarchical structure:

```tml
// Import from nested modules
use std::collections::vec::Vec
use std::io::stdio::stdin
use std::sync::mutex::Mutex

// Re-exports for convenience
use std::collections::{Vec, Map, Set}
use std::io::{Read, Write}
```

## Best Practices

1. **Use `Maybe` instead of null**
   ```tml
   // ❌ Don't use sentinel values
   func find(x: I32) -> I32 { -1 }

   // ✅ Use Maybe
   func find(x: I32) -> Maybe[I32] { ... }
   ```

2. **Use `Outcome` for error handling**
   ```tml
   // ❌ Don't panic for expected errors
   func parse(s: Str) -> I32 {
       panic("invalid") // Too aggressive
   }

   // ✅ Return Outcome
   func parse(s: Str) -> Outcome[I32, ParseError] { ... }
   ```

3. **Prefer `Vec` for dynamic arrays**
   ```tml
   // ✅ Efficient and flexible
   let mut items: Vec[I32] = Vec::new()
   items.push(1)
   ```

4. **Use standard time API for benchmarking**
   ```tml
   // ✅ Clear and precise
   let start = Instant::now()
   work()
   println("Took: {:.3} ms", start.elapsed().as_millis_f64())
   ```

## See Also

- [Appendix C - Builtin Functions](appendix-03-builtins.md)
- [Chapter 11 - Testing](ch11-00-testing.md)
- [Collections Chapter](ch08-00-collections.md)
