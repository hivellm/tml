# RFC-0001: Core Language

## Status
Active (v0.5.0)

## Implementation Status

| Feature | Status | Notes |
|---------|--------|-------|
| Primitive Types | ✅ Complete | I8-I128, U8-U128, F32, F64, Bool, Str |
| Structs | ✅ Complete | Named fields, construction, access |
| Enums | ✅ Complete | Simple and with data, pattern matching |
| **Generics** | ✅ Complete | Monomorphization (Rust-style) |
| Generic Structs | ✅ Complete | `Pair[T]`, `Entry[K, V]` |
| Generic Enums | ✅ Complete | `Maybe[T]`, `Outcome[T, E]` |
| Generic Functions | ✅ Complete | Full codegen with monomorphization |
| Bounds/Constraints | ✅ Complete | `where T: Addable` syntax |
| String Interpolation | ✅ Complete | `"Hello {name}!"` syntax |
| Effects | ❌ Not Started | `with io, panic` |
| Ownership | ✅ Basic | Move semantics, no borrow checker |
| **Concurrency** | ✅ Complete | Atomics, fences, spinlocks |

## Summary

This RFC defines the core language of TML: the type system, effect system, ownership model, and canonical IR format. The core is the semantic foundation—surface syntax (RFC-0002) desugars into it.

## Motivation

LLMs generate code that must be:
1. **Verifiable** - Types and effects catch errors statically
2. **Deterministic** - Same IR produces same behavior
3. **Patchable** - Stable IDs enable precise edits without full regeneration

The core language is minimal and explicit. All sugar lives in the surface syntax.

---

## 1. Type System

### 1.1 Primitive Types

| Type | Size | Description |
|------|------|-------------|
| `Unit` | 0 | No value, written `()` |
| `Bool` | 1 bit | `true` or `false` |
| `I8`, `I16`, `I32`, `I64`, `I128` | signed | Two's complement integers |
| `U8`, `U16`, `U32`, `U64`, `U128` | unsigned | Unsigned integers |
| `F32`, `F64` | IEEE 754 | Floating point |
| `Char` | 4 bytes | Unicode scalar value |
| `Never` | 0 | Uninhabited type (diverges) |

### 1.2 Composite Types

```
Type ::= PrimitiveType
       | TypeName GenericArgs?
       | TupleType
       | FunctionType
       | ReferenceType
       | PointerType

GenericArgs    ::= '[' Type (',' Type)* ']'
TupleType      ::= '(' Type (',' Type)* ')'
FunctionType   ::= 'func' '(' ParamTypes? ')' '->' Type EffectSet?
ReferenceType  ::= 'ref' Type | 'mut' 'ref' Type
PointerType    ::= '*' Type | '*' 'mut' Type
```

### 1.3 Algebraic Data Types

```
// Sum type (tagged union)
type Outcome[T, E] = Ok(T) | Err(E)
type Maybe[T] = Just(T) | Nothing

// Product type (struct)
type Point = { x: F64, y: F64 }

// Newtype wrapper
type UserId = U64
```

**IR Representation:**

```json
{
  "kind": "type_def",
  "name": "Outcome",
  "params": ["T", "E"],
  "body": {
    "kind": "sum",
    "variants": [
      { "name": "Ok", "fields": [{ "type": { "ref": "T" } }] },
      { "name": "Err", "fields": [{ "type": { "ref": "E" } }] }
    ]
  }
}
```

### 1.4 Generic Constraints

```
func max[T: Ord](a: T, b: T) -> T
func serialize[T: Serialize + Display](value: T) -> String
```

Constraints use behaviors (traits). Multiple constraints use `+`.

---

## 2. Effect System

Effects track what a function MAY do. Effects are:
- **Declared** in function signatures
- **Inferred** when omitted (with warning in strict mode)
- **Checked** at call sites

### 2.1 Built-in Effects

| Effect | Description |
|--------|-------------|
| `pure` | No effects (default if none declared) |
| `io` | File system, network, environment |
| `async` | May suspend execution |
| `panic` | May panic/abort |
| `alloc` | May allocate heap memory |
| `unsafe` | May perform unsafe operations |

