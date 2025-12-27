# Bitwise Operations

Bitwise operations manipulate individual bits of integer values. They're
essential for low-level programming, flags, and optimization.

## Bitwise Operators

TML provides the following bitwise operators:

| Operator | Name | Description |
|----------|------|-------------|
| `&` | AND | Both bits must be 1 |
| `\|` | OR | At least one bit must be 1 |
| `^` | XOR | Exactly one bit must be 1 |
| `~` | NOT | Flip all bits |
| `<<` | Left Shift | Shift bits left |
| `>>` | Right Shift | Shift bits right |

## Bitwise AND (`&`)

Returns 1 only where both bits are 1:

```tml
func main() {
    let a = 0b1100  // 12 in binary
    let b = 0b1010  // 10 in binary
    let c = a & b   // 0b1000 = 8

    println(c)  // 8
}
```

Common use: Masking bits, checking if a bit is set.

```tml
func main() {
    let flags = 0b1010
    let mask = 0b0010

    // Check if second bit is set
    if (flags & mask) != 0 {
        println("Bit is set")
    }
}
```

## Bitwise OR (`|`)

Returns 1 where at least one bit is 1:

```tml
func main() {
    let a = 0b1100  // 12
    let b = 0b1010  // 10
    let c = a | b   // 0b1110 = 14

    println(c)  // 14
}
```

Common use: Setting bits.

```tml
func main() {
    let mut flags = 0b1000
    let bit_to_set = 0b0010

    flags = flags | bit_to_set  // 0b1010
    println(flags)  // 10
}
```

## Bitwise XOR (`^`)

Returns 1 where bits are different:

```tml
func main() {
    let a = 0b1100  // 12
    let b = 0b1010  // 10
    let c = a ^ b   // 0b0110 = 6

    println(c)  // 6
}
```

Common use: Toggling bits, simple encryption.

```tml
func main() {
    let value = 42
    let key = 0xFF

    let encrypted = value ^ key
    let decrypted = encrypted ^ key  // Back to 42

    println(encrypted)  // 213
    println(decrypted)  // 42
}
```

## Bitwise NOT (`~`)

Flips all bits (one's complement):

```tml
func main() {
    let a = 0b1100
    let b = ~a

    println(b)  // -13 (in two's complement)
}
```

## Left Shift (`<<`)

Shifts bits left, filling with zeros:

```tml
func main() {
    let a = 1
    println(a << 1)  // 2
    println(a << 2)  // 4
    println(a << 3)  // 8

    // Equivalent to multiplying by 2^n
    let b = 5
    println(b << 2)  // 20 (5 * 4)
}
```

## Right Shift (`>>`)

Shifts bits right:

```tml
func main() {
    let a = 16
    println(a >> 1)  // 8
    println(a >> 2)  // 4
    println(a >> 3)  // 2

    // Equivalent to dividing by 2^n
    let b = 100
    println(b >> 2)  // 25 (100 / 4)
}
```

## Practical Examples

### Check if Even

```tml
func is_even(n: I32) -> Bool {
    (n & 1) == 0  // Check if last bit is 0
}

func main() {
    println(is_even(4))   // true
    println(is_even(7))   // false
}
```

### Power of Two Check

```tml
func is_power_of_two(n: I32) -> Bool {
    n > 0 and (n & (n - 1)) == 0
}

func main() {
    println(is_power_of_two(8))   // true
    println(is_power_of_two(10))  // false
}
```

### Swap Without Temp Variable

```tml
func main() {
    let mut a = 5
    let mut b = 10

    a = a ^ b
    b = a ^ b
    a = a ^ b

    println(a)  // 10
    println(b)  // 5
}
```

### Bit Flags

```tml
const FLAG_READ = 0b001
const FLAG_WRITE = 0b010
const FLAG_EXECUTE = 0b100

func main() {
    let mut permissions = 0

    // Grant read and write
    permissions = permissions | FLAG_READ | FLAG_WRITE

    // Check permissions
    if (permissions & FLAG_READ) != 0 {
        println("Has read permission")
    }
    if (permissions & FLAG_EXECUTE) != 0 {
        println("Has execute permission")
    } else {
        println("No execute permission")
    }
}
```
