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

    loop _ in 2..=n {
        let temp = a + b
        a = b
        b = temp
    }

    return b
}

#[test]
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
    next: Option[Box[Node[T]]],
}

public type LinkedList[T] {
    head: Option[Box[Node[T]]],
    len: U64,
}

extend LinkedList[T] {
    public func new() -> This {
        return This { head: None, len: 0 }
    }

    public func push_front(this, value: T) {
        let new_node = Box.new(Node {
            value: value,
            next: this.head.take(),
        })
        this.head = Some(new_node)
        this.len += 1
    }

    public func pop_front(this) -> Option[T] {
        when this.head.take() {
            Some(node) -> {
                this.head = node.next
                this.len -= 1
                return Some(node.value)
            },
            None -> return None,
        }
    }

    public func len(this) -> U64 {
        return this.len
    }

    public func is_empty(this) -> Bool {
        return this.len == 0
    }
}

#[test]
func test_linked_list() {
    var list = LinkedList[I32].new()

    list.push_front(1)
    list.push_front(2)
    list.push_front(3)

    assert_eq(list.len(), 3)
    assert_eq(list.pop_front(), Some(3))
    assert_eq(list.pop_front(), Some(2))
    assert_eq(list.pop_front(), Some(1))
    assert_eq(list.pop_front(), None)
}
```

## 4. Binary Search Tree

```tml
module bst

public type Tree[T: Ord] {
    root: Option[Box[TreeNode[T]]],
}

type TreeNode[T] {
    value: T,
    left: Option[Box[TreeNode[T]]],
    right: Option[Box[TreeNode[T]]],
}

extend Tree[T: Ord] {
    public func new() -> This {
        return This { root: None }
    }

    public func insert(this, value: T) {
        this.root = This.insert_node(this.root.take(), value)
    }

    func insert_node(node: Option[Box[TreeNode[T]]], value: T) -> Option[Box[TreeNode[T]]] {
        when node {
            None -> return Some(Box.new(TreeNode {
                value: value,
                left: None,
                right: None,
            })),
            Some(n) -> {
                when value.cmp(n.value) {
                    Less -> n.left = This.insert_node(n.left.take(), value),
                    Greater -> n.right = This.insert_node(n.right.take(), value),
                    Equal -> unit,  // duplicate ignored
                }
                return Some(n)
            },
        }
    }

    public func contains(this, value: &T) -> Bool {
        return This.search_node(&this.root, value)
    }

    func search_node(node: &Option[Box[TreeNode[T]]], value: &T) -> Bool {
        when node {
            None -> return false,
            Some(n) -> {
                when value.cmp(&n.value) {
                    Less -> return This.search_node(&n.left, value),
                    Greater -> return This.search_node(&n.right, value),
                    Equal -> return true,
                }
            },
        }
    }
}

#[test]
func test_bst() {
    var tree = Tree[I32].new()

    tree.insert(5)
    tree.insert(3)
    tree.insert(7)
    tree.insert(1)

    assert(tree.contains(&5))
    assert(tree.contains(&3))
    assert(not tree.contains(&10))
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

    public func get_user(this, id: U64) -> Result[User, Error]
    effects: [io.network.http]
    {
        let url = this.base_url + "/users/" + id.to_string()
        let response = this.client.get(url)!

        if response.status != 200 {
            return Err(Error.new("HTTP " + response.status.to_string()))
        }

        let json = Json.parse(response.body)!
        let user = User.from_json(json)!

        return Ok(user)
    }

    public func create_user(this, name: String, email: String) -> Result[User, Error]
    effects: [io.network.http]
    {
        let url = this.base_url + "/users"
        let body = Json.object()
            .set("name", name)
            .set("email", email)
            .to_string()

        let response = this.client.post(url, body)!

        if response.status != 201 {
            return Err(Error.new("Failed to create user"))
        }

        let json = Json.parse(response.body)!
        return User.from_json(json)
    }
}

