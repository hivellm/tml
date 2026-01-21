# TML Standard Library (`std`)

The TML standard library provides common data structures and I/O operations.

## Modules

### `std::collections`

Generic data structures with object-oriented API:

- **List[T]** - Generic dynamic array
- **HashMap[K, V]** - Generic key-value store
- **Buffer** - Byte buffer for binary data

```tml
use std::collections

func main() -> I32 {
    // Create list with initial values
    let numbers: List[I32] = List[I32].of3(10, 20, 30)
    print(numbers.get(0))   // 10
    print(numbers.len())    // 3

    // Add more elements
    numbers.push(40)
    numbers.insert(0, 5)    // Insert at beginning
    print(numbers.first())  // 5
    print(numbers.last())   // 40

    // Resize and reserve
    numbers.resize(10)      // Extend to 10 elements (fills with 0)
    numbers.reserve(100)    // Pre-allocate capacity

    // Manipulation
    numbers.reverse()
    let removed: I32 = numbers.remove(0)  // Remove first
    numbers.shrink_to_fit()

    numbers.destroy()

    let names: List[Str] = List[Str].default()
    names.push("Alice")
    names.push("Bob")
    names.destroy()

    // Generic HashMap
    let scores: HashMap[Str, I32] = HashMap[Str, I32].default()
    scores.set("Alice", 100)
    scores.set("Bob", 85)
    if scores.has("Alice") {
        print(scores.get("Alice"))  // 100
    }
    scores.destroy()

    // Buffer (for binary data)
    let buf: Buffer = Buffer.new(64)
    buf.write_i32(12345)
    buf.write_i64(9876543210)
    buf.reset_read()
    print(buf.read_i32())  // 12345
    print(buf.read_i64())  // 9876543210
    buf.destroy()

    0
}
```

### `std::file`

File I/O and path operations:

- **File** - File handle with read/write methods
- **Path** - Static path utilities

```tml
use std::file

func main() -> I32 {
    // Write to file (static method)
    File.write_all("hello.txt", "Hello, World!")

    // Read entire file (static method)
    let content: Str = File.read_all("hello.txt")
    print(content)

    // Object-oriented file handling
    let f: File = File.open_read("data.txt")
    if f.is_open() {
        loop (true) {
            let line: Str = f.read_line()
            if str_len(line) == 0 then break
            print(line)
        }
        f.close()
    }

    // Write with object
    let out: File = File.open_write("output.txt")
    out.write_str("Line 1\n")
    out.write_str("Line 2\n")
    out.close()

    // Path utilities (all static)
    if Path.exists("mydir") {
        Path.create_dir("mydir/subdir")
    }
    let name: Str = Path.filename("/path/to/file.txt")  // "file.txt"
    let ext: Str = Path.extension("/path/to/file.txt")  // ".txt"
    let parent: Str = Path.parent("/path/to/file.txt")  // "/path/to"
    let full: Str = Path.join("dir", "file.txt")        // "dir/file.txt"

    0
}
```

## API Reference

### List[T]

| Method | Description |
|--------|-------------|
| `List[T].new(capacity: I64) -> List[T]` | Create with initial capacity |
| `List[T].default() -> List[T]` | Create with default capacity (8) |
| `List[T].of1(a) -> List[T]` | Create with 1 initial value |
| `List[T].of2(a, b) -> List[T]` | Create with 2 initial values |
| `List[T].of3(a, b, c) -> List[T]` | Create with 3 initial values |
| `List[T].of4(a, b, c, d) -> List[T]` | Create with 4 initial values |
| `List[T].of5(a, b, c, d, e) -> List[T]` | Create with 5 initial values |
| `list.push(value: T)` | Add element to end |
| `list.pop() -> T` | Remove and return last element |
| `list.get(index: I64) -> T` | Get element at index |
| `list.set(index: I64, value: T)` | Set element at index |
| `list.first() -> T` | Get first element |
| `list.last() -> T` | Get last element |
| `list.insert(index: I64, value: T)` | Insert at index |
| `list.remove(index: I64) -> T` | Remove at index and return |
| `list.len() -> I64` | Number of elements |
| `list.capacity() -> I64` | Current capacity |
| `list.resize(new_len: I64)` | Resize list (truncate or extend with 0) |
| `list.reserve(capacity: I64)` | Pre-allocate capacity |
| `list.shrink_to_fit()` | Shrink capacity to length |
| `list.reverse()` | Reverse in place |
| `list.is_empty() -> Bool` | Check if empty |
| `list.clear()` | Remove all elements |
| `list.destroy()` | Free memory |

