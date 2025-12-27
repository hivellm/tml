#ifndef TML_COMMON_HPP
#define TML_COMMON_HPP

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace tml {

// Version info
constexpr const char* VERSION = "0.1.0";
constexpr int VERSION_MAJOR = 0;
constexpr int VERSION_MINOR = 1;
constexpr int VERSION_PATCH = 0;

// Global compiler options
struct CompilerOptions {
    static inline bool verbose = false;       // Enable verbose/debug output
    static inline int optimization_level = 0; // 0-3: O0-O3, 4: Os, 5: Oz
    static inline bool debug_info = false;    // Include DWARF debug info
};

// Convenience macro for debug output (only prints when verbose is enabled)
#define TML_DEBUG(msg) \
    do { if (::tml::CompilerOptions::verbose) { std::cerr << msg; } } while(0)

#define TML_DEBUG_LN(msg) \
    do { if (::tml::CompilerOptions::verbose) { std::cerr << msg << "\n"; } } while(0)

// Source location for error reporting
struct SourceLocation {
    std::string_view file;
    uint32_t line;
    uint32_t column;
    uint32_t offset;
    uint32_t length;

    [[nodiscard]] auto operator==(const SourceLocation& other) const -> bool = default;
};

// Span of source code (start to end location)
struct SourceSpan {
    SourceLocation start;
    SourceLocation end;

    [[nodiscard]] static auto merge(const SourceSpan& a, const SourceSpan& b) -> SourceSpan {
        return {a.start, b.end};
    }
};

// Result type for operations that can fail
template <typename T, typename E = std::string>
using Result = std::variant<T, E>;

template <typename T, typename E>
[[nodiscard]] constexpr auto is_ok(const Result<T, E>& result) -> bool {
    return std::holds_alternative<T>(result);
}

template <typename T, typename E>
[[nodiscard]] constexpr auto is_err(const Result<T, E>& result) -> bool {
    return std::holds_alternative<E>(result);
}

template <typename T, typename E>
[[nodiscard]] constexpr auto unwrap(Result<T, E>& result) -> T& {
    return std::get<T>(result);
}

template <typename T, typename E>
[[nodiscard]] constexpr auto unwrap(const Result<T, E>& result) -> const T& {
    return std::get<T>(result);
}

template <typename T, typename E>
[[nodiscard]] constexpr auto unwrap_err(Result<T, E>& result) -> E& {
    return std::get<E>(result);
}

template <typename T, typename E>
[[nodiscard]] constexpr auto unwrap_err(const Result<T, E>& result) -> const E& {
    return std::get<E>(result);
}

// Smart pointer aliases
template <typename T>
using Box = std::unique_ptr<T>;

template <typename T>
using Rc = std::shared_ptr<T>;

template <typename T, typename... Args>
[[nodiscard]] auto make_box(Args&&... args) -> Box<T> {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
[[nodiscard]] auto make_rc(Args&&... args) -> Rc<T> {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

} // namespace tml

#endif // TML_COMMON_HPP
