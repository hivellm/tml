# TML v1.0 — Complete Examples

## 1. Hello World

```tml
module hello

pub func main() {
    println("Hello, TML!")
}
```

## 1.1 String Interpolation

```tml
module greeting

pub func main() {
    let name: Str = "World"
    let count: I32 = 42

    // Variable interpolation
    println("Hello {name}!")           // "Hello World!"

    // Expression interpolation
    println("Count: {count * 2}")      // "Count: 84"

    // Method calls
    let items: List[I32] = [1, 2, 3]
    println("Items: {items.len()}")    // "Items: 3"

    // Complex expressions
    let a: I32 = 10
    let b: I32 = 5
    println("{a} + {b} = {a + b}")     // "10 + 5 = 15"

    // Escaped braces
    println("Use \\{ and \\} for literals")
}
```

## 2. Fibonacci

```tml
module fibonacci

pub func fib(n: U64) -> U64 {
    if n <= 1 then return n
    return fib(n - 1) + fib(n - 2)
}

// Iterative version (more efficient)
pub func fib_iter(n: U64) -> U64 {
    if n <= 1 then return n

    var a: U64 = 0
    var b: U64 = 1

    loop _ in 2 through n {
        let temp: U64 = a + b
        a = b
        b = temp
    }

    return b
}

@test
func test_fib() {
    assert_eq(fib(0), 0)
    assert_eq(fib(1), 1)
    assert_eq(fib(10), 55)
    assert_eq(fib_iter(10), 55)
}
```

## 3. Linked List

```tml
module linked_list

pub type Node[T] {
    value: T,
    next: Maybe[Heap[Node[T]]],
}

pub type LinkedList[T] {
    head: Maybe[Heap[Node[T]]],
    len: U64,
}

extend LinkedList[T] {
    pub func new() -> This {
        return This { head: Nothing, len: 0 }
    }

    pub func push_front(this, value: T) {
        let new_node: Heap[Node[T]] = Heap.new(Node {
            value: value,
            next: this.head.take(),
        })
        this.head = Just(new_node)
        this.len += 1
    }

    pub func pop_front(this) -> Maybe[T] {
        when this.head.take() {
            Just(node) -> {
                this.head = node.next
                this.len -= 1
                return Just(node.value)
            },
            Nothing -> return Nothing,
        }
    }

    pub func len(this) -> U64 {
        return this.len
    }

    pub func is_empty(this) -> Bool {
        return this.len == 0
    }
}

@test
func test_linked_list() {
    var list: LinkedList[I32].new()

    list.push_front(1)
    list.push_front(2)
    list.push_front(3)

    assert_eq(list.len(), 3)
    assert_eq(list.pop_front(), Just(3))
    assert_eq(list.pop_front(), Just(2))
    assert_eq(list.pop_front(), Just(1))
    assert_eq(list.pop_front(), Nothing)
}
```

## 4. Binary Search Tree

```tml
module bst

pub type Tree[T: Ordered] {
    root: Maybe[Heap[TreeNode[T]]],
}

type TreeNode[T] {
    value: T,
    left: Maybe[Heap[TreeNode[T]]],
    right: Maybe[Heap[TreeNode[T]]],
}

extend Tree[T: Ordered] {
    pub func new() -> This {
        return This { root: Nothing }
    }

    pub func insert(this, value: T) {
        this.root = This.insert_node(this.root.take(), value)
    }

    func insert_node(node: Maybe[Heap[TreeNode[T]]], value: T) -> Maybe[Heap[TreeNode[T]]] {
        when node {
            Nothing -> return Just(Heap.new(TreeNode {
                value: value,
                left: Nothing,
                right: Nothing,
            })),
            Just(n) -> {
                when value.cmp(n.value) {
                    Less -> n.left = This.insert_node(n.left.take(), value),
                    Greater -> n.right = This.insert_node(n.right.take(), value),
                    Equal -> unit,  // duplicate ignored
                }
                return Just(n)
            },
        }
    }

    pub func contains(this, value: ref T) -> Bool {
        return This.search_node(ref this.root, value)
    }

    func search_node(node: ref Maybe[Heap[TreeNode[T]]], value: ref T) -> Bool {
        when node {
            Nothing -> return false,
            Just(n) -> {
                when value.cmp(ref n.value) {
                    Less -> return This.search_node(ref n.left, value),
                    Greater -> return This.search_node(ref n.right, value),
                    Equal -> return true,
                }
            },
        }
    }
}

@test
func test_bst() {
    var tree: Tree[I32].new()

    tree.insert(5)
    tree.insert(3)
    tree.insert(7)
    tree.insert(1)

    assert(tree.contains(ref 5))
    assert(tree.contains(ref 3))
    assert(not tree.contains(ref 10))
}
```

