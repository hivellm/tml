//! # Type Environment - Lookups
//!
//! This file implements type and definition lookups.
//!
//! ## Lookup Methods
//!
//! | Method             | Looks Up                         |
//! |--------------------|----------------------------------|
//! | `lookup_struct()`  | Struct definition by name        |
//! | `lookup_enum()`    | Enum definition by name          |
//! | `lookup_behavior()`| Behavior definition by name      |
//! | `lookup_function()`| Function signatures (overloaded) |
//! | `lookup_method()`  | Method on type                   |
//!
//! ## Import Resolution
//!
//! Lookups check local definitions first, then imported modules
//! via `resolve_imported_symbol()` and the module registry.
//!
//! ## Method Resolution
//!
//! `lookup_method()` searches:
//! 1. Inherent methods (impl blocks on the type)
//! 2. Behavior methods (impl Behavior for Type)

#include "types/env.hpp"
#include "types/module.hpp"

#include <algorithm>
#include <set>
#include <unordered_set>

namespace tml::types {

auto TypeEnv::lookup_struct(const std::string& name) const -> std::optional<StructDef> {
    auto it = structs_.find(name);
    if (it != structs_.end())
        return it->second;
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_struct(module_path, symbol_name);
            }
        }
    }
    return std::nullopt;
}

auto TypeEnv::lookup_enum(const std::string& name) const -> std::optional<EnumDef> {
    auto it = enums_.find(name);
    if (it != enums_.end())
        return it->second;
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_enum(module_path, symbol_name);
            }
        }
        // Fallback: search all modules for the enum
        // This is necessary when library code is re-parsed during codegen
        // and the import context isn't available
        const auto& all_modules = module_registry_->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            auto enum_it = mod.enums.find(name);
            if (enum_it != mod.enums.end()) {
                return enum_it->second;
            }
        }
    }
    return std::nullopt;
}

auto TypeEnv::lookup_behavior(const std::string& name) const -> std::optional<BehaviorDef> {
    auto it = behaviors_.find(name);
    if (it != behaviors_.end())
        return it->second;
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_behavior(module_path, symbol_name);
            }
        }
    }
    return std::nullopt;
}

bool TypeEnv::types_match(const TypePtr& a, const TypePtr& b) {
    if (!a || !b)
        return false;
    if (a->kind.index() != b->kind.index())
        return false;
    if (a->is<PrimitiveType>() && b->is<PrimitiveType>()) {
        return a->as<PrimitiveType>().kind == b->as<PrimitiveType>().kind;
    }
    if (a->is<NamedType>() && b->is<NamedType>()) {
        return a->as<NamedType>().name == b->as<NamedType>().name;
    }
    if (a->is<RefType>() && b->is<RefType>()) {
        const auto& ref_a = a->as<RefType>();
        const auto& ref_b = b->as<RefType>();
        return ref_a.is_mut == ref_b.is_mut && types_match(ref_a.inner, ref_b.inner);
    }
    if (a->is<FuncType>() && b->is<FuncType>()) {
        const auto& func_a = a->as<FuncType>();
        const auto& func_b = b->as<FuncType>();
        if (func_a.params.size() != func_b.params.size())
            return false;
        for (size_t i = 0; i < func_a.params.size(); ++i) {
            if (!types_match(func_a.params[i], func_b.params[i]))
                return false;
        }
        return types_match(func_a.return_type, func_b.return_type);
    }
    return false;
}

