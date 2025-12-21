# TML Standard Library: Logging

> `std.log` — Structured logging framework.

## Overview

The log package provides a flexible, structured logging framework with support for multiple outputs, log levels, and formatting options.

**Capability**: `io.file` (optional, for file output)

## Import

```tml
import std.log
import std.log.{Logger, Level, info, warn, error}
```

---

## Quick Start

```tml
import std.log.{info, warn, error, debug, trace}

func main() {
    info("Application started")
    debug("Debug message", count = 42)
    warn("Warning message", component = "database")
    error("Error occurred", err = "connection failed", retry = 3)
}
```

---

## Log Levels

```tml
/// Log severity levels
public type Level =
    | Trace   // Most verbose
    | Debug   // Development information
    | Info    // General information
    | Warn    // Warnings
    | Error   // Errors

extend Level {
    /// Returns the numeric value (higher = more severe)
    public func as_u8(this) -> U8 {
        when this {
            Trace -> 0,
            Debug -> 1,
            Info -> 2,
            Warn -> 3,
            Error -> 4,
        }
    }

    /// Creates from string
    public func from_str(s: ref String) -> Maybe[Level] {
        when s.to_lowercase().as_str() {
            "trace" -> Just(Trace),
            "debug" -> Just(Debug),
            "info" -> Just(Info),
            "warn" | "warning" -> Just(Warn),
            "error" -> Just(Error),
            _ -> Nothing,
        }
    }
}

implement Ord for Level {
    func cmp(this, other: &Level) -> Ordering {
        this.as_u8().cmp(&other.as_u8())
    }
}
```

---

## Logging Macros

```tml
/// Log at trace level
public macro trace! {
    ($msg:expr $(, $key:ident = $value:expr)*) => {
        log!(Level.Trace, $msg $(, $key = $value)*)
    }
}

/// Log at debug level
public macro debug! {
    ($msg:expr $(, $key:ident = $value:expr)*) => {
        log!(Level.Debug, $msg $(, $key = $value)*)
    }
}

/// Log at info level
public macro info! {
    ($msg:expr $(, $key:ident = $value:expr)*) => {
        log!(Level.Info, $msg $(, $key = $value)*)
    }
}

/// Log at warn level
public macro warn! {
    ($msg:expr $(, $key:ident = $value:expr)*) => {
        log!(Level.Warn, $msg $(, $key = $value)*)
    }
}

/// Log at error level
public macro error! {
    ($msg:expr $(, $key:ident = $value:expr)*) => {
        log!(Level.Error, $msg $(, $key = $value)*)
    }
}

/// Generic log macro
public macro log! {
    ($level:expr, $msg:expr $(, $key:ident = $value:expr)*) => {
        if LOGGER.is_enabled($level) then {
            LOGGER.log(Record {
                level: $level,
                message: $msg,
                target: module_path!(),
                file: file!(),
                line: line!(),
                fields: &[$(($key.to_string(), $value.to_string())),*],
            })
        }
    }
}
```

---

## Logger

### Global Logger

```tml
/// The global logger instance
public static LOGGER: Logger = Logger.default()

/// Sets the global logger
public func set_logger(logger: Logger) {
    LOGGER = logger
}

/// Sets the global log level
public func set_level(level: Level) {
    LOGGER.set_level(level)
}

/// Initializes logging from environment
public func init()
    caps: [io.process.env]
{
    let level = env.var("TML_LOG")
        .ok()
        .and_then(|s| Level.from_str(ref s))
        .unwrap_or(Level.Info)

    set_level(level)
}
```

### Logger Type

