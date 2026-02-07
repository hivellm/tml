//! # Logger Implementation
//!
//! Implements the Logger singleton, ConsoleSink, FileSink, and LogFilter.

#include "log/log.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#define TML_ISATTY(fd) _isatty(fd)
#define TML_FILENO(f) _fileno(f)
#else
#include <unistd.h>
#define TML_ISATTY(fd) isatty(fd)
#define TML_FILENO(f) fileno(f)
#endif

namespace tml::log {

// ============================================================================
// Terminal Color Detection
// ============================================================================

/// Detects if stderr supports ANSI color codes.
static bool detect_terminal_colors() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_ERROR_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
        return false;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
        return false;

    // Try to enable virtual terminal processing on Windows 10+
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (SetConsoleMode(hOut, dwMode))
        return true;

    return TML_ISATTY(TML_FILENO(stderr)) != 0;
#else
    if (!TML_ISATTY(TML_FILENO(stderr)))
        return false;

    const char* term = std::getenv("TERM");
    if (!term)
        return false;

    return std::string(term) != "dumb";
#endif
}

// ============================================================================
// ConsoleSink
// ============================================================================

ConsoleSink::ConsoleSink(bool use_colors)
    : colors_enabled_(use_colors && detect_terminal_colors()) {}

const char* ConsoleSink::level_color(LogLevel level) const {
    switch (level) {
    case LogLevel::Trace:
        return "\033[90m"; // Dark gray
    case LogLevel::Debug:
        return "\033[36m"; // Cyan
    case LogLevel::Info:
        return "\033[32m"; // Green
    case LogLevel::Warn:
        return "\033[33m"; // Yellow
    case LogLevel::Error:
        return "\033[31m"; // Red
    case LogLevel::Fatal:
        return "\033[1;31m"; // Bold red
    case LogLevel::Off:
        return "";
    }
    return "";
}

void ConsoleSink::write(const LogRecord& record) {
    if (format_ == LogFormat::JSON) {
        write_json(record);
    } else {
        write_text(record);
    }
}

void ConsoleSink::write_text(const LogRecord& record) {
    std::ostringstream oss;
    oss << get_timestamp() << " ";

    if (colors_enabled_) {
        oss << level_color(record.level);
    }

    // Pad level name to 5 chars for alignment
    const char* name = level_name(record.level);
    oss << std::left << std::setw(5) << name;

    if (colors_enabled_) {
        oss << "\033[0m";
    }

    oss << " [" << record.module << "] " << record.message << "\n";

    std::cerr << oss.str();
}

void ConsoleSink::write_json(const LogRecord& record) {
    std::ostringstream oss;
    oss << "{\"ts\":" << record.timestamp_ms << ","
        << "\"level\":\"" << level_name(record.level) << "\","
        << "\"module\":\"" << record.module << "\","
        << "\"msg\":\"";

    // Escape JSON special characters in message
    for (char c : record.message) {
        switch (c) {
        case '"':
            oss << "\\\"";
            break;
        case '\\':
            oss << "\\\\";
            break;
        case '\n':
            oss << "\\n";
            break;
        case '\r':
            oss << "\\r";
            break;
        case '\t':
            oss << "\\t";
            break;
        default:
            oss << c;
        }
    }

    oss << "\"}\n";
    std::cerr << oss.str();
}

void ConsoleSink::flush() {
    std::cerr.flush();
}

// ============================================================================
// FileSink
// ============================================================================

FileSink::FileSink(const std::string& path, bool append)
    : file_(path, append ? (std::ios::out | std::ios::app) : std::ios::out) {}

FileSink::~FileSink() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void FileSink::write(const LogRecord& record) {
    if (!file_.is_open())
        return;

    if (format_ == LogFormat::JSON) {
        write_json(record);
    } else {
        write_text(record);
    }

    // Auto-flush on Error and Fatal
    if (record.level >= LogLevel::Error) {
        file_.flush();
    }
}

void FileSink::write_text(const LogRecord& record) {
    file_ << get_timestamp() << " " << std::left << std::setw(5) << level_name(record.level) << " ["
          << record.module << "] " << record.message << "\n";
}

void FileSink::write_json(const LogRecord& record) {
    file_ << "{\"ts\":" << record.timestamp_ms << ","
          << "\"level\":\"" << level_name(record.level) << "\","
          << "\"module\":\"" << record.module << "\","
          << "\"msg\":\"";

    for (char c : record.message) {
        switch (c) {
        case '"':
            file_ << "\\\"";
            break;
        case '\\':
            file_ << "\\\\";
            break;
        case '\n':
            file_ << "\\n";
            break;
        case '\r':
            file_ << "\\r";
            break;
        case '\t':
            file_ << "\\t";
            break;
        default:
            file_ << c;
        }
    }

    file_ << "\"}\n";
}