auto TypeEnv::lookup_func(const std::string& name) const -> std::optional<FuncSig> {
    // Debug: trace function lookups for impl methods
    if (name.find("::") != std::string::npos) {
        // Uncomment to debug: std::cerr << "[LOOKUP] Looking for " << name << " in functions_
        // (size=" << functions_.size() << ")\n";
    }
    auto it = functions_.find(name);
    if (it != functions_.end() && !it->second.empty()) {
        return it->second[0];
    }
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_function(module_path, symbol_name);
            }
        }
        // If name contains "::" (like "Range::next" or "SDL2::init"), try direct lookup
        auto method_pos = name.find("::");
        if (method_pos != std::string::npos) {
            std::string module_name = name.substr(0, method_pos);
            std::string func_name = name.substr(method_pos + 2);

            // First, try direct module lookup (works for FFI modules like SDL2::init)
            auto direct_result = module_registry_->lookup_function(module_name, func_name);
            if (direct_result) {
                return direct_result;
            }

            // Try to resolve the type to its module (for Type::method patterns)
            auto type_import_path = resolve_imported_symbol(module_name);
            if (type_import_path) {
                auto pos = type_import_path->rfind("::");
                if (pos != std::string::npos) {
                    std::string module_path = type_import_path->substr(0, pos);
                    // Lookup "Type::method" in the module
                    return module_registry_->lookup_function(module_path, name);
                }
            }

            // Try to match module_name as a short alias for a loaded module
            // E.g., "json" matches "std::json", "collections" matches "std::collections"
            const auto& all_modules = module_registry_->get_all_modules();
            for (const auto& [mod_path, mod] : all_modules) {
                // Check if module_name matches the last segment of the module path
                // e.g., "json" matches "std::json", "thread" matches "std::thread"
                auto last_sep = mod_path.rfind("::");
                std::string short_name =
                    (last_sep != std::string::npos) ? mod_path.substr(last_sep + 2) : mod_path;
                if (short_name == module_name) {
                    // Found a matching module - look up the function in it
                    auto func_it = mod.functions.find(func_name);
                    if (func_it != mod.functions.end()) {
                        return func_it->second;
                    }
                }
            }

            // Final fallback: search all modules for "Type::method" pattern
            // This handles cases where we're inside module code generation
            // and need to find methods on types defined in the same or other modules
            for (const auto& [mod_path, mod] : all_modules) {
                auto func_it = mod.functions.find(name);
                if (func_it != mod.functions.end()) {
                    return func_it->second;
                }
            }
        }
    }
    return std::nullopt;
}

auto TypeEnv::lookup_func_overload(const std::string& name,
                                   const std::vector<TypePtr>& arg_types) const
    -> std::optional<FuncSig> {
    auto it = functions_.find(name);
    if (it != functions_.end()) {
        for (const auto& sig : it->second) {
            if (sig.params.size() != arg_types.size())
                continue;
            bool matches = true;
            for (size_t i = 0; i < arg_types.size(); ++i) {
                if (!types_match(arg_types[i], sig.params[i])) {
                    matches = false;
                    break;
                }
            }
            if (matches)
                return sig;
        }
    }
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                auto sig = module_registry_->lookup_function(module_path, symbol_name);
                if (sig && sig->params.size() == arg_types.size()) {
                    bool matches = true;
                    for (size_t i = 0; i < arg_types.size(); ++i) {
                        if (!types_match(arg_types[i], sig->params[i])) {
                            matches = false;
                            break;
                        }
                    }
                    if (matches)
                        return sig;
                }
            }
        }
    }
    return std::nullopt;
}

auto TypeEnv::get_all_overloads(const std::string& name) const -> std::vector<FuncSig> {
    auto it = functions_.find(name);
    if (it != functions_.end())
        return it->second;
    return {};
}

auto TypeEnv::lookup_type_alias(const std::string& name) const -> std::optional<TypePtr> {
    auto it = type_aliases_.find(name);
    if (it != type_aliases_.end())
        return it->second;
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_type_alias(module_path, symbol_name);
            }
        }
    }
    return std::nullopt;
}

auto TypeEnv::lookup_type_alias_generics(const std::string& name) const
    -> std::optional<std::vector<std::string>> {
    auto it = type_alias_generics_.find(name);
    if (it != type_alias_generics_.end())
        return it->second;
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_type_alias_generics(module_path, symbol_name);
            }
        }
    }
    return std::nullopt;
}

auto TypeEnv::lookup_class(const std::string& name) const -> std::optional<ClassDef> {
    auto it = classes_.find(name);
    if (it != classes_.end())
        return it->second;
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_class(module_path, symbol_name);
            }
        }
    }
    return std::nullopt;
}

auto TypeEnv::lookup_interface(const std::string& name) const -> std::optional<InterfaceDef> {
    auto it = interfaces_.find(name);
    if (it != interfaces_.end())
        return it->second;
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_interface(module_path, symbol_name);
            }
        }
    }
    return std::nullopt;
}

auto TypeEnv::all_enums() const -> const std::unordered_map<std::string, EnumDef>& {
    return enums_;
}

auto TypeEnv::all_structs() const -> const std::unordered_map<std::string, StructDef>& {
    return structs_;
}

