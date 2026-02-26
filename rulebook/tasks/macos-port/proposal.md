# Proposal: macOS/ARM64 Port

## Why

The TML compiler and runtime were developed primarily on Windows/MSVC. To support macOS (and Linux) as a first-class platform, multiple issues need to be resolved across the compiler C++ code, C runtime, standard library, build system, and tooling (MCP server). Without this work, TML programs cannot be built or tested on macOS.

## What Changes

### Compiler C++ (Clang compatibility)
- Suppress Clang warnings that MSVC doesn't emit
- Use `->template is<>()` / `->template as<>()` syntax required by Clang
- Fix use-after-move UB in parser_type.cpp (segfault on ARM64)
- Add `[[maybe_unused]]`, `static_cast` where needed
- Move shared types outside `#ifdef _WIN32` blocks

### C Runtime (POSIX compatibility)
- Create `compat.h` mapping MSVC functions to POSIX (`_strdup`->`strdup`, etc.)
- Guard Windows-only code (SEH, crash backtrace) with `#ifdef _WIN32`
- Add missing includes (`<unistd.h>`) and fix variadic macro warnings

### Standard Library (TML)
- Add missing `as_raw()` to `RawSocket`
- Fix platform constants (`AF_INET6`, `SOL_SOCKET` differ on Unix)
- Implement POSIX socket syscalls in runtime
- Port `tml_str_free` to use `malloc_usable_size` on POSIX

### Build System & Tooling
- Functional `build.sh` for macOS with Homebrew LLVM
- Fix MCP server to find `tml` binary on Unix
- Link runtime objects when building TML programs on macOS
- Fix test runner SIGBUS crash in full suite execution

## Impact
- Affected specs: None
- Affected code: compiler/, lib/std/, scripts/, .mcp.json
- Breaking change: NO
- User benefit: TML compiles, builds, and runs on macOS/ARM64 and Linux
