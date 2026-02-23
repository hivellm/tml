TML_MODULE("compiler")

//! # TML Preprocessor Implementation
//!
//! Implements C-style preprocessor directives for conditional compilation.

#include "preprocessor/preprocessor.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace tml::preprocessor {

// ============================================================================
// Constructor
// ============================================================================

Preprocessor::Preprocessor() : config_(host_config()) {
    setup_predefined_symbols();
}

Preprocessor::Preprocessor(const PreprocessorConfig& config) : config_(config) {
    setup_predefined_symbols();

    // Add user-defined symbols
    for (const auto& [symbol, value] : config.defines) {
        define(symbol, value);
    }
}

// ============================================================================
// Configuration
// ============================================================================

void Preprocessor::set_target_os(TargetOS os) {
    config_.target_os = os;
    setup_predefined_symbols();
}

void Preprocessor::set_target_arch(TargetArch arch) {
    config_.target_arch = arch;
    setup_predefined_symbols();
}

void Preprocessor::set_target_env(TargetEnv env) {
    config_.target_env = env;
    setup_predefined_symbols();
}

void Preprocessor::set_build_mode(BuildMode mode) {
    config_.build_mode = mode;
    setup_predefined_symbols();
}

void Preprocessor::define(const std::string& symbol) {
    defined_symbols_.insert(symbol);
    symbol_values_[symbol] = "";
}

void Preprocessor::define(const std::string& symbol, const std::string& value) {
    defined_symbols_.insert(symbol);
    symbol_values_[symbol] = value;
}

void Preprocessor::undefine(const std::string& symbol) {
    defined_symbols_.erase(symbol);
    symbol_values_.erase(symbol);
}

bool Preprocessor::is_defined(const std::string& symbol) const {
    return defined_symbols_.count(symbol) > 0;
}

