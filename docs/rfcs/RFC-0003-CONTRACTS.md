# RFC-0003: Contracts

## Status
Draft

## Summary

This RFC defines TML's contract system: preconditions, postconditions, invariants, and quantifiers. Contracts serve dual purposes—static verification hints and runtime assertions.

## Motivation

LLM-generated code benefits from explicit contracts:
1. **Documentation** - Contracts express intent precisely
2. **Verification** - Static analyzers can prove properties
3. **Testing** - Contracts generate test cases automatically
4. **Debugging** - Runtime violations pinpoint bugs

---

## 1. Contract Types

### 1.1 Preconditions (@pre)

Conditions that MUST hold before function execution.

```tml
@pre(index < array.len())
func get[T](array: ref Array[T], index: U64) -> ref T {
    array[index]
}

@pre(divisor != 0)
func divide(dividend: I32, divisor: I32) -> I32 {
    dividend / divisor
}
```

### 1.2 Postconditions (@post)

Conditions that MUST hold after function execution. Use `result` to refer to return value.

```tml
@post(result >= 0)
func abs(x: I32) -> I32 {
    if x < 0 then -x else x
}

@post(result.len() == a.len() + b.len())
func concat(a: String, b: String) -> String {
    a + b
}
```

### 1.3 Invariants (@invariant)

Conditions that MUST hold throughout a type's lifetime.

```tml
@invariant(this.len <= this.capacity)
@invariant(this.capacity > 0)
type Vec[T] = {
    data: *mut T,
    len: U64,
    capacity: U64,
}
```

### 1.4 Assertions (@assert)

Inline conditions that MUST hold at a specific point.

```tml
func process(data: List[I32]) -> I32 {
    let sorted = data.sort()
    @assert(sorted.is_sorted())

    let mid = sorted.len() / 2
    @assert(mid < sorted.len())

    sorted[mid]
}
```

---

## 2. Quantifiers

### 2.1 Universal Quantifier (@forall)

Condition holds for ALL elements.

```tml
@post(forall(i in 0 to result.len()) result[i] >= 0)
func abs_all(nums: List[I32]) -> List[I32] {
    nums.map(do(n) if n < 0 then -n else n)
}

@invariant(forall(i in 0 to this.len()) this[i] != null)
type NonNullList[T] = { ... }
```

### 2.2 Existential Quantifier (@exists)

Condition holds for AT LEAST ONE element.

```tml
@pre(exists(x in items) predicate(x))
func find_first[T](items: List[T], predicate: func(ref T) -> Bool) -> T {
    // Can assume at least one match exists
    for item in items {
        if predicate(item) then return item
    }
    unreachable!()
}
```

### 2.3 Quantifier Syntax

```peg
Quantifier   <- ForAll / Exists
ForAll       <- 'forall' '(' Binding 'in' Range ')' Condition
Exists       <- 'exists' '(' Binding 'in' Range ')' Condition
Binding      <- Ident (':' Type)?
Range        <- Expr 'to' Expr / Expr 'through' Expr / Expr
Condition    <- Expr
```

---

## 3. Static vs Runtime

Contracts have two enforcement modes:

### 3.1 Static Checking

The compiler/analyzer attempts to PROVE contracts at compile time.

```tml
@pre(n > 0)
func factorial(n: U64) -> U64 {
    if n == 1 then 1 else n * factorial(n - 1)
}

// Compiler proves: recursive call has n-1 > 0 when n > 1
// No runtime check needed
```

**Static checking levels:**

| Level | Behavior |
|-------|----------|
| `none` | No static checking |
| `basic` | Simple bounds, null checks |
| `full` | SMT solver integration |

### 3.2 Runtime Checking

When static proof is impossible, contracts become runtime assertions.

```tml
@pre(is_valid_email(email))  // Cannot prove statically
func send_email(email: String) { ... }

// Compiles to:
func send_email(email: String) {
    if not is_valid_email(email) then {
        panic!("Precondition failed: is_valid_email(email)")
    }
    // ... body
}
```

**Runtime checking modes:**

| Mode | Behavior | Use Case |
|------|----------|----------|
| `debug` | All contracts checked | Development |
| `release` | Only `@assert` checked | Production |
| `unsafe` | No runtime checks | Performance-critical |

### 3.3 Configuration

```toml
# tml.toml
[contracts]
static_level = "full"      # none | basic | full
runtime_mode = "debug"     # debug | release | unsafe

[contracts.release]
pre = false               # Disable preconditions
post = false              # Disable postconditions
invariant = true          # Keep invariants
assert = true             # Keep assertions
```

---

## 4. Contract Inheritance

### 4.1 Behavior Contracts

Contracts on behaviors apply to all implementors.

```tml
behavior Sorted[T: Ord] {
    @post(forall(i in 0 to result.len() - 1) result[i] <= result[i + 1])
    func sort(this: mut ref This) -> ref This
}

impl Sorted[I32] for List[I32] {
    // Must satisfy the postcondition
    func sort(this: mut ref This) -> ref This {
        // ... sorting implementation
        this
    }
}
```

### 4.2 Strengthening and Weakening

| Contract Type | Subtype Rule |
|---------------|--------------|
| Precondition | MAY be weakened (accept more) |
| Postcondition | MAY be strengthened (guarantee more) |
| Invariant | MUST be maintained exactly |

```tml
behavior Container[T] {
    @pre(true)  // Accept any input
    func add(this: mut ref This, item: T)
}

// Valid: precondition is equivalent
impl Container[I32] for BoundedList[I32] {
    @pre(this.len() < this.capacity())  // Stronger, but acceptable
    func add(this: mut ref This, item: I32) { ... }
}
```

