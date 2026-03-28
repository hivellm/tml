# Spec: Path Module

## Overview

Cross-platform path manipulation with borrowed (`Path`) and owned (`PathBuf`) types.

## Types

### Path

Borrowed path slice (like `Str`).

```tml
pub type Path {
    inner: [U8],
}

extend Path {
    /// Create from string
    pub func new(s: Str) -> ref Path {
        // Safety: Path has same layout as [U8]
        lowlevel { transmute(s.as_bytes()) }
    }

    /// Convert to string slice
    pub func as_str(this) -> Str {
        Str::from_utf8(ref this.inner).unwrap_or("")
    }

    /// Convert to owned PathBuf
    pub func to_path_buf(this) -> PathBuf {
        PathBuf { inner: Vec::from(ref this.inner) }
    }

    /// Get parent directory
    pub func parent(this) -> Maybe[ref Path] {
        // Find last separator, return path up to it
    }

    /// Get file name (last component)
    pub func file_name(this) -> Maybe[Str] {
        // Find last separator, return after it
    }

    /// Get file stem (file name without extension)
    pub func file_stem(this) -> Maybe[Str] {
        when this.file_name() {
            Just(name) => {
                when name.rfind('.') {
                    Just(i) if i > 0 => Just(name[0 to i]),
                    _ => Just(name),
                }
            },
            Nothing => Nothing,
        }
    }

    /// Get extension
    pub func extension(this) -> Maybe[Str] {
        when this.file_name() {
            Just(name) => {
                when name.rfind('.') {
                    Just(i) if i > 0 => Just(name[i + 1 to]),
                    _ => Nothing,
                }
            },
            Nothing => Nothing,
        }
    }

    /// Check if path is absolute
    pub func is_absolute(this) -> Bool {
        #if WINDOWS
            // Check for drive letter or UNC path
            this.inner.len() >= 2 and (
                (this.inner[1] == b':') or
                (this.inner[0] == b'\\' and this.inner[1] == b'\\')
            )
        #else
            this.inner.len() > 0 and this.inner[0] == b'/'
        #endif
    }

    /// Check if path is relative
    pub func is_relative(this) -> Bool {
        not this.is_absolute()
    }

    /// Join with another path
    pub func join(this, path: impl AsRef[Path]) -> PathBuf {
        let mut result = this.to_path_buf()
        result.push(path)
        result
    }

    /// Replace extension
    pub func with_extension(this, extension: Str) -> PathBuf {
        let mut result = this.to_path_buf()
        result.set_extension(extension)
        result
    }

    /// Replace file name
    pub func with_file_name(this, file_name: Str) -> PathBuf {
        let mut result = this.to_path_buf()
        result.set_file_name(file_name)
        result
    }

    /// Check if file exists
    pub func exists(this) -> Bool
    effects: [io::file.read]

    /// Check if path is a file
    pub func is_file(this) -> Bool
    effects: [io::file.read]

    /// Check if path is a directory
    pub func is_dir(this) -> Bool
    effects: [io::file.read]

    /// Iterate over path components
    pub func components(this) -> Components {
        Components::new(this)
    }

    /// Iterate over ancestors
    pub func ancestors(this) -> Ancestors {
        Ancestors { next: Just(this) }
    }

    /// Check if path starts with prefix
    pub func starts_with(this, base: impl AsRef[Path]) -> Bool

    /// Check if path ends with suffix
    pub func ends_with(this, child: impl AsRef[Path]) -> Bool

    /// Strip prefix if present
    pub func strip_prefix(this, base: impl AsRef[Path]) -> Outcome[ref Path, StripPrefixError]
}
```

### PathBuf

Owned path (like `Text`).

