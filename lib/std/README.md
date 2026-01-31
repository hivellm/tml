# TML Standard Library (`std`)

The TML standard library provides common data structures, I/O operations, networking, threading, and more.

**Status**: 694 tests passing

## Modules

### `std::collections`

Generic data structures with object-oriented API:

- **`List[T]`** - Generic dynamic array (Vec equivalent)
- **`HashMap[K, V]`** - Generic key-value store
- **`HashSet[T]`** - Unique value set
- **`Buffer`** - Byte buffer for binary data
- **`Deque[T]`** - Double-ended queue
- **`BinaryHeap[T]`** - Priority queue

```tml
use std::collections::{List, HashMap}

func main() -> I32 {
    // Generic List
    var numbers: List[I32] = List[I32]::new()
    numbers.push(10)
    numbers.push(20)
    numbers.push(30)
    println(numbers.get(0).to_string())  // 10
    println(numbers.len().to_string())   // 3

    // Generic HashMap
    var scores: HashMap[Str, I32] = HashMap[Str, I32]::new()
    scores.insert("Alice", 100)
    scores.insert("Bob", 85)
    when scores.get("Alice") {
        Just(score) => println(score.to_string()),
        Nothing => println("Not found")
    }

    0
}
```

### `std::file`

File I/O and path operations:

- **`File`** - File handle with read/write methods
- **`Path`** - Path utilities
- **`FileReader`** / **`FileWriter`** - Buffered I/O

```tml
use std::file::{File, Path}

func main() -> I32 {
    // Write to file
    File::write_all("hello.txt", "Hello, World!")

    // Read entire file
    let content: Str = File::read_all("hello.txt")
    println(content)

    // Object-oriented file handling
    var f: File = File::open_read("data.txt")
    if f.is_open() {
        loop {
            let line: Str = f.read_line()
            if line.is_empty() then break
            println(line)
        }
        f.close()
    }

    // Path utilities
    if Path::exists("mydir") {
        Path::create_dir("mydir/subdir")
    }
    let name: Str = Path::filename("/path/to/file.txt")  // "file.txt"
    let ext: Str = Path::extension("/path/to/file.txt")  // ".txt"

    0
}
```

### `std::net`

Networking types and operations:

- **`IpAddr`** - IP address (v4 or v6)
- **`Ipv4Addr`** - IPv4 address
- **`Ipv6Addr`** - IPv6 address
- **`SocketAddr`** - Socket address (IP + port)
- **`SocketAddrV4`** / **`SocketAddrV6`** - Typed socket addresses

```tml
use std::net::{Ipv4Addr, SocketAddrV4}

let localhost: Ipv4Addr = Ipv4Addr::LOCALHOST()
let addr: SocketAddrV4 = SocketAddrV4::new(localhost, 8080 as U16)

assert(localhost.is_loopback())
assert(addr.port() == 8080 as U16)

// IPv4 address classification
let private: Ipv4Addr = Ipv4Addr::new(192, 168, 1, 1)
assert(private.is_private())
```

### `std::sync`

Synchronization primitives for thread-safe code:

- **`Mutex[T]`** - Mutual exclusion lock
- **`MutexGuard[T]`** - RAII lock guard
- **`RwLock[T]`** - Reader-writer lock
- **`RwLockReadGuard[T]`** / **`RwLockWriteGuard[T]`** - RAII guards
- **`Condvar`** - Condition variable
- **`Barrier`** - Thread barrier
- **`Once`** - One-time initialization

```tml
use std::sync::{Mutex, MutexGuard}

let mutex: Mutex[I32] = Mutex::new(42)

{
    var guard: MutexGuard[I32] = mutex.lock()
    *guard.get_mut() = 100
}  // guard dropped, lock released

{
    let guard: MutexGuard[I32] = mutex.lock()
    assert_eq(*guard.get(), 100)
}
```

### `std::sync::mpsc`

Multi-producer, single-consumer channels:

- **`Sender[T]`** - Channel sender (clonable)
- **`Receiver[T]`** - Channel receiver
- `channel[T]()` - Create unbounded channel

```tml
use std::sync::mpsc::{channel, Sender, Receiver}

let (tx, rx): (Sender[I32], Receiver[I32]) = channel[I32]()

tx.send(42)
when rx.recv() {
    Just(value) => println("Received: " + value.to_string()),
    Nothing => println("Channel closed")
}
```

### `std::thread`

Threading support:

- **`Thread`** - Thread handle
- **`JoinHandle[T]`** - Handle for joining threads
- `spawn()` - Spawn new thread
- `sleep()` - Sleep current thread
- `current()` - Get current thread
- `yield_now()` - Yield to scheduler

```tml
use std::thread::{spawn, sleep, JoinHandle}
use std::time::Duration

let handle: JoinHandle[I32] = spawn(do() -> I32 {
    sleep(Duration::from_millis(100))
    return 42
})

let result: I32 = handle.join()
println("Thread returned: " + result.to_string())
```

### `std::iter`

Extended iterator utilities:

- Additional iterator adapters beyond core
- Parallel iteration support

### `std::json`

JSON parsing and serialization:

- **`JsonValue`** - JSON value type
- **`JsonObject`** - JSON object
- **`JsonArray`** - JSON array
- `parse()` - Parse JSON string
- `stringify()` - Convert to JSON string

