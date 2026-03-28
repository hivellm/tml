# TML Unified Logging System Specification

This specification defines the requirements for a unified logging system spanning the C++ compiler, C runtime, and TML standard library, providing structured output with levels, module filtering, multiple sinks, file output, and format customization.

## ADDED Requirements

### Requirement: Log Level Hierarchy
The logging system MUST support 6 log levels in strict ascending severity order: Trace, Debug, Info, Warn, Error, Fatal, where setting a minimum level filters out all messages below that threshold.

#### Scenario: Level filtering
Given the log level is set to Warn
When a Debug message and an Error message are emitted
Then only the Error message SHALL be written to the output sink

#### Scenario: Trace captures everything
Given the log level is set to Trace
When messages at all 6 levels are emitted
Then all 6 messages SHALL be written to the output sink

#### Scenario: Off disables all logging
Given the log level is set to Off
When messages at all 6 levels are emitted
Then no messages SHALL be written to any output sink

### Requirement: Module-Based Filtering
The logging system MUST support per-module log level configuration, allowing different components to log at different verbosity levels using a filter string format.

#### Scenario: Per-module filter
Given the filter is set to "codegen=trace,borrow=debug,*=warn"
When a Trace message from module "codegen" and a Trace message from module "parser" are emitted
Then only the "codegen" Trace message SHALL be written and the "parser" Trace message SHALL be filtered out

#### Scenario: Wildcard default
Given the filter is set to "*=info"
When an Info message from any module is emitted
Then the message SHALL be written regardless of module name

#### Scenario: CLI filter flag
Given the user runs `tml build --log-filter=codegen,mir`
When compilation produces log messages from all modules
Then only messages from "codegen" and "mir" modules SHALL be displayed

### Requirement: Multiple Output Sinks
The logging system MUST support simultaneous output to multiple destinations including console (stderr), files, rotating files, and custom sinks.

#### Scenario: Console and file simultaneously
Given the logger is configured with ConsoleSink and FileSink("build.log")
When log messages are emitted
Then each message SHALL appear both on stderr and in the build.log file

#### Scenario: File rotation
Given a RotatingFileSink with max_size=1MB and max_files=3
When the log file exceeds 1MB
Then the current file SHALL be rotated to a backup name and a new file SHALL be created, keeping at most 3 backup files

#### Scenario: Custom sink
Given a TML program implements the Sink behavior with a custom write method
When the custom sink is registered with the logger
Then log messages SHALL be delivered to the custom sink's write method

### Requirement: File Logging
The logging system MUST support writing log output to files with configurable paths, append mode, and automatic flushing on error-level messages.

#### Scenario: Log file creation
Given the user runs `tml build --log-file=build.log`
When compilation completes
Then a file named build.log SHALL exist containing all log messages at or above the current level

#### Scenario: Append mode
Given a log file already exists from a previous build
When a new build runs with the same `--log-file` path
Then new log messages SHALL be appended to the existing file, not overwrite it

#### Scenario: Auto-flush on error
Given the logger is writing to a file
When an Error or Fatal level message is logged
Then the file sink SHALL flush immediately to ensure the message is persisted

### Requirement: Output Formats
The logging system MUST support at least two output formats: human-readable text with optional ANSI colors, and machine-parseable JSON with one object per line.

#### Scenario: Text format
Given the log format is set to text (default)
When an Info message "Server started on port 8080" is logged from module "server"
Then the output SHALL contain the timestamp, level, module, and message in human-readable format

#### Scenario: JSON format
Given the user runs `tml build --log-format=json`
When log messages are emitted
Then each line of output SHALL be a valid JSON object containing at minimum "ts", "level", "module", and "msg" fields

#### Scenario: Colored console output
Given the output terminal supports ANSI colors
When log messages are written to ConsoleSink
Then each log level SHALL be rendered in its designated color (Info=green, Warn=yellow, Error=red)

### Requirement: CLI Verbosity Flags
The TML compiler MUST support shorthand verbosity flags that map to log levels, maintaining backward compatibility with the existing --verbose flag.

#### Scenario: Single -v flag
Given the user runs `tml build -v`
When compilation runs
Then the log level SHALL be set to Info, equivalent to `--log-level=info`

