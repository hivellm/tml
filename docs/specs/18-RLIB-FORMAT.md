# 18. TML Library Format (.rlib)

## Overview

The `.rlib` file format is TML's native library format for distributing compiled code with metadata. It enables:
- **Type-safe linking**: Compiler validates types across library boundaries
- **Incremental compilation**: Only recompile when dependencies change
- **Content-based caching**: Cache invalidation based on actual code changes
- **Dependency tracking**: Automatic resolution of transitive dependencies

## File Format

An `.rlib` file is a standard archive format (ar/lib) containing:

```
mylib.rlib
├── mylib.o           # Compiled object file(s)
├── metadata.json     # Library metadata
└── exports.txt       # List of exported symbols
```

### Archive Format

- **Windows**: Uses `lib.exe` to create `.rlib` (same as `.lib` format)
- **Linux/macOS**: Uses `ar` to create `.rlib` (same as `.a` format)

The `.rlib` extension distinguishes TML libraries from plain static libraries:
- `.rlib`: Contains TML metadata, managed by TML compiler
- `.lib/.a`: Plain static library, no TML-specific metadata

## Metadata Format

The `metadata.json` file contains library information:

```json
{
  "format_version": "1.0",
  "library": {
    "name": "mylib",
    "version": "1.0.0",
    "tml_version": "0.1.0"
  },
  "modules": [
    {
      "name": "mylib",
      "file": "mylib.o",
      "hash": "sha256:abc123...",
      "exports": [
        {
          "name": "add",
          "symbol": "tml_add",
          "type": "func(I32, I32) -> I32",
          "public": true
        },
        {
          "name": "Point",
          "symbol": "tml_Point",
          "type": "struct { x: I32, y: I32 }",
          "public": true
        }
      ]
    }
  ],
  "dependencies": [
    {
      "name": "std",
      "version": "0.1.0",
      "hash": "sha256:def456..."
    }
  ]
}
```

### Metadata Fields

**format_version**: Metadata format version (semver)
- Allows future format changes without breaking old compilers
- Current version: `"1.0"`

**library**: Library identification
- `name`: Library name (matches crate name)
- `version`: Library version (semver)
- `tml_version`: TML compiler version used to build this library

**modules**: List of compiled modules in this library
- `name`: Module name (TML module path)
- `file`: Object file name in archive
- `hash`: Content hash of original source (for cache invalidation)
- `exports`: List of public symbols

**exports**: Public symbols from this module
- `name`: TML identifier (as written in source)
- `symbol`: Mangled symbol name in object file
- `type`: Type signature (for type checking)
- `public`: Visibility (true for pub items)

**dependencies**: Libraries this library depends on
- `name`: Dependency library name
- `version`: Required version (semver constraint)
- `hash`: Content hash of dependency .rlib (for exact matching)

## Symbol Exports

The `exports.txt` file lists all public symbols for quick filtering:

```
tml_add
tml_Point_new
tml_Point_x_get
tml_Point_y_get
```

This allows fast extraction of public API without parsing JSON.

## Build Process

### Creating .rlib

```bash
tml build mylib.tml --crate-type rlib
```

**Steps:**
1. Compile source to LLVM IR
2. Generate object file (.o/.obj)
3. Generate metadata.json
4. Generate exports.txt from public items
5. Create archive with ar/lib.exe
6. Write to `build/debug/mylib.rlib`

### Using .rlib

```bash
tml build main.tml --extern mylib=path/to/mylib.rlib
```

**Steps:**
1. Read metadata.json from mylib.rlib
2. Validate tml_version compatibility
3. Check dependency constraints
4. Extract type information for imported items
5. Type-check main.tml against mylib exports
6. Extract mylib.o from archive
7. Link main.o + mylib.o into executable

## Type Checking Across Libraries

When importing from .rlib, the compiler:

1. **Reads metadata.json** to get export signatures
2. **Type-checks imports** against export signatures
3. **Validates usage** of imported types/functions
4. **Reports type errors** before linking

Example:

```tml
// mylib.tml
pub func add(a: I32, b: I32) -> I32 {
    return a + b
}
```

Generated metadata:
```json
{
  "name": "add",
  "symbol": "tml_add",
  "type": "func(I32, I32) -> I32"
}
```

Usage:
```tml
// main.tml
import mylib::add

func main() {
    let x = add(5, 10)        // OK: I32, I32
    let y = add(5.0, 10.0)    // ERROR: Expected I32, found F64
}
```

The compiler catches the type error **before linking** by comparing against metadata.

## Dependency Resolution

### Simple Case: Direct Dependency

```
main.tml
  └── depends on mylib.rlib
```

The compiler reads `mylib.rlib` metadata and links against it.

### Transitive Dependencies

```
main.tml
  └── depends on mylib.rlib
        └── depends on mathlib.rlib
```

**Steps:**
1. Read `mylib.rlib` metadata → sees dependency on `mathlib`
2. Locate `mathlib.rlib` (via search paths)
3. Read `mathlib.rlib` metadata
4. Link: main.o + mylib.o + mathlib.o

### Dependency Search Paths

The compiler searches for dependencies in:
1. Explicit `--extern name=path` flags
2. `build/debug/deps/` directory
3. System library paths (future)

