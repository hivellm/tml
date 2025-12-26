# Proposal: Object File Build System

## Why

Currently, TML compiles directly from LLVM IR (.ll) to executables (.exe), missing critical intermediate steps that modern compilers use. This prevents:

1. **Incremental compilation**: Every build recompiles everything from scratch
2. **Build caching**: No reuse of previously compiled code
3. **Library generation**: Cannot create .dll/.so/.lib for C interop
4. **Parallel compilation**: Cannot compile modules in parallel
5. **Fast testing**: Each test rebuild is slow (no cache)

**Comparison with mature ecosystems:**

| Feature | Rust | Go | TML (current) | TML (proposed) |
|---------|------|----|--------------:|---------------:|
| Object files (.o) | ✅ | ✅ | ❌ | ✅ |
| Incremental build | ✅ | ✅ | ❌ | ✅ |
| Build cache | ✅ | ✅ | ❌ | ✅ |
| Static lib export | ✅ | ✅ | ❌ | ✅ |
| Dynamic lib export | ✅ | ✅ | ❌ | ✅ |
| C FFI export | ✅ | ✅ | ❌ | ✅ |
| Parallel compilation | ✅ | ✅ | ❌ | ✅ |

**Real-world impact:**
- **std library**: Currently 10+ source files compiled to single .exe every time
  - With cache: Only modified files recompile → ~10x faster
  - As library: `libstd.dll` can be used from C/Python/Node.js
- **Tests**: 45 test files, each full rebuild
  - With cache: Instant reruns if code unchanged
  - Parallel: Compile 45 tests simultaneously

## What Changes

### New Build Pipeline

**Current (inefficient):**
```
.tml → Parser → Type Check → LLVM IR (.ll) → clang → .exe
                                                ↓
                                        always full rebuild
```

**Proposed (efficient):**
```
.tml → Parser → Type Check → LLVM IR (.ll)
                                ↓
                         llc/clang -c
                                ↓
                         Object file (.o)
                                ↓
                      Build cache (hash-based)
                                ↓
                    Linker (based on crate-type)
                    ┌───────┬─────────┬──────────┐
                    ↓       ↓         ↓          ↓
                  .exe    .a/.lib  .so/.dll   .rlib
```

### New Components

1. **Object File Generator** (`src/cli/object_compiler.hpp`, `.cpp`)
   - Compile .ll → .o using `clang -c` or `llc`
   - Handle optimization levels (-O0, -O1, -O2, -O3)
   - Platform-specific flags (Windows vs Linux)

2. **Build Cache Manager** (`src/cli/build_cache.hpp`, `.cpp`)
   - Hash-based caching (source content + compiler version + flags)
   - Cache storage: `build/debug/.cache/{hash}.o`
   - Cache index: `build/debug/.cache/index.json`
   - Cache invalidation on source changes

3. **Linker Manager** (`src/cli/linker.hpp`, `.cpp`)
   - Link multiple .o files to final output
   - Support multiple output modes:
     - `bin`: Executable (.exe)
     - `staticlib`: Static library (.a/.lib) + C header
     - `cdylib`: Dynamic library (.so/.dll) + C header
     - `rlib`: TML library with metadata (.rlib)

4. **Build Configuration** (`src/cli/build_config.hpp`, `.cpp`)
   - Parse `tml.toml` (package manifest)
   - Determine crate type (lib vs bin)
   - Handle build flags and dependencies

5. **C Header Generator** (`src/codegen/c_header_gen.hpp`, `.cpp`)
   - Generate C-compatible headers from `@[export]` functions
   - Type mapping: I32 → int32_t, F64 → double, etc.
   - Extern "C" declarations

### File Structure Changes

```
project/
├── tml.toml                    # NEW: Package manifest
├── src/
│   ├── lib.tml                # Library entry point
│   └── main.tml               # Binary entry point
└── build/
    ├── debug/
    │   ├── .cache/            # NEW: Object file cache
    │   │   ├── {hash}.o
    │   │   ├── {hash}.meta
    │   │   └── index.json
    │   ├── deps/              # Compiled dependencies
    │   │   └── essential.obj
    │   ├── libmylib.a         # NEW: Static library
    │   ├── mylib.dll          # NEW: Dynamic library
    │   ├── mylib.h            # NEW: C header
    │   ├── mylib.rlib         # NEW: TML library
    │   └── myapp.exe          # Executable
    └── release/
        └── ...
```

### CLI Changes

**New flags:**
```bash
# Build modes
tml build                           # Default: executable
tml build --crate-type staticlib    # Static library + C header
tml build --crate-type cdylib       # Dynamic library + C header
tml build --crate-type rlib         # TML library with metadata

# Cache control
tml build --no-cache                # Disable cache
tml build --clean                   # Clean cache before build
tml cache clean                     # Clear build cache
tml cache info                      # Show cache statistics

# Header generation
tml build --emit-header             # Generate C header for exports
```

