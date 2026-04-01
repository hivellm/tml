# TML Standard Library: Profiler

> `std::profiler` -- Runtime profiling with .cpuprofile output (Chrome DevTools compatible).

## Overview

Provides runtime profiling capabilities that export to `.cpuprofile` format, compatible with Chrome DevTools and VS Code. All core functions use FFI (`@extern`) to the TML profiler runtime. Convenience functions built on top combine initialization and control into single calls.

Use `init` + `start`/`stop` for full control, or `begin`/`stop` for quick setup. Manual instrumentation with `enter`/`exit` marks individual function boundaries. Use `section`/`end_section` for conditional profiling that only records when the profiler is active.

## Import

```tml
use std::profiler
```

---

## Initialization and Control

```tml
@extern func init(output_path: Str)       // Set output file path
@extern func start()                       // Begin profiling
@extern func stop()                        // Stop and write .cpuprofile
@extern func is_active() -> I32            // 1 if active, 0 otherwise
```

## Manual Instrumentation

```tml
@extern func enter(func_name: Str, file_name: Str, line: I32)  // Record function entry
@extern func exit()                                              // Record function exit
@extern func sample()                                            // Add a sample point
```

`enter` and `exit` must be paired. Use `sample()` in hot loops for sampling-based profiling without full instrumentation.

## Convenience Functions

```tml
func begin(output_path: Str)    // init() + start() in one call
func begin_default()            // begin("profile.cpuprofile")
func section(name: Str, file: Str, line: I32)  // Conditional enter (only if active)
func end_section()              // Conditional exit (only if active)
```

---

## Example

```tml
use std::profiler

func main() -> I32 {
    profiler::init("my_program.cpuprofile")
    profiler::start()

    do_work()

    profiler::stop()
    return 0
}

func do_work() {
    profiler::enter("do_work", "main.tml", 10)

    // ... computation ...

    profiler::exit()
}
```

Open the resulting `.cpuprofile` file in Chrome DevTools (Performance tab) or VS Code to visualize the call tree and timing data.

---

## Flame Graph Generation

Use the CLI to convert any `.cpuprofile` into a flame graph without opening a browser:

```bash
# ASCII output (terminal)
tml profile flamegraph my_program.cpuprofile

# Interactive SVG with dark theme and tooltips
tml profile flamegraph my_program.cpuprofile -o flamegraph.svg
```

---

## Automatic MIR Instrumentation

Build with `--profile` to instrument every function automatically — no manual `enter`/`exit` calls required:

```bash
tml run my_program.tml --profile
```

The compiler emits `tml_profiler_enter` / `tml_profiler_exit` calls at every function boundary during MIR codegen. A `tml_profiler_is_active()` guard ensures zero overhead when the profiler is inactive.

---

## positionTicks

`.cpuprofile` files produced by TML include a `positionTicks` array on every call frame, recording per-line sample counts. This enables line-level hotspot highlighting in Chrome DevTools and VS Code.

---

## Inspector Integration

The profiler domain is also accessible through the Chrome DevTools Protocol inspector. See [PROFILING.md](../PROFILING.md) for how to use `tml inspect` and `--inspect` / `--inspect-brk` flags.