auto TypeEnv::all_behaviors() const -> const std::unordered_map<std::string, BehaviorDef>& {
    return behaviors_;
}

auto TypeEnv::all_func_names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    for (const auto& [name, _] : functions_) {
        names.push_back(name);
    }
    return names;
}

auto TypeEnv::get_module(const std::string& module_path) const -> std::optional<Module> {
    if (!module_registry_)
        return std::nullopt;
    return module_registry_->get_module(module_path);
}

auto TypeEnv::get_all_modules() const -> std::vector<std::pair<std::string, Module>> {
    std::vector<std::pair<std::string, Module>> result;
    if (module_registry_) {
        for (const auto& [path, mod] : module_registry_->get_all_modules()) {
            result.emplace_back(path, mod);
        }
    }
    return result;
}

void TypeEnv::register_impl(const std::string& type_name, const std::string& behavior_name) {
    behavior_impls_[type_name].push_back(behavior_name);
}

// Helper function to check if a behavior inherits from target_behavior with cycle detection
static bool behavior_inherits_from(const TypeEnv& env, const std::string& behavior_name,
                                   const std::string& target_behavior,
                                   std::unordered_set<std::string>& visited) {
    // Prevent cycles
    if (visited.count(behavior_name) > 0) {
        return false;
    }
    visited.insert(behavior_name);

    auto behavior_def = env.lookup_behavior(behavior_name);
    if (!behavior_def) {
        return false;
    }

    for (const auto& super : behavior_def->super_behaviors) {
        if (super == target_behavior) {
            return true;
        }
        // Recursively check super behaviors
        if (behavior_inherits_from(env, super, target_behavior, visited)) {
            return true;
        }
    }
    return false;
}

bool TypeEnv::type_implements(const std::string& type_name,
                              const std::string& behavior_name) const {
    // First check explicit implementations
    auto it = behavior_impls_.find(type_name);
    if (it != behavior_impls_.end()) {
        const auto& behaviors = it->second;
        if (std::find(behaviors.begin(), behaviors.end(), behavior_name) != behaviors.end()) {
            return true;
        }

        // Check if any of the implemented behaviors have this as a super behavior
        // i.e., if type implements ChildBehavior and ChildBehavior: ParentBehavior,
        // then type also implements ParentBehavior
        for (const auto& impl_behavior : behaviors) {
            std::unordered_set<std::string> visited;
            if (behavior_inherits_from(*this, impl_behavior, behavior_name, visited)) {
                return true;
            }
        }
    }

    // Auto-derive Send/Sync for composite types (structs, enums)
    // A type is Send if all its fields are Send
    // A type is Sync if all its fields are Sync
    if (behavior_name == "Send" || behavior_name == "Sync") {
        // Check if this is a struct
        auto struct_def = lookup_struct(type_name);
        if (struct_def) {
            // Struct is Send/Sync if all fields are Send/Sync
            for (const auto& field : struct_def->fields) {
                if (!type_implements(field.type, behavior_name)) {
                    return false;
                }
            }
            return true;
        }

        // Check if this is an enum
        auto enum_def = lookup_enum(type_name);
        if (enum_def) {
            // Enum is Send/Sync if all variant payloads are Send/Sync
            for (const auto& [variant_name, payload_types] : enum_def->variants) {
                for (const auto& payload_type : payload_types) {
                    if (!type_implements(payload_type, behavior_name)) {
                        return false;
                    }
                }
            }
            return true;
        }

        // Check if this is a class
        auto class_def = lookup_class(type_name);
        if (class_def) {
            // Check base class
            if (class_def->base_class) {
                if (!type_implements(*class_def->base_class, behavior_name)) {
                    return false;
                }
            }
            // Check all fields
            for (const auto& field : class_def->fields) {
                if (!type_implements(field.type, behavior_name)) {
                    return false;
                }
            }
            return true;
        }
    }

    return false;
}

