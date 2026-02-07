//! # TML Unified Logging System
//!
//! A structured logging library for the TML compiler with:
//! - 6 log levels (Trace, Debug, Info, Warn, Error, Fatal)
//! - Module-tagged messages for per-component filtering
//! - Multiple output sinks (Console, File, Null)
//! - Thread-safe output with mutex protection
//! - Compile-time level elision via TML_MIN_LOG_LEVEL
//! - ANSI colored console output with terminal detection
//!
//! ## Usage
//!
//! ```cpp
//! TML_LOG_INFO("build", "Compiling " << input << " -> " << output);
//! TML_LOG_DEBUG("codegen", "Generating IR for function " << name);
//! TML_LOG_WARN("types", "Implicit narrowing from " << from << " to " << to);
//! ```

#ifndef TML_LOG_HPP
#define TML_LOG_HPP

#include <chrono>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Platform includes are in logger.cpp to avoid windows.h macro pollution.
// Only the .cpp file needs terminal detection.

namespace tml::log {

// ============================================================================
// Log Levels
// ============================================================================

/// Log severity levels in ascending order.
/// Setting a minimum level filters out all messages below that threshold.
enum class LogLevel : int {
    Trace = 0, ///< Fine-grained internal tracing
    Debug = 1, ///< Debugging information
    Info = 2,  ///< General informational messages
    Warn = 3,  ///< Potential issues or deprecations
    Error = 4, ///< Recoverable errors
    Fatal = 5, ///< Unrecoverable errors (typically followed by abort)
    Off = 6    ///< Disables all logging
};

/// Returns the short string name for a log level (e.g., "TRACE", "DEBUG").
inline const char* level_name(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "TRACE";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Fatal:
        return "FATAL";
    case LogLevel::Off:
        return "OFF";
    }
    return "???";
}

/// Parses a log level from a string (case-insensitive).
/// Returns LogLevel::Info if the string is not recognized.
inline LogLevel parse_level(std::string_view s) {
    if (s == "trace" || s == "TRACE")
        return LogLevel::Trace;
    if (s == "debug" || s == "DEBUG")
        return LogLevel::Debug;
    if (s == "info" || s == "INFO")
        return LogLevel::Info;
    if (s == "warn" || s == "WARN")
        return LogLevel::Warn;
    if (s == "error" || s == "ERROR")
        return LogLevel::Error;
    if (s == "fatal" || s == "FATAL")
        return LogLevel::Fatal;
    if (s == "off" || s == "OFF")
        return LogLevel::Off;
    return LogLevel::Info;
}

// ============================================================================
// Log Record
// ============================================================================

/// A single log message with metadata.
struct LogRecord {
    LogLevel level;          ///< Severity level
    std::string_view module; ///< Module tag (e.g., "codegen", "build")
    std::string message;     ///< Formatted message text
    const char* file;        ///< Source file (__FILE__)
    int line;                ///< Source line (__LINE__)
    int64_t timestamp_ms;    ///< Milliseconds since epoch
};

// ============================================================================
// Output Format
// ============================================================================

/// Output format for log messages.
enum class LogFormat {
    Text, ///< Human-readable text with optional ANSI colors
    JSON  ///< Machine-parseable JSON (one object per line)
};

// ============================================================================
// Log Sinks
// ============================================================================

/// Abstract base class for log output destinations.
class LogSink {
public:
    virtual ~LogSink() = default;

    /// Write a log record to the sink.
    virtual void write(const LogRecord& record) = 0;

    /// Flush any buffered output.
    virtual void flush() = 0;
};

/// Console sink that writes to stderr with optional ANSI colors.
class ConsoleSink : public LogSink {
public:
    explicit ConsoleSink(bool use_colors = true);

    void write(const LogRecord& record) override;
    void flush() override;

    void set_color_enabled(bool enabled) {
        colors_enabled_ = enabled;
    }
    void set_format(LogFormat format) {
        format_ = format;
    }

private:
    bool colors_enabled_;
    LogFormat format_ = LogFormat::Text;

    const char* level_color(LogLevel level) const;
    void write_text(const LogRecord& record);
    void write_json(const LogRecord& record);
};

/// File sink that writes log messages to a file.
/// Auto-flushes on Error and Fatal messages.
class FileSink : public LogSink {
public:
    explicit FileSink(const std::string& path, bool append = true);
    ~FileSink() override;

    void write(const LogRecord& record) override;
    void flush() override;

    bool is_open() const {
        return file_.is_open();
    }

    void set_format(LogFormat format) {
        format_ = format;
    }

private:
    std::ofstream file_;
    LogFormat format_ = LogFormat::Text;

    void write_text(const LogRecord& record);
    void write_json(const LogRecord& record);
};

/// Null sink that discards all messages (for testing/benchmarking).
class NullSink : public LogSink {
public:
    void write(const LogRecord& /*record*/) override {}
    void flush() override {}
};

