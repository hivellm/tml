# Pointers and References

Pointers and references let you work with memory addresses directly.
They're essential for efficient data handling and interfacing with
low-level systems.

## References with `ref`

A reference is a pointer to a value. Use `ref` to create a reference:

```tml
func main() {
    let x = 42
    let ptr = ref x  // ptr points to x

    // Dereference with *
    println(*ptr)  // 42
}
```

## Mutable References

To modify the value through a reference, both the variable and reference
must be mutable:

```tml
func main() {
    let mut x = 42
    let ptr = ref x

    *ptr = 100  // Modify through pointer
    println(x)  // 100
}
```

## Pointer Arithmetic

Use `ptr_offset` to move a pointer:

```tml
func main() {
    let mem = alloc(16)  // Allocate 16 bytes (4 integers)

    // Write values at different offsets
    write_i32(mem, 10)
    write_i32(ptr_offset(mem, 1), 20)
    write_i32(ptr_offset(mem, 2), 30)
    write_i32(ptr_offset(mem, 3), 40)

    // Read them back
    println(read_i32(mem))                  // 10
    println(read_i32(ptr_offset(mem, 1)))   // 20
    println(read_i32(ptr_offset(mem, 2)))   // 30
    println(read_i32(ptr_offset(mem, 3)))   // 40

    dealloc(mem)
}
```

## Pass by Reference

Pass values by reference to avoid copying:

```tml
func increment(ptr: ref I32) {
    *ptr = *ptr + 1
}

func main() {
    let mut x = 5
    increment(ref x)
    println(x)  // 6
}
```

## Reference Rules

TML enforces safety rules for references:

1. **One mutable reference OR many immutable references** (not both)
2. **References must always be valid** (no dangling pointers)
3. **No null references** by default

```tml
func main() {
    let mut x = 5

    let r1 = ref x  // OK: first reference
    let r2 = ref x  // OK: multiple immutable refs

    // Cannot have mutable ref while immutable refs exist
    // let r3 = ref mut x  // Error!
}
```

## When to Use Pointers

Use pointers when you need to:

1. **Modify a value in a function** without returning it
2. **Work with dynamic memory** (heap allocation)
3. **Build data structures** like linked lists
4. **Interface with C code** or system APIs

## Safety Considerations

Unlike C, TML prevents common pointer errors:

- No null pointer dereferences (use `Maybe[ref T]` for optional refs)
- No use-after-free (ownership system tracks lifetimes)
- No double-free (single ownership)

For truly unsafe operations, use `lowlevel` blocks (not covered in this
introductory guide).
