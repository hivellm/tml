# TML Standard Library: Glob

> `std::glob` — High-performance glob pattern matching with filesystem search.

## Overview

The `std::glob` module provides Unix-style glob pattern matching for both in-memory string matching and filesystem directory traversal. Supports wildcards, character classes, recursive descent, and brace expansion.

## Import

```tml
use std::glob
use std::glob::{Glob, matches, find_all}
```

---

## Pattern Syntax

| Pattern | Matches | Example |
|---------|---------|---------|
| `*` | Any characters within a path segment | `*.tml` matches `main.tml` |
| `?` | Exactly one character | `file?.txt` matches `file1.txt` |
| `**` | Any number of directories (recursive) | `src/**/*.tml` matches `src/a/b/c.tml` |
| `[abc]` | Any one of the listed characters | `[abc].txt` matches `a.txt` |
| `[a-z]` | Any character in the range | `[0-9].log` matches `5.log` |
| `[!abc]` | Any character NOT listed | `[!0-9].txt` matches `a.txt` |
| `{a,b}` | Either alternative (brace expansion) | `*.{tml,rs}` matches `main.tml` or `lib.rs` |

---

## Types

### Glob

An iterator over filesystem entries matching a glob pattern.

```tml
pub type Glob {
    handle: *Unit,
}

impl Glob {
    /// Finds all files matching `pattern` under `base_dir`.
    /// Returns a Glob iterator. Call `next()` to retrieve results.
    pub func find(base_dir: Str, pattern: Str) -> Glob

    /// Returns the next matching path, or an empty string when done.
    pub func next(mut this) -> Str

    /// Returns the total number of matches (exhausts the iterator).
    pub func count(mut this) -> I64

    /// Frees the internal iterator resources.
    pub func free(mut this)
}
```

---

## Free Functions

### matches

Tests whether a string matches a glob pattern. Does not access the filesystem.

```tml
/// Returns true if `text` matches the glob `pattern`.
pub func matches(pattern: Str, text: Str) -> Bool
```

### find_all

Convenience function that returns all matches as a single newline-separated string.

```tml
/// Returns all matching paths under `base_dir` as a newline-separated string.
pub func find_all(base_dir: Str, pattern: Str) -> Str
```

---

## Example

```tml
use std::glob::{Glob, matches, find_all}

func main() {
    // In-memory pattern matching (no filesystem)
    assert(matches("*.tml", "hello.tml"))
    assert(matches("src/**/*.rs", "src/lib/core.rs"))
    assert(not matches("*.txt", "hello.tml"))

    // Filesystem search with iterator
    var glob = Glob::find("./lib", "**/*.test.tml")
    loop {
        let path = glob.next()
        if path == "" { break }
        print("Found test: {path}\n")
    }
    glob.free()

    // Convenience: get all matches at once
    let all = find_all("./src", "*.tml")
    print("All source files:\n{all}\n")
}
```

---

## See Also

- [std::file](./05-FILE.md) — File system operations
- [core::str](./01-STR.md) — String matching utilities