**New manifest (tml.toml):**
```toml
[package]
name = "mylib"
version = "1.0.0"

[lib]
crate-type = ["rlib", "cdylib"]  # Output types

[[bin]]
name = "myapp"
path = "src/main.tml"
```

### Code Changes

**Export decorator for FFI:**
```tml
// math.tml
@[export]
func add(a: I32, b: I32) -> I32 {
    return a + b
}
```

**Generated C header (math.h):**
```c
#ifndef TML_MATH_H
#define TML_MATH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int32_t add(int32_t a, int32_t b);

#ifdef __cplusplus
}
#endif

#endif // TML_MATH_H
```

**Usage from C:**
```c
#include "math.h"

int main() {
    int result = add(5, 3);  // Calls TML code!
    printf("%d\n", result);
    return 0;
}
```

## Impact

### Affected Specs
- `docs/09-CLI.md`: Add build modes, cache management
- `docs/16-COMPILER-ARCHITECTURE.md`: Add object file pipeline
- New: `docs/17-BUILD-SYSTEM.md`: Complete build system docs
- New: `docs/18-C-FFI.md`: C interop documentation

### Affected Code
- `packages/compiler/src/cli/cmd_build.cpp`: Refactor to use object files
- New: `packages/compiler/src/cli/object_compiler.{hpp,cpp}`
- New: `packages/compiler/src/cli/build_cache.{hpp,cpp}`
- New: `packages/compiler/src/cli/linker.{hpp,cpp}`
- New: `packages/compiler/src/cli/build_config.{hpp,cpp}`
- New: `packages/compiler/src/codegen/c_header_gen.{hpp,cpp}`

### Breaking Changes
**NO** - This is additive:
- Existing `tml build` behavior unchanged (still produces .exe)
- New functionality available via flags
- Users opt-in to new features

### User Benefits

1. **Faster builds**: 10-100x faster for incremental changes
2. **Faster tests**: Cached test binaries, instant reruns
3. **Library export**: Share TML code as .dll/.so with other languages
4. **Better modularization**: Each module compiles independently
5. **Parallel builds**: Utilize all CPU cores during compilation

### Dependencies

- **Required**: bootstrap-llvm-backend (LLVM IR generation)
- **Required**: llc or clang installed (object file compilation)
- **Required**: System linker (ld, lld, or MSVC link.exe)
- **Optional**: ar (for static library creation)

### Performance Estimates

**Current (no cache):**
- Full build: ~5 seconds
- Rebuild after 1-line change: ~5 seconds (same!)
- Test suite (45 tests): ~90 seconds

**With object file cache:**
- Full build: ~6 seconds (slight overhead)
- Rebuild after 1-line change: ~0.5 seconds (10x faster)
- Test suite (45 tests): ~10 seconds (9x faster)

**With parallel compilation (future):**
- Full build: ~2 seconds (3x faster)
- Test suite: ~3 seconds (30x faster)

## Migration Path

### Phase 1: Object File Generation (1-2 days)
- Implement compile_to_object() function
- Generate .o files, link to .exe
- Keep existing behavior, add .o as intermediate step
- **Goal**: Same output, but with .o files created

### Phase 2: Build Cache (2-3 days)
- Implement hash-based caching
- Cache .o files by content hash
- Add cache management commands
- **Goal**: Incremental builds work

### Phase 3: Static Library Mode (2 days)
- Implement --crate-type staticlib
- Add ar invocation for .a/.lib creation
- **Goal**: Can generate libfoo.a

### Phase 4: Dynamic Library Mode (2 days)
- Implement --crate-type cdylib
- Add shared library linking
- **Goal**: Can generate libfoo.so/foo.dll

### Phase 5: C Header Generation (2-3 days)
- Implement @[export] decorator
- Generate C headers with type mapping
- Add examples and docs
- **Goal**: TML code callable from C

### Phase 6: TML Library Format (3 days)
- Define .rlib format (metadata + objects)
- Implement rlib reading/writing
- **Goal**: TML-to-TML library dependencies

**Total estimate**: 12-15 days for complete implementation

## Success Criteria

1. ✅ `tml build` produces same .exe as before (backward compatible)
2. ✅ Second build after no changes completes in <1 second (cache works)
3. ✅ `tml build --crate-type staticlib` produces usable .a/.lib
4. ✅ `tml build --crate-type cdylib` produces usable .so/.dll
5. ✅ Generated C header can be used from C program
6. ✅ Test suite runs 5x faster with caching
7. ✅ `tml cache info` shows cache statistics
8. ✅ Documentation updated with examples
9. ✅ All 45 existing tests still pass

## References

- **Rust**: https://doc.rust-lang.org/cargo/reference/cargo-targets.html
- **Go**: https://go.dev/cmd/go/#hdr-Build_modes
- **LLVM**: https://llvm.org/docs/CommandGuide/llc.html
- **Internals**: ARCHITECTURE_PROPOSAL.md (detailed technical design)