### HashMap[K, V]

| Method | Description |
|--------|-------------|
| `HashMap[K, V].new(capacity: I64) -> HashMap[K, V]` | Create with initial capacity |
| `HashMap[K, V].default() -> HashMap[K, V]` | Create with default capacity (16) |
| `map.set(key: K, value: V)` | Set key-value pair |
| `map.get(key: K) -> V` | Get value (default if not found) |
| `map.has(key: K) -> Bool` | Check if key exists |
| `map.remove(key: K) -> Bool` | Remove key (returns success) |
| `map.len() -> I64` | Number of entries |
| `map.clear()` | Remove all entries |
| `map.destroy()` | Free memory |

### Buffer

| Method | Description |
|--------|-------------|
| `Buffer.new(capacity: I64) -> Buffer` | Create with initial capacity |
| `Buffer.default() -> Buffer` | Create with default capacity (64) |
| `buf.write_byte(byte: I32)` | Write single byte |
| `buf.write_i32(value: I32)` | Write I32 (little-endian) |
| `buf.write_i64(value: I64)` | Write I64 (little-endian) |
| `buf.read_byte() -> I32` | Read byte (-1 if EOF) |
| `buf.read_i32() -> I32` | Read I32 (little-endian) |
| `buf.read_i64() -> I64` | Read I64 (little-endian) |
| `buf.len() -> I64` | Bytes written |
| `buf.remaining() -> I64` | Bytes left to read |
| `buf.reset_read()` | Reset read position |
| `buf.clear()` | Clear all data |
| `buf.destroy()` | Free memory |

### File

| Method | Description |
|--------|-------------|
| `File.open(path: Str, mode: I32) -> File` | Open with mode flags |
| `File.open_read(path: Str) -> File` | Open for reading |
| `File.open_write(path: Str) -> File` | Open for writing |
| `File.open_append(path: Str) -> File` | Open for appending |
| `File.read_all(path: Str) -> Str` | Read entire file |
| `File.write_all(path: Str, content: Str) -> Bool` | Write to file |
| `File.append_all(path: Str, content: Str) -> Bool` | Append to file |
| `file.is_open() -> Bool` | Check if open |
| `file.read_line() -> Str` | Read single line |
| `file.write_str(content: Str) -> Bool` | Write string |
| `file.size() -> I64` | File size in bytes |
| `file.position() -> I64` | Current position |
| `file.seek(pos: I64) -> Bool` | Seek to position |
| `file.rewind()` | Seek to beginning |
| `file.close()` | Close file |

### Path

| Method | Description |
|--------|-------------|
| `Path.exists(path: Str) -> Bool` | Check if exists |
| `Path.is_file(path: Str) -> Bool` | Check if file |
| `Path.is_dir(path: Str) -> Bool` | Check if directory |
| `Path.create_dir(path: Str) -> Bool` | Create directory |
| `Path.create_dir_all(path: Str) -> Bool` | Create with parents |
| `Path.remove(path: Str) -> Bool` | Remove file |
| `Path.remove_dir(path: Str) -> Bool` | Remove directory |
| `Path.rename(from: Str, to: Str) -> Bool` | Rename/move |
| `Path.copy(from: Str, to: Str) -> Bool` | Copy file |
| `Path.join(base: Str, child: Str) -> Str` | Join paths |
| `Path.parent(path: Str) -> Str` | Get parent directory |
| `Path.filename(path: Str) -> Str` | Get filename |
| `Path.extension(path: Str) -> Str` | Get extension |
| `Path.absolute(path: Str) -> Str` | Get absolute path |

## Building

The std package has C runtime implementations in `runtime/`:
- `collections.c` / `collections.h`
- `file.c` / `file.h`

These are linked automatically when using `use std::collections` or `use std::file`.

## Status

| Module | Status |
|--------|--------|
| collections | Implemented (generic List[T], HashMap[K,V], Buffer) |
| file | Implemented |
| string | Planned |
| fmt | Planned |
| json | Planned |
