# TML v1.0 — Complete Examples

## 1. Hello World

```tml
module hello

public func main() {
    println("Hello, TML!")
}
```

## 2. Fibonacci

```tml
module fibonacci

public func fib(n: U64) -> U64 {
    if n <= 1 then return n
    return fib(n - 1) + fib(n - 2)
}

// Iterative version (more efficient)
public func fib_iter(n: U64) -> U64 {
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

public type Node[T] {
    value: T,
    next: Maybe[Heap[Node[T]]],
}

public type LinkedList[T] {
    head: Maybe[Heap[Node[T]]],
    len: U64,
}

extend LinkedList[T] {
    public func new() -> This {
        return This { head: Nothing, len: 0 }
    }

    public func push_front(this, value: T) {
        let new_node: Heap[Node[T]] = Heap.new(Node {
            value: value,
            next: this.head.take(),
        })
        this.head = Just(new_node)
        this.len += 1
    }

    public func pop_front(this) -> Maybe[T] {
        when this.head.take() {
            Just(node) -> {
                this.head = node.next
                this.len -= 1
                return Just(node.value)
            },
            Nothing -> return Nothing,
        }
    }

    public func len(this) -> U64 {
        return this.len
    }

    public func is_empty(this) -> Bool {
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

public type Tree[T: Ordered] {
    root: Maybe[Heap[TreeNode[T]]],
}

type TreeNode[T] {
    value: T,
    left: Maybe[Heap[TreeNode[T]]],
    right: Maybe[Heap[TreeNode[T]]],
}

extend Tree[T: Ordered] {
    public func new() -> This {
        return This { root: Nothing }
    }

    public func insert(this, value: T) {
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

    public func contains(this, value: ref T) -> Bool {
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

caps: [io.network.http]

import std.io.http.{Request, Response, Client}
import std.json.Json

public type ApiClient {
    base_url: String,
    client: Client,
}

public type User {
    id: U64,
    name: String,
    email: String,
}

extend ApiClient {
    public func new(base_url: String) -> This {
        return This {
            base_url: base_url,
            client: Client.new(),
        }
    }

    public func get_user(this, id: U64) -> Outcome[User, Error]
    effects: [io.network.http]
    {
        let url: String = this.base_url + "/users/" + id.to_string()
        let response: Response = this.client.get(url)!

        if response.status != 200 {
            return Err(Error.new("HTTP " + response.status.to_string()))
        }

        let json: Json = Json.parse(response.body)!
        let user: User = User.from_json(json)!

        return Ok(user)
    }

    public func create_user(this, name: String, email: String) -> Outcome[User, Error]
    effects: [io.network.http]
    {
        let url: String = this.base_url + "/users"
        let body: String = Json.object()
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
            name: json.get("name").as_string()!,
            email: json.get("email").as_string()!,
        })
    }
}
```

## 6. Concurrent Counter

