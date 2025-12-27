# RFC-0006: Object-Oriented Sugar

## Status
Draft (Low Priority)

## Summary

This RFC defines optional OO syntactic sugar: `class`, `state`, and enhanced `this` patterns. These desugar to core constructs (types + impl blocks) and exist purely for ergonomics.

## Motivation

Many developers and LLMs are familiar with OO patterns. Providing familiar syntax:
1. **Lowers barrier** for developers from OO languages
2. **Matches mental models** for stateful abstractions
3. **Generates cleaner** code for common patterns

**Important**: This is purely syntactic sugar. No new semantics are introduced.

---

## 1. Class Declaration

### 1.1 Basic Class

```tml
class Counter {
    count: I32,

    func new() -> This {
        This { count: 0 }
    }

    func increment(this: mut ref This) {
        this.count += 1
    }

    func get(this: ref This) -> I32 {
        this.count
    }
}
```

### 1.2 Desugaring

The above desugars to:

```tml
type Counter = {
    count: I32,
}

impl Counter {
    func new() -> This {
        This { count: 0 }
    }

    func increment(this: mut ref This) {
        this.count += 1
    }

    func get(this: ref This) -> I32 {
        this.count
    }
}
```

### 1.3 Class with Visibility

```tml
pub class User {
    pub name: String,
    pub(crate) id: U64,
    password_hash: String,  // private

    pub func new(name: String, password: String) -> This {
        This {
            name,
            id: generate_id(),
            password_hash: hash(password),
        }
    }
}
```

---

## 2. State (Mutable Class)

### 2.1 State Declaration

`state` is sugar for classes with mutable state management:

```tml
state Connection {
    socket: Socket,
    buffer: Vec[U8],
    is_open: Bool = true,  // Default value

    func new(addr: String) -> Outcome[This, Error] {
        let socket = Socket.connect(addr)!
        Ok(This {
            socket,
            buffer: Vec.new(),
            is_open: true,
        })
    }

    func send(this, data: ref [U8]) -> Outcome[Unit, Error] {
        if not this.is_open then return Err(Error::Closed)
        this.socket.write(data)
    }

    func close(this) {
        this.is_open = false
        this.socket.close()
    }
}
```

### 2.2 Implicit mut ref this

In `state` blocks, methods take `mut ref This` by default:

```tml
// In state, this is shorthand:
func send(this, data: ref [U8])

// Expands to:
func send(this: mut ref This, data: ref [U8])
```

### 2.3 Desugaring

```tml
state Counter {
    value: I32 = 0,

    func inc(this) { this.value += 1 }
    func get(this) -> I32 { this.value }
}

// Desugars to:
type Counter = {
    value: I32,
}

impl Counter {
    func new() -> This {
        This { value: 0 }
    }

    func inc(this: mut ref This) {
        this.value += 1
    }

    func get(this: ref This) -> I32 {
        this.value
    }
}
```

---

## 3. Enhanced this Patterns

### 3.1 Receiver Shorthand

```tml
impl Point {
    // Full form
    func distance(this: ref This, other: ref Point) -> F64 { ... }

    // Shorthand: ref this
    func distance(ref this, other: ref Point) -> F64 { ... }

    // Shorthand: mut ref this
    func translate(mut ref this, dx: F64, dy: F64) { ... }

    // Shorthand: owned this (consumes)
    func into_tuple(this) -> (F64, F64) { (this.x, this.y) }
}
```

### 3.2 Builder Pattern

```tml
class RequestBuilder {
    method: String = "GET",
    url: String,
    headers: Map[String, String] = Map.new(),
    body: Maybe[Vec[U8]] = Nothing,

    func new(url: String) -> This {
        This { url, ..Default::default() }
    }

    func method(this, method: String) -> This {
        this.method = method
        this  // Return self for chaining
    }

    func header(this, key: String, value: String) -> This {
        this.headers.insert(key, value)
        this
    }

    func body(this, data: Vec[U8]) -> This {
        this.body = Just(data)
        this
    }

    func build(this) -> Request {
        Request {
            method: this.method,
            url: this.url,
            headers: this.headers,
            body: this.body,
        }
    }
}

// Usage:
let request = RequestBuilder::new("https://api.example.com")
    .method("POST")
    .header("Content-Type", "application/json")
    .body(json_bytes)
    .build()
```

---

## 4. Behavior Implementation

### 4.1 Class Implementing Behaviors

