# TML v1.0 — Unified Logging System

## 1. Overview

TML has a three-layer logging architecture:

| Layer | Language | Location | Purpose |
|-------|----------|----------|---------|
| C++ Logger | C++ | `compiler/include/log/`, `compiler/src/log/` | Internal compiler diagnostics |
| C Runtime Logger | C | `compiler/runtime/log.h`, `log.c` | Runtime library diagnostics |
| TML `std::log` | TML | `lib/std/src/log.tml` | User program logging |

All three layers share the same 6 log levels and can be connected via a callback
bridge so that runtime messages flow through the compiler's logger during test execution.

## 2. Log Levels

| Level | Value | Description | When to Use |
|-------|-------|-------------|-------------|
| Trace | 0 | Fine-grained internal tracing | Function entry/exit, loop iterations |
| Debug | 1 | Debugging information | Variable values, state changes |
| Info | 2 | General informational messages | Build progress, file processing |
| Warn | 3 | Potential issues or deprecations | Implicit conversions, fallback paths |
| Error | 4 | Recoverable errors | Failed operations that can continue |
| Fatal | 5 | Unrecoverable errors | Panics, assertion failures |
| Off | 6 | Disables all logging | Silent mode |

The default level is **Warn** — only warnings, errors, and fatal messages are shown.

## 3. CLI Flags

### 3.1 Verbosity Shortcuts

```bash
tml build file.tml              # Default: Warn level
tml build file.tml -v           # Info level (same as --verbose)
tml build file.tml -vv          # Debug level
tml build file.tml -vvv         # Trace level (maximum detail)
tml build file.tml -q           # Error level (quiet mode)
```

### 3.2 Explicit Configuration

```bash
# Set exact log level
tml build file.tml --log-level=debug

# Filter by module (comma-separated module=level pairs)
tml build file.tml --log-filter=codegen=trace,build=info

# Write logs to a file
tml build file.tml --log-file=build.log

# JSON output format (one JSON object per line)
tml build file.tml --log-format=json
```

### 3.3 Environment Variable

The `TML_LOG` environment variable configures logging when no CLI flags are given:

```bash
# Set global level
TML_LOG=debug tml build file.tml

# Per-module filtering
TML_LOG=codegen=trace,*=warn tml build file.tml

# Windows (PowerShell)
$env:TML_LOG="codegen=trace"; tml build file.tml

# Windows (cmd)
set TML_LOG=codegen=trace && tml build file.tml
```

### 3.4 Filter Syntax

The filter string is a comma-separated list of `module=level` pairs:

```
codegen=trace,build=info,*=warn
```

- `module=level` — set level for a specific module
- `*=level` — set the default level for all unmatched modules
- `module` (without `=level`) — set module to Trace (show everything)

**Compiler module tags**: `lexer`, `parser`, `types`, `borrow`, `hir`, `mir`,
`codegen`, `build`, `test`, `cache`, `link`, `cli`, `runtime`

## 4. TML Standard Library: `std::log`

### 4.1 Quick Start

```tml
use std::log
use std::log::{TRACE, DEBUG, INFO, WARN, ERROR, FATAL}

// Log at specific levels
log::info("server", "Listening on port 8080")
log::warn("pool", "Connection pool at 90%")
log::error("db", "Query failed, retrying...")

// Change log level at runtime
log::set_level(DEBUG)

// Check before expensive formatting
if log::enabled(TRACE) {
    log::trace("app", "Request payload: ...")
}
```

### 4.2 API Reference

#### Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `TRACE` | 0 | Fine-grained tracing |
| `DEBUG` | 1 | Debugging information |
| `INFO` | 2 | Informational messages |
| `WARN` | 3 | Potential issues |
| `ERROR` | 4 | Recoverable errors |
| `FATAL` | 5 | Unrecoverable errors |

Import with: `use std::log::{TRACE, DEBUG, INFO, WARN, ERROR, FATAL}`

#### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `trace` | `(module: Str, message: Str)` | Log at TRACE level |
| `debug` | `(module: Str, message: Str)` | Log at DEBUG level |
| `info` | `(module: Str, message: Str)` | Log at INFO level |
| `warn` | `(module: Str, message: Str)` | Log at WARN level |
| `error` | `(module: Str, message: Str)` | Log at ERROR level |
| `fatal` | `(module: Str, message: Str)` | Log at FATAL level |
| `msg` | `(level: I32, module: Str, message: Str)` | Log at arbitrary level |
| `set_level` | `(level: I32)` | Set minimum log level |
| `get_level` | `() -> I32` | Get current log level |
| `enabled` | `(level: I32) -> Bool` | Check if level is enabled |

