# Collections

TML provides built-in support for dynamic collections: Lists (dynamic arrays), HashMaps, and Buffers.

## Arrays and Lists

### Array Literal Syntax

The simplest way to create a list is with array literal syntax:

```tml
// Create a list with initial values
let numbers = [1, 2, 3, 4, 5]

// Create an empty list
let empty = []

// Create a list with repeated values
let zeros = [0; 10]  // 10 zeros
let ones = [1; 5]    // 5 ones
```

### Indexing

Access elements using square bracket notation:

```tml
let arr = [10, 20, 30, 40, 50]

let first = arr[0]   // 10
let third = arr[2]   // 30
let last = arr[4]    // 50
```

### List Methods

TML supports method-call syntax for list operations:

```tml
let arr = [1, 2, 3]

// Get length
let len = arr.len()        // 3

// Get element at index
let val = arr.get(0)       // 1

// Set element at index
arr.set(0, 100)            // arr[0] is now 100

// Add element to end
arr.push(4)                // arr is now [100, 2, 3, 4]

// Remove and return last element
let last = arr.pop()       // returns 4

// Check if empty
let is_empty = list_is_empty(arr)  // false

// Get capacity
let cap = arr.capacity()   // internal buffer size

// Clear all elements
arr.clear()                // arr is now []
```

### Function-Call Syntax

You can also use function-call syntax:

```tml
let list = list_create(4)  // Create with capacity 4

list_push(list, 10)
list_push(list, 20)

let len = list_len(list)          // 2
let first = list_get(list, 0)     // 10

list_set(list, 0, 100)

let popped = list_pop(list)       // 20
list_clear(list)

list_destroy(list)  // Clean up when done
```

### Example: Sum of Elements

```tml
func sum_list() -> I32 {
    let arr = [10, 20, 30, 40, 50]

    let mut total = 0
    for i in 5 {
        total = total + arr[i]
    }

    list_destroy(arr)
    return total  // 150
}
```

## HashMap

HashMaps store key-value pairs with O(1) average lookup time.

### Creating a HashMap

```tml
let map = hashmap_create(16)  // Create with capacity 16
```

### HashMap Operations

```tml
let map = hashmap_create(16)

// Set values
hashmap_set(map, 1, 100)
hashmap_set(map, 2, 200)
hashmap_set(map, 3, 300)

// Get values
let val = hashmap_get(map, 1)  // 100

// Check if key exists
let exists = hashmap_has(map, 2)  // true

// Get number of entries
let len = hashmap_len(map)  // 3

// Remove a key
let removed = hashmap_remove(map, 1)  // true

// Clear all entries
hashmap_clear(map)

// Clean up
hashmap_destroy(map)
```

### Example: Counting Occurrences

```tml
func count_values() {
    let counts = hashmap_create(16)

    // Count some values (key = value, count = count)
    hashmap_set(counts, 1, 3)   // value 1 appears 3 times
    hashmap_set(counts, 2, 5)   // value 2 appears 5 times
    hashmap_set(counts, 3, 2)   // value 3 appears 2 times

    let has_key = hashmap_has(counts, 2)
    if has_key {
        let count = hashmap_get(counts, 2)
        println(count)  // 5
    }

    hashmap_destroy(counts)
}
```

## Buffer

Buffers are byte arrays useful for I/O and binary data.

### Creating a Buffer

```tml
let buf = buffer_create(32)  // Create with capacity 32 bytes
```

### Buffer Operations

```tml
let buf = buffer_create(32)

// Write bytes
buffer_write_byte(buf, 65)  // Write 'A' (ASCII 65)
buffer_write_byte(buf, 66)  // Write 'B'
buffer_write_byte(buf, 67)  // Write 'C'

// Write 32-bit integer
buffer_write_i32(buf, 12345678)

// Get buffer info
let len = buffer_len(buf)           // Number of bytes written
let cap = buffer_capacity(buf)      // Total capacity
let rem = buffer_remaining(buf)     // Bytes left to read

// Read bytes (advances read position)
let b1 = buffer_read_byte(buf)      // 65
let b2 = buffer_read_byte(buf)      // 66

// Read 32-bit integer
let val = buffer_read_i32(buf)

// Reset read position to beginning
buffer_reset_read(buf)

// Clear buffer
buffer_clear(buf)

// Clean up
buffer_destroy(buf)
```

### Example: Binary Protocol

```tml
func encode_message() {
    let buf = buffer_create(64)

    // Write header (message type = 1)
    buffer_write_byte(buf, 1)

    // Write payload length
    buffer_write_i32(buf, 100)

    // Write checksum
    buffer_write_i32(buf, 12345)

    // Now read it back
    let msg_type = buffer_read_byte(buf)
    let length = buffer_read_i32(buf)
    let checksum = buffer_read_i32(buf)

    println(msg_type)   // 1
    println(length)     // 100
    println(checksum)   // 12345

    buffer_destroy(buf)
}
```

## Memory Management

All collections allocate memory that must be freed:

```tml
let arr = [1, 2, 3]
// ... use the array ...
list_destroy(arr)  // Free memory

let map = hashmap_create(16)
// ... use the map ...
hashmap_destroy(map)  // Free memory

let buf = buffer_create(32)
// ... use the buffer ...
buffer_destroy(buf)  // Free memory
```

**Important:** Always call the appropriate `destroy` function when you're done with a collection to avoid memory leaks.
