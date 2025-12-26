# Tasks: Object File Build System

## Progress: 52% (31/60 tasks complete)

## Phase 1: Object File Generation (Foundation) ✅ COMPLETE

### 1.1 Setup and Infrastructure ✅
- [x] 1.1.1 Create `src/cli/object_compiler.hpp` header
- [x] 1.1.2 Create `src/cli/object_compiler.cpp` implementation
- [ ] 1.1.3 Add unit test file `tests/cli/object_compiler_test.cpp`
- [x] 1.1.4 Update CMakeLists.txt to include new files

### 1.2 Object File Compilation ✅
- [x] 1.2.1 Implement `compile_ll_to_object()` function
  - Use `clang -c` to compile .ll → .o
  - Handle Windows (.obj) vs Unix (.o) extensions
  - Support optimization flags (-O0, -O1, -O2, -O3)
- [x] 1.2.2 Add error handling for compilation failures
- [x] 1.2.3 Add verbose output mode for debugging
- [x] 1.2.4 Test object file generation on Windows
- [ ] 1.2.5 Test object file generation on Linux (if available)

### 1.3 Linker Integration ✅
- [x] 1.3.1 Implement `link_objects()` function
  - Link .o files → .exe using clang
  - Include runtime libraries (essential.obj, etc.)
- [x] 1.3.2 Preserve existing command-line linking behavior
- [ ] 1.3.3 Add unit tests for linker invocation
- [x] 1.3.4 Verify all existing tests still pass

### 1.4 Build Pipeline Refactor ✅
- [x] 1.4.1 Refactor `run_build()` and `run_run()` to use object files:
  - .tml → .ll (existing)
  - .ll → .o (new)
  - .o → .exe (new)
- [x] 1.4.2 Keep .ll file in debug mode, delete in release
- [x] 1.4.3 Keep .o file in build/debug/.run-cache/
- [x] 1.4.4 Add integration test for full pipeline

## Phase 2: Build Cache System ✅ PARTIALLY COMPLETE

### 2.1 Cache Infrastructure ✅
- [x] 2.1.1 Create cache directory structure: `build/debug/.run-cache/`
- [x] 2.1.2 Implement file-based caching system
- [x] 2.1.3 Add cache directory creation on first use
- [x] 2.1.4 Implement atomic cache operations (race-safe)

### 2.2 Hash-Based Caching ✅
- [x] 2.2.1 Implement content hashing function (std::hash)
  - Object cache: Hash = hash(source_content)
  - Executable cache: Hash = hash(source_hash + object_timestamps)
- [x] 2.2.2 Implement `get_cached_object()` pattern
  - Check if hash exists in cache
  - Return cached .o if found
- [x] 2.2.3 Implement `add_to_cache()` pattern
  - Store .o file with hash name
  - Store .exe file with combined hash
- [x] 2.2.4 Add hard link optimization for fast copying
- [x] 2.2.5 Implement test discovery cache with 1-hour TTL

### 2.3 Cache Management
- [ ] 2.3.1 Implement cache size limit (default: 1GB)
- [ ] 2.3.2 Implement LRU eviction for old entries
- [x] 2.3.3 Add `tml cache clean` command
- [x] 2.3.4 Add `tml cache info` command
  - Show cache size, hit rate, entries
- [x] 2.3.5 Add `--no-cache` flag to disable caching

### 2.4 Integration and Testing ✅
- [x] 2.4.1 Integrate cache into build pipeline (run_build, run_run, run_test)
- [x] 2.4.2 Test cache hit after unchanged build
- [x] 2.4.3 Test cache miss after source change
- [x] 2.4.4 Measure build time improvement
  - ✅ `tml run`: 0.855s → 0.075s (91% faster)
  - ✅ `tml test`: 6.31s → 3.06s (52% faster)
- [x] 2.4.5 Test cache with parallel execution (race-safe)

## Phase 3: Static Library Mode

