# Proposal: Implement Unified Logging System

## Status: PROPOSED

## Why

The TML project currently has **zero logging infrastructure**. All output across both the C++ compiler and TML runtime is done through raw `printf`, `std::cout`, and `std::cerr` calls with no structure, no levels, no filtering, and no file output capability.

### Current State Assessment

**C++ Compiler (compiler/src/):**
- **~1,039 direct print calls** scattered across **88 files**
- **438 verbose flag checks** (`CompilerOptions::verbose`) — ad-hoc conditional output
- **2 debug macros** (`TML_DEBUG`, `TML_DEBUG_LN`) with only ~20 uses
- **1 thread-safe macro** (`VERBOSE_LOG`) but only in test_runner.cpp
- No log levels, no categories, no file output, no structured format
- Verbose output is either "everything" or "nothing" — no granularity

**TML Runtime (compiler/runtime/):**
- **106 printf/fprintf calls** across 9 C files
- **9 print functions** (print, println, print_i32, etc.) — all go to stdout
- **Binary suppress flag** (`tml_suppress_output`) — on or off, no levels
- Memory tracker (`mem_track.c`) uses fprintf with configurable FILE* but no log framework
- Backtrace system outputs directly to stderr

**TML Standard Library (lib/):**
- **No logging module exists** — `log` in intrinsics.tml is the math `ln()` function
- Test framework uses plain `println()` with ANSI colors for formatting
- No file output, no log levels, no structured output

### Problems This Causes

1. **Debugging the compiler is painful** — you either get zero output or a flood of unfiltered verbose text
2. **No way to log to files** — all output goes to stdout/stderr, lost when terminal closes
3. **No per-module filtering** — can't enable debug output for just the borrow checker or just the codegen
4. **No structured output** — can't pipe logs to analysis tools or dashboards
5. **Thread-safety gaps** — only the test runner has mutex-protected output
6. **No way for TML programs to log** — users have only `print()` and `println()`
7. **No timestamp/context** — only the test runner adds timestamps via `DEBUG_LOG`
8. **Replacing prints is error-prone** — 1,039 print calls with no consistent pattern

## What Changes

This task implements a **two-layer unified logging system** that spans both the C++ compiler and the TML language:

### Layer 1: C++ Compiler Logger (`tml::log`)

A lightweight C++ logging library embedded in the compiler that replaces all 1,039 print calls.

**Architecture:**
```
tml::log::trace("codegen", "Generating LLVM IR for {}", func_name);
tml::log::debug("borrow", "Checking borrow for place {}", place_id);
tml::log::info("build", "Compiling {} -> {}", input, output);
tml::log::warn("types", "Implicit narrowing conversion {} -> {}", from, to);
tml::log::error("parse", "Unexpected token {} at line {}", tok, line);
```

**Key Design:**
- **6 log levels**: Trace, Debug, Info, Warn, Error, Fatal
- **Module tags**: "lexer", "parser", "types", "borrow", "hir", "mir", "codegen", "build", "test", "cache", "link"
- **Compile-time filtering**: `TML_LOG_LEVEL` preprocessor define for zero-cost disabled levels
- **Runtime filtering**: `--log-level=debug`, `--log-filter=codegen,borrow`
- **Multiple sinks**: stderr (default), file (`--log-file=build.log`), JSON (`--log-format=json`)
- **Thread-safe**: Lock-free ring buffer with per-thread formatting
- **Format strings**: `fmt::format`-compatible with `{}` placeholders
- **Zero overhead when disabled**: Macros that compile to nothing at `-O2+`

### Layer 2: TML Language Logger (`std::log`)

A TML standard library module that TML programs can use for structured logging.

**Architecture:**
```tml
use std::log

func main() {
    log::init(LogConfig {
        level: LogLevel::Debug,
        sinks: [ConsoleSink::colored(), FileSink::new("app.log")],
        format: "{time} [{level}] {module}: {message}",
    })

    log::info("server", "Starting on port {}", port)
    log::debug("db", "Query executed in {}ms", elapsed)
    log::error("auth", "Login failed for user {}", username)
}
```

**Key Design:**
- **Same 6 levels** as C++ layer (Trace, Debug, Info, Warn, Error, Fatal)
- **Module-based filtering**: `log::set_filter("server=info,db=debug")`
- **Pluggable sinks**: Console (colored), File (with rotation), Custom (via `Sink` behavior)
- **Structured fields**: `log::info("req", "Request handled"; "method" => method, "status" => 200, "duration_ms" => elapsed)`
- **Global logger singleton**: Thread-safe, set once at startup
- **Format customization**: Template string with `{time}`, `{level}`, `{module}`, `{message}`, `{file}`, `{line}`

### Layer 3: Runtime Bridge

The C runtime (`essential.c`) gets logging functions that are callable from both C++ and TML:

```c
// Runtime logging API (C)
void tml_log(int level, const char* module, const char* message);
void tml_log_to_file(const char* path);
void tml_log_set_level(int level);
void tml_log_set_filter(const char* filter_spec);
```

### CLI Integration

New compiler flags:
```bash
tml build file.tml --log-level=debug          # Set minimum log level
tml build file.tml --log-filter=codegen,mir   # Only show these modules
tml build file.tml --log-file=build.log       # Also log to file
tml build file.tml --log-format=json          # JSON structured output
tml build file.tml -v                         # Shorthand for --log-level=info (current verbose)
tml build file.tml -vv                        # Shorthand for --log-level=debug
tml build file.tml -vvv                       # Shorthand for --log-level=trace
tml build file.tml -q                         # Quiet mode (errors only)
```

## Migration Strategy

### Phase 1: Build the logging library (C++ and C runtime)
### Phase 2: Replace compiler prints incrementally
- Start with builder/ subsystem (150+ prints, most verbose)
- Then tester/ subsystem (200+ prints)
- Then commands/ (100+ prints)
- Then codegen/ (debug output)
- Leave diagnostic.cpp as-is (it's already structured)
### Phase 3: Build TML std::log module
### Phase 4: Integrate CLI flags

**Diagnostic system (`diagnostic.cpp`) is NOT replaced** — it's a separate concern (user-facing error reporting). The logger handles internal compiler output only.

## Impact

- Affected specs: CLI specification (new flags), standard library (new module)
- Affected code: compiler/src/ (88 files with prints), compiler/runtime/ (9 C files), lib/std/ (new module)
- Breaking change: NO (existing `-v`/`--verbose` becomes alias for `--log-level=info`)
- User benefit: Debuggable compiler, filterable output, file logging, structured logging for TML programs

## Design References

| Feature | Inspired By | Adaptation |
|---------|-------------|------------|
| Log levels | spdlog, Rust `log` crate | Same 6 levels (Trace-Fatal) |
| Module filtering | Rust `env_logger` (`RUST_LOG=module=level`) | `--log-filter=module=level` |
| Compile-time elision | spdlog `SPDLOG_ACTIVE_LEVEL` | `TML_LOG_LEVEL` preprocessor define |
| Structured fields | `tracing` crate (Rust) | `key => value` syntax in TML |
| Sink abstraction | spdlog sinks, log4j appenders | `Sink` behavior in TML |
| Ring buffer | spdlog async logger | Lock-free per-thread buffer |
| Format strings | `fmt` library (C++), Rust `format!` | `{}` placeholder syntax |
| File rotation | logrotate, spdlog rotating_file_sink | Size-based + time-based rotation |
| Colored output | `env_logger` (Rust), chalk (Node.js) | ANSI with terminal detection |