std::optional<std::string> Preprocessor::get_value(const std::string& symbol) const {
    auto it = symbol_values_.find(symbol);
    if (it != symbol_values_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ============================================================================
// Predefined Symbols Setup
// ============================================================================

void Preprocessor::setup_predefined_symbols() {
    defined_symbols_.clear();
    symbol_values_.clear();

    // OS symbols
    switch (config_.target_os) {
    case TargetOS::Windows:
        define("WINDOWS");
        break;
    case TargetOS::Linux:
        define("LINUX");
        define("UNIX");
        define("POSIX");
        break;
    case TargetOS::MacOS:
        define("MACOS");
        define("UNIX");
        define("POSIX");
        break;
    case TargetOS::Android:
        define("ANDROID");
        define("LINUX");
        define("UNIX");
        define("POSIX");
        break;
    case TargetOS::IOS:
        define("IOS");
        define("UNIX");
        define("POSIX");
        break;
    case TargetOS::FreeBSD:
        define("FREEBSD");
        define("UNIX");
        define("POSIX");
        break;
    case TargetOS::Unknown:
        break;
    }

    // Architecture symbols
    switch (config_.target_arch) {
    case TargetArch::X86_64:
        define("X86_64");
        define("PTR_64");
        break;
    case TargetArch::X86:
        define("X86");
        define("PTR_32");
        break;
    case TargetArch::ARM64:
        define("ARM64");
        define("PTR_64");
        break;
    case TargetArch::ARM:
        define("ARM");
        define("PTR_32");
        break;
    case TargetArch::WASM32:
        define("WASM32");
        define("PTR_32");
        break;
    case TargetArch::RISCV64:
        define("RISCV64");
        define("PTR_64");
        break;
    case TargetArch::Unknown:
        if (config_.is_64bit) {
            define("PTR_64");
        } else {
            define("PTR_32");
        }
        break;
    }

    // Endianness
    if (config_.is_little_endian) {
        define("LITTLE_ENDIAN");
    } else {
        define("BIG_ENDIAN");
    }

    // Environment
    switch (config_.target_env) {
    case TargetEnv::MSVC:
        define("MSVC");
        break;
    case TargetEnv::GNU:
        define("GNU");
        break;
    case TargetEnv::Musl:
        define("MUSL");
        break;
    case TargetEnv::Unknown:
        break;
    }

    // Build mode
    switch (config_.build_mode) {
    case BuildMode::Debug:
        define("DEBUG");
        break;
    case BuildMode::Release:
        define("RELEASE");
        break;
    case BuildMode::Test:
        define("TEST");
        define("DEBUG"); // Tests typically include debug info
        break;
    }
}

// ============================================================================
// Host Detection
// ============================================================================

TargetOS Preprocessor::detect_host_os() {
#if defined(_WIN32) || defined(_WIN64)
    return TargetOS::Windows;
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IOS
    return TargetOS::IOS;
#else
    return TargetOS::MacOS;
#endif
#elif defined(__ANDROID__)
    return TargetOS::Android;
#elif defined(__FreeBSD__)
    return TargetOS::FreeBSD;
#elif defined(__linux__)
    return TargetOS::Linux;
#else
    return TargetOS::Unknown;
#endif
}

TargetArch Preprocessor::detect_host_arch() {
#if defined(__x86_64__) || defined(_M_X64)
    return TargetArch::X86_64;
#elif defined(__i386__) || defined(_M_IX86)
    return TargetArch::X86;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return TargetArch::ARM64;
#elif defined(__arm__) || defined(_M_ARM)
    return TargetArch::ARM;
#elif defined(__wasm32__)
    return TargetArch::WASM32;
#elif defined(__riscv) && __riscv_xlen == 64
    return TargetArch::RISCV64;
#else
    return TargetArch::Unknown;
#endif
}

TargetEnv Preprocessor::detect_host_env() {
#if defined(_MSC_VER)
    return TargetEnv::MSVC;
#elif defined(__GLIBC__)
    return TargetEnv::GNU;
#elif defined(__MUSL__)
    return TargetEnv::Musl;
#else
    return TargetEnv::Unknown;
#endif
}

PreprocessorConfig Preprocessor::host_config() {
    PreprocessorConfig config;
    config.target_os = detect_host_os();
    config.target_arch = detect_host_arch();
    config.target_env = detect_host_env();
    config.build_mode = BuildMode::Debug;
    config.is_64bit = sizeof(void*) == 8;
    config.is_little_endian = true; // Most modern systems are little-endian

    // Check endianness
    union {
        uint32_t i;
        char c[4];
    } test = {0x01020304};
    config.is_little_endian = (test.c[0] == 0x04);

    return config;
}

PreprocessorConfig Preprocessor::parse_target_triple(const std::string& triple) {
    PreprocessorConfig config;
    config.build_mode = BuildMode::Debug;
    config.is_little_endian = true;

    // Parse target triple: arch-vendor-os-env
    // Examples: x86_64-unknown-linux-gnu, aarch64-apple-darwin, x86_64-pc-windows-msvc

    std::vector<std::string> parts;
    std::stringstream ss(triple);
    std::string part;
    while (std::getline(ss, part, '-')) {
        parts.push_back(part);
    }

    if (parts.empty()) {
        return config;
    }

    // Parse architecture
    const std::string& arch = parts[0];
    if (arch == "x86_64" || arch == "amd64") {
        config.target_arch = TargetArch::X86_64;
        config.is_64bit = true;
    } else if (arch == "i686" || arch == "i386" || arch == "x86") {
        config.target_arch = TargetArch::X86;
        config.is_64bit = false;
    } else if (arch == "aarch64" || arch == "arm64") {
        config.target_arch = TargetArch::ARM64;
        config.is_64bit = true;
    } else if (arch == "arm" || arch == "armv7" || arch == "thumbv7") {
        config.target_arch = TargetArch::ARM;
        config.is_64bit = false;
    } else if (arch == "wasm32") {
        config.target_arch = TargetArch::WASM32;
        config.is_64bit = false;
    } else if (arch == "riscv64") {
        config.target_arch = TargetArch::RISCV64;
        config.is_64bit = true;
    }

    // Parse OS (usually third part)
    for (const auto& p : parts) {
        if (p == "windows" || p == "win32") {
            config.target_os = TargetOS::Windows;
        } else if (p == "linux") {
            config.target_os = TargetOS::Linux;
        } else if (p == "darwin" || p == "macos") {
            config.target_os = TargetOS::MacOS;
        } else if (p == "android") {
            config.target_os = TargetOS::Android;
        } else if (p == "ios") {
            config.target_os = TargetOS::IOS;
        } else if (p == "freebsd") {
            config.target_os = TargetOS::FreeBSD;
        } else if (p == "msvc") {
            config.target_env = TargetEnv::MSVC;
        } else if (p == "gnu") {
            config.target_env = TargetEnv::GNU;
        } else if (p == "musl") {
            config.target_env = TargetEnv::Musl;
        }
    }

    return config;
}

// ============================================================================
// Processing
// ============================================================================

PreprocessorResult Preprocessor::process(std::string_view source, const std::string& filename) {
    ProcessingState state;
    state.source = source;
    state.filename = filename;
    state.pos = 0;
    state.line = 1;
    state.column = 1;
    state.output_line = 1;

    process_impl(state);

    // Check for unclosed conditionals
    if (!state.condition_stack.empty()) {
        report_error(state, "Unterminated #if directive");
    }

    PreprocessorResult result;
    result.output = std::move(state.output);
    result.line_map = std::move(state.line_map);
    result.diagnostics = std::move(state.diagnostics);
    return result;
}

void Preprocessor::process_impl(ProcessingState& state) {
    while (state.pos < state.source.size()) {
        // Find end of line
        size_t line_start = state.pos;
        size_t line_end = state.source.find('\n', state.pos);
        if (line_end == std::string_view::npos) {
            line_end = state.source.size();
        }

        std::string_view line = state.source.substr(line_start, line_end - line_start);

        // Process the line
        process_line(state, line);

        // Move to next line
        state.pos = line_end;
        if (state.pos < state.source.size() && state.source[state.pos] == '\n') {
            state.pos++;
        }
        state.line++;
        state.column = 1;
    }
}

void Preprocessor::process_line(ProcessingState& state, std::string_view line) {
    if (is_directive_line(line)) {
        process_directive(state, line);
    } else if (is_outputting(state)) {
        output_line(state, line);
    }
    // If not outputting, the line is skipped
}

bool Preprocessor::is_directive_line(std::string_view line) {
    std::string_view trimmed = trim_start(line);
    return !trimmed.empty() && trimmed[0] == '#';
}

void Preprocessor::process_directive(ProcessingState& state, std::string_view line) {
    std::string_view trimmed = trim_start(line);

    // Skip the '#'
    trimmed.remove_prefix(1);
    trimmed = trim_start(trimmed);

    // Read directive name
    std::string_view directive = read_identifier(trimmed);
    trimmed = trim_start(trimmed);

    // Handle directives that work even when not outputting
    if (directive == "if") {
        handle_if(state, trimmed);
    } else if (directive == "ifdef") {
        handle_ifdef(state, trimmed);
    } else if (directive == "ifndef") {
        handle_ifndef(state, trimmed);
    } else if (directive == "elif") {
        handle_elif(state, trimmed);
    } else if (directive == "else") {
        handle_else(state);
    } else if (directive == "endif") {
        handle_endif(state);
    } else if (is_outputting(state)) {
        // These directives only execute when outputting
        if (directive == "define") {
            handle_define(state, trimmed);
        } else if (directive == "undef") {
            handle_undef(state, trimmed);
        } else if (directive == "error") {
            handle_error(state, trimmed);
        } else if (directive == "warning") {
            handle_warning(state, trimmed);
        } else {
            report_error(state, "Unknown preprocessor directive: #" + std::string(directive));
        }
    }
}

void Preprocessor::output_line(ProcessingState& state, std::string_view line) {
    // Add line mapping
    LineMapping mapping;
    mapping.output_line = state.output_line;
    mapping.source_line = state.line;
    mapping.filename = state.filename;
    state.line_map.push_back(mapping);

    // Output the line
    state.output += line;
    state.output += '\n';
    state.output_line++;
}

// ============================================================================
// Directive Handlers
// ============================================================================

void Preprocessor::handle_if(ProcessingState& state, std::string_view expr) {
    bool parent_outputting = is_outputting(state);
    bool condition = false;

    if (parent_outputting) {
        condition = evaluate_expression(state, expr);
    }

    state.condition_stack.push_back(parent_outputting && condition);
    state.branch_taken_stack.push_back(condition);
}

void Preprocessor::handle_ifdef(ProcessingState& state, std::string_view symbol) {
    std::string_view sym = trim(symbol);
    std::string sym_str(sym);

    bool parent_outputting = is_outputting(state);
    bool condition = is_defined(sym_str);

    state.condition_stack.push_back(parent_outputting && condition);
    state.branch_taken_stack.push_back(condition);
}

void Preprocessor::handle_ifndef(ProcessingState& state, std::string_view symbol) {
    std::string_view sym = trim(symbol);
    std::string sym_str(sym);

    bool parent_outputting = is_outputting(state);
    bool condition = !is_defined(sym_str);

    state.condition_stack.push_back(parent_outputting && condition);
    state.branch_taken_stack.push_back(condition);
}

void Preprocessor::handle_elif(ProcessingState& state, std::string_view expr) {
    if (state.condition_stack.empty()) {
        report_error(state, "#elif without matching #if");
        return;
    }

    // Get parent condition (one level up)
    bool parent_outputting = state.condition_stack.size() > 1
                                 ? state.condition_stack[state.condition_stack.size() - 2]
                                 : true;

    // Only evaluate if parent is outputting and no branch taken yet
    bool branch_taken = state.branch_taken_stack.back();
    bool condition = false;

    if (parent_outputting && !branch_taken) {
        condition = evaluate_expression(state, expr);
    }

    state.condition_stack.back() = parent_outputting && !branch_taken && condition;
    if (condition) {
        state.branch_taken_stack.back() = true;
    }
}

void Preprocessor::handle_else(ProcessingState& state) {
    if (state.condition_stack.empty()) {
        report_error(state, "#else without matching #if");
        return;
    }

    // Get parent condition
    bool parent_outputting = state.condition_stack.size() > 1
                                 ? state.condition_stack[state.condition_stack.size() - 2]
                                 : true;

    bool branch_taken = state.branch_taken_stack.back();
    state.condition_stack.back() = parent_outputting && !branch_taken;
    state.branch_taken_stack.back() = true; // Mark branch as taken
}

void Preprocessor::handle_endif(ProcessingState& state) {
    if (state.condition_stack.empty()) {
        report_error(state, "#endif without matching #if");
        return;
    }

    state.condition_stack.pop_back();
    state.branch_taken_stack.pop_back();
}

void Preprocessor::handle_define(ProcessingState& state, std::string_view rest) {
    std::string_view sym = read_identifier(rest);
    if (sym.empty()) {
        report_error(state, "#define requires a symbol name");
        return;
    }

    rest = trim_start(rest);
    std::string symbol(sym);
    std::string value(trim(rest));

    define(symbol, value);
}

void Preprocessor::handle_undef(ProcessingState& state, std::string_view symbol) {
    std::string_view sym = trim(symbol);
    if (sym.empty()) {
        report_error(state, "#undef requires a symbol name");
        return;
    }

    undefine(std::string(sym));
}

void Preprocessor::handle_error(ProcessingState& state, std::string_view message) {
    std::string_view msg = trim(message);

    // Remove surrounding quotes if present
    if (msg.size() >= 2 && msg.front() == '"' && msg.back() == '"') {
        msg = msg.substr(1, msg.size() - 2);
    }

    report_error(state, "#error: " + std::string(msg));
}

void Preprocessor::handle_warning(ProcessingState& state, std::string_view message) {
    std::string_view msg = trim(message);

    // Remove surrounding quotes if present
    if (msg.size() >= 2 && msg.front() == '"' && msg.back() == '"') {
        msg = msg.substr(1, msg.size() - 2);
    }

    report_warning(state, "#warning: " + std::string(msg));
}

// ============================================================================
// Expression Evaluation
// ============================================================================

bool Preprocessor::evaluate_expression(ProcessingState& state, std::string_view expr) {
    std::string_view e = trim(expr);
    return parse_or_expr(state, e);
}

// Operator precedence (lowest to highest):
// ||  (OR)
// &&  (AND)
// !   (NOT)
// primary (identifier, defined(), parentheses)

bool Preprocessor::parse_or_expr(ProcessingState& state, std::string_view& expr) {
    bool result = parse_and_expr(state, expr);

    while (true) {
        skip_whitespace(expr);
        if (starts_with(expr, "||")) {
            expr.remove_prefix(2);
            bool rhs = parse_and_expr(state, expr);
            result = result || rhs;
        } else if (starts_with(expr, "or")) {
            // Support keyword 'or' as alternative to ||
            if (expr.size() > 2 && !std::isalnum(expr[2]) && expr[2] != '_') {
                expr.remove_prefix(2);
                bool rhs = parse_and_expr(state, expr);
                result = result || rhs;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    return result;
}

bool Preprocessor::parse_and_expr(ProcessingState& state, std::string_view& expr) {
    bool result = parse_unary_expr(state, expr);

    while (true) {
        skip_whitespace(expr);
        if (starts_with(expr, "&&")) {
            expr.remove_prefix(2);
            bool rhs = parse_unary_expr(state, expr);
            result = result && rhs;
        } else if (starts_with(expr, "and")) {
            // Support keyword 'and' as alternative to &&
            if (expr.size() > 3 && !std::isalnum(expr[3]) && expr[3] != '_') {
                expr.remove_prefix(3);
                bool rhs = parse_unary_expr(state, expr);
                result = result && rhs;
            } else {
                break;
            }
        } else {
            break;
        }
    }

    return result;
}

bool Preprocessor::parse_unary_expr(ProcessingState& state, std::string_view& expr) {
    skip_whitespace(expr);

    if (!expr.empty() && expr[0] == '!') {
        expr.remove_prefix(1);
        return !parse_unary_expr(state, expr);
    }

    if (starts_with(expr, "not")) {
        if (expr.size() > 3 && !std::isalnum(expr[3]) && expr[3] != '_') {
            expr.remove_prefix(3);
            return !parse_unary_expr(state, expr);
        }
    }

    return parse_primary_expr(state, expr);
}

bool Preprocessor::parse_primary_expr(ProcessingState& state, std::string_view& expr) {
    skip_whitespace(expr);

    if (expr.empty()) {
        report_error(state, "Unexpected end of preprocessor expression");
        return false;
    }

    // Parenthesized expression
    if (expr[0] == '(') {
        expr.remove_prefix(1);
        bool result = parse_or_expr(state, expr);
        skip_whitespace(expr);
        if (expr.empty() || expr[0] != ')') {
            report_error(state, "Missing ')' in preprocessor expression");
        } else {
            expr.remove_prefix(1);
        }
        return result;
    }

    // Check for 'defined' function
    if (starts_with(expr, "defined")) {
        expr.remove_prefix(7);
        skip_whitespace(expr);

        bool has_paren = false;
        if (!expr.empty() && expr[0] == '(') {
            has_paren = true;
            expr.remove_prefix(1);
            skip_whitespace(expr);
        }

        std::string_view symbol = read_identifier(expr);
        if (symbol.empty()) {
            report_error(state, "'defined' requires a symbol name");
            return false;
        }

        if (has_paren) {
            skip_whitespace(expr);
            if (expr.empty() || expr[0] != ')') {
                report_error(state, "Missing ')' after 'defined(symbol'");
            } else {
                expr.remove_prefix(1);
            }
        }

        return is_defined(std::string(symbol));
    }

    // Simple identifier - treat as defined check
    std::string_view symbol = read_identifier(expr);
    if (symbol.empty()) {
        report_error(state, "Expected symbol in preprocessor expression");
        return false;
    }

    return is_defined(std::string(symbol));
}

// ============================================================================
// Helpers
// ============================================================================

bool Preprocessor::is_outputting(const ProcessingState& state) const {
    if (state.condition_stack.empty()) {
        return true;
    }
    return state.condition_stack.back();
}

void Preprocessor::report_error(ProcessingState& state, const std::string& message) {
    PreprocessorDiagnostic diag;
    diag.severity = DiagnosticSeverity::Error;
    diag.message = message;
    diag.line = state.line;
    diag.column = state.column;
    state.diagnostics.push_back(diag);
}

void Preprocessor::report_warning(ProcessingState& state, const std::string& message) {
    PreprocessorDiagnostic diag;
    diag.severity = DiagnosticSeverity::Warning;
    diag.message = message;
    diag.line = state.line;
    diag.column = state.column;
    state.diagnostics.push_back(diag);
}

std::string_view Preprocessor::trim(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return sv;
}

std::string_view Preprocessor::trim_start(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    return sv;
}

bool Preprocessor::starts_with(std::string_view sv, std::string_view prefix) {
    return sv.size() >= prefix.size() && sv.substr(0, prefix.size()) == prefix;
}

std::string_view Preprocessor::skip_whitespace(std::string_view& sv) {
    size_t start = 0;
    while (start < sv.size() && std::isspace(static_cast<unsigned char>(sv[start]))) {
        start++;
    }
    sv.remove_prefix(start);
    return sv;
}

std::string_view Preprocessor::read_identifier(std::string_view& sv) {
    skip_whitespace(sv);

    if (sv.empty()) {
        return {};
    }

    // First character must be letter or underscore
    if (!std::isalpha(static_cast<unsigned char>(sv[0])) && sv[0] != '_') {
        return {};
    }

    size_t end = 1;
    while (end < sv.size() &&
           (std::isalnum(static_cast<unsigned char>(sv[end])) || sv[end] == '_')) {
        end++;
    }

    std::string_view result = sv.substr(0, end);
    sv.remove_prefix(end);
    return result;
}

} // namespace tml::preprocessor
