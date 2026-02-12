# TML v1.0 — Semantics

## 1. Ownership

### 1.1 Fundamental Rules

1. Every value has exactly one owner
2. When owner goes out of scope, value is destroyed
3. Values can be moved or borrowed

### 1.2 Move Semantics

```tml
let s1: String = String.from("hello")
let s2 = s1        // s1 moved to s2

print(s1)          // ERROR: s1 is no longer valid
print(s2)          // OK
```

### 1.3 Borrowing

```tml
let s: String = String.from("hello")

// Immutable borrow
let len: U64 = calculate_length(ref s)
print(s)  // OK: s is still valid

func calculate_length(s: ref String) -> U64 {
    return s.len()
}
```

```tml
var s: String = String.from("hello")

// Mutable borrow
append_world(mut ref s)
print(s)  // "hello world"

func append_world(s: mut ref String) {
    s.push(" world")
}
```

### 1.4 Borrowing Rules

**OWN-1: Multiple immutable borrows OK**
```tml
let data: List[I32] = List.of(1, 2, 3)
let r1: ref List[I32] = ref data
let r2: ref List[I32] = ref data
print(r1.len())  // OK
print(r2.len())  // OK
```

**OWN-2: One exclusive mutable borrow**
```tml
var data: List[I32] = List.of(1, 2, 3)
let r: mut ref List[I32] = mut ref data
// ERROR: cannot have another borrow while r exists
let r2: ref List[I32] = ref data
```

**OWN-3: Don't mix ref and mut ref**
```tml
var data: List[I32] = List.of(1, 2, 3)
let r1: ref List[I32] = ref data
let r2: mut ref List[I32] = mut ref data  // ERROR: already has immutable borrow
```

### 1.5 Copy Types

Small types are copied, not moved:

```tml
let x: I32 = 42
let y: I32 = x  // copy, not move
print(x)       // OK: x still valid

// Copy types:
// - All numeric primitives
// - Bool, Char
// - Tuples of Copy types
// - Arrays of Copy types
```

## 2. Lifetimes

### 2.1 Automatic Inference

TML infers lifetimes automatically in most cases:

```tml
// Lifetimes inferred
func first(items: ref List[I32]) -> ref I32 {
    return ref items[0]
}

// The compiler understands that the return lives as long as items
```

### 2.2 Elision Rules

1. Each ref T parameter gets a separate lifetime
2. If there's a this/mut ref this, return has same lifetime
3. If there's exactly one ref parameter, return uses it

```tml
// Rule 3: return lives as long as s
func get_first(s: ref String) -> ref Char {
    return ref s.chars()[0]
}
```

### 2.3 Complex Cases

When inference doesn't work, use the compiler to guide:

```tml
// ERROR: compiler doesn't know which lifetime to use
func longest(a: ref String, b: ref String) -> ref String {
    if a.len() > b.len() then a else b
}

// The compiler suggests: both must have same lifetime
// Solution: return owned value
func longest(a: ref String, b: ref String) -> String {
    if a.len() > b.len() then a.duplicate() else b.duplicate()
}
```

## 3. Pattern Matching

### 3.1 Exhaustiveness

All patterns must be covered:

```tml
type Status = Active | Inactive | Pending

func describe(s: Status) -> String {
    when s {
        Active -> "running",
        Inactive -> "stopped",
        // ERROR: Pending not covered
    }
}
```

### 3.2 Bindings

```tml
when value {
    Just(x) -> use(x),      // x is binding
    Nothing -> default(),
}

when point {
    Point { x: 0, y } -> "on Y axis at " + y.to_string(),
    Point { x, y: 0 } -> "on X axis at " + x.to_string(),
    Point { x, y } -> "at " + x.to_string() + ", " + y.to_string(),
}
```

### 3.3 No Guards

TML doesn't have guards in when. Use inline if:

```tml
// Doesn't exist in TML:
// when x {
//     n if n > 0 => ...
// }

// Use:
when x {
    n -> if n > 0 then positive(n) else other(n),
}
```

## 4. Error Handling

### 4.1 Outcome[T, E]

```tml
type Outcome[T, E] = Ok(T) | Err(E)

func parse_int(s: String) -> Outcome[I32, ParseError] {
    // ...
}
```

### 4.2 The `!` Operator

Every fallible call ends with `!` - highly visible error points:

```tml
func process() -> Outcome[Data, Error] {
    let file: File = File.open("data.txt")!       // propagate on error, unwraps to File
    let content: String = file.read()!            // propagate on error, unwraps to String
    let parsed: Data = parse(content)!            // propagate on error, unwraps to Data
    return Ok(parsed)
}

// In non-Outcome function, ! panics on error
func must_load() -> Config {
    let content: String = File.read("config.json")!  // panic if fails, unwraps to String
    return parse(content)!
}
```

### 4.3 Inline Recovery with `else`

```tml
// Simple default
let port: U16 = env.get("PORT")!.parse[U16]()! else 8080

// With error binding
let data: Data = fetch(url)! else |err| {
    log.warn("Fetch failed: " + err.to_string())
    load_cached()! else Data.default()
}

// Early return
let user: User = find_user(id)! else {
    return Err(Error.NotFound)
}
```

### 4.4 Block-Level Error Handling (catch)

```tml
func sync_data() -> Outcome[Unit, SyncError] {
    catch {
        let local: Data = load_local()!
        let remote: Data = fetch_remote()!
        save(merge(local, remote))!
        return Ok(())
    } else |err| {
        log.error("Sync failed: " + err.to_string())
        return Err(SyncError.from(err))
    }
}
```

### 4.5 Panic

For unrecoverable errors:

```tml
func assert(condition: Bool, msg: String) {
    if not condition then panic(msg)
}

func get_required(name: String) -> String {
    env.get(name)! else {
        panic("Required: " + name)
    }
}
```

**See also**: [15-ERROR-HANDLING.md](./15-ERROR-HANDLING.md) for complete error handling specification.

## 5. Evaluation

### 5.1 Strict Order

Arguments evaluated left to right:

```tml
foo(a(), b(), c())
// Order: a(), then b(), then c(), then foo
```

### 5.2 Short-Circuit

`and` and `or` short-circuit:

```tml
false and expensive()  // expensive() not called
true or expensive()    // expensive() not called
```

### 5.3 Expressions

Almost everything is an expression:

```tml
let x: I32 = if cond then 1 else 2

let y: I32 = when opt {
    Just(n) -> n,
    Nothing -> 0,
}

let z: I32 = {
    let temp: I32 = compute()
    temp * 2
}
```

---

*Previous: [04-TYPES.md](./04-TYPES.md)*
*Next: [06-MEMORY.md](./06-MEMORY.md) — Memory Management*