```tml
/// Logger configuration
public type Logger {
    level: AtomicU8,
    outputs: Vec[Heap[dyn Output]],
    format: Format,
}

extend Logger {
    /// Creates a default logger (stderr, text format)
    public func default() -> Logger {
        Logger {
            level: AtomicU8.new(Level.Info.as_u8()),
            outputs: vec![Box.new(StderrOutput.new())],
            format: Format.Text,
        }
    }

    /// Creates a logger builder
    public func builder() -> LoggerBuilder {
        LoggerBuilder.new()
    }

    /// Returns the current log level
    public func level(this) -> Level {
        Level.from_u8(this.level.load(Ordering.Relaxed)).unwrap()
    }

    /// Sets the log level
    public func set_level(this, level: Level) {
        this.level.store(level.as_u8(), Ordering.Relaxed)
    }

    /// Returns true if the level is enabled
    public func is_enabled(this, level: Level) -> Bool {
        level >= this.level()
    }

    /// Logs a record
    public func log(this, record: Record) {
        if not this.is_enabled(record.level) then return

        let formatted = this.format.format(&record)

        loop output in this.outputs.iter() {
            output.write(ref formatted, record.level)
        }
    }

    /// Flushes all outputs
    public func flush(this) {
        loop output in this.outputs.iter() {
            output.flush()
        }
    }
}
```

### Logger Builder

```tml
/// Builder for configuring a logger
public type LoggerBuilder {
    level: Level,
    outputs: Vec[Heap[dyn Output]],
    format: Format,
}

extend LoggerBuilder {
    /// Creates a new builder
    public func new() -> LoggerBuilder {
        LoggerBuilder {
            level: Level.Info,
            outputs: Vec.new(),
            format: Format.Text,
        }
    }

    /// Sets the minimum log level
    public func level(mut this, level: Level) -> LoggerBuilder {
        this.level = level
        return this
    }

    /// Sets the format
    public func format(mut this, format: Format) -> LoggerBuilder {
        this.format = format
        return this
    }

    /// Adds stderr output
    public func stderr(mut this) -> LoggerBuilder {
        this.outputs.push(Box.new(StderrOutput.new()))
        return this
    }

    /// Adds stdout output
    public func stdout(mut this) -> LoggerBuilder {
        this.outputs.push(Box.new(StdoutOutput.new()))
        return this
    }

    /// Adds file output
    public func file(mut this, path: ref String) -> LoggerBuilder
        caps: [io.file]
    {
        this.outputs.push(Box.new(FileOutput.new(path).unwrap()))
        return this
    }

    /// Adds rotating file output
    public func rotating_file(
        mut this,
        path: ref String,
        max_size: U64,
        max_files: U64,
    ) -> LoggerBuilder
        caps: [io.file]
    {
        this.outputs.push(Box.new(RotatingFileOutput.new(path, max_size, max_files).unwrap()))
        return this
    }

    /// Adds a custom output
    public func output(mut this, output: Heap[dyn Output]) -> LoggerBuilder {
        this.outputs.push(output)
        return this
    }

    /// Builds the logger
    public func build(this) -> Logger {
        Logger {
            level: AtomicU8.new(this.level.as_u8()),
            outputs: this.outputs,
            format: this.format,
        }
    }

    /// Builds and sets as global logger
    public func install(this) {
        set_logger(this.build())
    }
}
```

---

## Log Record

```tml
/// A log record
public type Record {
    level: Level,
    message: String,
    target: String,    // Module path
    file: String,      // Source file
    line: U32,         // Source line
    fields: Vec[(String, String)],
}

extend Record {
    /// Returns a field value by key
    public func field(this, key: ref String) -> Maybe[ref String] {
        this.fields.iter()
            .find(do((k, _)) k == key)
            .map(do((_, v)) v)
    }
}
```

---

## Formats