## 5. HTTP Client

```tml
module http_client

use std::http::{Request, Response, HttpClient}
use std::json::Json

pub type ApiClient {
    base_url: Str,
    client: HttpClient,
}

pub type User {
    id: U64,
    name: Str,
    email: Str,
}

extend ApiClient {
    pub func new(base_url: Str) -> This {
        return This {
            base_url: base_url,
            client: HttpClient::new(),
        }
    }

    pub func get_user(this, id: U64) -> Outcome[User, Error] {
        let url: Str = this.base_url + "/users/" + id.to_string()
        let response: Response = this.client.get(url)!

        if response.status != 200 {
            return Err(Error.new("HTTP " + response.status.to_string()))
        }

        let json: Json = Json.parse(response.body)!
        let user: User = User.from_json(json)!

        return Ok(user)
    }

    pub func create_user(this, name: Str, email: Str) -> Outcome[User, Error] {
        let url: Str = this.base_url + "/users"
        let body: Str = Json.object()
            .set("name", name)
            .set("email", email)
            .to_string()

        let response: Response = this.client.post(url, body)!

        if response.status != 201 {
            return Err(Error.new("Failed to create user"))
        }

        let json: Json = Json.parse(response.body)!
        return User.from_json(json)
    }
}

extend User {
    func from_json(json: Json) -> Outcome[This, Error] {
        return Ok(This {
            id: json.get("id").as_u64()!,
            name: json.get("name").as_str()!,
            email: json.get("email").as_str()!,
        })
    }
}
```

## 6. Concurrent Counter

```tml
module counter

use std::sync::{Sync, Mutex}
use std::thread

pub type Counter {
    value: Sync[Mutex[I64]],
}

extend Counter {
    pub func new() -> This {
        return This {
            value: Sync.new(Mutex.new(0)),
        }
    }

    pub func increment(this) {
        var guard = this.value.lock()
        guard.set(guard.get() + 1)
    }

    pub func get(this) -> I64 {
        let guard = this.value.lock()
        return guard.get()
    }
}

pub func parallel_count(n: I32) -> I64 {
    let counter: Counter = Counter.new()
    var handles: List[thread::JoinHandle] = List.new()

    loop _ in 0 to n {
        let c: Counter = counter.duplicate()
        let handle: thread::JoinHandle = thread::spawn(do() {
            loop _ in 0 to 1000 {
                c.increment()
            }
        })
        handles.push(handle)
    }

    loop handle in handles {
        handle.join()
    }

    return counter.get()
}

@test
func test_parallel_count() {
    let result: I64 = parallel_count(4)
    assert_eq(result, 4000)
}
```

## 7. JSON Parser