bool TypeEnv::type_implements(const TypePtr& type, const std::string& behavior_name) const {
    if (!type) {
        return false;
    }

    // ========================================================================
    // Thread Safety Marker Handling (Send/Sync)
    // ========================================================================

    if (behavior_name == "Send" || behavior_name == "Sync") {
        // Raw pointers are NOT Send or Sync by default
        // They represent raw memory access without any safety guarantees
        if (type->is<PtrType>()) {
            return false;
        }

        // References: depends on mutability
        if (type->is<RefType>()) {
            const auto& ref_type = type->as<RefType>();
            if (ref_type.is_mut) {
                // Mutable references: mut ref T is Send if T is Send
                // (exclusive access can be transferred)
                if (behavior_name == "Send") {
                    return type_implements(ref_type.inner, "Send");
                }
                // mut ref T is NOT Sync because we can't share mutable access
                // (two threads with mut ref would violate exclusive access)
                return false;
            } else {
                // Immutable references: ref T is Send if T is Sync
                // (because sending a reference means sharing it across threads)
                if (behavior_name == "Send") {
                    return type_implements(ref_type.inner, "Sync");
                }
                // ref T is Sync if T is Sync
                return type_implements(ref_type.inner, "Sync");
            }
        }

        // Function pointers are Send and Sync (just code addresses)
        if (type->is<FuncType>()) {
            return true;
        }

        // Closures: Send if all captures are Send, Sync if all captures are Sync
        // For now, we conservatively say closures are not Send/Sync
        // unless explicitly marked (requires capture analysis)
        if (type->is<ClosureType>()) {
            // TODO: Analyze captured variables to determine Send/Sync
            // For now, assume closures that only capture Send types are Send
            return false;
        }

        // Tuples: Send/Sync if all elements are Send/Sync
        if (type->is<TupleType>()) {
            const auto& tuple = type->as<TupleType>();
            for (const auto& elem : tuple.elements) {
                if (!type_implements(elem, behavior_name)) {
                    return false;
                }
            }
            return true;
        }

        // Arrays: Send/Sync if element type is Send/Sync
        if (type->is<ArrayType>()) {
            const auto& arr = type->as<ArrayType>();
            return type_implements(arr.element, behavior_name);
        }

        // Slices: Send/Sync if element type is Send/Sync
        if (type->is<SliceType>()) {
            const auto& slice = type->as<SliceType>();
            return type_implements(slice.element, behavior_name);
        }
    }

    // ========================================================================
    // Standard Behavior Handling
    // ========================================================================

    // Handle ClosureType - closures automatically implement Fn, FnMut, FnOnce
    if (type->is<ClosureType>()) {
        // Closures implement Fn, FnMut, and FnOnce
        // The specific trait depends on how captures are used:
        // - No mut captures: Fn (can call multiple times immutably)
        // - Has mut captures: FnMut (can call multiple times with mutation)
        // - Consumes captures: FnOnce (can only call once)
        // For now, assume all closures implement all three (conservative)
        if (behavior_name == "Fn" || behavior_name == "FnMut" || behavior_name == "FnOnce") {
            return true;
        }
        return false;
    }

    // Handle FuncType - function pointers also implement Fn traits
    if (type->is<FuncType>()) {
        if (behavior_name == "Fn" || behavior_name == "FnMut" || behavior_name == "FnOnce") {
            return true;
        }
        return false;
    }

    // For named types, delegate to the string-based lookup
    if (type->is<NamedType>()) {
        return type_implements(type->as<NamedType>().name, behavior_name);
    }

    // For primitive types, check by name
    if (type->is<PrimitiveType>()) {
        return type_implements(primitive_kind_to_string(type->as<PrimitiveType>().kind),
                               behavior_name);
    }

    // For class types, check if the class implements the behavior (as an interface)
    if (type->is<ClassType>()) {
        return class_implements_interface(type->as<ClassType>().name, behavior_name);
    }

    // For generic types, we need to check if the type parameter has the bound
    // This will be handled by where clause checking at call sites
    if (type->is<GenericType>()) {
        // Generic types are assumed NOT to implement behaviors unless bounded
        return false;
    }

    return false;
}