### 2.2 Effect Syntax

```
// Explicit effects
func read_file(path: String) -> String with io, panic

// Multiple effects
func fetch(url: String) -> Response with io, async, panic

// Pure function (no effects)
func add(a: I32, b: I32) -> I32

// Effect polymorphism
func map[T, U, E](list: List[T], f: func(T) -> U with E) -> List[U] with E
```

### 2.3 Effect Rules

1. A function MAY call functions with subset of its effects
2. A function MUST NOT call functions with effects it doesn't declare
3. Effect inference adds the union of callee effects
4. `unsafe` effect is never inferred—must be explicit

**IR Representation:**

```json
{
  "kind": "func_def",
  "name": "read_file",
  "params": [{ "name": "path", "type": "String" }],
  "return_type": "String",
  "effects": ["io", "panic"],
  "body": { ... }
}
```

---

## 3. Ownership and Borrowing

TML uses Rust-style ownership with simplified syntax.

### 3.1 Ownership Rules

1. Every value has exactly one owner
2. When owner goes out of scope, value is dropped
3. Ownership can be transferred (moved)
4. Values can be borrowed (references)

### 3.2 Reference Types

| Syntax | Meaning | Rules |
|--------|---------|-------|
| `ref T` | Immutable borrow | Many allowed, no mutation |
| `mut ref T` | Mutable borrow | Exactly one, exclusive access |
| `T` | Owned value | Moved on use (unless Copy) |

### 3.3 Lifetime Inference

TML does NOT expose lifetime annotations. Lifetimes are:
- Inferred by the compiler
- Errors reported in terms of scope ("borrowed value does not live long enough")
- Complex cases may require restructuring code

```
// Compiler infers: returns reference with lifetime of input
func first(items: ref List[T]) -> ref T

// Compiler error if ambiguous:
func bad(a: ref T, b: ref T) -> ref T  // Error: cannot infer lifetime
```

### 3.4 Copy and Move

```
// Copy types: primitives, small structs with Copy
let x: I32 = 5
let y = x  // Copy, both valid

// Move types: heap-allocated, large structs
let s: String = "hello"
let t = s  // Move, s is invalid

// Explicit duplicate
let u = t.duplicate()  // t still valid
```

---

## 4. Concurrency Primitives

TML provides low-level concurrency primitives for building lock-free data structures and synchronization mechanisms.

### 4.1 Atomic Operations

All atomic operations use sequentially consistent (SeqCst) ordering by default.

| Function | Signature | Description |
|----------|-----------|-------------|
| `atomic_load` | `(ptr: *Unit) -> I32` | Thread-safe read |
| `atomic_store` | `(ptr: *Unit, val: I32) -> Unit` | Thread-safe write |
| `atomic_add` | `(ptr: *Unit, val: I32) -> I32` | Fetch-and-add, returns old value |
| `atomic_sub` | `(ptr: *Unit, val: I32) -> I32` | Fetch-and-subtract, returns old value |
| `atomic_exchange` | `(ptr: *Unit, val: I32) -> I32` | Swap, returns old value |
| `atomic_cas` | `(ptr: *Unit, expected: I32, new: I32) -> Bool` | Compare-and-swap |
| `atomic_and` | `(ptr: *Unit, val: I32) -> I32` | Bitwise AND, returns old value |
| `atomic_or` | `(ptr: *Unit, val: I32) -> I32` | Bitwise OR, returns old value |
| `atomic_xor` | `(ptr: *Unit, val: I32) -> I32` | Bitwise XOR, returns old value |

### 4.2 Memory Fences

| Function | Description |
|----------|-------------|
| `fence()` | Full memory barrier (SeqCst) |
| `fence_acquire()` | Acquire barrier - loads/stores after cannot move before |
| `fence_release()` | Release barrier - loads/stores before cannot move after |

### 4.3 Spinlock Primitives