```tml
module json_parser

pub type JsonValue =
    | Null
    | Bool(Bool)
    | Number(F64)
    | Text(Str)
    | Array(List[JsonValue])
    | Object(HashMap[Str, JsonValue])

pub type ParseError {
    message: Str,
    position: U64,
}

type Parser {
    input: Str,
    pos: U64,
}

extend Parser {
    func new(input: Str) -> This {
        return This { input: input, pos: 0 }
    }

    func parse(this) -> Outcome[JsonValue, ParseError] {
        this.skip_whitespace()
        let value: JsonValue = this.parse_value()!
        this.skip_whitespace()

        if this.pos < this.input.len() {
            return Err(ParseError {
                message: "unexpected characters after value",
                position: this.pos,
            })
        }

        return Ok(value)
    }

    func parse_value(this) -> Outcome[JsonValue, ParseError] {
        this.skip_whitespace()

        when this.peek() {
            Just('n') -> return this.parse_null(),
            Just('t') -> return this.parse_true(),
            Just('f') -> return this.parse_false(),
            Just('"') -> return this.parse_string(),
            Just('[') -> return this.parse_array(),
            Just('{') -> return this.parse_object(),
            Just(c) -> {
                if c == '-' or c.is_digit() {
                    return this.parse_number()
                }
                return Err(this.error("unexpected character"))
            },
            Nothing -> return Err(this.error("unexpected end of input")),
        }
    }

    func parse_null(this) -> Outcome[JsonValue, ParseError] {
        this.expect("null")!
        return Ok(Null)
    }

    func parse_true(this) -> Outcome[JsonValue, ParseError] {
        this.expect("true")!
        return Ok(Bool(true))
    }

    func parse_false(this) -> Outcome[JsonValue, ParseError] {
        this.expect("false")!
        return Ok(Bool(false))
    }

    func parse_string(this) -> Outcome[JsonValue, ParseError] {
        this.expect("\"")!
        var result: Str = ""

        loop {
            when this.next() {
                Just('"') -> return Ok(Text(result)),
                Just('\\') -> {
                    let escaped: Char = this.parse_escape()!
                    result.push(escaped)
                },
                Just(c) -> result.push(c),
                Nothing -> return Err(this.error("unterminated string")),
            }
        }
    }

    func parse_number(this) -> Outcome[JsonValue, ParseError] {
        let start: U64 = this.pos

        if this.peek() == Just('-') {
            this.pos += 1
        }

        loop while this.peek().map(do(c) c.is_digit()).unwrap_or(false) {
            this.pos += 1
        }

        if this.peek() == Just('.') {
            this.pos += 1
            loop while this.peek().map(do(c) c.is_digit()).unwrap_or(false) {
                this.pos += 1
            }
        }

        let num_str: Str = this.input.slice(start, this.pos)
        when num_str.parse[F64]() {
            Ok(n) -> return Ok(Number(n)),
            Err(_) -> return Err(this.error("invalid number")),
        }
    }

    func parse_array(this) -> Outcome[JsonValue, ParseError] {
        this.expect("[")!
        this.skip_whitespace()

        var items: List[JsonValue] = List.new()

        if this.peek() == Just(']') {
            this.pos += 1
            return Ok(Array(items))
        }

        loop {
            let value: JsonValue = this.parse_value()!
            items.push(value)

            this.skip_whitespace()

            when this.peek() {
                Just(',') -> {
                    this.pos += 1
                    this.skip_whitespace()
                },
                Just(']') -> {
                    this.pos += 1
                    return Ok(Array(items))
                },
                _ -> return Err(this.error("expected ',' or ']'")),
            }
        }
    }

    func parse_object(this) -> Outcome[JsonValue, ParseError] {
        this.expect("{")!
        this.skip_whitespace()

        var entries: HashMap[Str, JsonValue] = HashMap.new(16)

        if this.peek() == Just('}') {
            this.pos += 1
            return Ok(Object(entries))
        }

        loop {
            let key: Str = when this.parse_string()! {
                Text(s) -> s,
                _ -> return Err(this.error("expected string key")),
            }

            this.skip_whitespace()
            this.expect(":")!
            this.skip_whitespace()

            let value: JsonValue = this.parse_value()!
            entries.insert(key, value)

            this.skip_whitespace()

            when this.peek() {
                Just(',') -> {
                    this.pos += 1
                    this.skip_whitespace()
                },
                Just('}') -> {
                    this.pos += 1
                    return Ok(Object(entries))
                },
                _ -> return Err(this.error("expected ',' or '}'")),
            }
        }
    }

    // Helper methods
    func peek(this) -> Maybe[Char] {
        return this.input.chars().nth(this.pos)
    }

    func next(this) -> Maybe[Char] {
        let c: Maybe[Char] = this.peek()
        if c.is_just() {
            this.pos += 1
        }
        return c
    }

    func skip_whitespace(this) {
        loop while this.peek().map(do(c) c.is_whitespace()).unwrap_or(false) {
            this.pos += 1
        }
    }

    func expect(this, s: Str) -> Outcome[Unit, ParseError] {
        loop c in s.chars() {
            if this.next() != Just(c) {
                return Err(this.error("expected '" + s + "'"))
            }
        }
        return Ok(unit)
    }

    func error(this, message: Str) -> ParseError {
        return ParseError { message: message, position: this.pos }
    }
}

pub func parse(input: Str) -> Outcome[JsonValue, ParseError] {
    var parser: Parser = Parser.new(input)
    return parser.parse()
}

@test
func test_parse_primitives() {
    assert_eq(parse("null"), Ok(Null))
    assert_eq(parse("true"), Ok(Bool(true)))
    assert_eq(parse("42"), Ok(Number(42.0)))
    assert_eq(parse("\"hello\""), Ok(Text("hello")))
}

@test
func test_parse_array() {
    let result: Outcome[JsonValue, ParseError] = parse("[1, 2, 3]")
    assert(result.is_ok())
}

@test
func test_parse_object() {
    let result: Outcome[JsonValue, ParseError] = parse("{\"name\": \"Alice\", \"age\": 30}")
    assert(result.is_ok())
}
```