bool TypeEnv::type_needs_drop(const std::string& type_name) const {
    // Check if type explicitly implements Drop
    if (type_implements(type_name, "Drop")) {
        return true;
    }

    // Primitive types never need drop
    static const std::set<std::string> primitives = {"I8",  "I16", "I32",  "I64",  "I128",
                                                     "U8",  "U16", "U32",  "U64",  "U128",
                                                     "F32", "F64", "Bool", "Char", "Unit"};
    if (primitives.count(type_name) > 0) {
        return false;
    }

    // Str is a special case - it may need drop depending on implementation
    // For now, we don't drop Str since it's managed by the runtime
    if (type_name == "Str") {
        return false;
    }

    // Check if it's a struct with fields that need drop
    auto struct_def = lookup_struct(type_name);
    if (struct_def) {
        for (const auto& field : struct_def->fields) {
            if (type_needs_drop(field.type)) {
                return true;
            }
        }
        return false;
    }

    // Check if it's a class with fields that need drop
    auto class_def = lookup_class(type_name);
    if (class_def) {
        // Check if base class needs drop
        if (class_def->base_class) {
            if (type_needs_drop(*class_def->base_class)) {
                return true;
            }
        }

        // Check if any field needs drop
        for (const auto& field : class_def->fields) {
            if (type_needs_drop(field.type)) {
                return true;
            }
        }
        return false;
    }

    // Check if it's an enum with variants that need drop
    auto enum_def = lookup_enum(type_name);
    if (enum_def) {
        for (const auto& [variant_name, variant_types] : enum_def->variants) {
            for (const auto& variant_type : variant_types) {
                if (type_needs_drop(variant_type)) {
                    return true;
                }
            }
        }
        return false;
    }

    // Unknown types don't need drop by default
    return false;
}

bool TypeEnv::type_needs_drop(const TypePtr& type) const {
    if (!type) {
        return false;
    }

    return std::visit(
        [this](const auto& t) -> bool {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, PrimitiveType>) {
                // Primitives never need drop (includes Unit and Never kinds)
                return false;
            } else if constexpr (std::is_same_v<T, NamedType>) {
                // Check if the named type needs drop
                return type_needs_drop(t.name);
            } else if constexpr (std::is_same_v<T, RefType>) {
                // References don't own the data, so no drop needed
                return false;
            } else if constexpr (std::is_same_v<T, PtrType>) {
                // Raw pointers don't own the data, so no drop needed
                return false;
            } else if constexpr (std::is_same_v<T, TupleType>) {
                // Tuple needs drop if any element needs drop
                for (const auto& elem : t.elements) {
                    if (type_needs_drop(elem)) {
                        return true;
                    }
                }
                return false;
            } else if constexpr (std::is_same_v<T, ArrayType>) {
                // Array needs drop if element type needs drop
                return type_needs_drop(t.element);
            } else if constexpr (std::is_same_v<T, SliceType>) {
                // Slices don't own the data (they're fat pointers/views)
                return false;
            } else if constexpr (std::is_same_v<T, FuncType>) {
                // Function types don't need drop
                return false;
            } else if constexpr (std::is_same_v<T, ClosureType>) {
                // Closures may capture values that need drop
                // For now, conservative: let the closure handle it
                return false;
            } else if constexpr (std::is_same_v<T, GenericType>) {
                // Generic types - we don't know at compile time, assume no drop needed
                // The monomorphized version will be checked
                return false;
            } else if constexpr (std::is_same_v<T, TypeVar>) {
                // Type variables - not resolved yet
                return false;
            } else if constexpr (std::is_same_v<T, ConstGenericType>) {
                // Const generics are values, not types with drop
                return false;
            } else if constexpr (std::is_same_v<T, DynBehaviorType>) {
                // Trait objects may need drop - depends on the underlying type
                // For boxed trait objects, the box handles the drop
                return false; // Conservative: let the container handle it
            } else if constexpr (std::is_same_v<T, ImplBehaviorType>) {
                // Impl behavior types - check the underlying type
                return false;
            } else if constexpr (std::is_same_v<T, ClassType>) {
                // Classes may need drop - check the class definition
                return type_needs_drop(t.name);
            } else if constexpr (std::is_same_v<T, InterfaceType>) {
                // Interface types are abstract - don't need drop directly
                return false;
            } else {
                // Default: no drop needed
                return false;
            }
        },
        type->kind);
}

// ============================================================================
// Trivial Destructor Detection
// ============================================================================