```tml
class Point {
    x: F64,
    y: F64,
}

impl Display for Point {
    func fmt(ref this, f: mut ref Formatter) -> Outcome[Unit, FmtError] {
        write!(f, "({}, {})", this.x, this.y)
    }
}

impl Eq for Point {
    func eq(ref this, other: ref This) -> Bool {
        this.x == other.x and this.y == other.y
    }
}
```

### 4.2 Inline Behavior Implementation

```tml
class Point {
    x: F64,
    y: F64,

    impl Display {
        func fmt(ref this, f: mut ref Formatter) -> Outcome[Unit, FmtError> {
            write!(f, "({}, {})", this.x, this.y)
        }
    }

    impl Eq {
        func eq(ref this, other: ref This) -> Bool {
            this.x == other.x and this.y == other.y
        }
    }
}
```

This desugars to separate impl blocks.

---

## 5. Constructor Patterns

### 5.1 Default Constructor

```tml
class Config {
    timeout: U32 = 5000,
    retries: U32 = 3,
    debug: Bool = false,
}

// Auto-generates:
impl Config {
    func new() -> This {
        This {
            timeout: 5000,
            retries: 3,
            debug: false,
        }
    }
}
```

### 5.2 Named Constructors

```tml
class Color {
    r: U8, g: U8, b: U8, a: U8,

    func rgb(r: U8, g: U8, b: U8) -> This {
        This { r, g, b, a: 255 }
    }

    func rgba(r: U8, g: U8, b: U8, a: U8) -> This {
        This { r, g, b, a }
    }

    func from_hex(hex: U32) -> This {
        This {
            r: ((hex >> 16) & 0xFF) as U8,
            g: ((hex >> 8) & 0xFF) as U8,
            b: (hex & 0xFF) as U8,
            a: 255,
        }
    }
}

// Usage:
let red = Color::rgb(255, 0, 0)
let transparent = Color::rgba(0, 0, 0, 128)
let blue = Color::from_hex(0x0000FF)
```

---

## 6. Generics with Class

### 6.1 Generic Class

```tml
class Stack[T] {
    items: Vec[T],

    func new() -> This {
        This { items: Vec.new() }
    }

    func push(this, item: T) {
        this.items.push(item)
    }

    func pop(this) -> Maybe[T] {
        this.items.pop()
    }

    func peek(ref this) -> Maybe[ref T] {
        this.items.last()
    }

    func is_empty(ref this) -> Bool {
        this.items.is_empty()
    }
}
```

### 6.2 Constrained Generics

```tml
class SortedSet[T: Ord] {
    items: Vec[T],

    func insert(this, item: T) -> Bool {
        when this.items.binary_search(ref item) {
            Ok(_) -> false,  // Already exists
            Err(pos) -> {
                this.items.insert(pos, item)
                true
            }
        }
    }
}
```

---

## 7. Access Patterns

### 7.1 Getters and Setters

```tml
class Temperature {
    celsius: F64,

    // Getter (returns value)
    func celsius(ref this) -> F64 {
        this.celsius
    }

    // Computed getter
    func fahrenheit(ref this) -> F64 {
        this.celsius * 9.0 / 5.0 + 32.0
    }

    // Setter
    func set_celsius(this, value: F64) {
        this.celsius = value
    }

    // Computed setter
    func set_fahrenheit(this, value: F64) {
        this.celsius = (value - 32.0) * 5.0 / 9.0
    }
}
```

### 7.2 Property Syntax (Future)

**Note**: Property syntax may be added in future:

```tml
// Potential future syntax (not in this RFC):
class Temperature {
    property celsius: F64

    property fahrenheit: F64 {
        get { this.celsius * 9.0 / 5.0 + 32.0 }
        set { this.celsius = (value - 32.0) * 5.0 / 9.0 }
    }
}
```

---

## 8. Examples

### 8.1 Complete Class Example

```tml
class BankAccount {
    id: U64,
    holder: String,
    balance: U64 = 0,
    transactions: Vec[Transaction] = Vec.new(),

    func new(holder: String) -> This {
        This {
            id: generate_account_id(),
            holder,
            balance: 0,
            transactions: Vec.new(),
        }
    }

    @pre(amount > 0)
    @post(this.balance == old(this.balance) + amount)
    func deposit(this, amount: U64) {
        this.balance += amount
        this.transactions.push(Transaction::deposit(amount))
    }

    @pre(amount > 0)
    @pre(amount <= this.balance)
    @post(this.balance == old(this.balance) - amount)
    func withdraw(this, amount: U64) -> Outcome[Unit, InsufficientFunds] {
        if amount > this.balance then {
            return Err(InsufficientFunds { requested: amount, available: this.balance })
        }
        this.balance -= amount
        this.transactions.push(Transaction::withdraw(amount))
        Ok(())
    }

    func get_balance(ref this) -> U64 {
        this.balance
    }

    func get_statement(ref this) -> Statement {
        Statement {
            account_id: this.id,
            holder: this.holder.duplicate(),
            transactions: this.transactions.duplicate(),
        }
    }
}
```

