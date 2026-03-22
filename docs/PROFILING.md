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

## Example

See `examples/profiling_demo.tml` for a complete example.