## 8. Modern Language Features

This section demonstrates recently implemented TML features.

### 8.1 If-Let Pattern Matching

```tml
module pattern_matching

// If-let for Maybe unwrapping
func get_user_name(user_id: U64) -> Str {
    let maybe_user: Maybe[User] = find_user(user_id)

    if let Just(user) = maybe_user {
        return user.name
    } else {
        return "Unknown"
    }
}

// If-let for Outcome unwrapping
func load_config() -> Config {
    let result: Outcome[Config, Error] = Config.load()

    if let Ok(config) = result {
        return config
    } else {
        return Config.default()
    }
}

// Nested if-let
func process_nested(data: Maybe[Outcome[Str, Error]]) -> Str {
    if let Just(result) = data {
        if let Ok(value) = result {
            return value
        }
    }
    return "failed"
}
```

### 8.2 If-Then-Else Expressions

```tml
module conditionals

// Expression form with 'then' keyword
func abs(x: I32) -> I32 {
    return if x < 0 then -x else x
}

// Multi-line expression form
func sign(x: I32) -> Str {
    return if x < 0 then
        "negative"
    else if x > 0 then
        "positive"
    else
        "zero"
}

// Block form (both syntaxes work)
func max(a: I32, b: I32) -> I32 {
    // Block form
    let result1: I32 = if a > b {
        a
    } else {
        b
    }

    // Expression form
    let result2: I32 = if a > b then a else b

    return result1
}
```

### 8.3 Generic Constraints with Where Clauses

```tml
module generic_constraints

// Simple where clause
pub func find_max[T](items: List[T]) -> Maybe[T]
where T: Ordered
{
    if items.is_empty() {
        return Nothing
    }

    var max: T = items[0]
    loop item in items {
        if item > max {
            max = item
        }
    }
    return Just(max)
}

// Multiple trait bounds
pub func sort_and_display[T](items: mut ref List[T])
where T: Ordered + Display
{
    items.sort()
    loop item in items {
        println(item.to_string())
    }
}

// Multiple type parameters with constraints
pub func merge_sorted[T, U](left: List[T], right: List[U]) -> List[Str]
where T: Ordered + Display, U: Ordered + Display
{
    var result: List[Str] = List.new()
    // ... merge logic
    return result
}
```

### 8.4 Function Types