bool TypeEnv::is_trivially_destructible(const std::string& type_name) const {
    // Primitive types are trivially destructible
    if (type_name == "I8" || type_name == "I16" || type_name == "I32" || type_name == "I64" ||
        type_name == "I128" || type_name == "U8" || type_name == "U16" || type_name == "U32" ||
        type_name == "U64" || type_name == "U128" || type_name == "F32" || type_name == "F64" ||
        type_name == "Bool" || type_name == "Unit" || type_name == "Char") {
        return true;
    }

    // If type explicitly implements Drop, it's NOT trivially destructible
    if (type_implements(type_name, "Drop")) {
        return false;
    }

    // Check if it's a struct - all fields must be trivially destructible
    auto struct_def = lookup_struct(type_name);
    if (struct_def) {
        for (const auto& field : struct_def->fields) {
            if (!is_trivially_destructible(field.type)) {
                return false;
            }
        }
        return true;
    }

    // Check if it's a class - all fields must be trivially destructible
    // AND no custom destructor logic (no Drop implementation)
    auto class_def = lookup_class(type_name);
    if (class_def) {
        // Classes with base classes need to check base class too
        if (class_def->base_class) {
            if (!is_trivially_destructible(*class_def->base_class)) {
                return false;
            }
        }

        // Check all fields
        for (const auto& field : class_def->fields) {
            if (!is_trivially_destructible(field.type)) {
                return false;
            }
        }
        return true;
    }

    // Check if it's an enum - all variants must be trivially destructible
    auto enum_def = lookup_enum(type_name);
    if (enum_def) {
        for (const auto& [variant_name, variant_type_ptrs] : enum_def->variants) {
            for (const auto& variant_type_ptr : variant_type_ptrs) {
                if (!is_trivially_destructible(variant_type_ptr)) {
                    return false;
                }
            }
        }
        return true;
    }

    // Handle mangled generic type names like "NonNull__I64" or "Skip__RangeIterI64"
    // These are monomorphized types where __ separates the base type from type arguments
    auto sep_pos = type_name.find("__");
    if (sep_pos != std::string::npos) {
        std::string base_type = type_name.substr(0, sep_pos);

        // If the base type implements Drop, not trivially destructible
        if (type_implements(base_type, "Drop")) {
            return false;
        }

        // Also check if there's a Drop impl in module source code
        // This handles imported generic types like MutexGuard[T] that have Drop impls
        if (module_registry_) {
            const auto& all_modules = module_registry_->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                // Check if module source has "impl... Drop for BaseType"
                // This handles generic Drop impls like "impl[T] Drop for MutexGuard[T]"
                if (!mod.source_code.empty()) {
                    std::string pattern1 = "Drop for " + base_type + "[";
                    std::string pattern2 = "Drop for " + base_type + " ";
                    std::string pattern3 = "Drop for " + base_type + "{";
                    if (mod.source_code.find(pattern1) != std::string::npos ||
                        mod.source_code.find(pattern2) != std::string::npos ||
                        mod.source_code.find(pattern3) != std::string::npos) {
                        return false; // Has Drop impl, not trivially destructible
                    }
                }
            }
        }

        // Helper lambda to parse type arguments from mangled string respecting type param count
        // For N type params, we split only N-1 times so the last arg can contain nested generics
        // e.g., Outcome__I32__Maybe__Str with 2 params -> ["I32", "Maybe__Str"]
        auto parse_type_args = [](const std::string& remaining,
                                  size_t num_type_params) -> std::vector<std::string> {
            std::vector<std::string> args;
            if (num_type_params == 0)
                return args;
            if (num_type_params == 1) {
                args.push_back(remaining);
                return args;
            }

            // Split only num_type_params - 1 times
            size_t pos = 0;
            for (size_t i = 0; i < num_type_params - 1 && pos < remaining.size(); ++i) {
                auto next_sep = remaining.find("__", pos);
                if (next_sep != std::string::npos) {
                    args.push_back(remaining.substr(pos, next_sep - pos));
                    pos = next_sep + 2;
                } else {
                    // No more separators - remaining goes into this arg
                    args.push_back(remaining.substr(pos));
                    pos = remaining.size();
                }
            }
            // Last arg gets everything remaining (may contain nested generics)
            if (pos < remaining.size()) {
                args.push_back(remaining.substr(pos));
            }
            return args;
        };

        // Check if the base struct exists and doesn't implement Drop
        auto base_struct = lookup_struct(base_type);
        if (base_struct) {
            // Base type exists and doesn't implement Drop
            // Check if any type argument is non-trivially destructible
            std::string remaining = type_name.substr(sep_pos + 2);
            size_t num_type_params = base_struct->type_params.size();

            auto type_args = parse_type_args(remaining, num_type_params);
            for (const auto& type_arg : type_args) {
                if (!type_arg.empty() && !is_trivially_destructible(type_arg)) {
                    return false;
                }
            }
            return true;
        }

        // Also check for enum
        auto base_enum = lookup_enum(base_type);
        if (base_enum) {
            // Similar logic - base enum exists and doesn't implement Drop
            std::string remaining = type_name.substr(sep_pos + 2);
            size_t num_type_params = base_enum->type_params.size();

            auto type_args = parse_type_args(remaining, num_type_params);
            for (const auto& type_arg : type_args) {
                if (!type_arg.empty() && !is_trivially_destructible(type_arg)) {
                    return false;
                }
            }
            return true;
        }

        // If we can't find the base struct/enum but the base type doesn't implement Drop,
        // it's from an imported module and is trivially destructible (otherwise it would
        // have registered its Drop implementation or have a drop function)
        return true;
    }

    // For unknown types that we can't find in our type registry:
    // If the type doesn't implement Drop (which we already checked above), it's
    // trivially destructible. TML types don't have implicit destructors - only types
    // that explicitly implement Drop need drop calls.
    // This handles types imported from modules (like OnceI32 from core::iter) that
    // aren't in our local struct registry but don't implement Drop.
    return true;
}

