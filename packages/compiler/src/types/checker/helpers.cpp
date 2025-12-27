// Type checker helper functions
// Shared utilities used by other checker modules

#include "tml/types/checker.hpp"
#include <algorithm>

namespace tml::types {

namespace {

// Convert PrimitiveKind to string name
std::string primitive_to_string(PrimitiveKind kind) {
    switch (kind) {
        case PrimitiveKind::I8: return "I8";
        case PrimitiveKind::I16: return "I16";
        case PrimitiveKind::I32: return "I32";
        case PrimitiveKind::I64: return "I64";
        case PrimitiveKind::I128: return "I128";
        case PrimitiveKind::U8: return "U8";
        case PrimitiveKind::U16: return "U16";
        case PrimitiveKind::U32: return "U32";
        case PrimitiveKind::U64: return "U64";
        case PrimitiveKind::U128: return "U128";
        case PrimitiveKind::F32: return "F32";
        case PrimitiveKind::F64: return "F64";
        case PrimitiveKind::Bool: return "Bool";
        case PrimitiveKind::Char: return "Char";
        case PrimitiveKind::Str: return "Str";
        case PrimitiveKind::Unit: return "Unit";
        case PrimitiveKind::Never: return "Never";
    }
    return "unknown";
}

} // anonymous namespace

// Helper to check if a type is an integer type
bool is_integer_type(const TypePtr& type) {
    if (!type->is<PrimitiveType>()) return false;
    auto kind = type->as<PrimitiveType>().kind;
    return kind == PrimitiveKind::I8 || kind == PrimitiveKind::I16 ||
           kind == PrimitiveKind::I32 || kind == PrimitiveKind::I64 ||
           kind == PrimitiveKind::I128 ||
           kind == PrimitiveKind::U8 || kind == PrimitiveKind::U16 ||
           kind == PrimitiveKind::U32 || kind == PrimitiveKind::U64 ||
           kind == PrimitiveKind::U128;
}

// Helper to check if a type is a float type
bool is_float_type(const TypePtr& type) {
    if (!type->is<PrimitiveType>()) return false;
    auto kind = type->as<PrimitiveType>().kind;
    return kind == PrimitiveKind::F32 || kind == PrimitiveKind::F64;
}

// Extract library name from @link path for FFI namespace
// Examples:
//   "SDL2"           -> "SDL2"
//   "SDL2.dll"       -> "SDL2"
//   "libSDL2.so"     -> "SDL2"
//   "./vendor/foo.a" -> "foo"
//   "user32"         -> "user32"
std::string extract_ffi_module_name(const std::string& link_path) {
    std::string name = link_path;

    // Remove path prefix (keep only filename)
    auto slash_pos = name.find_last_of("/\\");
    if (slash_pos != std::string::npos) {
        name = name.substr(slash_pos + 1);
    }

    // Remove common library extensions
    const std::vector<std::string> extensions = {
        ".dll", ".so", ".dylib", ".lib", ".a"
    };
    for (const auto& ext : extensions) {
        if (name.size() > ext.size() &&
            name.substr(name.size() - ext.size()) == ext) {
            name = name.substr(0, name.size() - ext.size());
            break;
        }
    }

    // Remove "lib" prefix (common on Unix)
    if (name.size() > 3 && name.substr(0, 3) == "lib") {
        name = name.substr(3);
    }

    return name;
}

// Check if types are compatible (allowing numeric coercion)
bool types_compatible(const TypePtr& expected, const TypePtr& actual) {
    if (types_equal(expected, actual)) return true;

    // Allow integer literal (I64) to be assigned to any integer type
    if (is_integer_type(expected) && is_integer_type(actual)) return true;

    // Allow float literal (F64) to be assigned to any float type
    if (is_float_type(expected) && is_float_type(actual)) return true;

    // Allow array [T; N] to be assigned to slice [T]
    if (expected->is<SliceType>() && actual->is<ArrayType>()) {
        const auto& slice_elem = expected->as<SliceType>().element;
        const auto& array_elem = actual->as<ArrayType>().element;
        return types_compatible(slice_elem, array_elem);
    }

    // Allow closure to be assigned to function type if signatures match
    if (expected->is<FuncType>() && actual->is<ClosureType>()) {
        const auto& func = expected->as<FuncType>();
        const auto& closure = actual->as<ClosureType>();

        if (func.params.size() != closure.params.size()) return false;
        for (size_t i = 0; i < func.params.size(); ++i) {
            if (!types_equal(func.params[i], closure.params[i])) return false;
        }
        return types_equal(func.return_type, closure.return_type);
    }

    return false;
}

// Levenshtein distance for similar name suggestions
auto TypeChecker::levenshtein_distance(const std::string& s1, const std::string& s2) -> size_t {
    const size_t m = s1.size();
    const size_t n = s2.size();

    if (m == 0) return n;
    if (n == 0) return m;

    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));

    for (size_t i = 0; i <= m; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= n; ++j) dp[0][j] = j;

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            size_t cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i - 1][j] + 1,       // deletion
                dp[i][j - 1] + 1,       // insertion
                dp[i - 1][j - 1] + cost // substitution
            });
        }
    }

    return dp[m][n];
}

auto TypeChecker::get_all_known_names() -> std::vector<std::string> {
    std::vector<std::string> names;

    // Get all variable names from current scope chain
    for (auto scope = env_.current_scope(); scope != nullptr; scope = scope->parent()) {
        for (const auto& [name, _] : scope->symbols()) {
            names.push_back(name);
        }
    }

    // Get all function names
    for (const auto& name : env_.all_func_names()) {
        names.push_back(name);
    }

    // Get all struct names
    for (const auto& [name, _] : env_.all_structs()) {
        names.push_back(name);
    }

    // Get all behavior names
    for (const auto& [name, _] : env_.all_behaviors()) {
        names.push_back(name);
    }

    // Get all enum names
    for (const auto& [name, _] : env_.all_enums()) {
        names.push_back(name);
    }

    return names;
}

auto TypeChecker::find_similar_names(const std::string& name,
                                      const std::vector<std::string>& candidates,
                                      size_t max_suggestions) -> std::vector<std::string> {
    // Calculate maximum allowed distance (scales with name length)
    size_t max_distance = std::max(size_t(2), name.length() / 2);

    std::vector<std::pair<std::string, size_t>> scored;
    for (const auto& candidate : candidates) {
        if (candidate == name) continue; // Skip exact match

        size_t dist = levenshtein_distance(name, candidate);
        if (dist <= max_distance) {
            scored.push_back({candidate, dist});
        }
    }

    // Sort by distance (closest first)
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    // Take top N suggestions
    std::vector<std::string> result;
    for (size_t i = 0; i < std::min(max_suggestions, scored.size()); ++i) {
        result.push_back(scored[i].first);
    }

    return result;
}

} // namespace tml::types
