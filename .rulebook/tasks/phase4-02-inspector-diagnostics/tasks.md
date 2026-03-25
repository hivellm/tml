# Tasks: TML Inspector — Complete Runtime Diagnostics System

**Status**: Proposed (0%)
**Priority**: Medium (depends on Phase 1 codegen fixes)
**Depends on**: `test-failures` (closures/generics), `implement-reflection` (object inspection), `developer-tooling` (LSP)

## Phase 1: Inspector Core & WebSocket Server

- [ ] 1.1 Add `--inspect`, `--inspect-brk`, `--inspect-port=PORT` CLI flags to `cmd_run` and `cmd_build`
- [ ] 1.2 Implement minimal WebSocket server in C (RFC 6455: handshake, frame encode/decode, ping/pong)
- [ ] 1.3 Implement CDP JSON-RPC message router (method dispatch, id tracking, event emission)
- [ ] 1.4 Implement session management (connect/disconnect, multiple clients)
- [ ] 1.5 Implement `tml_inspector_init(port, host)` C API called at program startup
- [ ] 1.6 Implement `tml_inspector_shutdown()` C API called at program exit
- [ ] 1.7 Implement `tml_inspector_wait_for_debugger()` for `--inspect-brk` mode
- [ ] 1.8 Create `std::inspector` TML module with `Inspector.open()`, `.close()`, `.url()`, `.wait_for_debugger()`
- [ ] 1.9 Auto-enable `-g` debug info when `--inspect` is used
- [ ] 1.10 Print inspector URL on startup: `Debugger listening on ws://127.0.0.1:9229/...`
- [ ] 1.11 Verify Chrome DevTools connects and receives initial handshake

## Phase 2: Profiler Domain (Enhanced CPU Profiling)

- [ ] 2.1 Implement `Profiler.enable` / `Profiler.disable` CDP handlers
- [ ] 2.2 Implement `Profiler.start` / `Profiler.stop` returning CDP `Profile` object
- [ ] 2.3 Implement `Profiler.setSamplingInterval(interval_us)` CDP handler
- [ ] 2.4 Emit `tml_profiler_enter`/`tml_profiler_exit` calls in codegen when `--profile` flag is set (currently missing)
- [ ] 2.5 Add runtime profiler activation check (`tml_profiler_is_active()` gate in codegen)
- [ ] 2.6 Implement sampling profiler mode (separate thread, configurable interval)
- [ ] 2.7 Generate `positionTicks` data in `.cpuprofile` output (line-level hit counts)
- [ ] 2.8 Implement `Profiler.startPreciseCoverage` / `Profiler.stopPreciseCoverage` / `Profiler.takePreciseCoverage`
- [ ] 2.9 Implement `Profiler.getBestEffortCoverage` CDP handler
- [ ] 2.10 Implement `Profiler.consoleProfileStarted` / `Profiler.consoleProfileFinished` events
- [ ] 2.11 Generate flame graph SVG from profile data (`tml profile flamegraph <file>`)
- [ ] 2.12 Generate terminal ASCII flame graph
- [ ] 2.13 Verify `.cpuprofile` loads correctly in Chrome DevTools Performance tab
- [ ] 2.14 Verify `.cpuprofile` loads correctly in VSCode JavaScript Profiler extension

## Phase 3: Runtime Domain (Execution Context & Object Inspection)

- [ ] 3.1 Implement `Runtime.enable` / `Runtime.disable` CDP handlers
- [ ] 3.2 Implement `Runtime.executionContextCreated` / `Runtime.executionContextDestroyed` events
- [ ] 3.3 Implement object mirror system (`RemoteObject` protocol for TML values)
- [ ] 3.4 Implement `Runtime.getProperties(objectId)` — enumerate struct fields, enum variants
- [ ] 3.5 Implement `Runtime.getHeapUsage` — current allocation statistics from `mem_track`
- [ ] 3.6 Implement `Runtime.consoleAPICalled` event — forward console output to CDP
- [ ] 3.7 Implement `Runtime.exceptionThrown` event — capture panics as CDP exceptions
- [ ] 3.8 Implement `Runtime.evaluate(expression)` — parse and evaluate simple TML expressions
- [ ] 3.9 Implement `Runtime.callFunctionOn(objectId, functionDeclaration)`
- [ ] 3.10 Implement `Runtime.releaseObject` / `Runtime.releaseObjectGroup` for mirror cleanup
- [ ] 3.11 Implement `Runtime.globalLexicalScopeNames` — list module-level variables
- [ ] 3.12 Verify variables inspectable in Chrome DevTools Console tab