bool TypeEnv::is_trivially_destructible(const TypePtr& type) const {
    if (!type) {
        return true; // No type = no destructor needed
    }

    return std::visit(
        [this](const auto& t) -> bool {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, PrimitiveType>) {
                // Primitives are always trivially destructible
                return true;
            } else if constexpr (std::is_same_v<T, NamedType>) {
                // For generic instantiations like Maybe[I32], we need to check:
                // 1. The base type doesn't implement Drop
                // 2. All type arguments are trivially destructible
                // This avoids the issue where checking the generic definition
                // would fail because it contains type parameter T which is
                // conservatively NOT trivially destructible.
                if (!t.type_args.empty()) {
                    // This is a generic instantiation
                    // If the base type doesn't implement Drop, check all type args
                    if (!type_implements(t.name, "Drop")) {
                        for (const auto& arg : t.type_args) {
                            if (!is_trivially_destructible(arg)) {
                                return false;
                            }
                        }
                        return true;
                    }
                    return false; // Base type implements Drop
                }
                // Non-generic named type - check by name
                return is_trivially_destructible(t.name);
            } else if constexpr (std::is_same_v<T, RefType>) {
                // References don't own data - trivially destructible
                return true;
            } else if constexpr (std::is_same_v<T, PtrType>) {
                // Raw pointers don't own data - trivially destructible
                return true;
            } else if constexpr (std::is_same_v<T, TupleType>) {
                // Tuple is trivially destructible if all elements are
                for (const auto& elem : t.elements) {
                    if (!is_trivially_destructible(elem)) {
                        return false;
                    }
                }
                return true;
            } else if constexpr (std::is_same_v<T, ArrayType>) {
                // Array is trivially destructible if element type is
                return is_trivially_destructible(t.element);
            } else if constexpr (std::is_same_v<T, SliceType>) {
                // Slices don't own data - trivially destructible
                return true;
            } else if constexpr (std::is_same_v<T, FuncType>) {
                // Function types are trivially destructible
                return true;
            } else if constexpr (std::is_same_v<T, GenericType>) {
                // Generic types are conservatively NOT trivially destructible
                return false;
            } else if constexpr (std::is_same_v<T, ClosureType>) {
                // Closures may capture non-trivial values
                for (const auto& capture : t.captures) {
                    if (!is_trivially_destructible(capture.type)) {
                        return false;
                    }
                }
                return true;
            } else if constexpr (std::is_same_v<T, ClassType>) {
                // Check the class definition
                return is_trivially_destructible(t.name);
            } else if constexpr (std::is_same_v<T, InterfaceType>) {
                // Interface types are abstract - conservatively NOT trivially destructible
                // (the actual implementing type may not be trivial)
                return false;
            } else {
                // Default: NOT trivially destructible
                return false;
            }
        },
        type->kind);
}

// ============================================================================
// Interior Mutability Checking
// ============================================================================

/// Set of built-in interior mutable type names.
static const std::unordered_set<std::string> INTERIOR_MUTABLE_TYPES = {
    "Cell",   // Single-threaded interior mutability
    "Mutex",  // Thread-safe interior mutability with locking
    "Shared", // Reference counted (Rc equivalent)
    "Sync",   // Thread-safe reference counted (Arc equivalent)
};

