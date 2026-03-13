# Proposal: Tracy Profiler Integration

## Status: PROPOSED

## Why

The TML compiler has known performance bottlenecks (build time ~100s, linking ~37s) but no way to do fine-grained profiling. Tracy (https://github.com/wolfpld/tracy) is a real-time, nanosecond resolution profiler designed for C++ applications. It provides:

- Frame-level, zone-level, and GPU profiling
- Memory allocation tracking
- Lock contention visualization
- Call stack sampling
- A rich GUI viewer for analyzing traces

Integrating Tracy will allow us to:
1. Identify hot spots in the compilation pipeline (lexer, parser, type checker, borrow checker, HIR, MIR, codegen, LLVM backend, LLD linking)
2. Find memory allocation patterns that cause slowdowns
3. Optimize incremental compilation cache hit/miss ratios
4. Profile the test runner subprocess coordination
5. **Profile TML programs at runtime** — stdlib operations (HashMap, JSON parsing, string ops, I/O) appear as named zones in the same Tracy timeline
6. Guide future optimization work with hard data instead of guesswork

### Current State

- No profiling instrumentation in the compiler
- Build performance analysis was done externally (`.sandbox/profile_analysis.md`) with coarse timing
- The `--profile` flag in the test runner only measures wall-clock time per test, not compiler internals

## What Changes

### Phase 1: Tracy Integration Setup

Add Tracy as a dependency and create a minimal instrumentation API that can be compiled out with zero overhead when profiling is disabled.

### Phase 2: Pipeline Instrumentation

Instrument the query-based compilation pipeline:
- `ReadSource` → `Tokenize` → `Parse` → `Typecheck` → `Borrowcheck` → `HirLower` → `MirBuild` → `CodegenUnit` → LLVM → LLD

### Phase 3: Detailed Instrumentation

Add finer-grained zones for:
- Individual type checking passes
- Generic monomorphization
- MIR optimization passes
- LLVM IR generation per function
- Memory allocations (arena allocators, AST nodes)
- Lock contention in parallel compilation

### Phase 4: TML Runtime & Standard Library Profiling

Expose Tracy to TML code via `@extern("c")` FFI bindings + RAII guard:

```tml
// lib/core/src/profiler.tml
use core::profiler

func main() {
    let _z = profiler::zone("main")          // auto-ends on drop
    let list = List::with_capacity[I32](1000) // HashMap::insert shows as zone
    profiler::message("starting work")        // appears in Tracy messages
    profiler::plot("items", list.len())        // custom metric plot
}
```

Instrument all performance-critical stdlib operations:
- `core/alloc` — mem_alloc/mem_free → TracyAlloc/TracyFree
- `core/str` — from, concat, split, replace
- `core/fmt` — format(), to_string()
- `core/iter` — collect(), fold()
- `core/slice` — sort(), binary_search()
- `std/collections` — HashMap insert/get/resize, List push/sort
- `std/json` — parse(), stringify()
- `std/file` — read(), write()
- `std/crypto` — hash/encrypt/decrypt
- `std/net` — connect/read/write

All instrumentation compiles out via `#ifdef PROFILE` conditional compilation — zero overhead when not profiling.

### Phase 5: End-to-End Application Profiling

- `tml run --profile` / `tml test --profile` — combined compiler + runtime view
- Compiler zones (C++) and runtime zones (TML) in the same Tracy timeline
- Example profiled program in `examples/profiling_demo.tml`

### Phase 6: Developer Workflow & CI

- `tml build --tracy` flag to enable profiled compilation
- Documentation on using Tracy viewer
- CI integration for performance regression detection
- `.tracy` capture file export for before/after comparison

## Technical Approach

### Option A: Tracy as Git Submodule (RECOMMENDED)

```
src/tracy/    # git submodule → https://github.com/wolfpld/tracy
```

Tracy is header-only for the client side. Include `tracy/TracyClient.cpp` in the build and use `ZoneScoped` / `ZoneScopedN` macros.

### Option B: vcpkg

`vcpkg install tracy` — simpler but less control over version.

### Compile-Out Strategy

```cpp
// compiler/include/profiler.hpp
#ifdef TML_PROFILE
  #include <tracy/Tracy.hpp>
  #define TML_ZONE_SCOPED ZoneScoped
  #define TML_ZONE_NAMED(name) ZoneScopedN(name)
  #define TML_ZONE_TEXT(text, len) ZoneText(text, len)
  #define TML_FRAME_MARK FrameMark
#else
  #define TML_ZONE_SCOPED
  #define TML_ZONE_NAMED(name)
  #define TML_ZONE_TEXT(text, len)
  #define TML_FRAME_MARK
#endif
```

When `TML_PROFILE` is not defined, all macros expand to nothing — zero runtime cost.

### TML-Side Architecture

```
lib/core/src/profiler.tml          — Public TML API (zone, message, plot)
compiler/runtime/core/essential.c  — C shim (TracyCZoneBegin/End, no-ops when disabled)
```

The TML module exposes a `Zone` type with `drop` behavior so zones auto-close:

```tml
// lib/core/src/profiler.tml
type Zone {
    id: U64
}

impl Zone {
    func new(name: Str) -> Zone {
        Zone { id: tracy_zone_begin(name) }
    }
}

impl Drop for Zone {
    func drop(mut ref self) {
        tracy_zone_end(self.id)
    }
}

func zone(name: Str) -> Zone { Zone::new(name) }
func message(text: Str) { tracy_message(text.as_ptr(), text.len()) }
func plot(name: Str, value: F64) { tracy_plot(name.as_ptr(), value) }
```

The C shim in `essential.c`:

```c
#ifdef TML_PROFILE
  #include <tracy/TracyC.h>
  uint64_t tml_tracy_zone_begin(const char* name) { /* TracyCZoneBegin */ }
  void tml_tracy_zone_end(uint64_t id) { /* TracyCZoneEnd */ }
  void tml_tracy_message(const char* text, size_t len) { TracyCMessage(text, len); }
  void tml_tracy_plot(const char* name, double val) { TracyCPlot(name, val); }
#else
  uint64_t tml_tracy_zone_begin(const char* name) { return 0; }
  void tml_tracy_zone_end(uint64_t id) { (void)id; }
  void tml_tracy_message(const char* text, size_t len) { (void)text; (void)len; }
  void tml_tracy_plot(const char* name, double val) { (void)name; (void)val; }
#endif
```

### Build Integration

```bash
scripts\build.bat --profile    # Enables -DTML_PROFILE=ON, links Tracy
scripts\build.bat              # Normal build, Tracy compiled out
```

## Risk Assessment

- **LOW**: Tracy client is battle-tested (used by Unreal Engine, Blender, etc.)
- **LOW**: Zero overhead when disabled (preprocessor macros)
- **MEDIUM**: Build time increase when Tracy is enabled (~5-10s extra)
- **LOW**: No impact on release builds or test infrastructure