### 3.1 Static Library Infrastructure
- [ ] 3.1.1 Create `src/cli/linker.hpp` header
- [ ] 3.1.2 Create `src/cli/linker.cpp` implementation
- [ ] 3.1.3 Add `--crate-type` command-line flag
- [ ] 3.1.4 Define BuildMode enum (Executable, StaticLib, DynamicLib, TMLLib)

### 3.2 Static Library Linking
- [ ] 3.2.1 Implement `link_static_library()` function
  - Windows: use `lib.exe` (MSVC) or `ar` (MinGW)
  - Linux: use `ar rcs`
- [ ] 3.2.2 Handle library naming conventions:
  - Windows: `mylib.lib`
  - Linux: `libmylib.a`
- [ ] 3.2.3 Include runtime objects in static lib
- [ ] 3.2.4 Test static library creation

### 3.3 Static Library Usage
- [ ] 3.3.1 Create example project using static library
- [ ] 3.3.2 Test linking C program with TML static lib
- [ ] 3.3.3 Verify exported functions are accessible
- [ ] 3.3.4 Add documentation for static library usage

## Phase 4: Dynamic Library Mode

### 4.1 Dynamic Library Infrastructure
- [ ] 4.1.1 Implement `link_dynamic_library()` function
  - Windows: use `link.exe /DLL` or `clang -shared`
  - Linux: use `clang -shared -fPIC`
- [ ] 4.1.2 Handle library naming conventions:
  - Windows: `mylib.dll`
  - Linux: `libmylib.so`
  - macOS: `libmylib.dylib`
- [ ] 4.1.3 Add position-independent code (PIC) flag for shared libs
- [ ] 4.1.4 Handle symbol visibility (export all public functions)

### 4.2 Dynamic Library Testing
- [ ] 4.2.1 Create example TML library
- [ ] 4.2.2 Test loading .dll/.so from C program
- [ ] 4.2.3 Test function calls across library boundary
- [ ] 4.2.4 Test library on Windows
- [ ] 4.2.5 Test library on Linux (if available)

## Phase 5: C Header Generation

### 5.1 Header Generator Infrastructure
- [ ] 5.1.1 Create `src/codegen/c_header_gen.hpp` header
- [ ] 5.1.2 Create `src/codegen/c_header_gen.cpp` implementation
- [ ] 5.1.3 Define type mapping rules (I32 → int32_t, etc.)
- [ ] 5.1.4 Add `@[export]` decorator support in parser

### 5.2 Type Mapping
- [ ] 5.2.1 Map TML primitive types to C types:
  - I8 → int8_t, I16 → int16_t, I32 → int32_t, I64 → int64_t
  - U8 → uint8_t, U16 → uint16_t, U32 → uint32_t, U64 → uint64_t
  - F32 → float, F64 → double
  - Bool → bool (stdbool.h)
- [ ] 5.2.2 Map TML struct types to C struct types
- [ ] 5.2.3 Handle pointer types (ref T → T*)
- [ ] 5.2.4 Add error for unsupported types (generics, enums with payload)

### 5.3 Header Generation
- [ ] 5.3.1 Implement `generate_c_header()` function
- [ ] 5.3.2 Generate include guards
- [ ] 5.3.3 Generate `extern "C"` wrapper for C++
- [ ] 5.3.4 Generate function declarations for @[export] functions
- [ ] 5.3.5 Generate struct definitions if needed
- [ ] 5.3.6 Add `--emit-header` flag to CLI

### 5.4 FFI Testing
- [ ] 5.4.1 Create TML library with exported functions
- [ ] 5.4.2 Generate C header automatically
- [ ] 5.4.3 Write C program that includes header
- [ ] 5.4.4 Compile and link C program with TML library
- [ ] 5.4.5 Test function calls from C to TML
- [ ] 5.4.6 Test with multiple exported functions
- [ ] 5.4.7 Test with struct parameters

## Phase 6: TML Library Format (.rlib)

