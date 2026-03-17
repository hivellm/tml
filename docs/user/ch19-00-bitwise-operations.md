# Chapter 19 — Bitwise Operations

Bitwise operations work directly on the binary representation of integer values.
They are useful for systems programming, compact flags, protocol encoding, and
performance-critical paths where arithmetic operations would be slower.

## Bitwise Operators

TML provides the full set of bitwise operators on all integer types (`I8`, `I16`,
`I32`, `I64`, `U8`, `U16`, `U32`, `U64`, `ISize`, `USize`):

| Operator | Name | Description |
|----------|------|-------------|
| `&` | AND | Output bit is 1 only where both input bits are 1 |
| `\|` | OR | Output bit is 1 where at least one input bit is 1 |
| `^` | XOR | Output bit is 1 where exactly one input bit is 1 |
| `~` | NOT | Flips every bit (bitwise complement) |
| `<<` | Left Shift | Shifts bits toward the most-significant position |
| `>>` | Right Shift | Shifts bits toward the least-significant position |

Note: `|` is the bitwise OR operator. TML uses `or` (the keyword) for logical OR
so that `|` is unambiguous.

## Bitwise AND

Bitwise AND produces a 1 in each position where both operands have a 1:

```tml
func main() {
    let a: I32 = 0b1100  // 12
    let b: I32 = 0b1010  // 10
    let c: I32 = a & b   // 0b1000 = 8
    println(c)           // 8
}
```

The most common use of AND is masking — extracting specific bits from a value:

```tml
func main() {
    let status: I32 = 0b1010_1100
    let lower_nibble_mask: I32 = 0b0000_1111

    let lower: I32 = status & lower_nibble_mask  // 0b0000_1100 = 12
    println(lower)  // 12
}
```

Testing whether a specific bit is set:

```tml
func has_flag(value: I32, flag: I32) -> Bool {
    (value & flag) != 0
}

func main() {
    let permissions: I32 = 0b0110
    println(has_flag(permissions, 0b0100))  // true
    println(has_flag(permissions, 0b0001))  // false
}
```

## Bitwise OR

Bitwise OR produces a 1 where at least one operand has a 1:

```tml
func main() {
    let a: I32 = 0b1100  // 12
    let b: I32 = 0b1010  // 10
    let c: I32 = a | b   // 0b1110 = 14
    println(c)           // 14
}
```

OR is the standard way to set (enable) a flag bit:

```tml
func main() {
    var mode: I32 = 0b0000
    let read_bit: I32  = 0b0001
    let write_bit: I32 = 0b0010

    mode = mode | read_bit   // enable read
    mode = mode | write_bit  // enable write
    println(mode)            // 3 (0b0011)
}
```

## Bitwise XOR

Bitwise XOR produces a 1 where the two operand bits differ:

```tml
func main() {
    let a: I32 = 0b1100  // 12
    let b: I32 = 0b1010  // 10
    let c: I32 = a ^ b   // 0b0110 = 6
    println(c)           // 6
}
```

XOR is used to toggle bits — applying it twice restores the original value:

```tml
func main() {
    var value: I32 = 0b1011
    let toggle_mask: I32 = 0b0110

    value = value ^ toggle_mask  // 0b1101
    value = value ^ toggle_mask  // 0b1011 — back to original
    println(value)               // 11
}
```

A classic property of XOR: `a ^ b ^ b == a`. This makes XOR useful for simple
reversible transformations:

```tml
func main() {
    let original: I32 = 42
    let key: I32 = 0x5A

    let encoded: I32 = original ^ key  // 112
    let decoded: I32 = encoded ^ key   // 42 — recovered

    println(decoded)  // 42
}
```

## Bitwise NOT

The NOT operator (`~`) flips every bit, producing the bitwise complement. For
signed integers, this is equivalent to `-(n + 1)` in two's complement:

```tml
func main() {
    let a: I32 = 0b0000_1111  // 15
    let b: I32 = ~a           // 0b1111_0000 = -16 (signed two's complement)
    println(b)  // -16
}
```

NOT combined with AND is used to clear (disable) a specific flag bit:

```tml
func clear_flag(value: I32, flag: I32) -> I32 {
    value & ~flag
}

func main() {
    var mode: I32 = 0b0111  // read + write + execute
    let write_bit: I32 = 0b0010

    mode = clear_flag(mode, write_bit)
    println(mode)  // 5 (0b0101 — write removed)
}
```

## Left Shift

Left shift (`<<`) moves bits toward the most-significant position. Bits shifted
off the top are discarded; zeros fill from the right. Each shift by one position
doubles the value (equivalent to multiplying by 2):

```tml
func main() {
    let a: I32 = 1
    println(a << 0)  // 1
    println(a << 1)  // 2
    println(a << 2)  // 4
    println(a << 3)  // 8
    println(a << 4)  // 16

    // Shift with a value other than 1
    let b: I32 = 3
    println(b << 3)  // 24 (3 * 8)
}
```

Left shift is often used to construct bitmask constants:

```tml
const BIT_0: I32 = 1 << 0   // 0b0000_0001
const BIT_1: I32 = 1 << 1   // 0b0000_0010
const BIT_4: I32 = 1 << 4   // 0b0001_0000
const BIT_7: I32 = 1 << 7   // 0b1000_0000
```

## Right Shift

Right shift (`>>`) moves bits toward the least-significant position. For unsigned
types, zeros fill from the left (logical shift). For signed types, TML performs
an arithmetic shift: the sign bit is replicated, preserving the sign of negative
numbers.

```tml
func main() {
    let a: I32 = 64
    println(a >> 1)  // 32
    println(a >> 2)  // 16
    println(a >> 3)  // 8

    // Division by powers of two
    let b: I32 = 100
    println(b >> 2)  // 25  (100 / 4)
}
```