## Phase 4: Debugger Domain (Breakpoints & Stepping)

- [ ] 4.1 Implement `Debugger.enable` / `Debugger.disable` CDP handlers
- [ ] 4.2 Implement `Debugger.scriptParsed` event — emit for each loaded source file
- [ ] 4.3 Implement `Debugger.getScriptSource(scriptId)` — return TML source
- [ ] 4.4 Implement `Debugger.setBreakpoint(location)` / `Debugger.setBreakpointByUrl`
- [ ] 4.5 Implement `Debugger.removeBreakpoint(breakpointId)`
- [ ] 4.6 Implement `Debugger.setBreakpointsActive(active)`
- [ ] 4.7 Implement debug trap handler (SIGTRAP on Linux, EXCEPTION_BREAKPOINT on Windows)
- [ ] 4.8 Implement breakpoint location table in debug metadata (source line → instruction address)
- [ ] 4.9 Emit `@llvm.debugtrap()` for `@breakpoint` directive in TML source
- [ ] 4.10 Implement dynamic breakpoint insertion via runtime code patching
- [ ] 4.11 Implement `Debugger.pause` / `Debugger.resume`
- [ ] 4.12 Implement `Debugger.stepInto` / `Debugger.stepOut` / `Debugger.stepOver`
- [ ] 4.13 Implement `Debugger.paused` event with call frames, reason, hit breakpoints
- [ ] 4.14 Implement `Debugger.resumed` event
- [ ] 4.15 Implement call frame walker (enumerate stack frames with locals)
- [ ] 4.16 Implement scope chain builder (nested scopes with variable visibility)
- [ ] 4.17 Implement `Debugger.evaluateOnCallFrame(callFrameId, expression)`
- [ ] 4.18 Implement `Debugger.setVariableValue(scopeNumber, variableName, newValue)`
- [ ] 4.19 Implement `Debugger.setPauseOnExceptions(state)` — none, uncaught, all
- [ ] 4.20 Implement `Debugger.setAsyncCallStackDepth(maxDepth)` (for future async support)
- [ ] 4.21 Implement `Debugger.getPossibleBreakpoints(start, end)` — valid breakpoint locations
- [ ] 4.22 Enhance DWARF debug info: emit `DICompositeType` for structs/enums
- [ ] 4.23 Enhance DWARF debug info: emit `DIDerivedType` for references, pointers, slices
- [ ] 4.24 Enhance DWARF debug info: emit proper scope nesting for block-level variables
- [ ] 4.25 Verify breakpoints work in Chrome DevTools Sources tab

## Phase 5: HeapProfiler Domain (Memory Inspection)

- [ ] 5.1 Implement `HeapProfiler.enable` / `HeapProfiler.disable` CDP handlers
- [ ] 5.2 Extend `mem_track` to record allocation call stacks (configurable depth)
- [ ] 5.3 Implement allocation sampling mode (sample every N bytes, default 32KB)
- [ ] 5.4 Implement `HeapProfiler.startSampling` / `HeapProfiler.stopSampling` / `HeapProfiler.getSamplingProfile`
- [ ] 5.5 Implement heap walker — enumerate all live allocations with type metadata
- [ ] 5.6 Implement `HeapProfiler.takeHeapSnapshot` — full heap dump in V8 format
- [ ] 5.7 Implement `HeapProfiler.addHeapSnapshotChunk` event — stream snapshot data
- [ ] 5.8 Implement `HeapProfiler.reportHeapSnapshotProgress` event
- [ ] 5.9 Implement `HeapProfiler.startTrackingHeapObjects` / `HeapProfiler.stopTrackingHeapObjects`
- [ ] 5.10 Implement `HeapProfiler.heapStatsUpdate` event — periodic heap statistics
- [ ] 5.11 Implement `HeapProfiler.getHeapObjectId` / `HeapProfiler.getObjectByHeapObjectId`
- [ ] 5.12 Implement `HeapProfiler.collectGarbage` — force drop checks / cleanup
- [ ] 5.13 Generate V8-compatible `.heapsnapshot` format for Chrome DevTools Memory tab
- [ ] 5.14 Verify heap snapshot loads in Chrome DevTools Memory tab