```tml
module function_types

// Function type aliases
pub type Predicate[T] = func(T) -> Bool
pub type Mapper[T, U] = func(T) -> U
pub type Comparator[T] = func(T, T) -> I32

// Using function types in structs
pub type EventHandler {
    on_click: func(I32, I32) -> (),
    on_hover: func(I32, I32) -> (),
    on_exit: func() -> (),
}

// Functions accepting function types
pub func filter[T](items: List[T], pred: Predicate[T]) -> List[T] {
    var result: List[T] = List.new()
    loop item in items {
        if pred(item) {
            result.push(item)
        }
    }
    return result
}

pub func map[T, U](items: List[T], mapper: Mapper[T, U]) -> List[U] {
    var result: List[U] = List.new()
    loop item in items {
        result.push(mapper(item))
    }
    return result
}
```

### 8.5 Closures (Do Expressions)

```tml
module closures

// Simple closure
func apply_twice(x: I32) -> I32 {
    let double: func(I32) -> I32 = do(n: I32) -> I32 n * 2
    return double(double(x))
}

// Closure with block body
func process_list(items: List[I32]) -> List[I32] {
    let transform: func(I32) -> I32 = do(x: I32) -> I32 {
        let doubled: I32 = x * 2
        let incremented: I32 = doubled + 1
        return incremented
    }

    var result: List[I32] = List.new()
    loop item in items {
        result.push(transform(item))
    }
    return result
}

// Inline closures in method calls
func filter_and_map(numbers: List[I32]) -> List[I32] {
    var filtered: List[I32] = List.new()

    // Filter with inline closure
    loop n in numbers {
        let is_positive: func(I32) -> Bool = do(x: I32) -> Bool x > 0
        if is_positive(n) {
            filtered.push(n)
        }
    }

    // Map with inline closure
    var mapped: List[I32] = List.new()
    loop n in filtered {
        let double: func(I32) -> I32 = do(x: I32) -> I32 x * 2
        mapped.push(double(n))
    }

    return mapped
}

// Closure type inference (when available)
func simple_ops() -> I32 {
    let add: func(I32, I32) -> I32 = do(a: I32, b: I32) -> I32 a + b
    let sub: func(I32, I32) -> I32 = do(a: I32, b: I32) -> I32 a - b
    let mul: func(I32, I32) -> I32 = do(a: I32, b: I32) -> I32 a * b

    let x: I32 = add(5, 3)
    let y: I32 = sub(x, 2)
    return mul(y, 2)
}
```

## 9. Low-Level Memory Operations

This section demonstrates TML's lowlevel blocks and pointer operations.

### 9.1 Basic Pointer Operations

```tml
module pointer_basics

// Swap two values using pointers
func swap_values() -> (I32, I32) {
    var a: I32 = 10
    var b: I32 = 20

    lowlevel {
        let pa: *I32 = &a
        let pb: *I32 = &b

        // Read both values
        let temp: I32 = pa.read()

        // Swap using pointer writes
        pa.write(pb.read())
        pb.write(temp)
    }

    return (a, b)  // Returns (20, 10)
}

@test
func test_swap() {
    let (a, b) = swap_values()
    assert_eq(a, 20)
    assert_eq(b, 10)
}
```

### 9.2 Pointer Arithmetic

```tml
module pointer_arithmetic

// Sum an array using pointer arithmetic
func sum_array() -> I32 {
    var arr: [I32; 5] = [1, 2, 3, 4, 5]
    var total: I32 = 0

    lowlevel {
        let base: *I32 = &arr[0]
        var i: I64 = 0

        loop {
            if i >= 5 then break

            let current: *I32 = base.offset(i)
            total = total + current.read()
            i = i + 1
        }
    }

    return total  // 15
}

@test
func test_sum() {
    assert_eq(sum_array(), 15)
}
```

### 9.3 Memory Manipulation