| Function | Signature | Description |
|----------|-----------|-------------|
| `spin_lock` | `(lock: *Unit) -> Unit` | Acquire lock, spins until acquired |
| `spin_unlock` | `(lock: *Unit) -> Unit` | Release lock |
| `spin_trylock` | `(lock: *Unit) -> Bool` | Try to acquire, returns immediately |

### 4.4 Thread Primitives

| Function | Signature | Description |
|----------|-----------|-------------|
| `thread_yield` | `() -> Unit` | Yield execution to other threads |
| `thread_id` | `() -> I64` | Get current thread ID |

### 4.5 Example: Lock-Free Counter

```tml
func increment_counter(counter: *Unit) -> I32 {
    atomic_add(counter, 1)
}

func main() {
    let counter: *Unit = alloc(4)
    atomic_store(counter, 0)

    // Increment atomically
    let old: I32 = increment_counter(counter)
    println("Incremented from {} to {}", old, old + 1)

    dealloc(counter)
}
```

### 4.6 Example: Spinlock Usage

```tml
func with_lock(lock: *Unit, critical_section: func() -> Unit) {
    spin_lock(lock)
    critical_section()
    spin_unlock(lock)
}

func main() {
    let lock: *Unit = alloc(4)
    atomic_store(lock, 0)  // Initialize unlocked

    with_lock(lock, do() {
        println("In critical section")
    })

    dealloc(lock)
}
```

---

## 5. Canonical IR

The IR is the single source of truth. All tools consume/produce IR.

### 5.1 Design Goals

1. **Stable IDs** - Every node has a content-addressable ID
2. **Round-trip** - Parse → IR → Unparse = semantically identical
3. **Diffable** - Patches are minimal JSON operations
4. **Versionable** - Schema version in every IR document

### 5.2 IR Schema (JSON)

```json
{
  "$schema": "https://tml-lang.org/ir/v1.schema.json",
  "version": "1.0.0",
  "module": {
    "name": "example",
    "items": [
      {
        "id": "fn_abc123",
        "kind": "func_def",
        "name": "greet",
        "params": [
          { "name": "name", "type": "String" }
        ],
        "return_type": "String",
        "effects": [],
        "body": {
          "id": "expr_def456",
          "kind": "binary_op",
          "op": "+",
          "left": { "kind": "string_lit", "value": "Hello, " },
          "right": { "kind": "ident", "name": "name" }
        }
      }
    ]
  }
}
```

### 5.3 Stable ID Generation

IDs are generated from content hash:

```
ID = base58(sha256(canonical_json(node_without_id)))[:12]
```

This means:
- Same code = same ID (content-addressable)
- Rename refactoring changes ID (intentional—semantics changed)
- Whitespace/comments don't affect ID

### 5.4 IR Node Kinds

```
ItemKind ::= 'func_def' | 'type_def' | 'const_def' | 'behavior_def' | 'impl_block'

ExprKind ::= 'ident' | 'literal' | 'binary_op' | 'unary_op' | 'call'
           | 'field_access' | 'index' | 'if' | 'when' | 'loop'
           | 'block' | 'return' | 'break' | 'continue'
           | 'let' | 'assign' | 'ref' | 'deref' | 'cast'
           | 'struct_init' | 'tuple_init' | 'array_init'
           | 'closure' | 'method_call'

PatternKind ::= 'ident' | 'wildcard' | 'literal' | 'tuple' | 'struct'
              | 'variant' | 'or' | 'guard'
```

### 5.5 Protobuf Schema

For binary serialization (10x smaller, 5x faster):

```protobuf
syntax = "proto3";
package tml.ir.v1;

message Module {
  string name = 1;
  repeated Item items = 2;
}

message Item {
  string id = 1;
  oneof kind {
    FuncDef func_def = 2;
    TypeDef type_def = 3;
    ConstDef const_def = 4;
    BehaviorDef behavior_def = 5;
    ImplBlock impl_block = 6;
  }
}

message FuncDef {
  string name = 1;
  repeated Param params = 2;
  Type return_type = 3;
  repeated string effects = 4;
  Expr body = 5;
}

message Expr {
  string id = 1;
  oneof kind {
    Ident ident = 2;
    Literal literal = 3;
    BinaryOp binary_op = 4;
    Call call = 5;
    // ... etc
  }
}
```