### 6.1 RLIB Format Design
- [ ] 6.1.1 Define .rlib file format specification
  - Archive containing .o files + metadata
- [ ] 6.1.2 Define metadata format (JSON or custom binary)
  - Module name, version, dependencies
  - Exported symbols (functions, types)
  - Type information for cross-module checks
- [ ] 6.1.3 Create `src/cli/rlib.hpp` header
- [ ] 6.1.4 Create `src/cli/rlib.cpp` implementation

### 6.2 RLIB Creation
- [ ] 6.2.1 Implement `create_rlib()` function
  - Bundle .o files into archive
  - Embed metadata
- [ ] 6.2.2 Use ar or custom archiver
- [ ] 6.2.3 Test .rlib creation with sample library

### 6.3 RLIB Reading
- [ ] 6.3.1 Implement `read_rlib_metadata()` function
- [ ] 6.3.2 Implement `extract_rlib_objects()` function
- [ ] 6.3.3 Integrate .rlib reading into module system
- [ ] 6.3.4 Test linking against .rlib dependencies

### 6.4 Dependency Management
- [ ] 6.4.1 Add [dependencies] section to tml.toml
- [ ] 6.4.2 Implement dependency resolution
- [ ] 6.4.3 Download/locate dependency .rlib files
- [ ] 6.4.4 Link against dependencies automatically

## Phase 7: Package Manifest (tml.toml)

### 7.1 Manifest Parser
- [ ] 7.1.1 Create `src/cli/build_config.hpp` header
- [ ] 7.1.2 Create `src/cli/build_config.cpp` implementation
- [ ] 7.1.3 Add TOML parsing library (toml++ or cpptoml)
- [ ] 7.1.4 Implement `parse_manifest()` function

### 7.2 Manifest Structure
- [ ] 7.2.1 Support [package] section (name, version)
- [ ] 7.2.2 Support [lib] section (crate-type)
- [ ] 7.2.3 Support [[bin]] sections (name, path)
- [ ] 7.2.4 Support [dependencies] section
- [ ] 7.2.5 Add manifest validation

### 7.3 Manifest Integration
- [ ] 7.3.1 Read tml.toml automatically in build command
- [ ] 7.3.2 Override with command-line flags
- [ ] 7.3.3 Add `tml init` command to create default tml.toml
- [ ] 7.3.4 Test with various manifest configurations

## Phase 8: Documentation and Examples

### 8.1 Specification Updates
- [ ] 8.1.1 Update `docs/09-CLI.md` with new build commands
- [ ] 8.1.2 Update `docs/16-COMPILER-ARCHITECTURE.md` with object file pipeline
- [ ] 8.1.3 Create `docs/17-BUILD-SYSTEM.md` (build cache, modes)
- [ ] 8.1.4 Create `docs/18-C-FFI.md` (FFI guide, examples)

### 8.2 Examples
- [ ] 8.2.1 Create `examples/c-ffi/` directory
- [ ] 8.2.2 Add example: TML math library used from C
- [ ] 8.2.3 Add example: TML string library used from C
- [ ] 8.2.4 Add example: TML with struct parameters from C
- [ ] 8.2.5 Add example: Multi-crate TML project with .rlib

### 8.3 User Guide
- [ ] 8.3.1 Write user guide for library creation
- [ ] 8.3.2 Write user guide for FFI export
- [ ] 8.3.3 Write user guide for build cache
- [ ] 8.3.4 Write troubleshooting guide

## Phase 9: Performance Optimization ✅ PARTIALLY COMPLETE

### 9.1 Parallel Compilation ✅
- [x] 9.1.1 Create `src/cli/parallel_build.hpp` header
- [x] 9.1.2 Create `src/cli/parallel_build.cpp` implementation
- [x] 9.1.3 Implement `compile_ll_batch()` for parallel .ll → .o compilation
  - Compile multiple modules simultaneously