void FileSink::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

// ============================================================================
// RotatingFileSink
// ============================================================================

namespace fs = std::filesystem;

RotatingFileSink::RotatingFileSink(const std::string& path, size_t max_size, size_t max_files)
    : path_(path), max_size_(max_size), max_files_(max_files) {
    open_file();
}

RotatingFileSink::~RotatingFileSink() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

void RotatingFileSink::open_file() {
    file_.open(path_, std::ios::out | std::ios::app);
    if (file_.is_open() && fs::exists(path_)) {
        current_size_ = static_cast<size_t>(fs::file_size(path_));
    } else {
        current_size_ = 0;
    }
}

void RotatingFileSink::rotate() {
    // Close the current file
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }

    // Rotate existing backups: path.N -> path.N+1 (drop oldest if > max_files)
    // Delete the oldest backup if it exists
    std::string oldest = path_ + "." + std::to_string(max_files_);
    if (fs::exists(oldest)) {
        fs::remove(oldest);
    }

    // Shift backups: path.N-1 -> path.N, ..., path.1 -> path.2
    for (size_t i = max_files_ - 1; i >= 1; --i) {
        std::string src = path_ + "." + std::to_string(i);
        std::string dst = path_ + "." + std::to_string(i + 1);
        if (fs::exists(src)) {
            fs::rename(src, dst);
        }
    }

    // Move current file to .1
    if (fs::exists(path_)) {
        fs::rename(path_, path_ + ".1");
    }

    // Open a fresh file
    current_size_ = 0;
    file_.open(path_, std::ios::out | std::ios::trunc);
}

void RotatingFileSink::write(const LogRecord& record) {
    if (!file_.is_open())
        return;

    // Check if we need to rotate before writing
    if (current_size_ >= max_size_) {
        rotate();
        if (!file_.is_open())
            return;
    }

    auto pos_before = file_.tellp();

    if (format_ == LogFormat::JSON) {
        write_json(record);
    } else {
        write_text(record);
    }

    auto pos_after = file_.tellp();
    if (pos_before >= 0 && pos_after >= 0) {
        current_size_ += static_cast<size_t>(pos_after - pos_before);
    }

    // Auto-flush on Error and Fatal
    if (record.level >= LogLevel::Error) {
        file_.flush();
    }
}

void RotatingFileSink::write_text(const LogRecord& record) {
    file_ << get_timestamp() << " " << std::left << std::setw(5) << level_name(record.level) << " ["
          << record.module << "] " << record.message << "\n";
}

void RotatingFileSink::write_json(const LogRecord& record) {
    file_ << "{\"ts\":" << record.timestamp_ms << ","
          << "\"level\":\"" << level_name(record.level) << "\","
          << "\"module\":\"" << record.module << "\","
          << "\"msg\":\"";

    for (char c : record.message) {
        switch (c) {
        case '"':
            file_ << "\\\"";
            break;
        case '\\':
            file_ << "\\\\";
            break;
        case '\n':
            file_ << "\\n";
            break;
        case '\r':
            file_ << "\\r";
            break;
        case '\t':
            file_ << "\\t";
            break;
        default:
            file_ << c;
        }
    }

    file_ << "\"}\n";
}

void RotatingFileSink::flush() {
    if (file_.is_open()) {
        file_.flush();
    }
}

// ============================================================================
// MultiSink
// ============================================================================

void MultiSink::write(const LogRecord& record) {
    for (auto& sink : sinks_) {
        sink->write(record);
    }
}

void MultiSink::flush() {
    for (auto& sink : sinks_) {
        sink->flush();
    }
}

void MultiSink::add(std::unique_ptr<LogSink> sink) {
    sinks_.push_back(std::move(sink));
}

// ============================================================================
// LogFormatter
// ============================================================================

LogFormatter::LogFormatter(std::string_view format_template) : template_(format_template) {}

void LogFormatter::set_template(std::string_view format_template) {
    template_ = std::string(format_template);
}