extend User {
    func from_json(json: Json) -> Result[This, Error] {
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

import std.sync.{Arc, Mutex}
import std.thread

public type Counter {
    value: Arc[Mutex[I64]],
}

extend Counter {
    public func new() -> This {
        return This {
            value: Arc.new(Mutex.new(0)),
        }
    }

    public func increment(this) {
        let guard = this.value.lock()
        *guard += 1
    }

    public func get(this) -> I64 {
        let guard = this.value.lock()
        return *guard
    }
}

public func parallel_count(n: I32) -> I64
effects: [io.sync]
{
    let counter = Counter.new()
    var handles = List.new()

    loop _ in 0..n {
        let c = counter.clone()
        let handle = thread.spawn(do() {
            loop _ in 0..1000 {
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

#[test]
func test_parallel_count() {
    let result = parallel_count(4)
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

    func parse(this) -> Result[JsonValue, ParseError] {
        this.skip_whitespace()
        let value = this.parse_value()!
        this.skip_whitespace()

        if this.pos < this.input.len() {
            return Err(ParseError {
                message: "unexpected characters after value",
                position: this.pos,
            })
        }

        return Ok(value)
    }

    func parse_value(this) -> Result[JsonValue, ParseError] {
        this.skip_whitespace()

        when this.peek() {
            Some('n') -> return this.parse_null(),
            Some('t') -> return this.parse_true(),
            Some('f') -> return this.parse_false(),
            Some('"') -> return this.parse_string(),
            Some('[') -> return this.parse_array(),
            Some('{') -> return this.parse_object(),
            Some(c) -> {
                if c == '-' or c.is_digit() {
                    return this.parse_number()
                }
                return Err(this.error("unexpected character"))
            },
            None -> return Err(this.error("unexpected end of input")),
        }
    }

    func parse_null(this) -> Result[JsonValue, ParseError] {
        this.expect("null")!
        return Ok(Null)
    }

    func parse_true(this) -> Result[JsonValue, ParseError] {
        this.expect("true")!
        return Ok(Bool(true))
    }

    func parse_false(this) -> Result[JsonValue, ParseError] {
        this.expect("false")!
        return Ok(Bool(false))
    }

    func parse_string(this) -> Result[JsonValue, ParseError] {
        this.expect("\"")!
        var result = String.new()

        loop {
            when this.next() {
                Some('"') -> return Ok(Str(result)),
                Some('\\') -> {
                    let escaped = this.parse_escape()!
                    result.push(escaped)
                },
                Some(c) -> result.push(c),
                None -> return Err(this.error("unterminated string")),
            }
        }
    }

    func parse_number(this) -> Result[JsonValue, ParseError] {
        let start = this.pos

        if this.peek() == Some('-') {
            this.pos += 1
        }

        loop while this.peek().map(do(c) c.is_digit()).unwrap_or(false) {
            this.pos += 1
        }

        if this.peek() == Some('.') {
            this.pos += 1
            loop while this.peek().map(do(c) c.is_digit()).unwrap_or(false) {
                this.pos += 1
            }
        }

        let num_str = this.input.slice(start, this.pos)
        when num_str.parse[F64]() {
            Ok(n) -> return Ok(Number(n)),
            Err(_) -> return Err(this.error("invalid number")),
        }
    }

    func parse_array(this) -> Result[JsonValue, ParseError] {
        this.expect("[")!
        this.skip_whitespace()

        var items = List.new()

        if this.peek() == Some(']') {
            this.pos += 1
            return Ok(Array(items))
        }

        loop {
            let value = this.parse_value()!
            items.push(value)

            this.skip_whitespace()

            when this.peek() {
                Some(',') -> {
                    this.pos += 1
                    this.skip_whitespace()
                },
                Some(']') -> {
                    this.pos += 1
                    return Ok(Array(items))
                },
                _ -> return Err(this.error("expected ',' or ']'")),
            }
        }
    }

    func parse_object(this) -> Result[JsonValue, ParseError] {
        this.expect("{")!
        this.skip_whitespace()

        var entries = Map.new()

        if this.peek() == Some('}') {
            this.pos += 1
            return Ok(Object(entries))
        }

        loop {
            let key = when this.parse_string()! {
                Str(s) -> s,
                _ -> return Err(this.error("expected string key")),
            }

            this.skip_whitespace()
            this.expect(":")!
            this.skip_whitespace()

            let value = this.parse_value()!
            entries.insert(key, value)

            this.skip_whitespace()

            when this.peek() {
                Some(',') -> {
                    this.pos += 1
                    this.skip_whitespace()
                },
                Some('}') -> {
                    this.pos += 1
                    return Ok(Object(entries))
                },
                _ -> return Err(this.error("expected ',' or '}'")),
            }
        }
    }

    // Helper methods
    func peek(this) -> Option[Char] {
        return this.input.chars().nth(this.pos)
    }

    func next(this) -> Option[Char] {
        let c = this.peek()
        if c.is_some() {
            this.pos += 1
        }
        return c
    }

    func skip_whitespace(this) {
        loop while this.peek().map(do(c) c.is_whitespace()).unwrap_or(false) {
            this.pos += 1
        }
    }

    func expect(this, s: String) -> Result[Unit, ParseError] {
        loop c in s.chars() {
            if this.next() != Some(c) {
                return Err(this.error("expected '" + s + "'"))
            }
        }
        return Ok(unit)
    }

    func error(this, message: String) -> ParseError {
        return ParseError { message: message, position: this.pos }
    }
}

public func parse(input: String) -> Result[JsonValue, ParseError] {
    var parser = Parser.new(input)
    return parser.parse()
}

#[test]
func test_parse_primitives() {
    assert_eq(parse("null"), Ok(Null))
    assert_eq(parse("true"), Ok(Bool(true)))
    assert_eq(parse("42"), Ok(Number(42.0)))
    assert_eq(parse("\"hello\""), Ok(Str("hello")))
}

#[test]
func test_parse_array() {
    let result = parse("[1, 2, 3]")
    assert(result.is_ok())
}

#[test]
func test_parse_object() {
    let result = parse("{\"name\": \"Alice\", \"age\": 30}")
    assert(result.is_ok())
}
```

## 8. CLI Application

```tml
module cli

caps: [io.file, io.process.env]

import std.env
import std.fs.{File, read_to_string}

type Args {
    input: String,
    output: Option[String],
    verbose: Bool,
}

public func main() -> Result[Unit, Error]
effects: [io.file, io.process.env]
{
    let args = parse_args()!

    if args.verbose {
        println("Reading from: " + args.input)
    }

    let content = read_to_string(args.input)!
    let processed = process(content)

    when args.output {
        Some(path) -> {
            File.write(path, processed)!
            if args.verbose {
                println("Wrote to: " + path)
            }
        },
        None -> println(processed),
    }

    return Ok(unit)
}

func parse_args() -> Result[Args, Error] {
    let argv = env.args()

    if argv.len() < 2 {
        return Err(Error.new("Usage: program <input> [-o output] [-v]"))
    }

    var input: Option[String] = None
    var output: Option[String] = None
    var verbose = false

    var i: U64 = 1
    loop while i < argv.len() {
        let arg = argv[i]

        when arg.as_str() {
            "-o" -> {
                i += 1
                if i >= argv.len() {
                    return Err(Error.new("-o requires argument"))
                }
                output = Some(argv[i])
            },
            "-v" -> verbose = true,
            _ -> {
                if input.is_none() {
                    input = Some(arg)
                } else {
                    return Err(Error.new("unexpected argument: " + arg))
                }
            },
        }
        i += 1
    }

    when input {
        Some(i) -> return Ok(Args { input: i, output: output, verbose: verbose }),
        None -> return Err(Error.new("input file required")),
    }
}

func process(content: String) -> String {
    return content.to_uppercase()
}
```

---

*Previous: [13-BUILTINS.md](./13-BUILTINS.md)*
*Next: [15-ERROR-HANDLING.md](./15-ERROR-HANDLING.md) — Error Handling*