```tml
use std::json::{JsonValue, parse, stringify}

let json: JsonValue = parse("{\"name\": \"Alice\", \"age\": 30}")
when json.get("name") {
    Just(name) => println(name.as_string()),
    Nothing => println("Not found")
}
```

### `std::text`

Text processing utilities:

- **`StringBuilder`** - Efficient string building
- String formatting and interpolation
- Text encoding/decoding

### `std::types`

Extended type utilities:

- **`Cow[T]`** - Clone-on-write
- Additional wrapper types

### `std::traits`

Extended behavior implementations:

- Common behavior implementations for std types

### `std::profiler`

Performance profiling:

- **`Profiler`** - Code profiler
- `start()` / `stop()` - Profile sections
- Timing and statistics

### `std::exception`

Exception handling:

- **`Exception`** - Base exception type
- Stack traces and error context

### `std::interfaces`

Common interfaces:

- **`Readable`** - Read interface
- **`Writable`** - Write interface
- **`Closeable`** - Resource cleanup

### `std::object`

Object-oriented utilities:

- Runtime type information
- Object comparison

## API Reference

### List[T]

| Method | Description |
|--------|-------------|
| `List[T]::new() -> List[T]` | Create empty list |
| `List[T]::with_capacity(n) -> List[T]` | Create with capacity |
| `list.push(value: T)` | Add element to end |
| `list.pop() -> Maybe[T]` | Remove and return last element |
| `list.get(index: I64) -> Maybe[T]` | Get element at index |
| `list.set(index: I64, value: T)` | Set element at index |
| `list.first() -> Maybe[T]` | Get first element |
| `list.last() -> Maybe[T]` | Get last element |
| `list.insert(index: I64, value: T)` | Insert at index |
| `list.remove(index: I64) -> T` | Remove at index and return |
| `list.len() -> I64` | Number of elements |
| `list.capacity() -> I64` | Current capacity |
| `list.reserve(capacity: I64)` | Pre-allocate capacity |
| `list.shrink_to_fit()` | Shrink capacity to length |
| `list.reverse()` | Reverse in place |
| `list.is_empty() -> Bool` | Check if empty |
| `list.clear()` | Remove all elements |
| `list.iter() -> ListIter[T]` | Get iterator |

### HashMap[K, V]

| Method | Description |
|--------|-------------|
| `HashMap[K, V]::new() -> HashMap[K, V]` | Create empty map |
| `map.insert(key: K, value: V)` | Insert key-value pair |
| `map.get(key: K) -> Maybe[V]` | Get value by key |
| `map.contains_key(key: K) -> Bool` | Check if key exists |
| `map.remove(key: K) -> Maybe[V]` | Remove and return value |
| `map.len() -> I64` | Number of entries |
| `map.clear()` | Remove all entries |
| `map.keys() -> Iterator[K]` | Iterate keys |
| `map.values() -> Iterator[V]` | Iterate values |
| `map.iter() -> Iterator[(K, V)]` | Iterate entries |

### Mutex[T]

| Method | Description |
|--------|-------------|
| `Mutex::new(value: T) -> Mutex[T]` | Create new mutex |
| `mutex.lock() -> MutexGuard[T]` | Acquire lock (blocking) |
| `mutex.try_lock() -> Maybe[MutexGuard[T]]` | Try to acquire lock |
| `mutex.is_locked() -> Bool` | Check if locked |
| `mutex.into_inner() -> T` | Consume mutex, get value |
| `mutex.get_mut() -> mut ref T` | Get mutable ref (requires exclusive access) |

### File

| Method | Description |
|--------|-------------|
| `File::open(path: Str, mode: I32) -> File` | Open with mode flags |
| `File::open_read(path: Str) -> File` | Open for reading |
| `File::open_write(path: Str) -> File` | Open for writing |
| `File::open_append(path: Str) -> File` | Open for appending |
| `File::read_all(path: Str) -> Str` | Read entire file |
| `File::write_all(path: Str, content: Str) -> Bool` | Write to file |
| `file.is_open() -> Bool` | Check if open |
| `file.read_line() -> Str` | Read single line |
| `file.write_str(content: Str) -> Bool` | Write string |
| `file.size() -> I64` | File size in bytes |
| `file.seek(pos: I64) -> Bool` | Seek to position |
| `file.close()` | Close file |

## Runtime

The std library includes C runtime implementations in `runtime/`:

- `collections.c` - Collection data structures
- `file.c` - File I/O operations
- `sync.c` - Synchronization primitives
- `thread.c` - Threading support
- `net.c` - Networking

These are linked automatically when importing std modules.

## Module Status

| Module | Status |
|--------|--------|
| collections | Implemented (List, HashMap, HashSet, Buffer, Deque) |
| file | Implemented |
| net | Implemented (IpAddr, SocketAddr) |
| sync | Implemented (Mutex, RwLock, MPSC channels) |
| thread | Implemented |
| json | Implemented |
| text | Implemented |
| profiler | Implemented |
| iter | Implemented |
| types | Implemented |
| traits | Implemented |
| exception | Implemented |
| interfaces | Implemented |
| object | Implemented |
