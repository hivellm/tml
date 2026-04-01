# TML Profiling Guide

## Overview

TML integrates with [Tracy Profiler](https://github.com/wolfpld/tracy) for real-time performance analysis of both the compiler (C++) and TML programs.

## Quick Start

### 1. Build with Profiling

```bash
scripts\build.bat --profile
```

This defines `TML_PROFILE` which enables Tracy zones in the compiler.

### 2. Run Tracy Viewer

Download Tracy from https://github.com/wolfpld/tracy/releases and run the viewer. It will auto-connect when a profiled program starts.

### 3. Run Your Program

```bash
build\debug\bin\tml.exe run your_program.tml
```

The Tracy viewer will show the compiler pipeline zones (lexer, parser, typechecker, HIR, MIR, codegen, LLVM, LLD).

## TML Runtime Profiling

Add profiler zones to your TML code:

```tml
use core::profiler

func handle_request(req: IncomingMessage) -> Str {
    let z = profiler::begin("handle_request")
    let response = do_work()
    profiler::end(z)
    return response
}
```

### Available Functions

| Function | Description |
|----------|-------------|
| `profiler::begin(name)` | Start a named zone, returns zone ID |
| `profiler::end(zone_id)` | End a zone |
| `profiler::message(text)` | Log message to Tracy timeline |
| `profiler::plot(name, value)` | Plot a value (time-series graph) |
| `profiler::frame_mark()` | Mark frame boundary |

### Zero Overhead

When built without `--profile`, all profiler functions are no-ops (return 0 or do nothing). There is zero runtime overhead in normal builds.

## Compiler Pipeline Zones

When profiling the compiler, these zones appear in Tracy:

| Zone | File | Description |
|------|------|-------------|
| `lexer::tokenize` | lexer_utils.cpp | Tokenization with file name |
| `parser::parse` | parser_core.cpp | Parsing |
| `types::check_module` | checker/core.cpp | Type checking |
| `hir::lower_module` | hir_builder.cpp | HIR lowering |
| `mir::build` | hir_mir_builder.cpp | MIR building |
| `mir::pass_manager::run` | mir_pass.cpp | MIR optimization |
| `mir::pass` | mir_pass.cpp | Individual MIR pass (with name) |
| `codegen::generate` | mir_codegen.cpp | LLVM IR generation |
| `llvm::compile_ir_to_object` | llvm_backend.cpp | IR to object file |
| `lld::link` | lld_linker.cpp | Object linking |
| `query::typecheck_module` | query_context.cpp | Query-level typecheck |
| `query::codegen_unit` | query_context.cpp | Query-level codegen |
| `test::run_tests` | testing_coordinator.cpp | Test execution |

## HTTP Server Zones

When profiling an HTTP server:

| Zone | Description |
|------|-------------|
| `http::connection` | Full connection lifecycle (accept → close) |
| `http::request` | Per-request processing (parse → route → handler → response) |

## Flame Graphs

The `tml profile flamegraph` command converts a `.cpuprofile` file into a visual flame graph.

```bash
# ASCII flame graph printed to terminal
tml profile flamegraph my_program.cpuprofile

# Interactive SVG with tooltips and dark theme
tml profile flamegraph my_program.cpuprofile -o flamegraph.svg
```

Open the SVG in a browser to explore the call tree interactively — hover over frames to see function names, self time, and total time.

## MIR Codegen Instrumentation

When `--profile` is passed at compile time, the compiler emits `tml_profiler_enter` / `tml_profiler_exit` calls at every function boundary in the MIR codegen stage. These calls are gated by a `tml_profiler_is_active()` runtime check and compile to a single branch — zero overhead when the profiler is not running.

This replaces the need for manual `profiler::enter` / `profiler::exit` calls in most programs: building with `--profile` instruments all functions automatically.

```bash
tml run my_program.tml --profile
```

## positionTicks in .cpuprofile

The `.cpuprofile` output now includes a `positionTicks` array on every call frame. Each entry records the source line number and the number of profiler samples that hit that line, enabling line-level hotspot identification in Chrome DevTools and VS Code.

## Chrome DevTools Inspector

For interactive debugging with the Chrome DevTools UI, use the inspector:

```bash
# Start with inspector enabled (port 9229)
tml run my_program.tml --inspect

# Break before user code and wait for debugger to attach
tml run my_program.tml --inspect-brk

# Custom port
tml run my_program.tml --inspect-port=9230
```

Open `chrome://inspect` in Chrome and click "inspect" next to the TML process. The Runtime, Profiler, Debugger, HeapProfiler, and Console CDP domains are all available.

## `tml inspect` Terminal Debugger

The `tml inspect` command provides a built-in terminal REPL debugger without requiring a browser:

```bash
tml inspect my_program.tml
```

Available REPL commands:

| Command | Description |
|---------|-------------|
| `break <file:line>` | Set a breakpoint |
| `continue` | Resume execution |
| `step` | Step into next call |
| `next` | Step over next statement |
| `out` | Step out of current function |
| `backtrace` | Print call stack |
| `print <expr>` | Evaluate and print expression |
| `locals` | List local variables |
| `watch <expr>` | Add watch expression |
| `heap` | Show heap usage |
| `profile start/stop` | Start or stop profiler |
| `help` | Show all commands |
| `quit` | Exit debugger |

Source lines are displayed with ANSI highlighting at each breakpoint pause.

## Example

See `examples/profiling_demo.tml` for a complete example.
