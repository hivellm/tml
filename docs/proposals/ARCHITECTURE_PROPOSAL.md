# TML Build System Architecture Proposal

## Current Issues

1. No intermediate object files (.o)
2. No compilation cache
3. Cannot generate libraries (.dll/.so/.lib)
4. Every test rebuilds everything
5. Cannot export to C/other languages

## Proposed Architecture

### Build Pipeline

```
┌─────────────┐
│ source.tml  │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Parser     │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│Type Checker │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  LLVM IR    │ (.ll file)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ llc/clang   │ (compile to object)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│  Object     │ (.o/.obj file)
└──────┬──────┘
       │
       ▼
┌─────────────────────────────┐
│  Linker (based on mode)     │
├─────────────────────────────┤
│ bin      → .exe             │
│ staticlib → .a/.lib         │
│ cdylib   → .so/.dll         │
│ rlib     → .tml.a (metadata)│
└─────────────┬───────────────┘
              │
              ▼
         ┌─────────┐
         │ Output  │
         └─────────┘
```

### Build Modes

```toml
# tml.toml
[package]
name = "mylib"
version = "1.0.0"

[lib]
# Multiple outputs can be specified
crate-type = ["rlib", "cdylib"]

[[bin]]
name = "myapp"
path = "src/main.tml"
```

**Mode: rlib** (TML library)
```bash
tml build --crate-type rlib
# Output: build/debug/libmylib.rlib
# Contains: .o files + metadata (types, exports)
```

**Mode: staticlib** (C-compatible static)
```bash
tml build --crate-type staticlib
# Output: build/debug/libmylib.a (Linux/Mac)
#         build/debug/mylib.lib (Windows)
# Also:   build/debug/mylib.h (C header)
```

**Mode: cdylib** (C-compatible dynamic)
```bash
tml build --crate-type cdylib
# Output: build/debug/libmylib.so (Linux)
#         build/debug/mylib.dll (Windows)
#         build/debug/libmylib.dylib (Mac)
# Also:   build/debug/mylib.h (C header)
```

**Mode: bin** (executable)
```bash
tml build
# Output: build/debug/myapp.exe
```

### Directory Structure

```
project/
├── tml.toml              # Package manifest
├── src/
│   ├── lib.tml           # Library root (if lib)
│   ├── main.tml          # Binary root (if bin)
│   └── utils/
│       └── mod.tml
└── build/
    ├── debug/
    │   ├── deps/         # Compiled dependencies
    │   │   ├── std.rlib
    │   │   └── core.rlib
    │   ├── .cache/       # Object file cache
    │   │   ├── lib.o
    │   │   ├── utils.o
    │   │   └── main.o
    │   ├── libmylib.rlib # TML library
    │   ├── libmylib.a    # Static lib
    │   ├── mylib.dll     # Dynamic lib
    │   ├── mylib.h       # C header
    │   └── myapp.exe     # Executable
    └── release/
        └── ...
```

### Compilation Cache

**Key:** Hash of (source_content + compiler_version + flags)
```
build/debug/.cache/
├── abc123def456.o        # Cached object file
├── abc123def456.meta     # Metadata (exports, types)
└── cache_index.json      # Map: source_path → hash
```

**Cache hit:**
```
1. Read source file
2. Compute hash
3. Check cache_index.json
4. If hash matches → reuse .o file
5. If hash differs → recompile → update cache
```

### Implementation Changes

#### 1. Add Object File Generation

```cpp
// cmd_build.cpp

enum class BuildMode {
    Executable,   // .exe
    StaticLib,    // .a/.lib
    DynamicLib,   // .so/.dll
    TMLLib,       // .rlib (with metadata)
};

struct BuildOptions {
    BuildMode mode = BuildMode::Executable;
    bool use_cache = true;
    bool emit_c_header = false;
};

// Compile .ll to .o using llc or clang -c
fs::path compile_to_object(const fs::path& ll_file) {
    fs::path obj_file = ll_file;
    obj_file.replace_extension(".o");

    std::string cmd = clang + " -c -O3 -o \"" + obj_file.string() +
                      "\" \"" + ll_file.string() + "\"";

    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        throw std::runtime_error("Object compilation failed");
    }

    return obj_file;
}
```

#### 2. Add Linker Stage

```cpp
// Link object files to create output
void link_objects(
    const std::vector<fs::path>& obj_files,
    const fs::path& output,
    BuildMode mode
) {
    std::string cmd;

    switch (mode) {
        case BuildMode::Executable:
            // Link to executable
            cmd = clang + " -o \"" + output.string() + "\"";
            for (const auto& obj : obj_files) {
                cmd += " \"" + obj.string() + "\"";
            }
            // Add runtime libs
            cmd += " -ltml_runtime";
            break;

        case BuildMode::StaticLib:
            // Create static library using ar
            cmd = "ar rcs \"" + output.string() + "\"";
            for (const auto& obj : obj_files) {
                cmd += " \"" + obj.string() + "\"";
            }
            break;

        case BuildMode::DynamicLib:
            // Create shared library
            cmd = clang + " -shared -o \"" + output.string() + "\"";
            for (const auto& obj : obj_files) {
                cmd += " \"" + obj.string() + "\"";
            }
            break;
    }

    std::system(cmd.c_str());
}
```

#### 3. Add Compilation Cache

```cpp
struct CacheEntry {
    std::string source_hash;
    fs::path object_file;
    uint64_t timestamp;
};

class CompilationCache {
    std::map<fs::path, CacheEntry> cache_;
    fs::path cache_dir_;

public:
    CompilationCache(const fs::path& cache_dir)
        : cache_dir_(cache_dir) {
        load_index();
    }

    std::optional<fs::path> get_cached_object(
        const fs::path& source_file,
        const std::string& source_content
    ) {
        auto hash = compute_hash(source_content);
        auto it = cache_.find(source_file);

        if (it != cache_.end() && it->second.source_hash == hash) {
            // Cache hit!
            return it->second.object_file;
        }

        return std::nullopt;
    }

    void add_entry(
        const fs::path& source_file,
        const std::string& source_content,
        const fs::path& object_file
    ) {
        auto hash = compute_hash(source_content);
        cache_[source_file] = CacheEntry{
            hash,
            object_file,
            current_timestamp()
        };
        save_index();
    }
};
```

### Benefits

✅ **Incremental compilation**: Only recompile changed files
✅ **Library support**: Can generate .dll/.so for C interop
✅ **Faster tests**: Reuse compiled .o files
✅ **Better modularization**: Each module → .o file
✅ **C FFI**: Export functions with `@[export]` decorator
✅ **Parallel compilation**: Compile multiple .tml → .o in parallel

### Example: Export to C

```tml
// math.tml
@[export]
func add(a: I32, b: I32) -> I32 {
    return a + b
}
```

```bash
tml build --crate-type cdylib --emit-header
```

Generated `math.h`:
```c
#ifndef TML_MATH_H
#define TML_MATH_H

#include <stdint.h>

int32_t add(int32_t a, int32_t b);

#endif
```

Generated `libmath.so` / `math.dll` can be used from C:
```c
#include "math.h"

int main() {
    int result = add(5, 3);  // Calls TML code
    return result;
}
```

### Migration Path

1. **Phase 1**: Add .o generation (keep single .o for now)
2. **Phase 2**: Add compilation cache
3. **Phase 3**: Add static/dynamic library modes
4. **Phase 4**: Add per-module .o (parallel compilation)
5. **Phase 5**: Add C header generation

This brings TML up to the same level as Rust/Go for library development
and FFI interoperability.