```tml
/// Log output format
public type Format = Text | Json | Pretty | Compact

extend Format {
    /// Formats a record
    public func format(this, record: ref Record) -> String {
        when this {
            Text -> this.format_text(record),
            Json -> this.format_json(record),
            Pretty -> this.format_pretty(record),
            Compact -> this.format_compact(record),
        }
    }

    func format_text(this, record: ref Record) -> String {
        // 2024-03-15T10:30:00Z INFO [module::path] Message key=value
        var line = DateTime.now_utc().to_rfc3339()
        line = line + " " + record.level.to_string().to_uppercase()
        line = line + " [" + record.target + "]"
        line = line + " " + record.message

        loop (key, value) in record.fields.iter() {
            line = line + " " + key + "=" + value
        }

        return line + "\n"
    }

    func format_json(this, record: ref Record) -> String {
        var obj = JsonObject.new()
        obj.insert("timestamp", DateTime.now_utc().to_rfc3339())
        obj.insert("level", record.level.to_string())
        obj.insert("target", record.target)
        obj.insert("message", record.message)
        obj.insert("file", record.file)
        obj.insert("line", record.line)

        loop (key, value) in record.fields.iter() {
            obj.insert(key, value)
        }

        return obj.to_string() + "\n"
    }

    func format_pretty(this, record: ref Record) -> String {
        let color = when record.level {
            Trace -> "\x1b[37m",    // Gray
            Debug -> "\x1b[36m",    // Cyan
            Info -> "\x1b[32m",     // Green
            Warn -> "\x1b[33m",     // Yellow
            Error -> "\x1b[31m",    // Red
        }
        let reset = "\x1b[0m"

        var line = color + record.level.to_string().to_uppercase().pad_left(5, ' ')
        line = line + reset + " " + record.message

        if not record.fields.is_empty() then {
            line = line + "\n"
            loop (key, value) in record.fields.iter() {
                line = line + "       " + key + ": " + value + "\n"
            }
        } else {
            line = line + "\n"
        }

        return line
    }

    func format_compact(this, record: ref Record) -> String {
        let prefix = when record.level {
            Trace -> "T",
            Debug -> "D",
            Info -> "I",
            Warn -> "W",
            Error -> "E",
        }

        return prefix + " " + record.message + "\n"
    }
}
```

---

## Outputs

### Output Trait

```tml
/// Log output destination
public behavior Output {
    /// Writes a formatted log line
    func write(this, line: ref String, level: Level)

    /// Flushes any buffered data
    func flush(this)
}
```

### Standard Outputs

```tml
/// Stderr output
public type StderrOutput {}

implement Output for StderrOutput {
    func write(this, line: ref String, level: Level) {
        io.stderr().write_all(line.as_bytes()).ok()
    }

    func flush(this) {
        io.stderr().flush().ok()
    }
}

/// Stdout output
public type StdoutOutput {}

implement Output for StdoutOutput {
    func write(this, line: ref String, level: Level) {
        io.stdout().write_all(line.as_bytes()).ok()
    }

    func flush(this) {
        io.stdout().flush().ok()
    }
}
```

### File Output

```tml
/// File output
public type FileOutput {
    file: Mutex[File],
}

extend FileOutput {
    public func new(path: ref String) -> Outcome[FileOutput, IoError]
        caps: [io.file]
    {
        let file = File.create(path)?
        return Ok(FileOutput { file: Mutex.new(file) })
    }
}

implement Output for FileOutput {
    func write(this, line: ref String, level: Level) {
        let guard = this.file.lock()
        guard.write_all(line.as_bytes()).ok()
    }

    func flush(this) {
        let guard = this.file.lock()
        guard.flush().ok()
    }
}

/// Rotating file output
public type RotatingFileOutput {
    path: String,
    max_size: U64,
    max_files: U64,
    current_file: Mutex[File],
    current_size: AtomicU64,
}

extend RotatingFileOutput {
    public func new(path: ref String, max_size: U64, max_files: U64) -> Outcome[RotatingFileOutput, IoError]
        caps: [io.file]

    func rotate(mut this)
        caps: [io.file]
}

implement Output for RotatingFileOutput {
    func write(this, line: ref String, level: Level) {
        let size = this.current_size.fetch_add(line.len() as U64, Ordering.Relaxed)
        if size > this.max_size then {
            this.rotate()
        }

        let guard = this.current_file.lock()
        guard.write_all(line.as_bytes()).ok()
    }

    func flush(this) {
        let guard = this.current_file.lock()
        guard.flush().ok()
    }
}
```

---

## Filtering