```tml
pub type PathBuf {
    inner: Vec[U8],
}

extend PathBuf {
    /// Create empty path
    pub func new() -> PathBuf {
        PathBuf { inner: Vec::new() }
    }

    /// Create with capacity
    pub func with_capacity(capacity: U64) -> PathBuf {
        PathBuf { inner: Vec::with_capacity(capacity) }
    }

    /// Create from string
    pub func from(s: impl Into[Text]) -> PathBuf {
        PathBuf { inner: Vec::from(s.into().as_bytes()) }
    }

    /// Borrow as Path
    pub func as_path(this) -> ref Path {
        lowlevel { transmute(ref this.inner[..]) }
    }

    /// Push path component
    pub func push(this, path: impl AsRef[Path]) {
        let path = path.as_ref()

        // If path is absolute, replace current
        if path.is_absolute() {
            this.inner.clear()
            this.inner.extend_from_slice(ref path.inner)
            return
        }

        // Add separator if needed
        if this.inner.len() > 0 {
            let last = this.inner[this.inner.len() - 1]
            if last != MAIN_SEPARATOR as U8 {
                this.inner.push(MAIN_SEPARATOR as U8)
            }
        }

        this.inner.extend_from_slice(ref path.inner)
    }

    /// Pop last component
    pub func pop(this) -> Bool {
        when this.parent() {
            Just(parent) => {
                this.inner.truncate(parent.inner.len())
                true
            },
            Nothing => false,
        }
    }

    /// Set file name (replace last component)
    pub func set_file_name(this, file_name: Str) {
        if this.file_name().is_some() {
            this.pop()
        }
        this.push(file_name)
    }

    /// Set extension
    pub func set_extension(this, extension: Str) -> Bool {
        when this.file_stem() {
            Just(stem) => {
                let parent_len = this.parent().map(do(p) p.inner.len()).unwrap_or(0)
                this.inner.truncate(parent_len)
                if parent_len > 0 {
                    this.inner.push(MAIN_SEPARATOR as U8)
                }
                this.inner.extend_from_slice(stem.as_bytes())
                if extension.len() > 0 {
                    this.inner.push(b'.')
                    this.inner.extend_from_slice(extension.as_bytes())
                }
                true
            },
            Nothing => false,
        }
    }

    /// Clear path
    pub func clear(this) {
        this.inner.clear()
    }

    /// Reserve capacity
    pub func reserve(this, additional: U64) {
        this.inner.reserve(additional)
    }

    /// Into raw bytes
    pub func into_bytes(this) -> Vec[U8] {
        this.inner
    }
}

// Delegate Path methods
extend PathBuf {
    pub func parent(this) -> Maybe[ref Path] { this.as_path().parent() }
    pub func file_name(this) -> Maybe[Str] { this.as_path().file_name() }
    pub func file_stem(this) -> Maybe[Str] { this.as_path().file_stem() }
    pub func extension(this) -> Maybe[Str] { this.as_path().extension() }
    pub func is_absolute(this) -> Bool { this.as_path().is_absolute() }
    pub func is_relative(this) -> Bool { this.as_path().is_relative() }
    pub func exists(this) -> Bool { this.as_path().exists() }
    pub func is_file(this) -> Bool { this.as_path().is_file() }
    pub func is_dir(this) -> Bool { this.as_path().is_dir() }
}
```

### Components

Path component iterator.

```tml
pub type Component =
    | Prefix(PrefixComponent)  // Windows only: C:, \\server\share
    | RootDir                  // / or \
    | CurDir                   // .
    | ParentDir                // ..
    | Normal(Str)              // Regular component

pub type Components {
    path: ref Path,
    pos: U64,
    back_pos: U64,
}

extend Components {
    func new(path: ref Path) -> Components {
        Components {
            path: path,
            pos: 0,
            back_pos: path.inner.len(),
        }
    }

    pub func as_path(this) -> ref Path {
        lowlevel { transmute(ref this.path.inner[this.pos to this.back_pos]) }
    }
}

extend Components with Iterator {
    type Item = Component

    func next(this) -> Maybe[Component] {
        // Parse next component from front
    }
}

extend Components with DoubleEndedIterator {
    func next_back(this) -> Maybe[Component] {
        // Parse next component from back
    }
}
```

### Ancestors

Ancestor path iterator.

```tml
pub type Ancestors {
    next: Maybe[ref Path],
}

extend Ancestors with Iterator {
    type Item = ref Path

    func next(this) -> Maybe[ref Path] {
        let current = this.next?
        this.next = current.parent()
        Just(current)
    }
}
```

## Platform Constants

```tml
#if WINDOWS
    pub const MAIN_SEPARATOR: I32 = '\\'
    pub const MAIN_SEPARATOR_STR: Str = "\\"
#else
    pub const MAIN_SEPARATOR: I32 = '/'
    pub const MAIN_SEPARATOR_STR: Str = "/"
#endif

/// Check if character is a path separator
pub func is_separator(c: I32) -> Bool {
    #if WINDOWS
        c == '/' or c == '\\'
    #else
        c == '/'
    #endif
}
```

## Example

```tml
use std::path::{Path, PathBuf}

func main() {
    // Create paths
    let p = Path::new("/home/user/file.txt")
    let mut pb = PathBuf::from("/home/user")

    // Components
    println("Parent: {p.parent()}")           // /home/user
    println("File: {p.file_name()}")          // file.txt
    println("Stem: {p.file_stem()}")          // file
    println("Ext: {p.extension()}")           // txt

    // Manipulation
    pb.push("documents")
    pb.push("report.md")
    println("Path: {pb.as_str()}")            // /home/user/documents/report.md

    pb.set_extension("pdf")
    println("Changed: {pb.as_str()}")         // /home/user/documents/report.pdf

    pb.pop()
    println("Popped: {pb.as_str()}")          // /home/user/documents

    // Joining
    let full = Path::new("/home").join("user").join("file.txt")
    println("Joined: {full.as_str()}")        // /home/user/file.txt

    // Iteration
    for component in p.components() {
        println("Component: {component}")
    }

    for ancestor in p.ancestors() {
        println("Ancestor: {ancestor.as_str()}")
    }
}
```