#### Output Format

By default, log messages are written to stderr in the format:

```
LEVEL [module] message
```

For example:

```
INFO [server] Listening on port 8080
WARN [pool] Connection pool at 90%
ERROR [db] Query failed, retrying...
```

When running under `tml test`, log messages are captured by the test runner
and routed through the compiler's unified logger, which adds timestamps and
color formatting.

### 4.3 Module Tag Convention

Use short, descriptive module tags:

```tml
log::info("http", "Request received: GET /api/users")
log::debug("cache", "Cache miss for key: session_abc")
log::warn("auth", "Token expires in 5 minutes")
log::error("db", "Connection refused: retrying in 2s")
```

## 5. C++ Compiler Logger API

### 5.1 Logging Macros

```cpp
#include "log/log.hpp"

// Stream-style message formatting
TML_LOG_TRACE("codegen", "Entering function " << name);
TML_LOG_DEBUG("build", "Compiling " << input << " -> " << output);
TML_LOG_INFO("build", "Build complete: " << count << " files");
TML_LOG_WARN("types", "Implicit narrowing: " << from << " -> " << to);
TML_LOG_ERROR("link", "Symbol not found: " << sym);
TML_LOG_FATAL("runtime", "Assertion failed at " << file << ":" << line);
```

### 5.2 Compile-Time Elision

Define `TML_MIN_LOG_LEVEL` to eliminate log calls below a threshold at compile time:

```cpp
// In CMakeLists.txt or build config:
// -DTML_MIN_LOG_LEVEL=2  (eliminates Trace and Debug calls)

TML_LOG_TRACE("x", "this is compiled out");  // Zero overhead
TML_LOG_DEBUG("x", "this too");              // Zero overhead
TML_LOG_INFO("x", "this remains");           // Normal call
```

### 5.3 Initialization

The logger is initialized from CLI flags in `dispatcher.cpp`:

```cpp
auto log_config = tml::log::parse_log_options(argc, argv);
tml::log::Logger::init(log_config);
```

### 5.4 Programmatic Configuration

```cpp
using namespace tml::log;

// Change level at runtime
Logger::instance().set_level(LogLevel::Debug);

// Change module filter
Logger::instance().set_filter("codegen=trace,build=info");

// Add a file sink
auto file_sink = std::make_unique<FileSink>("build.log");
Logger::instance().add_sink(std::move(file_sink));

// Flush all sinks
Logger::instance().flush();
```

## 6. C Runtime Logger API

### 6.1 Usage (from C runtime files)

```c
#include "log.h"

// Convenience macros
RT_TRACE("memory", "Allocated %zu bytes at %p", size, ptr);
RT_DEBUG("text", "SSO threshold: %d bytes", SSO_MAX);
RT_INFO("runtime", "Initialization complete");
RT_WARN("memory", "Program exited with %d leak(s)", leaks);
RT_ERROR("backtrace", "Failed to capture stack trace: %d", err);
RT_FATAL("runtime", "Assertion failed: %s", expr);

// Non-variadic entry point (used by TML FFI)
rt_log_msg(RT_LOG_INFO, "app", "pre-formatted message");

// Level control
rt_log_set_level(RT_LOG_DEBUG);
int level = rt_log_get_level();
if (rt_log_enabled(RT_LOG_TRACE)) { /* ... */ }
```

### 6.2 Callback Bridge

When TML programs run under `tml test`, the test runner sets a callback
so that C runtime log messages flow through the C++ Logger:

```c
// Set by test_runner.cpp at DLL load time
rt_log_set_callback(bridge_callback);
rt_log_set_level(compiler_log_level);
```

Without a callback (standalone programs), messages go directly to stderr.

## 7. Architecture

```
┌─────────────────────────────────────────────────┐
│                 TML Program                      │
│                                                  │
│   use std::log                                   │
│   log::info("app", "Hello")                      │
│         │                                        │
│         ▼                                        │
│   lowlevel { rt_log_msg(2, "app", "Hello") }     │
└─────────────────┬───────────────────────────────┘
                  │ (compiled to LLVM IR call)
                  ▼
┌─────────────────────────────────────────────────┐
│            C Runtime (log.c)                     │
│                                                  │
│   rt_log_msg(level, module, message)             │
│         │                                        │
│         ├─── callback set? ──► C++ Logger         │
│         │         │                              │
│         │         ▼                              │
│         │   Logger::log(level, module, msg)       │
│         │         │                              │
│         │         ├── ConsoleSink (colored)       │
│         │         └── FileSink (optional)         │
│         │                                        │
│         └─── no callback ──► fprintf(stderr)      │
└─────────────────────────────────────────────────┘
```