---

## 6. Examples

### 6.1 Simple Function

**Surface:**
```tml
func factorial(n: U64) -> U64 {
    if n <= 1 then 1 else n * factorial(n - 1)
}
```

**IR:**
```json
{
  "id": "fn_fact01",
  "kind": "func_def",
  "name": "factorial",
  "params": [{ "name": "n", "type": "U64" }],
  "return_type": "U64",
  "effects": [],
  "body": {
    "id": "if_abc123",
    "kind": "if",
    "condition": {
      "kind": "binary_op", "op": "<=",
      "left": { "kind": "ident", "name": "n" },
      "right": { "kind": "literal", "type": "U64", "value": 1 }
    },
    "then": { "kind": "literal", "type": "U64", "value": 1 },
    "else": {
      "kind": "binary_op", "op": "*",
      "left": { "kind": "ident", "name": "n" },
      "right": {
        "kind": "call",
        "func": "factorial",
        "args": [{
          "kind": "binary_op", "op": "-",
          "left": { "kind": "ident", "name": "n" },
          "right": { "kind": "literal", "type": "U64", "value": 1 }
        }]
      }
    }
  }
}
```

### 6.2 Generic with Effects

**Surface:**
```tml
func read_json[T: Deserialize](path: String) -> Outcome[T, Error] with io {
    let content = File.read(path)!
    let parsed = Json.parse[T](content)!
    Ok(parsed)
}
```

**IR:**
```json
{
  "id": "fn_rjson1",
  "kind": "func_def",
  "name": "read_json",
  "type_params": [{ "name": "T", "constraints": ["Deserialize"] }],
  "params": [{ "name": "path", "type": "String" }],
  "return_type": { "name": "Outcome", "args": [{ "ref": "T" }, "Error"] },
  "effects": ["io"],
  "body": {
    "kind": "block",
    "stmts": [
      {
        "kind": "let",
        "name": "content",
        "value": {
          "kind": "propagate",
          "expr": { "kind": "call", "func": "File.read", "args": [{ "kind": "ident", "name": "path" }] }
        }
      },
      {
        "kind": "let",
        "name": "parsed",
        "value": {
          "kind": "propagate",
          "expr": { "kind": "call", "func": "Json.parse", "type_args": [{ "ref": "T" }], "args": [{ "kind": "ident", "name": "content" }] }
        }
      },
      {
        "kind": "call",
        "func": "Ok",
        "args": [{ "kind": "ident", "name": "parsed" }]
      }
    ]
  }
}
```

---

## 7. Compatibility

- **RFC-0002**: Surface syntax desugars to this core
- **RFC-0003**: Contracts compile to runtime checks + static hints
- **RFC-0004**: Error handling is sugar over `Outcome` type
- **RFC-0005**: Modules wrap items with visibility
- **RFC-0006**: OO sugar desugars to functions + types

---

## 8. Alternatives Rejected

### 8.1 Explicit Lifetimes

Rust requires `'a` annotations. We rejected this because:
- LLMs struggle with lifetime syntax
- Most code works with inference
- Complex cases are rare and can be restructured

### 8.2 Subtyping

Java-style inheritance was rejected:
- Complicates type inference
- Variance rules confuse LLMs
- Composition via behaviors is simpler

### 8.3 Exceptions

Traditional try/catch was rejected:
- Hidden control flow
- Non-local reasoning required
- `Outcome` with `!` is explicit and local

### 8.4 Null

`null` was rejected in favor of `Maybe[T]`:
- Billion-dollar mistake
- Forces explicit handling
- Pattern matching is natural

---

## 9. References

- [Rust Reference: Type System](https://doc.rust-lang.org/reference/types.html)
- [Effect Systems Survey](https://arxiv.org/abs/1903.08049)
- [Algebraic Effects for the Rest of Us](https://overreacted.io/algebraic-effects-for-the-rest-of-us/)
- [Content-Addressable Storage](https://en.wikipedia.org/wiki/Content-addressable_storage)