/// Rotating file sink that rotates log files when they exceed a maximum size.
/// Keeps at most `max_files` backup files named path.1, path.2, etc.
class RotatingFileSink : public LogSink {
public:
    /// Create a rotating file sink.
    /// @param path     Base path for the log file
    /// @param max_size Maximum file size in bytes before rotation
    /// @param max_files Maximum number of backup files to keep
    RotatingFileSink(const std::string& path, size_t max_size, size_t max_files = 3);
    ~RotatingFileSink() override;

    void write(const LogRecord& record) override;
    void flush() override;

    bool is_open() const {
        return file_.is_open();
    }
    void set_format(LogFormat format) {
        format_ = format;
    }

    /// Returns the current file size (for testing).
    size_t current_size() const {
        return current_size_;
    }

private:
    std::string path_;
    size_t max_size_;
    size_t max_files_;
    size_t current_size_ = 0;
    std::ofstream file_;
    LogFormat format_ = LogFormat::Text;

    void rotate();
    void write_text(const LogRecord& record);
    void write_json(const LogRecord& record);
    void open_file();
};

/// Multi-sink that fans out log records to multiple child sinks.
class MultiSink : public LogSink {
public:
    void write(const LogRecord& record) override;
    void flush() override;

    /// Add a child sink.
    void add(std::unique_ptr<LogSink> sink);

    /// Returns the number of child sinks.
    size_t size() const {
        return sinks_.size();
    }

private:
    std::vector<std::unique_ptr<LogSink>> sinks_;
};

// ============================================================================
// Log Filter
// ============================================================================

/// Module-based log level filter.
///
/// Parses filter strings like "codegen=trace,borrow=debug,*=warn" and
/// provides fast `should_log(level, module)` checks.
class LogFilter {
public:
    LogFilter() = default;

    /// Parse a filter specification string.
    /// Format: "module1=level,module2=level,*=default_level"
    /// Module names without "=level" use the default level.
    void parse(std::string_view spec);

    /// Check if a message at the given level from the given module should be logged.
    bool should_log(LogLevel level, std::string_view module) const;

    /// Set the default level for modules not explicitly listed.
    void set_default_level(LogLevel level) {
        default_level_ = level;
    }

    /// Get the default level.
    LogLevel default_level() const {
        return default_level_;
    }

    /// Get the minimum configured level across all modules and the default.
    /// Used for the fast-path check in Logger::should_log().
    LogLevel min_level() const {
        LogLevel min = default_level_;
        for (const auto& [_, level] : module_levels_) {
            if (level < min)
                min = level;
        }
        return min;
    }

private:
    LogLevel default_level_ = LogLevel::Info;
    std::unordered_map<std::string, LogLevel> module_levels_;
};

// ============================================================================
// Log Formatter
// ============================================================================

/// Format template engine for log messages.
///
/// Supports tokens: {time}, {time_ms}, {level}, {level_short}, {module},
/// {message}, {file}, {line}, {thread}.
///
/// Default format: "{time} {level_short} [{module}] {message}"
class LogFormatter {
public:
    /// Create a formatter with the given template string.
    explicit LogFormatter(
        std::string_view format_template = "{time} {level_short} [{module}] {message}");

    /// Format a log record according to the template.
    std::string format(const LogRecord& record) const;

    /// Set a new format template.
    void set_template(std::string_view format_template);

    /// Get the current format template.
    const std::string& get_template() const {
        return template_;
    }

private:
    std::string template_;
};

/// Returns the short-form name for a log level (2 chars: TR, DB, IN, WN, ER, FA).
inline const char* level_short_name(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "TR";
    case LogLevel::Debug:
        return "DB";
    case LogLevel::Info:
        return "IN";
    case LogLevel::Warn:
        return "WN";
    case LogLevel::Error:
        return "ER";
    case LogLevel::Fatal:
        return "FA";
    case LogLevel::Off:
        return "--";
    }
    return "??";
}

// ============================================================================
// Logger Configuration
// ============================================================================

/// Configuration for logger initialization.
struct LogConfig {
    LogLevel level = LogLevel::Info;    ///< Global minimum log level
    LogFormat format = LogFormat::Text; ///< Output format
    std::string filter_spec;            ///< Module filter string
    std::string log_file;               ///< Path to log file (empty = no file)
    bool console = true;                ///< Enable console (stderr) output
    bool colors = true;                 ///< Enable ANSI colors on console
};

// ============================================================================
// Logger Singleton
// ============================================================================

/// Thread-safe global logger.
///
/// Manages sinks, filtering, and dispatches log records.
/// Must be initialized via `Logger::init()` before use (auto-inits with
/// defaults if not explicitly initialized).
class Logger {
public:
    /// Initialize the global logger with the given configuration.
    static void init(const LogConfig& config);

