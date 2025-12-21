# TML v1.0 — Semantics

## 1. Capabilities (Caps)

Capabilities declare what a module **can** do.

### 1.1 Syntax

```tml
module database {
    caps: [io.network, io.file]

    // code can use network and file I/O
}
```

### 1.2 Caps Hierarchy

```
io
├── io.file
│   ├── io.file.read
│   └── io.file.write
├── io.network
│   ├── io.network.tcp
│   ├── io.network.udp
│   └── io.network.http
├── io.process
│   ├── io.process.spawn
│   └── io.process.env
└── io.time

system
├── system.ffi
├── system.unsafe
└── system.alloc

crypto
├── crypto.random
├── crypto.hash
└── crypto.encrypt
```

### 1.3 Rules

**CAP-1: Usage requires cap**
```tml
module reader {
    caps: []  // no caps

    func read() -> String {
        // ERROR: io.file.read not declared
        return File.read("data.txt")
    }
}
```

**CAP-2: Caps are inherited**
```tml
module parent {
    caps: [io.file]
}

// submodule inherits caps from parent
module parent.child {
    // has io.file automatically
}
```

**CAP-3: More specific cap satisfies general**
```tml
module web {
    caps: [io.network.http]

    // OK: http is subcap of network
    func call_api() { ... }
}
```

**CAP-4: Functions cannot expand caps**
```tml
module safe {
    caps: [io.file.read]

    func process() {
        // ERROR: module only has read, not write
        File.write("out.txt", data)
    }
}
```

### 1.4 Caps Inference

By default, caps are inferred from code:

```tml
module auto {
    // caps inferred as [io.file.read, io.network.http]

    func load() -> Data {
        let local = File.read("cache.txt")
        let remote = Http.get("https://api.example.com/data")
        return merge(local, remote)
    }
}
```

To explicitly limit:

```tml
#[caps_boundary]
module restricted {
    caps: [io.file.read]  // explicit limit

    // ERROR if code tries to use more than this
}
```

## 2. Effects

Effects declare what a function **actually does**.

### 2.1 Syntax

```tml
func read_config() -> Config
effects: [io.file.read]
{
    return parse(File.read("config.tml"))
}

func save_data(data: Data)
effects: [io.file.write, io.network.http]
{
    File.write("backup.dat", data.serialize())
    Http.post("https://api.example.com/sync", data)
}
```

### 2.2 Effect Categories

| Category | Subcategories |
|----------|---------------|
| `pure` | No effect |
| `io.file.read` | File read |
| `io.file.write` | File write |
| `io.network.connect` | Network connection |
| `io.network.send` | Data send |
| `io.network.receive` | Data receive |
| `io.time.read` | Time read |
| `io.time.sleep` | Sleep/delay |
| `io.process.spawn` | Create process |
| `io.process.env.read` | Read env var |
| `io.process.env.write` | Write env var |
| `state.read` | Read mutable state |
| `state.write` | Write mutable state |
| `panic` | Can panic |
| `diverge` | May not return |

### 2.3 Rules

**EFF-1: Effects propagate**
```tml
func inner()
effects: [io.file.read]
{
    File.read("x.txt")
}

func outer()
// ERROR: effects not declared, but inner() has effects
{
    inner()
}

func outer_correct()
effects: [io.file.read]  // OK: propagates effects from inner
{
    inner()
}
```

**EFF-2: Function can declare more effects than used**
```tml
func flexible()
effects: [io.file.read, io.file.write]  // declares both
{
    // uses only read
    return File.read("data.txt")
}
```

**EFF-3: Pure is default**
```tml
func add(a: I32, b: I32) -> I32 {
    // no effects: = effects: [pure]
    return a + b
}
```

**EFF-4: Closure effects**
```tml
func process(items: List[I32], f: func(I32) -> I32)
effects: [state.read]  // effect from f
{
    loop item in items {
        f(item)
    }
}
```

### 2.4 Effects Inference

```tml
// Effects inferred automatically
func load_all() -> List[Data] {
    // inferred: [io.file.read, io.network.http]
    let files = File.list("data/")
    let remote = Http.get(API_URL)
    return merge(files, remote)
}
```

## 3. Contracts

Formal pre and post-conditions.

### 3.1 Syntax

```tml
func sqrt(x: F64) -> F64
pre: x >= 0.0
post(result): result >= 0.0 and result * result ~= x
{
    return x.sqrt_impl()
}

func divide(a: I32, b: I32) -> I32
pre: b != 0
{
    return a / b
}

func sort[T: Ord](items: List[T]) -> List[T]
post(result): result.is_sorted() and result.len() == items.len()
{
    // implementation
}
```

### 3.2 Rules

**CTR-1: Pre is checked on entry**
```tml
func example(x: I32)
pre: x > 0
{
    // x > 0 guaranteed here
}

example(-5)  // ERROR at runtime (or compile-time if detectable)
```

**CTR-2: Post is checked on exit**
```tml
func double(x: I32) -> I32
post(r): r == x * 2
{
    return x * 2  // OK
    // return x + 1  // ERROR: violates post
}
```

**CTR-3: Contracts are inherited**
```tml
trait Sortable {
    func sort(this) -> This
    post(r): r.is_sorted()
}

extend List[T: Ord] with Sortable {
    func sort(this) -> This {
        // must satisfy inherited post
    }
}
```

### 3.3 Verification Modes

```toml
# tml.toml
[contracts]
mode = "runtime"   # verify at runtime
# mode = "debug"   # only in debug builds
# mode = "static"  # static analysis (limited)
# mode = "off"     # disable (production)
```