## Cache Invalidation

### Content-Based Hashing

Each module in metadata includes a hash of the **original source**:

```json
{
  "name": "mylib",
  "file": "mylib.o",
  "hash": "sha256:abc123..."
}
```

When rebuilding:
1. Hash current source file
2. Compare with hash in cached .rlib
3. If **identical**: reuse .rlib
4. If **different**: rebuild .rlib

### Transitive Invalidation

If `mathlib` changes and `mylib` depends on it:

```
mathlib.tml (modified)
  ↓
mylib.rlib (depends on mathlib)
  ↓
main.tml (depends on mylib)
```

**Rebuild steps:**
1. Detect `mathlib.tml` changed → rebuild `mathlib.rlib` (new hash)
2. `mylib.rlib` depends on old hash → **invalidated** → rebuild
3. `main.tml` depends on mylib → **invalidated** → rebuild

The compiler rebuilds **only what changed** and dependencies.

## CLI Commands

### Build RLIB

```bash
tml build mylib.tml --crate-type rlib
```

Options:
- `--crate-type rlib`: Build as TML library
- `--out-dir PATH`: Output directory (default: build/debug/)
- `--verbose`: Show detailed build steps

### Link Against RLIB

```bash
tml build main.tml --extern mylib=mylib.rlib
```

Options:
- `--extern name=path`: Add external library
- `-L PATH`: Add library search path

### Inspect RLIB

```bash
tml rlib info mylib.rlib
```

Output:
```
TML Library: mylib v1.0.0
TML Version: 0.1.0
Modules: 1
  - mylib (sha256:abc123...)
    Exports: 5 public items
Dependencies: 1
  - std v0.1.0
```

```bash
tml rlib exports mylib.rlib
```

Output:
```
Public exports from mylib v1.0.0:
  func add(I32, I32) -> I32
  func multiply(I32, I32) -> I32
  struct Point { x: I32, y: I32 }
  func Point::new(I32, I32) -> Point
  func Point::distance(Point) -> F64
```

## Implementation Files

### Core Implementation

- **`src/cli/rlib.hpp`**: RLIB format definitions and functions
- **`src/cli/rlib.cpp`**: RLIB creation, reading, extraction
- **`src/cli/cmd_rlib.cpp`**: CLI commands for RLIB inspection

### Data Structures

```cpp
namespace tml::cli {

struct RlibMetadata {
    std::string format_version;

    struct Library {
        std::string name;
        std::string version;
        std::string tml_version;
    } library;

    struct Export {
        std::string name;
        std::string symbol;
        std::string type;
        bool is_public;
    };

    struct Module {
        std::string name;
        std::string file;
        std::string hash;
        std::vector<Export> exports;
    };

    std::vector<Module> modules;

    struct Dependency {
        std::string name;
        std::string version;
        std::string hash;
    };

    std::vector<Dependency> dependencies;
};

// Create .rlib from object file + metadata
bool create_rlib(
    const fs::path& object_file,
    const RlibMetadata& metadata,
    const fs::path& output_rlib
);

// Read metadata from .rlib
RlibMetadata read_rlib_metadata(const fs::path& rlib_file);

// Extract object files from .rlib for linking
std::vector<fs::path> extract_rlib_objects(
    const fs::path& rlib_file,
    const fs::path& temp_dir
);

} // namespace tml::cli
```

## Advantages Over Plain .lib/.a

**Plain static library (.lib/.a)**:
- ❌ No type information → linking errors are cryptic
- ❌ No version tracking → ABI breakage undetected
- ❌ No dependency information → manual management
- ❌ No content hashing → unnecessary rebuilds

**TML library (.rlib)**:
- ✅ Type information → type errors before linking
- ✅ Version tracking → detect incompatible versions
- ✅ Dependency tracking → automatic transitive linking
- ✅ Content hashing → minimal rebuilds

## Compatibility with C

TML libraries can still be used from C:

```bash
# Build as both .rlib and .lib for C compatibility
tml build mylib.tml --crate-type rlib,lib --emit-header

# Generates:
# - mylib.rlib (for TML projects)
# - mylib.lib (for C projects)
# - mylib.h (for C projects)
```

C projects use `.lib` + `.h` files.
TML projects use `.rlib` for enhanced features.

## Future Extensions

### Module-Level Granularity

Currently: One .rlib = one crate
Future: One .rlib = multiple modules

```json
{
  "modules": [
    {"name": "mylib::math", "file": "math.o"},
    {"name": "mylib::string", "file": "string.o"},
    {"name": "mylib::io", "file": "io.o"}
  ]
}
```

Allows selective linking of only used modules.

### Compressed Archives

Future: Support gzip compression for smaller .rlib files

```bash
tml build mylib.tml --crate-type rlib --compress
```

### Cryptographic Signatures

Future: Sign .rlib files for tamper detection

```json
{
  "signature": {
    "algorithm": "ed25519",
    "public_key": "...",
    "signature": "..."
  }
}
```

## References

- Rust's rlib format: https://doc.rust-lang.org/reference/linkage.html
- ar archive format: https://en.wikipedia.org/wiki/Ar_(Unix)
- PE/COFF lib format: https://docs.microsoft.com/en-us/windows/win32/debug/pe-format