    /// Get the global logger instance.
    static Logger& instance();

    /// Check if a message at the given level/module should be logged.
    /// This is the fast-path check used by macros before constructing the message.
    bool should_log(LogLevel level, std::string_view module) const;

    /// Log a pre-formatted record to all sinks.
    void log(const LogRecord& record);

    /// Log a message at the given level from the given module.
    void log(LogLevel level, std::string_view module, const std::string& message, const char* file,
             int line);

    /// Add a sink to the logger.
    void add_sink(std::unique_ptr<LogSink> sink);

    /// Set the global minimum log level.
    void set_level(LogLevel level);

    /// Get the current global log level.
    LogLevel level() const {
        return level_;
    }

    /// Set the module filter from a filter specification string.
    void set_filter(std::string_view spec);

    /// Flush all sinks.
    void flush();

private:
    Logger();

    LogLevel level_ = LogLevel::Info;
    LogFilter filter_;
    std::vector<std::unique_ptr<LogSink>> sinks_;
    mutable std::mutex mutex_;
    bool initialized_ = false;
};

// ============================================================================
// Timestamp Helper
// ============================================================================

/// Returns current time formatted as "HH:MM:SS.mmm".
inline std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm_buf;
#ifdef _WIN32
    localtime_s(&tm_buf, &now_c);
#else
    localtime_r(&now_c, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
        << now_ms.count();
    return oss.str();
}

/// Returns milliseconds since epoch (for LogRecord timestamps).
inline int64_t epoch_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// ============================================================================
// CLI Parsing
// ============================================================================

/// Parse logging-related CLI options from argv.
/// Extracts: --log-level, --log-filter, --log-file, --log-format, -v/-vv/-vvv, -q
/// Also checks TML_LOG environment variable as fallback.
LogConfig parse_log_options(int argc, char* argv[]);

// ============================================================================
// Logging Macros
// ============================================================================

// Compile-time minimum log level gate.
// Define TML_MIN_LOG_LEVEL before including this header to elide
// log calls below that level at compile time.
// Values: 0=Trace, 1=Debug, 2=Info, 3=Warn, 4=Error, 5=Fatal, 6=Off
#ifndef TML_MIN_LOG_LEVEL
#define TML_MIN_LOG_LEVEL 0
#endif

/// Internal macro — do not use directly.
/// Uses MSVC __pragma to suppress C4127 (constant conditional) for do-while(0)
/// and the compile-time level check.
#ifdef _MSC_VER
#define TML_LOG_IMPL(level, module_str, msg)                                                       \
    __pragma(warning(push)) __pragma(warning(disable : 4127)) do {                                 \
        if (static_cast<int>(level) >= TML_MIN_LOG_LEVEL) {                                        \
            auto& logger_ = ::tml::log::Logger::instance();                                        \
            if (logger_.should_log(level, module_str)) {                                           \
                std::ostringstream oss_;                                                           \
                oss_ << msg;                                                                       \
                logger_.log(level, module_str, oss_.str(), __FILE__, __LINE__);                    \
            }                                                                                      \
        }                                                                                          \
    }                                                                                              \
    while (0)                                                                                      \
    __pragma(warning(pop))
#else
#define TML_LOG_IMPL(level, module_str, msg)                                                       \
    do {                                                                                           \
        if (static_cast<int>(level) >= TML_MIN_LOG_LEVEL) {                                        \
            auto& logger_ = ::tml::log::Logger::instance();                                        \
            if (logger_.should_log(level, module_str)) {                                           \
                std::ostringstream oss_;                                                           \
                oss_ << msg;                                                                       \
                logger_.log(level, module_str, oss_.str(), __FILE__, __LINE__);                    \
            }                                                                                      \
        }                                                                                          \
    } while (0)
#endif

/// Log a trace-level message.
/// Usage: TML_LOG_TRACE("module", "message " << value);
#define TML_LOG_TRACE(module, msg) TML_LOG_IMPL(::tml::log::LogLevel::Trace, module, msg)

/// Log a debug-level message.
#define TML_LOG_DEBUG(module, msg) TML_LOG_IMPL(::tml::log::LogLevel::Debug, module, msg)

/// Log an info-level message.
#define TML_LOG_INFO(module, msg) TML_LOG_IMPL(::tml::log::LogLevel::Info, module, msg)

/// Log a warning-level message.
#define TML_LOG_WARN(module, msg) TML_LOG_IMPL(::tml::log::LogLevel::Warn, module, msg)

/// Log an error-level message.
#define TML_LOG_ERROR(module, msg) TML_LOG_IMPL(::tml::log::LogLevel::Error, module, msg)

/// Log a fatal-level message.
#define TML_LOG_FATAL(module, msg) TML_LOG_IMPL(::tml::log::LogLevel::Fatal, module, msg)

} // namespace tml::log

#endif // TML_LOG_HPP