```tml
module memory_ops

// Zero out a buffer using pointers
func zero_buffer(size: I32) {
    var buffer: [U8; 256] = [0xFF; 256]

    lowlevel {
        let ptr: *U8 = &buffer[0]
        var i: I64 = 0

        loop {
            if i >= size as I64 then break

            let current: *U8 = ptr.offset(i)
            current.write(0)
            i = i + 1
        }
    }

    // First 'size' bytes are now zero
}

// Check if pointer is valid before use
func safe_read(ptr: *I32) -> Maybe[I32] {
    lowlevel {
        if ptr.is_null() {
            return Nothing
        }
        return Just(ptr.read())
    }
}
```

### 9.4 FFI Integration Pattern

```tml
module ffi_example

// Example: Calling a C function that takes a buffer
// (Conceptual - actual FFI syntax may vary)

type CBuffer {
    data: *U8,
    len: I64,
}

func process_with_c_lib(input: ref [U8; 1024]) -> Outcome[I32, Error] {
    lowlevel {
        let ptr: *U8 = &input[0]

        // Create C-compatible buffer struct
        let c_buf: CBuffer = CBuffer {
            data: ptr,
            len: 1024,
        }

        // Call external C function (hypothetical)
        // let result: I32 = c_process_buffer(&c_buf)
        // return Ok(result)

        return Ok(0)
    }
}
```

## 10. CLI Application

```tml
module cli

use std::os::env
use std::file::File

type Args {
    input: Str,
    output: Maybe[Str],
    verbose: Bool,
}

pub func main() -> Outcome[Unit, Error] {
    let args: Args = parse_args()!

    if args.verbose {
        println("Reading from: " + args.input)
    }

    let content: Str = File.read_to_string(args.input)!
    let processed: Str = process(content)

    when args.output {
        Just(path) -> {
            File.write(path, processed)!
            if args.verbose {
                println("Wrote to: " + path)
            }
        },
        Nothing -> println(processed),
    }

    return Ok(unit)
}

func parse_args() -> Outcome[Args, Error] {
    let argv: List[Str] = env::args()

    if argv.len() < 2 {
        return Err(Error.new("Usage: program <input> [-o output] [-v]"))
    }

    var input: Maybe[Str] = Nothing
    var output: Maybe[Str] = Nothing
    var verbose: Bool = false

    var i: U64 = 1
    loop while i < argv.len() {
        let arg: Str = argv[i]

        when arg {
            "-o" -> {
                i += 1
                if i >= argv.len() {
                    return Err(Error.new("-o requires argument"))
                }
                output = Just(argv[i])
            },
            "-v" -> verbose = true,
            _ -> {
                if input.is_nothing() {
                    input = Just(arg)
                } else {
                    return Err(Error.new("unexpected argument: " + arg))
                }
            },
        }
        i += 1
    }

    when input {
        Just(i) -> return Ok(Args { input: i, output: output, verbose: verbose }),
        Nothing -> return Err(Error.new("input file required")),
    }
}

func process(content: Str) -> Str {
    return content.to_uppercase()
}
```

## 11. Conditional Compilation

This section demonstrates TML's preprocessor directives for platform-specific code.

### 11.1 Platform-Specific Implementation

```tml
module platform

// Platform detection
#if WINDOWS
func get_path_separator() -> Str {
    return "\\"
}

func get_home_dir() -> Str {
    return env::var("USERPROFILE").unwrap_or("C:\\Users\\Default")
}
#elif UNIX
func get_path_separator() -> Str {
    return "/"
}

func get_home_dir() -> Str {
    return env::var("HOME").unwrap_or("/home")
}
#endif

// Architecture-specific optimizations
#if X86_64
func fast_hash(data: ref [U8]) -> U64 {
    // Use x86-64 specific optimizations
    lowlevel {
        // SIMD-optimized implementation
        return hash_simd_x64(data)
    }
}
#elif ARM64
func fast_hash(data: ref [U8]) -> U64 {
    // Use ARM64 NEON instructions
    lowlevel {
        return hash_neon(data)
    }
}
#else
func fast_hash(data: ref [U8]) -> U64 {
    // Generic fallback implementation
    var hash: U64 = 0
    loop byte in data {
        hash = hash * 31 + byte as U64
    }
    return hash
}
#endif

pub func main() {
    print("Path separator: {get_path_separator()}\n")
    print("Home directory: {get_home_dir()}\n")
}
```