```tml
/// Filter for log records
public type Filter {
    targets: HashMap[String, Level],
    default_level: Level,
}

extend Filter {
    /// Creates a new filter
    public func new() -> Filter {
        Filter {
            targets: HashMap.new(),
            default_level: Level.Info,
        }
    }

    /// Sets the default level
    public func default_level(mut this, level: Level) -> Filter {
        this.default_level = level
        return this
    }

    /// Sets level for a specific target
    public func target(mut this, target: ref String, level: Level) -> Filter {
        this.targets.insert(target.duplicate(), level)
        return this
    }

    /// Returns true if the record should be logged
    public func is_enabled(this, record: ref Record) -> Bool {
        // Check specific target
        loop (prefix, level) in this.targets.iter() {
            if record.target.starts_with(prefix) then {
                return record.level >= *level
            }
        }
        return record.level >= this.default_level
    }
}
```

---

## Spans (Structured Context)

```tml
/// A logging span for structured context
public type Span {
    name: String,
    fields: Vec[(String, String)],
    start: Instant,
}

extend Span {
    /// Enters a new span
    public func enter(name: ref String) -> Span
        caps: [io.time]
    {
        Span {
            name: name.duplicate(),
            fields: Vec.new(),
            start: Instant.now(),
        }
    }

    /// Adds a field to the span
    public func field(mut this, key: ref String, value: impl ToString) -> Span {
        this.fields.push((key.duplicate(), value.to_string()))
        return this
    }

    /// Logs when the span ends
    public func exit(this)
        caps: [io.time]
    {
        let duration = this.start.elapsed()
        info!("span completed",
            span = this.name,
            duration_ms = duration.as_millis()
        )
    }
}

/// Guard that exits span on drop
public type SpanGuard {
    span: Maybe[Span],
}

implement Disposable for SpanGuard {
    func drop(mut this) {
        when this.span.take() {
            Just(span) -> span.exit(),
            Nothing -> {},
        }
    }
}

/// Macro to create a span guard
public macro span! {
    ($name:expr $(, $key:ident = $value:expr)*) => {
        let _span_guard = SpanGuard {
            span: Just(Span.enter($name)$(.field($key.to_string(), $value))*)
        }
    }
}
```

---

## Examples

### Basic Setup

```tml
import std.log.{Logger, Level, info, error}

func main() {
    // Simple setup
    log.init()

    info!("Application started", version = "1.0.0")

    // Custom setup
    Logger.builder()
        .level(Level.Debug)
        .format(Format.Pretty)
        .stderr()
        .file("app.log")
        .install()

    info!("Logger configured")
}
```

### Structured Logging

```tml
import std.log.{info, warn, error}

func process_request(request: ref Request)
    caps: [io.time]
{
    span!("process_request", request_id = request.id, method = request.method)

    info!("Processing request",
        path = request.path,
        user_agent = request.user_agent
    )

    when handle_request(request) {
        Ok(response) -> {
            info!("Request completed",
                status = response.status,
                bytes = response.body.len()
            )
        },
        Err(e) -> {
            error!("Request failed",
                error = e.to_string(),
                path = request.path
            )
        },
    }
}
```

### JSON Logging

```tml
import std.log.{Logger, Level, Format, info}

func setup_json_logging()
    caps: [io.file]
{
    Logger.builder()
        .level(Level.Info)
        .format(Format.Json)
        .stdout()
        .install()

    info!("Server started", port = 8080, env = "production")
    // {"timestamp":"2024-03-15T10:30:00Z","level":"INFO","target":"myapp::server","message":"Server started","port":"8080","env":"production"}
}
```

### Filtering by Module

```tml
import std.log.{Logger, Level, Filter}

func setup_filtered_logging() {
    let filter = Filter.new()
        .default_level(Level.Warn)
        .target("myapp::http", Level.Debug)
        .target("myapp::db", Level.Info)

    Logger.builder()
        .filter(filter)
        .stderr()
        .install()

    // Now only http module gets debug logs,
    // db module gets info+, everything else gets warn+
}
```

---

## See Also

- [std.datetime](./16-DATETIME.md) — Timestamps
- [std.json](./09-JSON.md) — JSON formatting
- [std.fs](./01-FS.md) — File output
