# TML v1.0 — Builtin Types and Functions

## 1. Primitive Types

### 1.1 Bool
```tml
true, false

// Operations
not b
a and b
a or b
```

### 1.2 Integers

| Type | Operations |
|------|------------|
| `I8`, `I16`, `I32`, `I64`, `I128` | `+`, `-`, `*`, `/`, `%`, `**` |
| `U8`, `U16`, `U32`, `U64`, `U128` | `&`, `\|`, `^`, `~`, `<<`, `>>` |

```tml
// Common methods
x.abs()              // absolute value (signed)
x.signum()           // sign: -1, 0, or 1
x.pow(n)             // power
x.to_string()        // conversion
x.to_i32()           // conversion between types
x.checked_add(y)     // Option[T], None on overflow
x.saturating_add(y)  // saturates at MAX/MIN
x.wrapping_add(y)    // wraps on overflow

// Constants
I32.MIN, I32.MAX
U64.MIN, U64.MAX
```

### 1.3 Floats

```tml
// F32, F64
x.abs()
x.floor()
x.ceil()
x.round()
x.sqrt()
x.sin(), x.cos(), x.tan()
x.ln(), x.log10(), x.log2()
x.exp()
x.is_nan()
x.is_infinite()

// Constants
F64.PI, F64.E
F64.INFINITY, F64.NEG_INFINITY
F64.NAN
```

### 1.4 Char

```tml
'a'.is_alphabetic()
'1'.is_numeric()
'A'.is_uppercase()
'a'.is_lowercase()
'A'.to_lowercase()  // 'a'
'a'.to_uppercase()  // 'A'
'a'.to_digit(10)    // Option[U32]
```

### 1.5 String

```tml
// Construction
String.new()
String.from("text")

// Methods
s.len()              // bytes
s.chars()            // Iterator[Char]
s.is_empty()
s.contains("sub")
s.starts_with("pre")
s.ends_with("suf")
s.find("sub")        // Option[U64]
s.replace("old", "new")
s.trim()
s.trim_start()
s.trim_end()
s.to_lowercase()
s.to_uppercase()
s.split(",")         // Iterator[String]
s.lines()            // Iterator[String]
s.slice(start, end)  // substring
s + other            // concatenation
```

### 1.6 Bytes

```tml
Bytes.new()
b"literal"

b.len()
b.get(index)         // Option[U8]
b.slice(start, end)
b.to_string()        // Result[String, Utf8Error]
```

## 2. Collections

### 2.1 List[T]

```tml
List.new()
List.of(1, 2, 3)
List.with_capacity(100)

list.len()
list.is_empty()
list.push(item)
list.pop()           // Option[T]
list.get(index)      // Option[T]
list.first()         // Option[T]
list.last()          // Option[T]
list.insert(index, item)
list.remove(index)   // T
list.clear()
list.contains(item)  // requires T: Eq
list.reverse()
list.sort()          // requires T: Ord
list.clone()         // requires T: Clone

// Iteration
list.iter()          // Iterator[&T]
list.iter_mut()      // Iterator[&mut T]
list.into_iter()     // Iterator[T]

// Functional
list.map(func)
list.filter(func)
list.fold(init, func)
list.find(func)      // Option[T]
list.any(func)       // Bool
list.all(func)       // Bool
```

### 2.2 Map[K, V]

```tml
Map.new()
Map.with_capacity(100)

map.len()
map.is_empty()
map.insert(key, value)  // Option[V] (old value)
map.get(key)            // Option[&V]
map.get_mut(key)        // Option[&mut V]
map.remove(key)         // Option[V]
map.contains(key)       // Bool
map.keys()              // Iterator[&K]
map.values()            // Iterator[&V]
map.entries()           // Iterator[(&K, &V)]
map.clear()
```

### 2.3 Set[T]

```tml
Set.new()
Set.of(1, 2, 3)

set.len()
set.is_empty()
set.insert(item)     // Bool (true if new)
set.remove(item)     // Bool (true if existed)
set.contains(item)   // Bool
set.union(other)     // Set[T]
set.intersection(other)
set.difference(other)
set.is_subset(other)
set.is_superset(other)
```

## 3. Option[T]

```tml
Some(value)
None

opt.is_some()        // Bool
opt.is_none()        // Bool
opt.unwrap()         // T (panic if None)
opt.unwrap_or(default)
opt.unwrap_or_else(func)
opt.expect(msg)      // T (panic with msg if None)
opt.map(func)        // Option[U]
opt.and_then(func)   // Option[U]
opt.or(other)        // Option[T]
opt.or_else(func)    // Option[T]
opt.filter(func)     // Option[T]
opt.ok_or(err)       // Result[T, E]
```