## Phase 6: Console Domain & Structured Logging

- [ ] 6.1 Implement `Console.enable` / `Console.disable` CDP handlers
- [ ] 6.2 Implement `Console.clearMessages` CDP handler
- [ ] 6.3 Create `std::console` TML module with `log`, `error`, `warn`, `debug`, `trace` functions
- [ ] 6.4 Implement `console.time(label)` / `console.time_end(label)` timing helpers
- [ ] 6.5 Implement `console.count(label)` / `console.count_reset(label)` call counting
- [ ] 6.6 Implement `console.group(label)` / `console.group_end()` output grouping
- [ ] 6.7 Implement `console.table(data)` tabular output
- [ ] 6.8 Implement `console.assert(condition, message)` conditional logging
- [ ] 6.9 Forward all console output to CDP `Runtime.consoleAPICalled` event when inspector active
- [ ] 6.10 Implement log level filtering via `TML_LOG` environment variable
- [ ] 6.11 Implement `--log-format=json` flag for structured JSON log output
- [ ] 6.12 Verify console output visible in Chrome DevTools Console tab

## Phase 7: `tml inspect` CLI Tool

- [ ] 7.1 Add `tml inspect` command to CLI dispatcher
- [ ] 7.2 Implement CDP WebSocket client (connect to `ws://host:port`)
- [ ] 7.3 Implement REPL loop with command parsing
- [ ] 7.4 Implement `break <location>` command (set breakpoint via CDP)
- [ ] 7.5 Implement `continue`, `step`, `next`, `out` commands (execution control)
- [ ] 7.6 Implement `backtrace` command (stack trace display)
- [ ] 7.7 Implement `print <expr>` command (evaluate expression via CDP)
- [ ] 7.8 Implement `locals` command (show local variables in current frame)
- [ ] 7.9 Implement `watch <expr>` command (watchpoint support)
- [ ] 7.10 Implement `heap` command (heap statistics summary)
- [ ] 7.11 Implement `profile start` / `profile stop` commands (CPU profiling via CDP)
- [ ] 7.12 Colorized source code display at breakpoints
- [ ] 7.13 Tab completion for commands, variable names, function names
- [ ] 7.14 Verify `tml inspect program.tml` provides interactive debugging session

## Phase 8: Concurrency Inspection (Future — depends on async/threading)

- [ ] 8.1 Thread/task listing with current state
- [ ] 8.2 Per-thread call stack inspection
- [ ] 8.3 Lock contention visualization
- [ ] 8.4 Async task tree visualization
- [ ] 8.5 Deadlock detection and reporting
- [ ] 8.6 Thread-specific breakpoints

## Validation

- [ ] V.1 `tml run --inspect program.tml` starts WebSocket server, Chrome DevTools connects
- [ ] V.2 CPU profile visible in Chrome DevTools Performance tab with correct source mapping
- [ ] V.3 Variables inspectable in Chrome DevTools Console
- [ ] V.4 Breakpoints settable and hittable in Chrome DevTools Sources tab
- [ ] V.5 Heap snapshot loadable in Chrome DevTools Memory tab
- [ ] V.6 `console.log()` output visible in Chrome DevTools Console
- [ ] V.7 `tml inspect` provides usable terminal debugging experience
- [ ] V.8 Zero overhead when inspector is not enabled (benchmark comparison)
- [ ] V.9 All inspector features work on both Windows and Linux