### 11.2 Debug/Release Builds

```tml
module logging

// Debug-only logging
#ifdef DEBUG
func log_debug(msg: Str) {
    print("[DEBUG] {msg}\n")
}

func assert_debug(condition: Bool, msg: Str) {
    if not condition {
        panic("Assertion failed: {msg}")
    }
}
#else
// Empty implementations in release mode
func log_debug(msg: Str) { }
func assert_debug(condition: Bool, msg: Str) { }
#endif

// Release-only optimizations
#ifdef RELEASE
const BUFFER_SIZE: I32 = 65536  // Larger buffers in release
#else
const BUFFER_SIZE: I32 = 4096   // Smaller buffers for debugging
#endif

// Conditional feature inclusion
#if DEBUG && !RELEASE
func dump_state(state: ref AppState) {
    print("=== State Dump ===\n")
    print("Users: {state.users.len()}\n")
    print("Sessions: {state.sessions.len()}\n")
    print("Memory: {state.memory_usage()} bytes\n")
}
#endif

pub func main() {
    log_debug("Application starting...")

    #ifdef DEBUG
    print("Running in DEBUG mode\n")
    #else
    print("Running in RELEASE mode\n")
    #endif
}
```

### 11.3 Feature Flags

```tml
module features

// Custom feature flags (passed via -D)
#ifdef FEATURE_ASYNC
use std::thread::{spawn, JoinHandle}

pub func process_async(items: List[Item]) -> List[Outcome[Unit, Error]] {
    var handles: List[JoinHandle] = List.new()
    loop item in items {
        handles.push(spawn(do() process_item(item)))
    }

    var results: List[Outcome[Unit, Error]] = List.new()
    loop h in handles {
        results.push(h.join())
    }
    return results
}
#else
pub func process_async(items: List[Item]) -> List[Outcome[Unit, Error]] {
    // Synchronous fallback
    var results: List[Outcome[Unit, Error]] = List.new()
    loop item in items {
        results.push(Ok(process_item(item)))
    }
    return results
}
#endif

#ifdef FEATURE_LOGGING
use std::log

pub func init_logging() {
    log::info("app", "Logging initialized")
}
#else
pub func init_logging() { }
#endif

// Compile-time error for required features
#if !defined(DATABASE_URL)
#error "DATABASE_URL must be defined. Use -DDATABASE_URL=... when building."
#endif

// Warnings for deprecated features
#ifdef USE_LEGACY_API
#warning "USE_LEGACY_API is deprecated. Migrate to the new API."
#endif
```

### 11.4 Cross-Platform Library

```tml
module network

// Platform-specific socket implementation
#if WINDOWS
type Socket {
    handle: SOCKET,  // Windows SOCKET type
}

extend Socket {
    pub func new() -> Outcome[This, Error] {
        lowlevel {
            let h: SOCKET = socket(AF_INET, SOCK_STREAM, 0)
            if h == INVALID_SOCKET {
                return Err(Error.new("Failed to create socket"))
            }
            return Ok(This { handle: h })
        }
    }

    pub func close(this) {
        lowlevel {
            closesocket(this.handle)
        }
    }
}
#elif UNIX
type Socket {
    fd: I32,  // File descriptor on Unix
}

extend Socket {
    pub func new() -> Outcome[This, Error] {
        lowlevel {
            let fd: I32 = socket(AF_INET, SOCK_STREAM, 0)
            if fd < 0 {
                return Err(Error.new("Failed to create socket"))
            }
            return Ok(This { fd: fd })
        }
    }

    pub func close(this) {
        lowlevel {
            close(this.fd)
        }
    }
}
#endif

// Portable public API
pub func connect(host: Str, port: U16) -> Outcome[Socket, Error] {
    let sock: Socket = Socket::new()!
    // ... connection logic (platform-independent)
    return Ok(sock)
}
```