---

## 5. Old Values

Postconditions can reference pre-call values using `old()`.

```tml
@post(result == old(this.len()) + 1)
func push[T](this: mut ref Vec[T], item: T) {
    // ... implementation
}

@post(this.balance == old(this.balance) - amount)
@post(target.balance == old(target.balance) + amount)
func transfer(this: mut ref Account, target: mut ref Account, amount: U64) {
    this.balance -= amount
    target.balance += amount
}
```

**Implementation:**

`old(expr)` captures value at function entry. For complex expressions:
1. Compiler generates temporary at function start
2. Postcondition references the temporary

```tml
// Compiles to:
func push[T](this: mut ref Vec[T], item: T) {
    let __old_len = this.len()  // Capture old value

    // ... implementation

    @assert(result == __old_len + 1)  // Check postcondition
}
```

---

## 6. Purity Requirements

Contract expressions MUST be pure (no side effects):

```tml
// Valid: pure expression
@pre(x > 0 and y > 0)
func multiply(x: I32, y: I32) -> I32

// Invalid: side effect in contract
@pre(log_and_check(x))  // Compile error: contract must be pure
func process(x: I32)
```

The compiler enforces purity by checking the effect set of contract expressions.

---

## 7. Examples

### 7.1 Binary Search

```tml
@pre(forall(i in 0 to arr.len() - 1) arr[i] <= arr[i + 1])
@post(
    when result {
        Just(idx) -> arr[idx] == target,
        Nothing -> forall(i in 0 to arr.len()) arr[i] != target,
    }
)
func binary_search[T: Ord](arr: ref Array[T], target: T) -> Maybe[U64] {
    let mut low = 0
    let mut high = arr.len()

    loop low < high {
        let mid = low + (high - low) / 2
        @assert(low <= mid and mid < high)

        when arr[mid].cmp(target) {
            Less -> low = mid + 1,
            Greater -> high = mid,
            Equal -> return Just(mid),
        }
    }

    Nothing
}
```

### 7.2 Bank Account

```tml
@invariant(this.balance >= 0)
type Account = {
    id: U64,
    balance: U64,
}

impl Account {
    @pre(amount > 0)
    @post(this.balance == old(this.balance) + amount)
    func deposit(this: mut ref This, amount: U64) {
        this.balance += amount
    }

    @pre(amount > 0)
    @pre(amount <= this.balance)
    @post(this.balance == old(this.balance) - amount)
    func withdraw(this: mut ref This, amount: U64) {
        this.balance -= amount
    }

    @pre(amount > 0)
    @pre(amount <= this.balance)
    @post(this.balance == old(this.balance) - amount)
    @post(target.balance == old(target.balance) + amount)
    func transfer(this: mut ref This, target: mut ref Account, amount: U64) {
        this.withdraw(amount)
        target.deposit(amount)
    }
}
```

### 7.3 Sorted Container

```tml
@invariant(forall(i in 0 to this.len() - 1) this.items[i] <= this.items[i + 1])
type SortedList[T: Ord] = {
    items: Vec[T],
}

impl SortedList[T: Ord] {
    func new() -> This {
        This { items: Vec.new() }
    }

    @post(exists(i in 0 to this.len()) this.items[i] == item)
    func insert(this: mut ref This, item: T) {
        let pos = this.items.binary_search(item).unwrap_or_else(do(p) p)
        this.items.insert(pos, item)
    }

    @post(result == true implies exists(i in 0 to this.len()) this.items[i] == item)
    func contains(this: ref This, item: ref T) -> Bool {
        this.items.binary_search(item).is_ok()
    }
}
```

---

## 8. IR Representation

Contracts are represented in IR as metadata:

```json
{
  "kind": "func_def",
  "name": "divide",
  "contracts": {
    "pre": [
      {
        "expr": { "kind": "binary_op", "op": "!=",
                  "left": { "kind": "ident", "name": "divisor" },
                  "right": { "kind": "literal", "value": 0 } },
        "message": "divisor != 0"
      }
    ],
    "post": [],
    "effects": []
  },
  "params": [
    { "name": "dividend", "type": "I32" },
    { "name": "divisor", "type": "I32" }
  ],
  "return_type": "I32",
  "body": { ... }
}
```

---

## 9. Compatibility

- **RFC-0001**: Contracts are metadata on core IR nodes
- **RFC-0002**: `@pre`, `@post`, etc. are decorator syntax
- **RFC-0004**: Contracts can use `Outcome` types in expressions

---

## 10. Alternatives Rejected

### 10.1 In-body Contracts (Eiffel-style)

```
// Rejected
func divide(a: I32, b: I32) -> I32 {
    require b != 0
    ensure result * b == a

    a / b
}
```

Problems:
- Mixes contracts with implementation
- Harder for tools to extract
- Less visible at call site

### 10.2 Type-level Contracts Only

Dependent types can encode contracts:

```
// Rejected approach
func get(array: Array[T], index: U64 where index < array.len()) -> T
```

Problems:
- Complex type signatures
- Difficult for LLMs to generate
- Overkill for most cases

### 10.3 No Runtime Checking

Some systems only do static verification:

Problems:
- Not all properties are statically provable
- Leaves gaps in safety
- Misses bugs in complex logic

---

## 11. References

- [Design by Contract (Meyer)](https://www.eiffel.org/doc/eiffel/ET-_Design_by_Contract_%28Tm%29%2C_Assertions_and_Exceptions)
- [Dafny](https://dafny.org/) - Verification-aware language
- [SPARK Ada](https://www.adacore.com/about-spark) - Contracts for safety-critical
- [Contracts for C++](http://www.yourcompany.com/contract-c++)