### 8.2 State Machine

```tml
type ConnectionState = Disconnected | Connecting | Connected | Error(String)

state TcpConnection {
    addr: String,
    socket: Maybe[Socket] = Nothing,
    state: ConnectionState = Disconnected,

    func new(addr: String) -> This {
        This { addr, socket: Nothing, state: Disconnected }
    }

    func connect(this) -> Outcome[Unit, Error] {
        if this.state != Disconnected then {
            return Err(Error::InvalidState)
        }

        this.state = Connecting
        when Socket.connect(this.addr) {
            Ok(sock) -> {
                this.socket = Just(sock)
                this.state = Connected
                Ok(())
            },
            Err(e) -> {
                this.state = Error(e.to_string())
                Err(e)
            }
        }
    }

    func send(this, data: ref [U8]) -> Outcome[U64, Error] {
        when this.state {
            Connected -> this.socket.unwrap().write(data),
            _ -> Err(Error::NotConnected),
        }
    }

    func disconnect(this) {
        if let Just(sock) = this.socket.take() then {
            sock.close()
        }
        this.state = Disconnected
    }
}
```

---

## 9. IR Representation

Class/state declarations are NOT preserved in IR. They desugar completely:

```json
// Source: class Point { x: F64, y: F64, func new(x, y) -> This { ... } }

// IR:
{
  "items": [
    {
      "kind": "type_def",
      "name": "Point",
      "body": {
        "kind": "struct",
        "fields": [
          { "name": "x", "type": "F64" },
          { "name": "y", "type": "F64" }
        ]
      }
    },
    {
      "kind": "impl_block",
      "type": "Point",
      "items": [
        {
          "kind": "func_def",
          "name": "new",
          "params": [...],
          "body": { ... }
        }
      ]
    }
  ]
}
```

---

## 10. Compatibility

- **RFC-0001**: Desugars to core types and functions
- **RFC-0002**: `class`/`state` are reserved keywords
- **RFC-0003**: Contracts work identically on class methods
- **RFC-0005**: Visibility modifiers apply to class members

---

## 11. Alternatives Rejected

### 11.1 Full OO with Inheritance

```java
// Rejected
class Animal { ... }
class Dog extends Animal { ... }
```

Problems:
- Inheritance hierarchies are fragile
- Diamond problem
- Violates "composition over inheritance"
- Behaviors provide better abstraction

### 11.2 Implicit this

```tml
// Rejected
class Counter {
    count: I32,
    func inc() { count += 1 }  // Implicit this.count
}
```

Problems:
- Ambiguous (local vs field)
- Confuses LLMs
- Explicit `this.` is clearer

### 11.3 new as Keyword

```tml
// Rejected
class Point {
    new(x: F64, y: F64) { ... }  // Special constructor syntax
}
```

Chosen approach:
- `new` is just a convention
- Any static method can construct
- Named constructors are more flexible

### 11.4 Private by Name Convention

```python
# Rejected
class Example:
    _private_field = 0
    __very_private = 0
```

Problems:
- Weak enforcement
- Leads to bugs
- Explicit visibility is better

---

## 12. Implementation Priority

This RFC is **LOW PRIORITY**. Implementation order:

1. RFC-0001 (Core) - Required foundation
2. RFC-0004 (Errors) - Required for real code
3. RFC-0005 (Modules) - Required for organization
4. RFC-0002 (Syntax) - Basic syntax
5. RFC-0003 (Contracts) - Nice to have
6. **RFC-0006 (OO)** - Pure sugar, can wait

The language is fully functional without this RFC. OO sugar is convenience, not necessity.

---

## 13. References

- [Rust Structs and Methods](https://doc.rust-lang.org/book/ch05-00-structs.html)
- [Kotlin Classes](https://kotlinlang.org/docs/classes.html)
- [Swift Classes and Structures](https://docs.swift.org/swift-book/documentation/the-swift-programming-language/classesandstructures/)