std::string LogFormatter::format(const LogRecord& record) const {
    std::string result;
    result.reserve(template_.size() + record.message.size() + 64);

    size_t i = 0;
    while (i < template_.size()) {
        if (template_[i] == '{') {
            size_t close = template_.find('}', i + 1);
            if (close != std::string::npos) {
                auto token = std::string_view(template_).substr(i + 1, close - i - 1);

                if (token == "time") {
                    result += get_timestamp();
                } else if (token == "time_ms") {
                    result += std::to_string(record.timestamp_ms);
                } else if (token == "level") {
                    result += level_name(record.level);
                } else if (token == "level_short") {
                    result += level_short_name(record.level);
                } else if (token == "module") {
                    result += record.module;
                } else if (token == "message") {
                    result += record.message;
                } else if (token == "file") {
                    result += record.file ? record.file : "";
                } else if (token == "line") {
                    result += std::to_string(record.line);
                } else if (token == "thread") {
                    std::ostringstream oss;
                    oss << std::this_thread::get_id();
                    result += oss.str();
                } else {
                    // Unknown token — keep as-is
                    result += '{';
                    result += token;
                    result += '}';
                }
                i = close + 1;
            } else {
                result += template_[i];
                ++i;
            }
        } else {
            result += template_[i];
            ++i;
        }
    }

    return result;
}

// ============================================================================
// LogFilter
// ============================================================================

void LogFilter::parse(std::string_view spec) {
    module_levels_.clear();

    // Parse comma-separated "module=level" pairs
    size_t pos = 0;
    while (pos < spec.size()) {
        size_t comma = spec.find(',', pos);
        if (comma == std::string_view::npos) {
            comma = spec.size();
        }

        auto token = spec.substr(pos, comma - pos);
        size_t eq = token.find('=');

        if (eq != std::string_view::npos) {
            auto mod = token.substr(0, eq);
            auto lvl = token.substr(eq + 1);
            if (mod == "*") {
                default_level_ = parse_level(lvl);
            } else {
                module_levels_[std::string(mod)] = parse_level(lvl);
            }
        } else if (!token.empty()) {
            // Bare module name without level — set to Trace (show everything from that module)
            module_levels_[std::string(token)] = LogLevel::Trace;
        }

        pos = comma + 1;
    }
}

bool LogFilter::should_log(LogLevel level, std::string_view module) const {
    auto it = module_levels_.find(std::string(module));
    if (it != module_levels_.end()) {
        return level >= it->second;
    }
    return level >= default_level_;
}

// ============================================================================
// Logger
// ============================================================================

Logger::Logger() = default;

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::init(const LogConfig& config) {
    auto& logger = instance();
    std::lock_guard<std::mutex> lock(logger.mutex_);

    // Clear existing sinks
    logger.sinks_.clear();

    // Set level
    logger.level_ = config.level;

    // Parse filter
    if (!config.filter_spec.empty()) {
        logger.filter_.parse(config.filter_spec);
        // If the filter spec didn't include a "*=level" wildcard,
        // use the CLI-provided level as the filter default.
        if (config.level < logger.filter_.default_level()) {
            logger.filter_.set_default_level(config.level);
        }
        // Set level_ to the minimum across all configured levels
        // so the fast-path in should_log() doesn't reject messages
        // that per-module overrides would accept.
        logger.level_ = logger.filter_.min_level();
    } else {
        logger.filter_.set_default_level(config.level);
    }

    // Add console sink
    if (config.console) {
        auto console = std::make_unique<ConsoleSink>(config.colors);
        console->set_format(config.format);
        logger.sinks_.push_back(std::move(console));
    }

    // Add file sink
    if (!config.log_file.empty()) {
        auto file = std::make_unique<FileSink>(config.log_file);
        file->set_format(config.format);
        if (file->is_open()) {
            logger.sinks_.push_back(std::move(file));
        } else {
            std::cerr << "warning: could not open log file: " << config.log_file << "\n";
        }
    }

    logger.initialized_ = true;
}

bool Logger::should_log(LogLevel level, std::string_view module) const {
    // Fast path: level check without lock
    if (level < level_)
        return false;

    return filter_.should_log(level, module);
}

void Logger::log(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sink : sinks_) {
        sink->write(record);
    }
}

void Logger::log(LogLevel level, std::string_view module, const std::string& message,
                 const char* file, int line) {
    LogRecord record;
    record.level = level;
    record.module = module;
    record.message = message;
    record.file = file;
    record.line = line;
    record.timestamp_ms = epoch_ms();

    log(record);
}

void Logger::add_sink(std::unique_ptr<LogSink> sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::set_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
    filter_.set_default_level(level);
}

void Logger::set_filter(std::string_view spec) {
    std::lock_guard<std::mutex> lock(mutex_);
    filter_.parse(spec);
}

void Logger::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& sink : sinks_) {
        sink->flush();
    }
}

} // namespace tml::log
