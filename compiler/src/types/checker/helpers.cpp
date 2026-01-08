//! # Type Checker - Helper Functions
//!
//! This file implements shared utilities used by other checker modules.
//!
//! ## Type Classification
//!
//! | Function          | Checks For                        |
//! |-------------------|-----------------------------------|
//! | `is_integer_type` | I8-I128, U8-U128                  |
//! | `is_float_type`   | F32, F64                          |
//! | `types_compatible`| Structural type compatibility     |
//!
//! ## Type Compatibility Rules
//!
//! `types_compatible()` handles:
//! - Exact type equality
//! - Type variable unification
//! - Integer/float literal coercion
//! - Null pointer compatibility
//! - Array to slice coercion
//! - Closure to function type compatibility
//! - `impl Behavior` type compatibility
//!
//! ## Error Suggestions
//!
//! - `levenshtein_distance()`: Edit distance for typo detection
//! - `get_all_known_names()`: Collects all symbols in scope
//! - `find_similar_names()`: Suggests corrections for unknown identifiers

#include "types/checker.hpp"

#include <algorithm>

namespace tml::types {

// Helper to check if a type is an integer type
bool is_integer_type(const TypePtr& type) {
    if (!type->is<PrimitiveType>())
        return false;
    auto kind = type->as<PrimitiveType>().kind;
    return kind == PrimitiveKind::I8 || kind == PrimitiveKind::I16 || kind == PrimitiveKind::I32 ||
           kind == PrimitiveKind::I64 || kind == PrimitiveKind::I128 || kind == PrimitiveKind::U8 ||
           kind == PrimitiveKind::U16 || kind == PrimitiveKind::U32 || kind == PrimitiveKind::U64 ||
           kind == PrimitiveKind::U128;
}

// Helper to check if a type is a float type
bool is_float_type(const TypePtr& type) {
    if (!type->is<PrimitiveType>())
        return false;
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
    const std::vector<std::string> extensions = {".dll", ".so", ".dylib", ".lib", ".a"};
    for (const auto& ext : extensions) {
        if (name.size() > ext.size() && name.substr(name.size() - ext.size()) == ext) {
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
    if (types_equal(expected, actual))
        return true;

    // Type variables are compatible with any type (they represent unknown types)
    // This handles cases like `let empty: [I32; 0] = []` where the empty array
    // has element type TypeVar that should unify with I32
    if (expected->is<TypeVar>() || actual->is<TypeVar>())
        return true;

    // Allow integer literal (I64) to be assigned to any integer type
    if (is_integer_type(expected) && is_integer_type(actual))
        return true;

    // Allow float literal (F64) to be assigned to any float type
    if (is_float_type(expected) && is_float_type(actual))
        return true;

    // Allow null (Ptr[Unit]) to be assigned to any pointer type
    // null literal has type Ptr[Unit], but can be assigned to/compared with any Ptr[T]
    // Note: Ptr[T] in TML syntax is parsed as NamedType{name="Ptr", type_args=[T]}
    //       while *T is parsed as PtrType

    // Helper lambdas to check if a type is a pointer type
    auto is_ptr_type = [](const TypePtr& t) -> bool {
        if (t->is<PtrType>())
            return true;
        if (t->is<NamedType>()) {
            const auto& named = t->as<NamedType>();
            return named.name == "Ptr" && named.type_args.size() == 1;
        }
        return false;
    };

    auto get_ptr_inner = [](const TypePtr& t) -> TypePtr {
        if (t->is<PtrType>())
            return t->as<PtrType>().inner;
        if (t->is<NamedType>()) {
            const auto& named = t->as<NamedType>();
            if (named.name == "Ptr" && !named.type_args.empty())
                return named.type_args[0];
        }
        return nullptr;
    };

    auto is_ptr_to_unit = [&](const TypePtr& t) -> bool {
        auto inner = get_ptr_inner(t);
        if (!inner)
            return false;
        return inner->is<PrimitiveType>() && inner->as<PrimitiveType>().kind == PrimitiveKind::Unit;
    };

    if (is_ptr_type(expected) && is_ptr_type(actual)) {
        // Check if actual is Ptr[Unit] (null literal type)
        if (is_ptr_to_unit(actual)) {
            return true; // null is compatible with any pointer type
        }

        // Also check if expected is Ptr[Unit] (for comparisons like ptr == null)
        if (is_ptr_to_unit(expected)) {
            return true; // any pointer type is compatible with null
        }
    }

    // Allow array [T; N] to be assigned to slice [T]
    if (expected->is<SliceType>() && actual->is<ArrayType>()) {
        const auto& slice_elem = expected->as<SliceType>().element;
        const auto& array_elem = actual->as<ArrayType>().element;
        return types_compatible(slice_elem, array_elem);
    }

    // Allow array [T1; N] to be compatible with array [T2; N] if element types are compatible
    // This handles cases like let arr: [I32; 5] = [1, 2, 3, 4, 5] where literals are I64
    if (expected->is<ArrayType>() && actual->is<ArrayType>()) {
        const auto& expected_arr = expected->as<ArrayType>();
        const auto& actual_arr = actual->as<ArrayType>();
        // Sizes must match
        if (expected_arr.size != actual_arr.size)
            return false;
        // Element types must be compatible
        return types_compatible(expected_arr.element, actual_arr.element);
    }

    // Allow array [T; N] to be assigned to List[T]
    // This enables: let list: List[I32] = [1, 2, 3]
    if (expected->is<NamedType>() && actual->is<ArrayType>()) {
        const auto& named = expected->as<NamedType>();
        if (named.name == "List" && !named.type_args.empty()) {
            const auto& list_elem = named.type_args[0];
            const auto& array_elem = actual->as<ArrayType>().element;
            return types_compatible(list_elem, array_elem);
        }
        // Allow array [T; N] to be assigned to Slice[T]
        // This enables automatic coercion in function calls: func foo(s: Slice[I32])
        // can be called with an array: foo([1, 2, 3])
        if (named.name == "Slice" && !named.type_args.empty()) {
            const auto& slice_elem = named.type_args[0];
            const auto& array_elem = actual->as<ArrayType>().element;
            return types_compatible(slice_elem, array_elem);
        }
    }

    // Allow closure to be assigned to function type if signatures match
    if (expected->is<FuncType>() && actual->is<ClosureType>()) {
        const auto& func = expected->as<FuncType>();
        const auto& closure = actual->as<ClosureType>();

        if (func.params.size() != closure.params.size())
            return false;
        for (size_t i = 0; i < func.params.size(); ++i) {
            if (!types_equal(func.params[i], closure.params[i]))
                return false;
        }
        return types_equal(func.return_type, closure.return_type);
    }

    // Allow any type that could implement a behavior to be assigned to impl Behavior
    // This is a simplified check - full implementation would verify the type actually implements
    // the behavior For now, accept any NamedType (struct/enum) as potentially implementing the
    // behavior
    if (expected->is<ImplBehaviorType>()) {
        // Accept any named type (struct/enum) as a valid implementation
        // The actual behavior implementation check happens elsewhere
        if (actual->is<NamedType>()) {
            return true;
        }
    }

    // Allow impl Behavior to be assigned to a concrete type
    // This enables: let x: ConcreteType = make_impl_behavior()
    // The caller is essentially downcasting to the known concrete type
    if (actual->is<ImplBehaviorType>()) {
        if (expected->is<NamedType>()) {
            return true;
        }
    }

    return false;
}

// Levenshtein distance for similar name suggestions
auto TypeChecker::levenshtein_distance(const std::string& s1, const std::string& s2) -> size_t {
    const size_t m = s1.size();
    const size_t n = s2.size();

    if (m == 0)
        return n;
    if (n == 0)
        return m;

    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));

    for (size_t i = 0; i <= m; ++i)
        dp[i][0] = i;
    for (size_t j = 0; j <= n; ++j)
        dp[0][j] = j;

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
        if (candidate == name)
            continue; // Skip exact match

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