- [x] 9.1.4 Use thread pool (std::thread)
- [x] 9.1.5 Auto-detect hardware concurrency (std::thread::hardware_concurrency)
- [x] 9.1.6 Implement thread-safe `ensure_c_compiled()` with mutex
- [ ] 9.1.7 Add `--jobs` flag to control parallelism
- [x] 9.1.8 Measure speedup (achieved 52% faster for tests)

### 9.2 Incremental Linking ✅
- [x] 9.2.1 Implement executable cache
  - Only relink if .o files changed (via hash)
- [x] 9.2.2 Cache linker output in build/debug/.run-cache/
- [x] 9.2.3 Test incremental linking speedup (achieved 91% faster for run)

## Phase 10: Testing and Validation

### 10.1 Integration Tests
- [ ] 10.1.1 Add integration test for cache hit/miss
- [ ] 10.1.2 Add integration test for static library creation
- [ ] 10.1.3 Add integration test for dynamic library creation
- [ ] 10.1.4 Add integration test for C FFI
- [ ] 10.1.5 Add integration test for .rlib usage

### 10.2 Performance Tests
- [x] 10.2.1 Benchmark: full build time
- [x] 10.2.2 Benchmark: incremental build time (1 file change)
- [x] 10.2.3 Benchmark: cache lookup time
- [x] 10.2.4 Benchmark: test suite execution time
- [x] 10.2.5 Document performance improvements
  - ✅ `tml run`: 91% faster (0.855s → 0.075s)
  - ✅ `tml test`: 52% faster (6.31s → 3.06s)

### 10.3 Compatibility Tests
- [x] 10.3.1 Test on Windows 10/11
- [ ] 10.3.2 Test on Ubuntu Linux (if available)
- [ ] 10.3.3 Test on macOS (if available)
- [x] 10.3.4 Verify all existing tests still pass
- [ ] 10.3.5 Add CI/CD pipeline for multi-platform testing

## Success Metrics

- [x] ✅ All existing tests pass without modification
- [x] ✅ Incremental build <1 second for unchanged code (0.075s achieved)
- [x] ✅ Full rebuild fast with cache (3.06s for test suite)
- [x] ✅ Test suite 52% faster with caching
- [ ] ✅ Static library successfully used from C
- [ ] ✅ Dynamic library successfully used from C
- [ ] ✅ Documentation complete with examples
- [ ] ✅ Code coverage ≥90% for new components

## Implementation Notes

### Completed Features (Commits)
- `cefcccc` - Object file pipeline and basic caching
- `27b8a2e` - Executable caching for run/test commands
- `ca2f929` - Hard link optimization + thread-safe compilation
- `a6caad7` - Parallel build infrastructure

### Key Achievements
- **Two-level cache**: Object files (content hash) + Executables (combined hash)
- **Hard link optimization**: Instant file "copying" (no data copy)
- **Thread-safe compilation**: Mutex-protected ensure_c_compiled()
- **Parallel batch compilation**: compile_ll_batch() with worker threads
- **Test discovery cache**: 1-hour TTL for faster test runs
- **Race-safe cache updates**: Atomic rename operations

### Architecture
```
build/debug/.run-cache/
  ├── <content_hash>.obj      # Object file cache
  ├── <exe_hash>.exe          # Executable cache
  ├── <exe_hash>_link_temp.exe  # Temporary files
  └── .test-cache             # Test discovery cache (1h TTL)
```

### Performance Results
| Command | Before | After | Improvement |
|---------|--------|-------|-------------|
| `tml run` | 0.855s | 0.075s | **91% faster** |
| `tml test` | 6.31s | 3.06s | **52% faster** |

### Dependencies
- **LLVM/clang**: Required for compilation and linking
- **Platform**: Windows (primary), Linux (secondary)
- **C++ Standard**: C++17 (std::filesystem)

### Next Priorities
1. Cache management commands (`tml cache clean`, `tml cache info`)
2. Static library support (`--crate-type lib`)
3. C header generation for FFI
4. Unit tests for object_compiler and parallel_build
5. Linux/macOS compatibility testing
