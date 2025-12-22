# Memory Management

TML provides explicit memory management functions for working with
heap-allocated memory. This chapter covers manual allocation, reading,
and writing memory.

## Memory Functions

TML provides these builtin memory functions:

| Function | Description |
|----------|-------------|
| `alloc(size)` | Allocate `size` bytes, returns a pointer |
| `dealloc(ptr)` | Free previously allocated memory |
| `read_i32(ptr)` | Read a 32-bit integer from pointer |
| `write_i32(ptr, value)` | Write a 32-bit integer to pointer |
| `ptr_offset(ptr, n)` | Move pointer by `n` elements |

## Basic Allocation

```tml
func main() {
    // Allocate space for one integer (4 bytes)
    let ptr = alloc(4)

    // Write a value
    write_i32(ptr, 42)

    // Read it back
    let value = read_i32(ptr)
    println(value)  // 42

    // Free the memory
    dealloc(ptr)
}
```

## Working with Arrays

Allocate space for multiple values:

```tml
func main() {
    // Allocate space for 5 integers (20 bytes)
    let arr = alloc(20)

    // Write values
    write_i32(arr, 10)
    write_i32(ptr_offset(arr, 1), 20)
    write_i32(ptr_offset(arr, 2), 30)
    write_i32(ptr_offset(arr, 3), 40)
    write_i32(ptr_offset(arr, 4), 50)

    // Calculate sum
    let mut sum = 0
    for i in 0 to 5 {
        sum = sum + read_i32(ptr_offset(arr, i))
    }
    println("Sum: ", sum)  // Sum: 150

    dealloc(arr)
}
```

## Dynamic Data Structures

Build dynamic data structures:

```tml
func main() {
    // Create a dynamic "vector"
    let capacity = 10
    let data = alloc(capacity * 4)
    let mut size = 0

    // Push some values
    write_i32(ptr_offset(data, size), 100)
    size = size + 1

    write_i32(ptr_offset(data, size), 200)
    size = size + 1

    write_i32(ptr_offset(data, size), 300)
    size = size + 1

    // Print all values
    for i in 0 to size {
        println(read_i32(ptr_offset(data, i)))
    }

    dealloc(data)
}
```

## Memoization Example

Use heap memory for caching computed values:

```tml
func main() {
    // Cache for factorial values (10 entries)
    let cache = alloc(40)

    // Initialize base cases
    write_i32(cache, 1)                    // fact(0) = 1
    write_i32(ptr_offset(cache, 1), 1)     // fact(1) = 1

    // Compute factorials 2 through 9
    for i in 2 to 10 {
        let prev = read_i32(ptr_offset(cache, i - 1))
        write_i32(ptr_offset(cache, i), prev * i)
    }

    // Print some results
    println("5! = ", read_i32(ptr_offset(cache, 5)))   // 120
    println("6! = ", read_i32(ptr_offset(cache, 6)))   // 720
    println("7! = ", read_i32(ptr_offset(cache, 7)))   // 5040

    dealloc(cache)
}
```

## Memory Safety Guidelines

1. **Always free allocated memory**: Every `alloc` should have a matching `dealloc`

2. **Don't use freed memory**: After `dealloc`, the pointer is invalid

3. **Don't free twice**: Only call `dealloc` once per allocation

4. **Respect boundaries**: Don't read/write past allocated size

```tml
func main() {
    let ptr = alloc(4)  // Only 4 bytes!

    write_i32(ptr, 10)           // OK
    // write_i32(ptr_offset(ptr, 1), 20)  // BAD: out of bounds!

    dealloc(ptr)
    // let x = read_i32(ptr)  // BAD: use after free!
}
```

## When to Use Manual Memory

Use manual memory management for:

1. **Performance-critical code**: Avoid garbage collection overhead
2. **Large data structures**: Control exactly when memory is freed
3. **Interfacing with C**: Match C's memory model
4. **Custom allocators**: Implement specialized allocation strategies

For most code, prefer stack allocation (regular variables) which is
automatically managed.