```tml
module counter

caps: [io.sync]

import std.sync.{Sync, Mutex}
import std.thread

public type Counter {
    value: Sync[Mutex[I64]],
}

extend Counter {
    public func new() -> This {
        return This {
            value: Sync.new(Mutex.new(0)),
        }
    }

    public func increment(this) {
        let guard: Mutex[I64].Guard = this.value.lock()
        *guard += 1
    }

    public func get(this) -> I64 {
        let guard: Mutex[I64].Guard = this.value.lock()
        return *guard
    }
}

public func parallel_count(n: I32) -> I64
effects: [io.sync]
{
    let counter: Counter = Counter.new()
    var handles: List[thread.Handle] = List.new()

    loop _ in 0 to n {
        let c: Counter = counter.duplicate()
        let handle: thread.Handle = thread.spawn(do() {
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

public type JsonValue =
    | Null
    | Bool(Bool)
    | Number(F64)
    | Str(String)
    | Array(List[JsonValue])
    | Object(Map[String, JsonValue])

public type ParseError {
    message: String,
    position: U64,
}

type Parser {
    input: String,
    pos: U64,
}

extend Parser {
    func new(input: String) -> This {
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
        var result: String = String.new()

        loop {
            when this.next() {
                Just('"') -> return Ok(Str(result)),
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

        let num_str: String = this.input.slice(start, this.pos)
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

        var entries: Map[String, JsonValue] = Map.new()

        if this.peek() == Just('}') {
            this.pos += 1
            return Ok(Object(entries))
        }

        loop {
            let key: String = when this.parse_string()! {
                Str(s) -> s,
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

    func expect(this, s: String) -> Outcome[Unit, ParseError] {
        loop c in s.chars() {
            if this.next() != Just(c) {
                return Err(this.error("expected '" + s + "'"))
            }
        }
        return Ok(unit)
    }

    func error(this, message: String) -> ParseError {
        return ParseError { message: message, position: this.pos }
    }
}

public func parse(input: String) -> Outcome[JsonValue, ParseError] {
    var parser: Parser = Parser.new(input)
    return parser.parse()
}

@test
func test_parse_primitives() {
    assert_eq(parse("null"), Ok(Null))
    assert_eq(parse("true"), Ok(Bool(true)))
    assert_eq(parse("42"), Ok(Number(42.0)))
    assert_eq(parse("\"hello\""), Ok(Str("hello")))
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
func get_user_name(user_id: U64) -> String {
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
func process_nested(data: Maybe[Outcome[String, Error]]) -> String {
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
func sign(x: I32) -> String {
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
public func find_max[T](items: List[T]) -> Maybe[T]
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
public func sort_and_display[T](items: mut ref List[T])
where T: Ordered + Display
{
    items.sort()
    loop item in items {
        println(item.to_string())
    }
}

// Multiple type parameters with constraints
public func merge_sorted[T, U](left: List[T], right: List[U]) -> List[String]
where T: Ordered + Display, U: Ordered + Display
{
    var result: List[String] = List.new()
    // ... merge logic
    return result
}
```

### 8.4 Function Types

```tml
module function_types

// Function type aliases
public type Predicate[T] = func(T) -> Bool
public type Mapper[T, U] = func(T) -> U
public type Comparator[T] = func(T, T) -> I32

// Using function types in structs
public type EventHandler {
    on_click: func(I32, I32) -> (),
    on_hover: func(I32, I32) -> (),
    on_exit: func() -> (),
}

// Functions accepting function types
public func filter[T](items: List[T], pred: Predicate[T]) -> List[T] {
    var result: List[T] = List.new()
    loop item in items {
        if pred(item) {
            result.push(item)
        }
    }
    return result
}

public func map[T, U](items: List[T], mapper: Mapper[T, U]) -> List[U] {
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
    let mut a: I32 = 10
    let mut b: I32 = 20

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
    let mut arr: [I32; 5] = [1, 2, 3, 4, 5]
    let mut total: I32 = 0

    lowlevel {
        let base: *I32 = &arr[0]
        let mut i: I64 = 0

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
caps: [system.lowlevel]

// Zero out a buffer using pointers
func zero_buffer(size: I32) {
    let mut buffer: [U8; 256] = [0xFF; 256]

    lowlevel {
        let ptr: *U8 = &buffer[0]
        let mut i: I64 = 0

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
caps: [system.ffi, system.lowlevel]

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

caps: [io.file, io.process.env]

import std.env
import std.fs.{File, read_to_string}

type Args {
    input: String,
    output: Maybe[String],
    verbose: Bool,
}

public func main() -> Outcome[Unit, Error]
effects: [io.file, io.process.env]
{
    let args: Args = parse_args()!

    if args.verbose {
        println("Reading from: " + args.input)
    }

    let content: String = read_to_string(args.input)!
    let processed: String = process(content)

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
    let argv: List[String] = env.args()

    if argv.len() < 2 {
        return Err(Error.new("Usage: program <input> [-o output] [-v]"))
    }

    var input: Maybe[String] = Nothing
    var output: Maybe[String] = Nothing
    var verbose: Bool = false

    var i: U64 = 1
    loop while i < argv.len() {
        let arg: String = argv[i]

        when arg.as_str() {
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

func process(content: String) -> String {
    return content.to_uppercase()
}
```

---

*Previous: [13-BUILTINS.md](./13-BUILTINS.md)*
*Next: [15-ERROR-HANDLING.md](./15-ERROR-HANDLING.md) — Error Handling*