#### Scenario: Double -vv flag
Given the user runs `tml build -vv`
When compilation runs
Then the log level SHALL be set to Debug

#### Scenario: Triple -vvv flag
Given the user runs `tml build -vvv`
When compilation runs
Then the log level SHALL be set to Trace, showing all internal compiler messages

#### Scenario: Quiet flag
Given the user runs `tml build -q`
When compilation runs
Then only Error and Fatal messages SHALL be displayed

#### Scenario: Backward compatibility
Given the user runs `tml build --verbose` (existing flag)
When compilation runs
Then the behavior SHALL be equivalent to `-v` (log level Info)

### Requirement: Environment Variable Configuration
The logging system MUST support configuration via the TML_LOG environment variable, following the same filter string format used by --log-filter.

#### Scenario: Environment variable level
Given the environment variable `TML_LOG=debug` is set
When `tml build` is run without explicit log flags
Then the log level SHALL be set to Debug

#### Scenario: Environment variable module filter
Given the environment variable `TML_LOG=codegen=trace,borrow=debug` is set
When `tml build` is run without explicit log flags
Then module-specific filtering SHALL be applied as specified

#### Scenario: CLI overrides environment
Given `TML_LOG=debug` is set and the user runs `tml build --log-level=warn`
When compilation runs
Then the CLI flag SHALL take precedence and the log level SHALL be Warn

### Requirement: Thread-Safe Logging
The logging system MUST guarantee that log messages from multiple threads are never interleaved, corrupted, or lost, with minimal contention overhead.

#### Scenario: Concurrent logging
Given 8 threads are logging simultaneously to the same ConsoleSink
When each thread logs 1000 messages
Then all 8000 messages SHALL appear in the output with no interleaving within a single message

#### Scenario: Fast-path for disabled levels
Given the log level is Info and a thread emits a Trace message
When the level check occurs
Then the check SHALL complete without acquiring any lock or mutex

### Requirement: Zero-Overhead Disabled Logging
The C++ compiler logging macros MUST compile to zero machine code when the log level is below the compile-time threshold, ensuring no runtime cost for disabled log levels in release builds.

#### Scenario: Compile-time elision
Given `TML_LOG_LEVEL` is set to `Info` at compile time
When a `TML_TRACE("module", "message")` call exists in the source
Then the compiler SHALL produce no machine instructions for that call in the compiled binary

#### Scenario: Release build overhead
Given the compiler is built in release mode with default log level
When compilation performance is benchmarked
Then disabled log calls SHALL add less than 1% overhead compared to raw print statements

### Requirement: TML Standard Library Logger
The TML standard library MUST provide a `std::log` module that allows TML programs to perform structured logging with the same level, filtering, and sink capabilities as the compiler logger.

#### Scenario: Basic TML logging
Given a TML program contains `use std::log` and calls `log::info("server", "Request handled")`
When the program is compiled and run
Then the log message SHALL appear on stderr with appropriate formatting

#### Scenario: File logging from TML
Given a TML program initializes the logger with a FileSink
When log messages are emitted during program execution
Then the messages SHALL be written to the specified file

#### Scenario: Structured fields
Given a TML program logs with structured fields `log::info("req", "handled"; "method" => "GET", "status" => "200")`
When the JsonFormatter is active
Then the JSON output SHALL include "method" and "status" as additional fields in the JSON object

#### Scenario: Auto-initialization
Given a TML program calls `log::info(...)` without prior `log::init()` call
When the first log message is emitted
Then the logger SHALL auto-initialize with default configuration (ConsoleSink, Info level, TextFormatter)

### Requirement: Diagnostic System Independence
The compiler diagnostic system (error/warning rendering in diagnostic.cpp) MUST remain independent from the logging system, as diagnostics are user-facing structured error reports, not internal log messages.

#### Scenario: Diagnostic output unchanged
Given the logging system is fully integrated
When the compiler encounters a type error
Then the diagnostic output format SHALL remain identical to the current Rust-style rendering with source snippets, labels, and suggestions

#### Scenario: Log level does not affect diagnostics
Given the log level is set to Off (quiet mode)
When the compiler encounters a compilation error
Then the error diagnostic SHALL still be displayed to the user regardless of log level