## 4. Result[T, E]

```tml
Ok(value)
Err(error)

res.is_ok()          // Bool
res.is_err()         // Bool
res.unwrap()         // T (panic if Err)
res.unwrap_err()     // E (panic if Ok)
res.unwrap_or(default)
res.expect(msg)
res.map(func)        // Result[U, E]
res.map_err(func)    // Result[T, F]
res.and_then(func)   // Result[U, E]
res.or(other)        // Result[T, E]
res.or_else(func)    // Result[T, F]
res.ok()             // Option[T]
res.err()            // Option[E]
```

## 5. Ranges

```tml
0..10                // Range[I32] (0 to 9)
0..=10               // RangeInclusive (0 to 10)
..10                 // RangeTo
10..                 // RangeFrom
..                   // RangeFull

range.contains(5)    // Bool
range.is_empty()     // Bool

loop i in 0..10 {
    // i = 0, 1, 2, ..., 9
}
```

## 6. Iterators

```tml
trait Iterator {
    type Item
    func next(this) -> Option[This.Item]
}

// Iterator methods
iter.count()
iter.last()
iter.nth(n)
iter.skip(n)
iter.take(n)
iter.step_by(n)
iter.chain(other)
iter.zip(other)
iter.enumerate()     // Iterator[(U64, T)]
iter.map(func)
iter.filter(func)
iter.filter_map(func)
iter.flat_map(func)
iter.flatten()
iter.fold(init, func)
iter.reduce(func)
iter.find(func)
iter.position(func)
iter.any(func)
iter.all(func)
iter.max()           // requires Ord
iter.min()
iter.sum()           // requires Add
iter.product()       // requires Mul
iter.collect()       // to List, Set, etc.
```

## 7. Global Functions

### 7.1 I/O

```tml
print(msg)           // prints without newline
println(msg)         // prints with newline
eprint(msg)          // stderr
eprintln(msg)        // stderr with newline
```

### 7.2 Control

```tml
panic(msg)           // terminates with error
unreachable()        // marks unreachable code
todo()               // placeholder for implementation
unimplemented()      // unimplemented feature
```

### 7.3 Debug

```tml
dbg(expr)            // prints and returns value
assert(cond)
assert(cond, msg)
assert_eq(a, b)
assert_ne(a, b)
debug_assert(cond)   // only in debug
```

### 7.4 Memory

```tml
size_of[T]()         // size in bytes
align_of[T]()        // alignment
drop(value)          // explicitly destroys value
forget(value)        // doesn't call destructor
```

## 8. Fundamental Traits

### 8.1 Eq

```tml
trait Eq {
    func eq(this, other: This) -> Bool
    func ne(this, other: This) -> Bool {
        return not this.eq(other)
    }
}

// Usage: a == b, a != b
```

### 8.2 Ord

```tml
type Ordering = Less | Equal | Greater

trait Ord: Eq {
    func cmp(this, other: This) -> Ordering

    func lt(this, other: This) -> Bool
    func le(this, other: This) -> Bool
    func gt(this, other: This) -> Bool
    func ge(this, other: This) -> Bool
    func max(this, other: This) -> This
    func min(this, other: This) -> This
}

// Usage: a < b, a <= b, a > b, a >= b
```

### 8.3 Clone

```tml
trait Clone {
    func clone(this) -> This
}

let copy = original.clone()
```

### 8.4 Default

```tml
trait Default {
    func default() -> This
}

let x = I32.default()    // 0
let s = String.default() // ""
let l = List.default()   // []
```

### 8.5 Debug

```tml
trait Debug {
    func debug(this) -> String
}

print(value.debug())
```

### 8.6 Hash

```tml
trait Hash {
    func hash(this, hasher: &mut Hasher)
}

// Required to use as key in Map/Set
```

### 8.7 Add, Sub, Mul, Div

```tml
trait Add[Rhs = This] {
    type Output
    func add(this, rhs: Rhs) -> This.Output
}

// Similar for Sub, Mul, Div, Rem
```

### 8.8 From / Into

```tml
trait From[T] {
    func from(value: T) -> This
}

trait Into[T] {
    func into(this) -> T
}

let s: String = String.from(42)
let n: I32 = "42".parse().unwrap()
```

---

*Previous: [12-ERRORS.md](./12-ERRORS.md)*
*Next: [14-EXAMPLES.md](./14-EXAMPLES.md) — Complete Examples*
