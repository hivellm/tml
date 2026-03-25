# Proposal: TML Inspector — Complete Runtime Diagnostics System

## Status: PROPOSED

**Inspired by**: Node.js `inspector` module + Chrome DevTools Protocol (CDP)
**Depends on**: Phase 1 codegen fixes (closures, generics), developer-tooling (LSP)

## Why

TML currently has a **minimal profiler** that generates `.cpuprofile` files with basic function entry/exit timing. This provides almost no useful information for debugging real applications:

- No breakpoint support in running programs
- No heap inspection or memory profiling
- No runtime variable inspection
- No async/concurrent task visibility
- No console protocol for remote debugging
- No way to attach to a running TML process
- No flame graph or allocation tracking
- The existing profiler (`compiler/src/profiler/profiler.cpp`) only tracks call stacks and timing — it cannot inspect values, set breakpoints, or profile memory

Node.js solved this by embedding the V8 Inspector Protocol (based on Chrome DevTools Protocol) directly into the runtime. Any Node.js application can be debugged, profiled, and inspected using Chrome DevTools, VSCode, or any CDP client. TML needs the same level of introspection for its compiled programs.

### Current State Analysis

| Component | Status | Limitations |
|-----------|--------|-------------|
| CPU Profiler (`profiler.cpp`) | Basic | Only function timing, no flame graph, no sampling modes, no hot-path analysis |
| DWARF Debug Info (`debug_info.cpp`) | Basic | DIFile, DICompileUnit, DISubprogram, DILocation, DIBasicType only. No composite types, no variable location tracking through optimizations |
| Coverage (`coverage.cpp`) | Working | Uses LLVM `llvm-profdata`/`llvm-cov`, function-level coverage. No branch coverage |
| PGO (`pgo.cpp`) | Implemented | Block/edge counts, but not connected to user-facing diagnostics |
| Memory Leak Detection | Working | Allocation tracking with leak report at exit, but no heap snapshots |
| Phase Timing (`run_profiled.cpp`) | Working | Compiler phase timing only (lexer, parser, typecheck, codegen, link) |
| Interactive Debugger | Spec only | `docs/specs/11-DEBUG.md` describes `tml debug` but nothing is implemented |
| Log System | Spec only | Described in spec but not implemented in runtime |

### Key Gap

The profiler C API (`tml_profiler_enter`/`tml_profiler_exit`) exists but **codegen never emits calls to it**. The instrumentation infrastructure is half-built — the runtime side exists but the compiler doesn't generate the instrumentation code. This is the pattern throughout: specifications exist, infrastructure is partially built, but nothing is connected end-to-end.

## Design Philosophy

### Principles

1. **Zero-cost when disabled** — Inspector features have ZERO overhead when not active. No conditional checks, no function call overhead. Achieved via compile-time flags and runtime activation.

2. **Protocol-first** — Use the Chrome DevTools Protocol (CDP) as the wire protocol. This gives free compatibility with Chrome DevTools, VSCode debugger, and hundreds of other tools.

3. **Embedded, not external** — The inspector is part of the TML runtime, not an external tool. Like Node.js, any TML program can be started with `--inspect` to enable debugging.

4. **Pure TML where possible** — Following the project's migration philosophy, implement as much as possible in TML. Only use C/C++ for the WebSocket server, OS-level I/O, and the low-level debug trap interface.

5. **Incremental adoption** — Each domain (Profiler, Debugger, HeapProfiler, Runtime, Console) can be implemented independently. The system is useful even with just one domain working.

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                      TML Application                         │
│  ┌──────────┐  ┌──────────┐  ┌───────────┐  ┌────────────┐ │
│  │ User Code│  │ Std Lib  │  │ Profiler  │  │ Allocator  │ │
│  └────┬─────┘  └────┬─────┘  └─────┬─────┘  └─────┬──────┘ │
│       │              │              │              │         │
│  ┌────┴──────────────┴──────────────┴──────────────┴──────┐ │
│  │              TML Inspector Agent (embedded)             │ │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────┐ │ │
│  │  │ Debugger │ │ Profiler │ │   Heap   │ │  Runtime  │ │ │
│  │  │ Domain   │ │ Domain   │ │ Profiler │ │  Domain   │ │ │
│  │  └────┬─────┘ └────┬─────┘ └────┬─────┘ └─────┬─────┘ │ │
│  │       └──────┬──────┴──────┬─────┘             │       │ │
│  │              │  CDP Router │                    │       │ │
│  │              └──────┬──────┘                    │       │ │
│  │                     │                           │       │ │
│  │              ┌──────┴──────┐              ┌─────┴─────┐ │ │
│  │              │  WebSocket  │              │  Console  │ │ │
│  │              │   Server    │              │  Channel  │ │ │
│  │              └──────┬──────┘              └───────────┘ │ │
│  └─────────────────────┼──────────────────────────────────┘ │
└─────────────────────────┼──────────────────────────────────┘
                          │
                   ws://127.0.0.1:9229
                          │
              ┌───────────┴───────────┐
              │  Chrome DevTools      │
              │  VSCode Debugger      │
              │  Any CDP Client       │
              │  tml inspect CLI      │
              └───────────────────────┘