### 11.5 Nested Conditionals

```tml
module nested

// Nested platform and architecture detection
#if WINDOWS
    #ifdef X86_64
    func get_arch_info() -> Str {
        return "Windows x64"
    }
    #elif X86
    func get_arch_info() -> Str {
        return "Windows x86"
    }
    #elif ARM64
    func get_arch_info() -> Str {
        return "Windows ARM64"
    }
    #endif
#elif MACOS
    #ifdef ARM64
    func get_arch_info() -> Str {
        return "macOS Apple Silicon"
    }
    #else
    func get_arch_info() -> Str {
        return "macOS Intel"
    }
    #endif
#elif LINUX
    #ifdef ARM64
    func get_arch_info() -> Str {
        return "Linux ARM64"
    }
    #else
    func get_arch_info() -> Str {
        return "Linux x64"
    }
    #endif
#endif

// Complex conditional expressions
#if (WINDOWS || LINUX) && X86_64 && !defined(NO_SIMD)
func use_simd_optimizations() -> Bool {
    return true
}
#else
func use_simd_optimizations() -> Bool {
    return false
}
#endif

pub func main() {
    print("Platform: {get_arch_info()}\n")
    print("SIMD enabled: {use_simd_optimizations()}\n")
}
```

**Building with different configurations:**
```bash
# Debug build on Windows
tml build platform.tml --debug
# Defines: WINDOWS, X86_64, DEBUG, PTR_64, LITTLE_ENDIAN, MSVC

# Release build with features
tml build features.tml --release -DFEATURE_ASYNC -DFEATURE_LOGGING -DDATABASE_URL=postgres://localhost

# Cross-compile for Linux ARM64
tml build platform.tml --target=aarch64-unknown-linux-gnu
# Defines: LINUX, ARM64, UNIX, POSIX, PTR_64, LITTLE_ENDIAN, GNU
```

## 12. Structured Logging

This section demonstrates TML's built-in structured logging via `std::log`.

### 12.1 Basic Logging

```tml
use std::log
use std::log::{TRACE, DEBUG, INFO, WARN, ERROR, FATAL}

func main() -> I32 {
    // Log at various levels
    log::info("app", "Application starting")
    log::debug("config", "Loaded 42 settings")
    log::warn("pool", "Connection pool at 90% capacity")
    log::error("db", "Query timeout after 30s")

    // Check before expensive formatting
    if log::enabled(TRACE) {
        log::trace("app", "Detailed request payload: ...")
    }

    return 0
}
```

### 12.2 Runtime Level Control

```tml
use std::log
use std::log::{TRACE, DEBUG, INFO, WARN}

func main() -> I32 {
    // Default level is WARN — only warnings and above are shown
    log::info("app", "This is hidden by default")
    log::warn("app", "This is visible by default")

    // Lower the level to see more messages
    log::set_level(DEBUG)
    log::debug("app", "Now debug messages are visible")
    log::info("app", "Info messages too")

    // Query the current level
    let level: I32 = log::get_level()

    // Raise the level to suppress noise
    log::set_level(WARN)
    log::debug("app", "This is hidden again")

    return 0
}
```

### 12.3 Module Tags for Filtering

```tml
use std::log

// Use short, descriptive module tags
func handle_request() {
    log::info("http", "GET /api/users")
    log::debug("auth", "Validating token")
    log::debug("cache", "Cache miss for key: users")
    log::info("db", "SELECT * FROM users (12ms)")
    log::info("http", "200 OK (45ms)")
}

// Run with filtering:
//   tml run app.tml --log-filter=db=trace,http=info,*=warn
// This shows all DB queries and HTTP requests but hides auth/cache noise.
```

---

*Previous: [13-BUILTINS.md](./13-BUILTINS.md)*
*Next: [15-ERROR-HANDLING.md](./15-ERROR-HANDLING.md) — Error Handling*