Arithmetic right shift on negative values:

```tml
func main() {
    let a: I32 = -8
    println(a >> 1)  // -4  (sign bit preserved)
    println(a >> 2)  // -2
}
```

For unsigned types, the shift is always logical:

```tml
func main() {
    let a: U32 = 0xFF00_0000 as U32
    println(a >> 8)  // 0x00FF_0000 — zeros fill from left
}
```

## Compound Assignment Operators

All bitwise operators have compound assignment forms that modify a variable
in place:

```tml
func main() {
    var flags: I32 = 0b1010

    flags &= 0b1100  // AND in place:  0b1000
    flags |= 0b0011  // OR in place:   0b1011
    flags ^= 0b0101  // XOR in place:  0b1110
    flags <<= 1      // shift left:    0b1_1100
    flags >>= 2      // shift right:   0b0111
}
```

## Practical Patterns

### Check Whether a Number Is Even or Odd

The lowest bit of an integer is 0 for even numbers and 1 for odd numbers:

```tml
func is_even(n: I32) -> Bool {
    (n & 1) == 0
}

func is_odd(n: I32) -> Bool {
    (n & 1) == 1
}

func main() {
    println(is_even(4))   // true
    println(is_even(7))   // false
    println(is_odd(9))    // true
}
```

### Check Whether a Number Is a Power of Two

A positive power of two has exactly one bit set. Subtracting 1 flips all lower
bits. AND-ing them together produces zero only for powers of two:

```tml
func is_power_of_two(n: I32) -> Bool {
    n > 0 and (n & (n - 1)) == 0
}

func main() {
    println(is_power_of_two(1))   // true
    println(is_power_of_two(8))   // true
    println(is_power_of_two(10))  // false
    println(is_power_of_two(0))   // false
}
```

### Round Up to the Next Power of Two

```tml
func next_power_of_two(n: U32) -> U32 {
    if n == 0 as U32 { return 1 as U32 }
    var v: U32 = n - 1 as U32
    v = v | (v >> 1 as U32)
    v = v | (v >> 2 as U32)
    v = v | (v >> 4 as U32)
    v = v | (v >> 8 as U32)
    v = v | (v >> 16 as U32)
    v + 1 as U32
}

func main() {
    println(next_power_of_two(5 as U32))   // 8
    println(next_power_of_two(16 as U32))  // 16
    println(next_power_of_two(17 as U32))  // 32
}
```

### Bit Flags for Permission and State

A common systems pattern: pack multiple boolean flags into a single integer.

```tml
const PERM_READ:    I32 = 1 << 0  // 0b001
const PERM_WRITE:   I32 = 1 << 1  // 0b010
const PERM_EXECUTE: I32 = 1 << 2  // 0b100

func grant(perms: I32, flag: I32)  -> I32 { perms | flag }
func revoke(perms: I32, flag: I32) -> I32 { perms & ~flag }
func has(perms: I32, flag: I32)    -> Bool { (perms & flag) != 0 }

func main() {
    var perms: I32 = 0

    perms = grant(perms, PERM_READ)
    perms = grant(perms, PERM_WRITE)

    println(has(perms, PERM_READ))     // true
    println(has(perms, PERM_EXECUTE))  // false

    perms = revoke(perms, PERM_WRITE)
    println(has(perms, PERM_WRITE))    // false
}
```

### Extracting and Inserting Bit Fields

To read a field of `width` bits starting at `offset`:

```tml
func extract_bits(value: I32, offset: I32, width: I32) -> I32 {
    let mask: I32 = (1 << width) - 1
    (value >> offset) & mask
}

func insert_bits(value: I32, field: I32, offset: I32, width: I32) -> I32 {
    let mask: I32 = ((1 << width) - 1) << offset
    (value & ~mask) | ((field << offset) & mask)
}

func main() {
    // Packed register: [7:4] = high nibble, [3:0] = low nibble
    let reg: I32 = 0b1010_0110  // 166

    let low:  I32 = extract_bits(reg, 0, 4)  // 6
    let high: I32 = extract_bits(reg, 4, 4)  // 10

    println(low)   // 6
    println(high)  // 10

    let updated: I32 = insert_bits(reg, 0b1111, 0, 4)  // replace low nibble
    println(updated)  // 0b1010_1111 = 175
}
```

### Population Count (Count Set Bits)

Count the number of 1-bits in an integer — useful in data compression, error
correction, and similarity measures:

```tml
func popcount(n: I32) -> I32 {
    var x: I32 = n
    x = x - ((x >> 1) & 0x55555555)
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333)
    x = (x + (x >> 4)) & 0x0F0F0F0F
    (x * 0x01010101) >> 24
}

func main() {
    println(popcount(0b0000_1111))  // 4
    println(popcount(0b1010_1010))  // 4
    println(popcount(0xFFFFFFFF))   // 32
}
```

## Operator Precedence

Bitwise operators have lower precedence than arithmetic but higher than
comparison. Use parentheses whenever mixing them with other operators to
make intent explicit:

```tml
func main() {
    // Without parentheses, & binds tighter than ==
    // but it is clearer to make it explicit:
    let flags: I32 = 0b1010
    let mask: I32  = 0b0010

    if (flags & mask) != 0 {
        println("flag is set")
    }

    // Precedence table (high to low):
    // ~  (prefix)
    // << >>
    // &
    // ^
    // |
}
```

---

*Previous: [Chapter 18 — Conditional Compilation](ch18-00-conditional-compilation.md)*
*Next: [Chapter 20 — Standard Library](ch20-00-standard-library.md)*