```

## What Changes

### Phase 1: Inspector Core & WebSocket Server

The foundation: a WebSocket server embedded in the TML runtime that speaks CDP.

**Compiler changes:**
- New `--inspect` flag: `tml run program.tml --inspect` (starts WebSocket server on port 9229)
- New `--inspect-brk` flag: pauses execution at first line until debugger connects
- New `--inspect-port=PORT` flag: custom port
- Compile with `-g` automatically when `--inspect` is used

**Runtime additions (C — minimal, this is the OS interface layer):**
- WebSocket server using OS sockets (no external dependencies)
- CDP JSON-RPC message router
- Session management (multiple simultaneous debugger connections)

**TML library additions:**
- `std::inspector` module — TML API for programmatic inspector access
- `Inspector.open(port, host, wait)` — activate inspector at runtime
- `Inspector.close()` — deactivate
- `Inspector.url()` — get WebSocket URL
- `Inspector.wait_for_debugger()` — block until client connects

### Phase 2: Profiler Domain (Enhanced CPU Profiling)

Replace the current basic profiler with a CDP-compatible Profiler domain.

**CDP methods to implement:**
- `Profiler.enable` / `Profiler.disable`
- `Profiler.start` / `Profiler.stop` → returns CDP `Profile` object
- `Profiler.setSamplingInterval(interval_us)`
- `Profiler.startPreciseCoverage(callCount, detailed)` / `Profiler.stopPreciseCoverage`
- `Profiler.takePreciseCoverage` → returns `ScriptCoverage[]`
- `Profiler.getBestEffortCoverage`

**CDP events to implement:**
- `Profiler.consoleProfileStarted`
- `Profiler.consoleProfileFinished`

**Compiler codegen changes:**
- Emit `tml_profiler_enter`/`tml_profiler_exit` calls when profiling enabled (currently missing!)
- Add sampling mode: separate profiler thread samples the main thread's call stack at intervals
- Generate `.cpuprofile` with complete data: nodes with `positionTicks`, `deoptReason`, proper `timeDeltas`

**Enhanced output formats:**
- Chrome DevTools compatible `.cpuprofile` (existing format, enhanced data)
- Flame graph SVG generation (`tml profile flamegraph`)
- Terminal flame graph (ASCII)
- JSON export for custom analysis

### Phase 3: Runtime Domain (Execution Context & Object Inspection)

Enable inspection of running program state.

**CDP methods to implement:**
- `Runtime.enable` / `Runtime.disable` — execution context reporting
- `Runtime.evaluate(expression)` — evaluate expression in current context
- `Runtime.getProperties(objectId)` — inspect object properties
- `Runtime.getHeapUsage` — current heap statistics
- `Runtime.callFunctionOn(objectId, functionDeclaration)` — call method on object
- `Runtime.releaseObject(objectId)` / `Runtime.releaseObjectGroup`
- `Runtime.globalLexicalScopeNames` — list global variables

**CDP events to implement:**
- `Runtime.consoleAPICalled` — console.log/error/warn output
- `Runtime.exceptionThrown` — unhandled exceptions/panics
- `Runtime.executionContextCreated` / `Runtime.executionContextDestroyed`

**Runtime infrastructure:**
- Object mirror system: safely inspect TML values through `RemoteObject` protocol
- Expression evaluator: parse and evaluate simple TML expressions at breakpoints
- Exception tracking: capture panic info and stack traces in CDP format

**Requires**: Reflection system (see `implement-reflection` task) for property enumeration

### Phase 4: Debugger Domain (Breakpoints & Stepping)

Full interactive debugging support.

**CDP methods to implement:**
- `Debugger.enable` / `Debugger.disable`
- `Debugger.setBreakpoint(location)` / `Debugger.setBreakpointByUrl`
- `Debugger.removeBreakpoint(breakpointId)`
- `Debugger.setBreakpointsActive(active)`
- `Debugger.pause` / `Debugger.resume`
- `Debugger.stepInto` / `Debugger.stepOut` / `Debugger.stepOver`
- `Debugger.evaluateOnCallFrame(callFrameId, expression)`
- `Debugger.setVariableValue(scopeNumber, variableName, newValue)`
- `Debugger.getScriptSource(scriptId)`
- `Debugger.setPauseOnExceptions(state)` — `none`, `uncaught`, `all`
- `Debugger.setAsyncCallStackDepth(maxDepth)`
- `Debugger.getPossibleBreakpoints(start, end)`

**CDP events to implement:**
- `Debugger.paused` — breakpoint/exception/debugger statement hit
- `Debugger.resumed` — execution continues
- `Debugger.scriptParsed` — source file loaded

**Compiler changes:**
- Emit software breakpoint traps (`int3` on x86_64) at marked locations
- Emit breakpoint location table in debug metadata
- Generate scope information for local variable inspection
- Support `@breakpoint` directive in source code

**Runtime infrastructure:**
- Debug trap handler (OS signal handler for `SIGTRAP` / `EXCEPTION_BREAKPOINT`)
- Breakpoint table: maps source locations to instruction addresses
- Call frame walker: enumerate stack frames with local variable access
- Scope chain builder: nested scopes with variable visibility

### Phase 5: HeapProfiler Domain (Memory Inspection)

Deep memory analysis beyond the current leak detection.

**CDP methods to implement:**
- `HeapProfiler.enable` / `HeapProfiler.disable`
- `HeapProfiler.takeHeapSnapshot` — full heap dump
- `HeapProfiler.startSampling` / `HeapProfiler.stopSampling` / `HeapProfiler.getSamplingProfile`
- `HeapProfiler.startTrackingHeapObjects` / `HeapProfiler.stopTrackingHeapObjects`
- `HeapProfiler.getHeapObjectId(objectId)`
- `HeapProfiler.getObjectByHeapObjectId(heapObjectId)`
- `HeapProfiler.collectGarbage` — force cleanup (for TML: force drop checks)

**CDP events to implement:**
- `HeapProfiler.addHeapSnapshotChunk` — stream heap snapshot data
- `HeapProfiler.heapStatsUpdate` — periodic heap statistics
- `HeapProfiler.reportHeapSnapshotProgress`

**Runtime infrastructure:**
- Heap walker: enumerate all live allocations with type info
- Allocation stack traces: record call stack at each `mem_alloc`
- Allocation sampling: sample allocations at configurable rate (default every 32KB)
- Heap snapshot format: V8-compatible `.heapsnapshot` for Chrome DevTools
- Retention graph: track which objects reference which (requires GC metadata or manual tracking)

**Integration with existing leak detection:**
- Current `mem_track.h` already tracks allocations — extend with stack traces
- Current leak report at exit becomes a subset of HeapProfiler functionality

### Phase 6: Console Domain & Structured Logging

Replace the spec-only logging with a real implementation connected to CDP.

**CDP methods to implement:**
- `Console.enable` / `Console.disable`
- `Console.clearMessages`

**TML API (`std::console`):**
- `console.log(args...)` — general output, sends `Runtime.consoleAPICalled` event
- `console.error(args...)` — error output
- `console.warn(args...)` — warning output
- `console.debug(args...)` — debug output
- `console.trace(args...)` — output with stack trace
- `console.time(label)` / `console.time_end(label)` — timing
- `console.count(label)` / `console.count_reset(label)` — call counting
- `console.group(label)` / `console.group_end()` — output grouping
- `console.table(data)` — tabular output
- `console.assert(condition, message)` — conditional logging

**Integration:**
- All console output goes to both stdout AND the CDP Console domain
- When inspector is not active, console functions are zero-cost wrappers around `print()`
- JSON format for structured log output (`--log-format=json`)
- Log level filtering via `TML_LOG` environment variable

### Phase 7: `tml inspect` CLI Tool

A terminal-based inspector client (like Node.js's `node inspect`).

- `tml inspect program.tml` — launch with debugger, REPL interface
- `tml inspect --port=9229` — connect to running process
- Commands: `break`, `continue`, `step`, `next`, `out`, `backtrace`, `print`, `watch`, `heap`
- REPL: evaluate TML expressions in paused context
- Colorized output with source code display
- Built on top of the CDP WebSocket protocol (connects to the inspector server)

### Phase 8: Concurrency Inspection (Future)

When TML gets async/threading support:

- Thread/task listing with state
- Per-thread call stacks
- Lock contention visualization
- Async task tree visualization
- Deadlock detection and reporting

## Implementation Strategy

### What's in C/C++ (Minimal)

| Component | Reason |
|-----------|--------|
| WebSocket server | OS sockets, HTTP upgrade, frame encoding |
| Debug trap handler | OS signal handling (`SIGTRAP`, structured exception handling) |
| Heap walker | Unsafe pointer traversal |
| Stack frame walker | Platform-specific unwinding (SEH on Windows, DWARF on Linux) |

### What's in TML (Everything Else)

| Component | Implementation |
|-----------|---------------|
| CDP message router | JSON parsing + method dispatch |
| Profiler domain | Call stack tracking, timing, CDP response formatting |
| Runtime domain | Object mirrors, expression evaluation |
| Console domain | Logging API, CDP event emission |
| Inspector API | `std::inspector` module |
| Heap snapshot format | V8 `.heapsnapshot` JSON generation |
| CLI inspect tool | CDP client + REPL |

### Compiler Codegen Changes

1. **Profiler instrumentation** (`--profile` flag):
   - Emit `call @tml_profiler_enter(func_name, file, line)` at function entry
   - Emit `call @tml_profiler_exit()` at every return point
   - Gate behind runtime check: `if (tml_profiler_is_active()) { ... }`

2. **Debug metadata** (`-g` flag, enhanced):
   - Emit `DICompositeType` for structs and enums (currently only `DIBasicType`)
   - Emit `DILocalVariable` with proper `DIExpression` for all locals
   - Emit `DIDerivedType` for references, pointers, slices
   - Emit scope nesting for block-level variable visibility

3. **Breakpoint support** (`--inspect` flag):
   - Emit breakpoint location table as module metadata
   - Emit `@llvm.debugtrap()` at user-marked breakpoints (`@breakpoint` directive)
   - Software breakpoint insertion via runtime patching (for dynamic breakpoints)

4. **Inspector activation** (`--inspect` flag):
   - Emit call to `tml_inspector_init(port, host)` at program entry (before `main()`)
   - Emit call to `tml_inspector_shutdown()` at program exit (after `main()`)
   - If `--inspect-brk`: emit `tml_inspector_wait_for_debugger()` before `main()`

## Success Criteria

| Milestone | Verification |
|-----------|-------------|
| Phase 1 complete | `tml run program.tml --inspect` opens WebSocket, Chrome DevTools connects |
| Phase 2 complete | CPU profile visible in Chrome DevTools Performance tab with source locations |
| Phase 3 complete | Variables inspectable in Chrome DevTools Console |
| Phase 4 complete | Breakpoints work in Chrome DevTools Sources tab |
| Phase 5 complete | Heap snapshot loadable in Chrome DevTools Memory tab |
| Phase 6 complete | `console.log()` output visible in Chrome DevTools Console |
| Phase 7 complete | `tml inspect program.tml` provides interactive debugging in terminal |

## References

- [Node.js Inspector API](https://nodejs.org/docs/latest/api/inspector.html)
- [Chrome DevTools Protocol](https://chromedevtools.github.io/devtools-protocol/v8/)
- [V8 CPU Profile Format](https://chromedevtools.github.io/devtools-protocol/v8/Profiler/)
- [V8 Heap Snapshot Format](https://chromedevtools.github.io/devtools-protocol/v8/HeapProfiler/)
- [CDP Debugger Domain](https://chromedevtools.github.io/devtools-protocol/v8/Debugger/)
- [CDP Runtime Domain](https://chromedevtools.github.io/devtools-protocol/v8/Runtime/)
- TML Debug Spec: `docs/specs/11-DEBUG.md`
- TML Profiler: `compiler/src/profiler/profiler.cpp`
- TML Debug Info: `compiler/src/codegen/llvm/core/debug_info.cpp`
- TML Coverage: `compiler/src/cli/tester/coverage.cpp`
- TML PGO: `compiler/src/mir/passes/pgo.cpp`