bool TypeEnv::is_interior_mutable(const std::string& type_name) const {
    // Check built-in interior mutable types (without generic args)
    // E.g., "Cell" matches "Cell[T]", "Mutex" matches "Mutex[T]"
    std::string base_name = type_name;
    auto bracket_pos = type_name.find('[');
    if (bracket_pos != std::string::npos) {
        base_name = type_name.substr(0, bracket_pos);
    }

    if (INTERIOR_MUTABLE_TYPES.count(base_name) > 0) {
        return true;
    }

    // Check if it's a struct with @interior_mutable decorator
    auto struct_def = lookup_struct(type_name);
    if (struct_def && struct_def->is_interior_mutable) {
        return true;
    }

    // Also check without module path
    if (type_name.find("::") != std::string::npos) {
        auto last_sep = type_name.rfind("::");
        std::string short_name = type_name.substr(last_sep + 2);
        auto short_struct_def = lookup_struct(short_name);
        if (short_struct_def && short_struct_def->is_interior_mutable) {
            return true;
        }
    }

    return false;
}

bool TypeEnv::is_interior_mutable(const TypePtr& type) const {
    if (!type)
        return false;

    return std::visit(
        [this](const auto& t) -> bool {
            using T = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<T, NamedType>) {
                return is_interior_mutable(t.name);
            } else if constexpr (std::is_same_v<T, RefType>) {
                // A reference to an interior mutable type is itself interior mutable
                return is_interior_mutable(t.inner);
            } else if constexpr (std::is_same_v<T, PtrType>) {
                // A pointer to an interior mutable type is itself interior mutable
                return is_interior_mutable(t.inner);
            } else if constexpr (std::is_same_v<T, ClassType>) {
                return is_interior_mutable(t.name);
            } else {
                // Primitives, tuples, arrays, etc. are not interior mutable
                return false;
            }
        },
        type->kind);
}

bool TypeEnv::is_value_class_candidate(const std::string& class_name) const {
    auto class_def = lookup_class(class_name);
    if (!class_def) {
        return false; // Not a class
    }

    // Abstract classes cannot be value classes
    if (class_def->is_abstract) {
        return false;
    }

    // Must be sealed (no subclasses) to ensure no dynamic dispatch needed
    if (!class_def->is_sealed) {
        return false;
    }

    // Check for virtual methods
    for (const auto& method : class_def->methods) {
        if (method.is_virtual || method.is_abstract) {
            return false; // Has virtual methods, needs vtable
        }
    }

    // Check base class (if any)
    if (class_def->base_class) {
        // Base class must also be a value class candidate
        // Or the class doesn't override any virtual methods from base
        auto base_def = lookup_class(*class_def->base_class);
        if (base_def) {
            // If base has virtual methods, we still need a vtable
            // even if this class doesn't add new virtual methods
            for (const auto& method : base_def->methods) {
                if (method.is_virtual || method.is_abstract) {
                    return false; // Base has virtual methods
                }
            }

            // Recursively check if base is also a value class candidate
            if (!is_value_class_candidate(*class_def->base_class)) {
                return false;
            }
        }
    }

    // All checks passed - this class can be treated as a value class
    return true;
}

bool TypeEnv::can_stack_allocate_class(const std::string& class_name) const {
    auto class_def = lookup_class(class_name);
    if (!class_def) {
        return false; // Not a class
    }

    // Abstract classes cannot be stack-allocated directly
    if (class_def->is_abstract) {
        return false;
    }

    // Any non-abstract class can be stack-allocated when we know the exact type
    // at the allocation site. The vtable pointer is still initialized.
    //
    // Safety is ensured by escape analysis:
    // - If the object doesn't escape the function, stack allocation is safe
    // - If it escapes (returned, stored to heap, etc.), codegen should use heap
    //
    // This function indicates TYPE eligibility; escape analysis determines
    // whether a specific ALLOCATION can use stack.

    // Size check: very large classes should still use heap to avoid stack overflow
    constexpr size_t MAX_STACK_CLASS_SIZE = 1024; // 1KB max for stack
    if (class_def->estimated_size > MAX_STACK_CLASS_SIZE) {
        return false;
    }

    return true;
}

} // namespace tml::types