## 4. Ownership

### 4.1 Fundamental Rules

1. Every value has exactly one owner
2. When owner goes out of scope, value is destroyed
3. Values can be moved or borrowed

### 4.2 Move Semantics

```tml
let s1 = String.from("hello")
let s2 = s1        // s1 moved to s2

print(s1)          // ERROR: s1 is no longer valid
print(s2)          // OK
```

### 4.3 Borrowing

```tml
let s = String.from("hello")

// Immutable borrow
let len = calculate_length(&s)
print(s)  // OK: s is still valid

func calculate_length(s: &String) -> U64 {
    return s.len()
}
```

```tml
var s = String.from("hello")

// Mutable borrow
append_world(&mut s)
print(s)  // "hello world"

func append_world(s: &mut String) {
    s.push(" world")
}
```

### 4.4 Borrowing Rules

**OWN-1: Multiple immutable borrows OK**
```tml
let data = vec![1, 2, 3]
let r1 = &data
let r2 = &data
print(r1.len())  // OK
print(r2.len())  // OK
```

**OWN-2: One exclusive mutable borrow**
```tml
var data = vec![1, 2, 3]
let r = &mut data
// ERROR: cannot have another borrow while r exists
let r2 = &data
```

**OWN-3: Don't mix & and &mut**
```tml
var data = vec![1, 2, 3]
let r1 = &data
let r2 = &mut data  // ERROR: already has immutable borrow
```

### 4.5 Copy Types

Small types are copied, not moved:

```tml
let x: I32 = 42
let y = x      // copy, not move
print(x)       // OK: x still valid

// Copy types:
// - All numeric primitives
// - Bool, Char
// - Tuples of Copy types
// - Arrays of Copy types
```

## 5. Lifetimes

### 5.1 Automatic Inference

TML infers lifetimes automatically in most cases:

```tml
// Lifetimes inferred
func first(items: &List[I32]) -> &I32 {
    return &items[0]
}

// The compiler understands that the return lives as long as items
```

### 5.2 Elision Rules

1. Each &T parameter gets a separate lifetime
2. If there's a &self/&mut self, return has same lifetime
3. If there's exactly one ref parameter, return uses it

```tml
// Rule 3: return lives as long as s
func get_first(s: &String) -> &Char {
    return &s.chars()[0]
}
```

### 5.3 Complex Cases

When inference doesn't work, use the compiler to guide:

```tml
// ERROR: compiler doesn't know which lifetime to use
func longest(a: &String, b: &String) -> &String {
    if a.len() > b.len() then a else b
}

// The compiler suggests: both must have same lifetime
// Solution: return owned value
func longest(a: &String, b: &String) -> String {
    if a.len() > b.len() then a.clone() else b.clone()
}
```

## 6. Pattern Matching

### 6.1 Exhaustiveness

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

### 6.2 Bindings

```tml
when value {
    Some(x) -> use(x),      // x is binding
    None -> default(),
}

when point {
    Point { x: 0, y } -> "on Y axis at " + y.to_string(),
    Point { x, y: 0 } -> "on X axis at " + x.to_string(),
    Point { x, y } -> "at " + x.to_string() + ", " + y.to_string(),
}
```

### 6.3 No Guards

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

## 7. Error Handling

### 7.1 Result[T, E]

```tml
type Result[T, E] = Ok(T) | Err(E)

func parse_int(s: String) -> Result[I32, ParseError] {
    // ...
}
```

### 7.2 The `!` Operator

Every fallible call ends with `!` - highly visible error points:

```tml
func process() -> Result[Data, Error] {
    let file = File.open("data.txt")!   // propagate on error
    let content = file.read()!           // propagate on error
    let parsed = parse(content)!         // propagate on error
    return Ok(parsed)
}

// In non-Result function, ! panics on error
func must_load() -> Config {
    let content = File.read("config.json")!  // panic if fails
    return parse(content)!
}
```

### 7.3 Inline Recovery with `else`

```tml
// Simple default
let port = env.get("PORT")!.parse[U16]()! else 8080

// With error binding
let data = fetch(url)! else |err| {
    log.warn("Fetch failed: " + err.to_string())
    load_cached()! else Data.default()
}

// Early return
let user = find_user(id)! else {
    return Err(Error.NotFound)
}
```

### 7.4 Block-Level Error Handling (catch)

```tml
func sync_data() -> Result[Unit, SyncError] {
    catch {
        let local = load_local()!
        let remote = fetch_remote()!
        save(merge(local, remote)!)!
        return Ok(())
    } else |err| {
        log.error("Sync failed: " + err.to_string())
        return Err(SyncError.from(err))
    }
}
```

### 7.5 Panic

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

## 8. Evaluation

### 8.1 Strict Order

Arguments evaluated left to right:

```tml
foo(a(), b(), c())
// Order: a(), then b(), then c(), then foo
```

### 8.2 Short-Circuit

`and` and `or` short-circuit:

```tml
false and expensive()  // expensive() not called
true or expensive()    // expensive() not called
```

### 8.3 Expressions

Almost everything is an expression:

```tml
let x = if cond then 1 else 2

let y = when opt {
    Some(n) -> n,
    None -> 0,
}

let z = {
    let temp = compute()
    temp * 2
}
```

---

*Previous: [04-TYPES.md](./04-TYPES.md)*
*Next: [06-MEMORY.md](./06-MEMORY.md) — Memory Management*
