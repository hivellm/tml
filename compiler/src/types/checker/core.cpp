TML_MODULE("compiler")

//! # Type Checker - Core
//!
//! This file implements the core type checking logic for modules.
//!
//! ## Module Checking Phases
//!
//! | Phase | Method              | Description                      |
//! |-------|---------------------|----------------------------------|
//! | 0     | `process_use_decl`  | Process import statements        |
//! | 1     | `register_*_decl`   | Register type declarations       |
//! | 2     | `check_func_decl`   | Register function signatures     |
//! | 3     | `check_func_body`   | Type-check function bodies       |
//!
//! ## Declaration Registration
//!
//! | Method                  | Registers                        |
//! |-------------------------|----------------------------------|
//! | `register_struct_decl`  | Struct with fields and generics  |
//! | `register_enum_decl`    | Enum with variants and payloads  |
//! | `register_trait_decl`   | Behavior with methods            |
//! | `register_type_alias`   | Type alias definitions           |
//!
//! ## Reserved Names
//!
//! The checker enforces reserved type and behavior names to prevent
//! user code from redefining builtin types like `Maybe`, `Outcome`,
//! `List`, `Eq`, `Ord`, etc.

#include "lexer/token.hpp"
#include "profiler.hpp"
#include "types/builtins_cache.hpp"
#include "types/checker.hpp"
#include "types/module.hpp"
#include "types/module_binary.hpp"

#include <algorithm>
#include <iostream>
#include <set>

namespace tml::types {

// Reserved type names - primitive types that cannot be redefined by user code
// Only language primitives are reserved - library types like Maybe, List can be shadowed
static const std::set<std::string> RESERVED_TYPE_NAMES = {
    // Primitive types
    "I8",
    "I16",
    "I32",
    "I64",
    "I128",
    "U8",
    "U16",
    "U32",
    "U64",
    "U128",
    "F32",
    "F64",
    "Bool",
    "Char",
    "Str",
    "Unit",
    "Never",
    // String builder
    "StringBuilder",
    // Async types
    "Future",
    "Context",
    "Waker",
};

// Reserved behavior (trait) names - builtin behaviors that cannot be redefined
static const std::set<std::string> RESERVED_BEHAVIOR_NAMES = {
    // Comparison behaviors
    "Eq",
    "Ord",
    "PartialEq",
    "PartialOrd",
    // Hashing
    "Hash",
    // Display/Debug
    "Display",
    "Debug",
    // Numeric
    "Numeric",
    // Default value
    "Default",
    // Cloning
    "Duplicate",
    // Iteration
    "Iterator",
    "IntoIterator",
    "FromIterator",
    // Conversion
    "Into",
    "From",
    "TryInto",
    "TryFrom",
    // Indexing
    "Index",
    "IndexMut",
    // Functions
    "Fn",
    "FnMut",
    "FnOnce",
    // Drop
    "Drop",
    // Sized
    "Sized",
    // Send/Sync (concurrency)
    "Send",
    // Async (Future behavior)
    "Future",
};

// Forward declarations from helpers.cpp
bool is_integer_type(const TypePtr& type);
bool is_float_type(const TypePtr& type);
std::string extract_ffi_module_name(const std::string& link_path);
bool types_compatible(const TypePtr& expected, const TypePtr& actual);

// ============================================================================
// Size Estimation for Stack Allocation Eligibility
// ============================================================================

/// Maximum class size for stack allocation eligibility (in bytes).
/// Classes larger than this are always heap-allocated.
static constexpr size_t MAX_STACK_CLASS_SIZE = 256;

/// Estimate the size of a type in bytes (for stack allocation eligibility).
/// Returns 0 for unsized types (slices, dyn, etc.).
static size_t estimate_type_size(const TypePtr& type) {
    if (!type) {
        return 8; // Default pointer size
    }

    return std::visit(
        [](const auto& kind) -> size_t {
            using T = std::decay_t<decltype(kind)>;

            if constexpr (std::is_same_v<T, PrimitiveType>) {
                switch (kind.kind) {
                case PrimitiveKind::Bool:
                case PrimitiveKind::I8:
                case PrimitiveKind::U8:
                    return 1;
                case PrimitiveKind::I16:
                case PrimitiveKind::U16:
                    return 2;
                case PrimitiveKind::I32:
                case PrimitiveKind::U32:
                case PrimitiveKind::F32:
                case PrimitiveKind::Char:
                    return 4;
                case PrimitiveKind::I64:
                case PrimitiveKind::U64:
                case PrimitiveKind::F64:
                    return 8;
                case PrimitiveKind::I128:
                case PrimitiveKind::U128:
                    return 16;
                case PrimitiveKind::Unit:
                case PrimitiveKind::Never:
                    return 0;
                case PrimitiveKind::Str:
                    return 24; // Str is typically ptr + len + capacity
                }
                return 8; // Default for any unknown primitives
            } else if constexpr (std::is_same_v<T, PtrType> || std::is_same_v<T, RefType> ||
                                 std::is_same_v<T, ClassType> || std::is_same_v<T, NamedType>) {
                return 8; // Pointer-sized (actual size computed during codegen for NamedType)
            } else if constexpr (std::is_same_v<T, TupleType>) {
                size_t total = 0;
                for (const auto& elem : kind.elements) {
                    total += estimate_type_size(elem);
                }
                return total;
            } else if constexpr (std::is_same_v<T, ArrayType>) {
                return estimate_type_size(kind.element) * kind.size;
            } else if constexpr (std::is_same_v<T, SliceType> ||
                                 std::is_same_v<T, DynBehaviorType>) {
                return 16; // Fat pointer (ptr + vtable/len)
            } else if constexpr (std::is_same_v<T, GenericType>) {
                return 8; // Conservative - treat as pointer-sized
            } else {
                return 8; // Default to pointer size
            }
        },
        type->kind);
}

TypeChecker::TypeChecker() : env_(BuiltinsSnapshot::instance().create_env()) {}

auto TypeChecker::check_module(const parser::Module& module)
    -> Result<TypeEnv, std::vector<TypeError>> {
    TML_ZONE("types::check_module");
    TML_DEBUG_LN("[DEBUG] check_module called");

    // Ensure module registry exists for FFI namespace support
    if (!env_.module_registry()) {
        env_.set_module_registry(std::make_shared<ModuleRegistry>());
    }

    // Pass 0: Process use declarations (imports)
    for (const auto& decl : module.decls) {
        if (decl->is<parser::UseDecl>()) {
            process_use_decl(decl->as<parser::UseDecl>());
        }
    }
    // First pass: register all type declarations
    for (const auto& decl : module.decls) {
        if (decl->is<parser::StructDecl>()) {
            register_struct_decl(decl->as<parser::StructDecl>());
        } else if (decl->is<parser::UnionDecl>()) {
            register_union_decl(decl->as<parser::UnionDecl>());
        } else if (decl->is<parser::EnumDecl>()) {
            register_enum_decl(decl->as<parser::EnumDecl>());
        } else if (decl->is<parser::TraitDecl>()) {
            register_trait_decl(decl->as<parser::TraitDecl>());
        } else if (decl->is<parser::TypeAliasDecl>()) {
            register_type_alias(decl->as<parser::TypeAliasDecl>());
        } else if (decl->is<parser::InterfaceDecl>()) {
            register_interface_decl(decl->as<parser::InterfaceDecl>());
        } else if (decl->is<parser::ClassDecl>()) {
            register_class_decl(decl->as<parser::ClassDecl>());
        } else if (decl->is<parser::NamespaceDecl>()) {
            // Namespaces handle all passes internally
            register_namespace_decl(decl->as<parser::NamespaceDecl>());
        }
    }

    // Second pass: register function signatures and constants
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            check_func_decl(decl->as<parser::FuncDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            check_impl_decl(decl->as<parser::ImplDecl>());
        } else if (decl->is<parser::ConstDecl>()) {
            check_const_decl(decl->as<parser::ConstDecl>());
        } else if (decl->is<parser::ClassDecl>()) {
            check_class_decl(decl->as<parser::ClassDecl>());
        } else if (decl->is<parser::InterfaceDecl>()) {
            check_interface_decl(decl->as<parser::InterfaceDecl>());
        }
    }

    // Third pass: check function bodies
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            check_func_body(decl->as<parser::FuncDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            check_impl_body(decl->as<parser::ImplDecl>());
        } else if (decl->is<parser::ClassDecl>()) {
            check_class_body(decl->as<parser::ClassDecl>());
        }
    }

    if (has_errors()) {
        return errors_;
    }
    return env_;
}

// ============================================================================
// Namespace Support
// ============================================================================

auto TypeChecker::qualified_name(const std::string& name) const -> std::string {
    if (current_namespace_.empty()) {
        return name;
    }
    std::string result;
    for (const auto& seg : current_namespace_) {
        result += seg + ".";
    }
    return result + name;
}

void TypeChecker::register_namespace_decl(const parser::NamespaceDecl& decl) {
    // Save current namespace and extend it
    auto saved_namespace = current_namespace_;
    for (const auto& seg : decl.path) {
        current_namespace_.push_back(seg);
    }

    // Process all declarations in this namespace
    // Pass 1: Register types
    for (const auto& item : decl.items) {
        if (item->is<parser::StructDecl>()) {
            register_struct_decl(item->as<parser::StructDecl>());
        } else if (item->is<parser::EnumDecl>()) {
            register_enum_decl(item->as<parser::EnumDecl>());
        } else if (item->is<parser::TraitDecl>()) {
            register_trait_decl(item->as<parser::TraitDecl>());
        } else if (item->is<parser::TypeAliasDecl>()) {
            register_type_alias(item->as<parser::TypeAliasDecl>());
        } else if (item->is<parser::InterfaceDecl>()) {
            register_interface_decl(item->as<parser::InterfaceDecl>());
        } else if (item->is<parser::ClassDecl>()) {
            register_class_decl(item->as<parser::ClassDecl>());
        } else if (item->is<parser::NamespaceDecl>()) {
            // Nested namespace - recurse
            register_namespace_decl(item->as<parser::NamespaceDecl>());
        }
    }

    // Pass 2: Check declarations
    for (const auto& item : decl.items) {
        if (item->is<parser::FuncDecl>()) {
            check_func_decl(item->as<parser::FuncDecl>());
        } else if (item->is<parser::ImplDecl>()) {
            check_impl_decl(item->as<parser::ImplDecl>());
        } else if (item->is<parser::ConstDecl>()) {
            check_const_decl(item->as<parser::ConstDecl>());
        } else if (item->is<parser::ClassDecl>()) {
            check_class_decl(item->as<parser::ClassDecl>());
        } else if (item->is<parser::InterfaceDecl>()) {
            check_interface_decl(item->as<parser::InterfaceDecl>());
        }
    }

    // Pass 3: Check bodies
    for (const auto& item : decl.items) {
        if (item->is<parser::FuncDecl>()) {
            check_func_body(item->as<parser::FuncDecl>());
        } else if (item->is<parser::ImplDecl>()) {
            check_impl_body(item->as<parser::ImplDecl>());
        } else if (item->is<parser::ClassDecl>()) {
            check_class_body(item->as<parser::ClassDecl>());
        }
    }

    // Restore namespace
    current_namespace_ = saved_namespace;
}

// Note: register_struct_decl and register_enum_decl moved to decl_struct.cpp

// ============================================================================
// Trait/Alias Registration
// ============================================================================

void TypeChecker::register_trait_decl(const parser::TraitDecl& decl) {
    // Check if the behavior name is reserved (builtin behavior)
    if (RESERVED_BEHAVIOR_NAMES.contains(decl.name)) {
        error("Cannot redefine builtin behavior '" + decl.name +
                  "'. Use the builtin behavior instead of defining your own.",
              decl.span, "T038");
        return;
    }

    std::vector<FuncSig> methods;
    std::set<std::string> methods_with_defaults;

    for (const auto& method : decl.methods) {
        std::vector<TypePtr> params;
        params.reserve(method.params.size());
        for (const auto& p : method.params) {
            params.push_back(resolve_type(*p.type));
        }
        TypePtr ret = method.return_type ? resolve_type(**method.return_type) : make_unit();

        // Extract method's type parameters (non-const generic params)
        std::vector<std::string> method_type_params;
        for (const auto& gp : method.generics) {
            if (!gp.is_const) {
                method_type_params.push_back(gp.name);
            }
        }

        // Extract method's const generic params
        std::vector<ConstGenericParam> method_const_params = extract_const_params(method.generics);

        methods.push_back(FuncSig{.name = method.name,
                                  .params = std::move(params),
                                  .return_type = std::move(ret),
                                  .type_params = std::move(method_type_params),
                                  .is_async = method.is_async,
                                  .span = method.span,
                                  .const_params = std::move(method_const_params)});

        // Track methods with default implementations
        if (method.body.has_value()) {
            methods_with_defaults.insert(method.name);
        }
    }

    std::vector<std::string> type_params;
    for (const auto& param : decl.generics) {
        if (!param.is_const) {
            type_params.push_back(param.name);
        }
    }

    // Extract const generic parameters
    std::vector<ConstGenericParam> const_params = extract_const_params(decl.generics);

    // Collect associated type declarations (including GATs with generic parameters)
    std::vector<AssociatedTypeDef> associated_types;
    for (const auto& assoc : decl.associated_types) {
        // Extract GAT type parameters
        std::vector<std::string> gat_type_params;
        for (const auto& param : assoc.generics) {
            if (!param.is_const) {
                gat_type_params.push_back(param.name);
            }
        }

        std::vector<std::string> bounds;
        for (const auto& bound : assoc.bounds) {
            // Convert TypePtr to string (extract name from parser::NamedType)
            if (bound && bound->is<parser::NamedType>()) {
                const auto& named = bound->as<parser::NamedType>();
                if (!named.path.segments.empty()) {
                    bounds.push_back(named.path.segments.back());
                }
            }
        }
        // Resolve default type if present
        std::optional<TypePtr> default_type = std::nullopt;
        if (assoc.default_type) {
            default_type = resolve_type(**assoc.default_type);
        }
        associated_types.push_back(AssociatedTypeDef{.name = assoc.name,
                                                     .type_params = std::move(gat_type_params),
                                                     .bounds = std::move(bounds),
                                                     .default_type = std::move(default_type)});
    }

    // Extract super behaviors from super_traits
    std::vector<std::string> super_behaviors;
    for (const auto& super : decl.super_traits) {
        if (super && super->is<parser::NamedType>()) {
            const auto& named = super->as<parser::NamedType>();
            if (!named.path.segments.empty()) {
                // Use the full path for the behavior name
                std::string behavior_name = named.path.segments[0];
                for (size_t i = 1; i < named.path.segments.size(); ++i) {
                    behavior_name += "::" + named.path.segments[i];
                }
                super_behaviors.push_back(behavior_name);
            }
        }
    }

    env_.define_behavior(BehaviorDef{.name = decl.name,
                                     .type_params = std::move(type_params),
                                     .const_params = std::move(const_params),
                                     .associated_types = std::move(associated_types),
                                     .methods = std::move(methods),
                                     .super_behaviors = std::move(super_behaviors),
                                     .methods_with_defaults = std::move(methods_with_defaults),
                                     .span = decl.span});
}

void TypeChecker::register_type_alias(const parser::TypeAliasDecl& decl) {
    // Check if the type alias name is reserved (builtin type)
    if (RESERVED_TYPE_NAMES.contains(decl.name)) {
        error("Cannot redefine builtin type '" + decl.name +
                  "'. Use the builtin type instead of defining your own.",
              decl.span, "T038");
        return;
    }

    std::vector<std::string> generic_params;
    generic_params.reserve(decl.generics.size());
    for (const auto& gp : decl.generics) {
        generic_params.push_back(gp.name);
    }
    env_.define_type_alias(decl.name, resolve_type(*decl.type), std::move(generic_params));
}

void TypeChecker::process_use_decl(const parser::UseDecl& use_decl) {
    if (use_decl.path.segments.empty()) {
        return;
    }

    // Build module path from segments
    std::string module_path;
    for (size_t i = 0; i < use_decl.path.segments.size(); ++i) {
        if (i > 0) {
            module_path += "::";
        }
        module_path += use_decl.path.segments[i];
    }

    // Helper: when a module path like "std::http::chunked" is not found on disk,
    // try resolving it through the parent module's re-exports.
    // If "std::http" has `pub use std::http::protocol::chunked::{...}`, then
    // "std::http::chunked" should resolve to "std::http::protocol::chunked".
    auto try_resolve_via_parent_reexports =
        [&](std::string& mod_path) -> std::optional<types::Module> {
        // Need at least 3 segments (e.g., std::http::chunked) to have a meaningful parent
        if (use_decl.path.segments.size() < 3) {
            return std::nullopt;
        }

        // Build parent path (all segments except the last)
        std::string parent_path;
        for (size_t i = 0; i < use_decl.path.segments.size() - 1; ++i) {
            if (i > 0)
                parent_path += "::";
            parent_path += use_decl.path.segments[i];
        }
        std::string leaf = use_decl.path.segments.back();
        std::string suffix = "::" + leaf;

        // Load and check parent module
        env_.load_native_module(parent_path, /*silent=*/true);
        auto parent_opt = env_.get_module(parent_path);
        if (!parent_opt.has_value()) {
            return std::nullopt;
        }

        // Search re-exports for a source_path ending with "::<leaf>"
        for (const auto& re : parent_opt->re_exports) {
            if (re.source_path.size() > suffix.size() &&
                re.source_path.compare(re.source_path.size() - suffix.size(), suffix.size(),
                                       suffix) == 0) {
                // Found a re-export whose source matches. Load the real module.
                env_.load_native_module(re.source_path, /*silent=*/true);
                auto resolved = env_.get_module(re.source_path);
                if (resolved.has_value()) {
                    mod_path = re.source_path; // update caller's module_path
                    return resolved;
                }
            }
        }

        // Also check submodules map of the parent
        auto sub_it = parent_opt->submodules.find(leaf);
        if (sub_it != parent_opt->submodules.end()) {
            env_.load_native_module(sub_it->second, /*silent=*/true);
            auto resolved = env_.get_module(sub_it->second);
            if (resolved.has_value()) {
                mod_path = sub_it->second;
                return resolved;
            }
        }

        return std::nullopt;
    };

    // Handle glob imports: use std::math::*
    if (use_decl.is_glob) {
        // Load the module
        env_.load_native_module(module_path, /*silent=*/true);
        auto module_opt = env_.get_module(module_path);

        // If not found, try resolving through parent re-exports
        if (!module_opt.has_value()) {
            module_opt = try_resolve_via_parent_reexports(module_path);
        }

        if (!module_opt.has_value()) {
            errors_.push_back(
                TypeError{"Module '" + module_path + "' not found", use_decl.span, {}, "T027"});
            return;
        }

        // Import all from module
        env_.import_all_from(module_path);
        return;
    }

    // Handle grouped imports: use std::math::{abs, sqrt, pow}
    if (use_decl.symbols.has_value()) {
        const auto& symbols = use_decl.symbols.value();

        // Load the module
        env_.load_native_module(module_path, /*silent=*/true);
        auto module_opt = env_.get_module(module_path);

        // If not found, try resolving through parent re-exports
        if (!module_opt.has_value()) {
            module_opt = try_resolve_via_parent_reexports(module_path);
        }

        if (!module_opt.has_value()) {
            errors_.push_back(
                TypeError{"Module '" + module_path + "' not found", use_decl.span, {}, "T027"});
            return;
        }

        // Load re-export source modules for imported symbols
        // This ensures that when we look up re-exported enums/constants, the source module is
        // loaded
        for (const auto& re_export : module_opt->re_exports) {
            bool needs_load = false;
            if (re_export.is_glob) {
                needs_load = true;
            } else {
                for (const auto& re_sym : re_export.symbols) {
                    for (const auto& imported_sym : symbols) {
                        if (re_sym == imported_sym) {
                            needs_load = true;
                            break;
                        }
                    }
                    if (needs_load) {
                        break;
                    }
                }
            }
            if (needs_load) {
                env_.load_native_module(re_export.source_path, /*silent=*/true);
            }
        }

        // Import each symbol individually
        for (const auto& symbol : symbols) {
            env_.import_symbol(module_path, symbol, std::nullopt);
        }
        return;
    }

    // Try first as complete module path
    env_.load_native_module(module_path, /*silent=*/true);
    auto module_opt = env_.get_module(module_path);

    // If module not found, last segment might be a symbol name
    if (!module_opt.has_value() && use_decl.path.segments.size() > 1) {
        // Try module path without last segment
        std::string base_module_path;
        for (size_t i = 0; i < use_decl.path.segments.size() - 1; ++i) {
            if (i > 0) {
                base_module_path += "::";
            }
            base_module_path += use_decl.path.segments[i];
        }

        env_.load_native_module(base_module_path, /*silent=*/true);
        module_opt = env_.get_module(base_module_path);

        // If the base module is also not found, try re-export resolution on it.
        // E.g., "std::http::server_response" -> resolve "server_response" through
        // "std::http" re-exports -> find "std::http::server::server_response"
        if (!module_opt.has_value() && use_decl.path.segments.size() > 2) {
            // Build grandparent path and leaf name for the base module
            std::string grandparent_path;
            for (size_t i = 0; i < use_decl.path.segments.size() - 2; ++i) {
                if (i > 0)
                    grandparent_path += "::";
                grandparent_path += use_decl.path.segments[i];
            }
            std::string base_leaf = use_decl.path.segments[use_decl.path.segments.size() - 2];
            std::string suffix = "::" + base_leaf;

            env_.load_native_module(grandparent_path, /*silent=*/true);
            auto grandparent_opt = env_.get_module(grandparent_path);
            if (grandparent_opt.has_value()) {
                // Search re-exports for source_path ending with "::<base_leaf>"
                for (const auto& re : grandparent_opt->re_exports) {
                    if (re.source_path.size() > suffix.size() &&
                        re.source_path.compare(re.source_path.size() - suffix.size(), suffix.size(),
                                               suffix) == 0) {
                        // Extract the module part of the re-export source path.
                        // E.g., re.source_path = "std::http::server::server_response"
                        // This is the resolved base module path.
                        std::string resolved_base = re.source_path;
                        env_.load_native_module(resolved_base, /*silent=*/true);
                        module_opt = env_.get_module(resolved_base);
                        if (module_opt.has_value()) {
                            base_module_path = resolved_base;
                            break;
                        }
                    }
                }

                // Also check submodules map
                if (!module_opt.has_value()) {
                    auto sub_it = grandparent_opt->submodules.find(base_leaf);
                    if (sub_it != grandparent_opt->submodules.end()) {
                        env_.load_native_module(sub_it->second, /*silent=*/true);
                        module_opt = env_.get_module(sub_it->second);
                        if (module_opt.has_value()) {
                            base_module_path = sub_it->second;
                        }
                    }
                }
            }
        }

        if (module_opt.has_value()) {
            // Last segment is a symbol name - import only that symbol
            std::string symbol_name = use_decl.path.segments.back();

            // Load re-export source modules for the imported symbol
            for (const auto& re_export : module_opt->re_exports) {
                bool needs_load = false;
                if (re_export.is_glob) {
                    needs_load = true;
                } else {
                    for (const auto& re_sym : re_export.symbols) {
                        if (re_sym == symbol_name) {
                            needs_load = true;
                            break;
                        }
                    }
                }
                if (needs_load) {
                    env_.load_native_module(re_export.source_path, /*silent=*/true);
                }
            }

            env_.import_symbol(base_module_path, symbol_name, use_decl.alias);
            return;
        }
    }

    // If still not found, try resolving through parent re-exports
    if (!module_opt.has_value()) {
        module_opt = try_resolve_via_parent_reexports(module_path);
    }

    if (!module_opt.has_value()) {
        errors_.push_back(
            TypeError{"Module '" + module_path + "' not found", use_decl.span, {}, "T027"});
        return;
    }

    // Import all from module
    env_.import_all_from(module_path);
}

void TypeChecker::check_func_decl(const parser::FuncDecl& func) {
    // Validate @extern decorator if present
    if (func.extern_abi.has_value()) {
        const std::string& abi = *func.extern_abi;
        if (abi != "c" && abi != "c++" && abi != "stdcall" && abi != "fastcall" &&
            abi != "thiscall") {
            error("Invalid @extern ABI '" + abi +
                      "'. "
                      "Valid options: \"c\", \"c++\", \"stdcall\", \"fastcall\", \"thiscall\"",
                  func.span, "T028");
        }

        // @extern functions must not have a body
        if (func.body.has_value()) {
            error("@extern function '" + func.name + "' must not have a body", func.span, "T028");
        }
    }

    // Validate @link paths for security (no directory traversal)
    for (const auto& lib : func.link_libs) {
        if (lib.find("..") != std::string::npos) {
            error("@link path '" + lib +
                      "' contains '..' which is not allowed for security reasons",
                  func.span, "T028");
        }
    }

    // Validate HTTP route decorators
    for (const auto& decorator : func.decorators) {
        if (decorator.name == "Get" || decorator.name == "Post" || decorator.name == "Put" ||
            decorator.name == "Delete" || decorator.name == "Patch" || decorator.name == "Head" ||
            decorator.name == "Options") {
            if (decorator.args.empty()) {
                error("@" + decorator.name + " requires a path argument, e.g. @" + decorator.name +
                          "(\"/path\")",
                      decorator.span, "T090");
            } else if (decorator.args.size() > 1) {
                error("@" + decorator.name + " takes exactly one path argument", decorator.span,
                      "T090");
            } else if (!decorator.args[0]->is<parser::LiteralExpr>() ||
                       decorator.args[0]->as<parser::LiteralExpr>().token.kind !=
                           lexer::TokenKind::StringLiteral) {
                error("@" + decorator.name +
                          " argument must be a string literal, e.g. \"/users/:id\"",
                      decorator.span, "T090");
            }
        } else if (decorator.name == "Controller") {
            error("@Controller is only valid on type declarations, not functions", decorator.span,
                  "T090");
        }
    }

    // Set up generic type parameters for proper resolution of T::AssociatedType in signatures
    // This is needed so resolve_type can recognize "T::Owned" as an associated type of param T
    std::unordered_map<std::string, TypePtr> saved_type_params = current_type_params_;
    for (const auto& param : func.generics) {
        auto type_var = std::make_shared<Type>();
        type_var->kind = NamedType{param.name, "", {}};
        current_type_params_[param.name] = type_var;
    }

    std::vector<TypePtr> params;
    for (const auto& p : func.params) {
        params.push_back(resolve_type(*p.type));
    }
    TypePtr ret = func.return_type ? resolve_type(**func.return_type) : make_unit();

    // Restore previous type params
    current_type_params_ = saved_type_params;

    // Process where clause constraints
    std::vector<WhereConstraint> where_constraints;
    if (func.where_clause) {
        for (const auto& [type_ptr, behaviors] : func.where_clause->constraints) {
            // Extract type parameter name from type
            std::string type_param_name;
            if (type_ptr->is<parser::NamedType>()) {
                const auto& named = type_ptr->as<parser::NamedType>();
                if (!named.path.segments.empty()) {
                    type_param_name = named.path.segments[0];
                }
            }

            // Extract behavior names and parameterized bounds from type pointers
            std::vector<std::string> behavior_names;
            std::vector<BoundConstraint> parameterized_bounds;
            for (const auto& behavior_type : behaviors) {
                if (behavior_type->is<parser::NamedType>()) {
                    const auto& named = behavior_type->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        std::string behavior_name = named.path.segments.back();

                        // Check if this has type arguments (parameterized bound)
                        if (named.generics && !named.generics->args.empty()) {
                            std::vector<TypePtr> type_args;
                            for (const auto& arg : named.generics->args) {
                                if (arg.is_type()) {
                                    type_args.push_back(resolve_type(*arg.as_type()));
                                }
                            }
                            parameterized_bounds.push_back(
                                BoundConstraint{behavior_name, std::move(type_args)});
                        } else {
                            behavior_names.push_back(behavior_name);
                        }
                    }
                }
            }

            if (!type_param_name.empty() &&
                (!behavior_names.empty() || !parameterized_bounds.empty())) {
                where_constraints.push_back(WhereConstraint{
                    type_param_name, std::move(behavior_names), std::move(parameterized_bounds)});
            }
        }
    }

    // Also process inline bounds from generic parameters (e.g., [T: Duplicate])
    // These need to be added to where_constraints so call-site checking can verify them
    for (const auto& param : func.generics) {
        if (!param.is_const && !param.is_lifetime && !param.bounds.empty()) {
            std::vector<std::string> behavior_names;
            std::vector<BoundConstraint> parameterized_bounds;

            for (const auto& bound : param.bounds) {
                if (bound->is<parser::NamedType>()) {
                    const auto& named = bound->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        std::string behavior_name = named.path.segments.back();

                        // Check if this has type arguments (parameterized bound)
                        if (named.generics && !named.generics->args.empty()) {
                            std::vector<TypePtr> type_args;
                            for (const auto& arg : named.generics->args) {
                                if (arg.is_type()) {
                                    type_args.push_back(resolve_type(*arg.as_type()));
                                }
                            }
                            parameterized_bounds.push_back(
                                BoundConstraint{behavior_name, std::move(type_args)});
                        } else {
                            behavior_names.push_back(behavior_name);
                        }
                    }
                }
            }

            if (!behavior_names.empty() || !parameterized_bounds.empty()) {
                where_constraints.push_back(WhereConstraint{param.name, std::move(behavior_names),
                                                            std::move(parameterized_bounds)});
            }
        }
    }

    // Extract type parameter names from generics (excluding const params and lifetimes)
    std::vector<std::string> func_type_params;
    std::unordered_map<std::string, std::string> lifetime_bounds;
    for (const auto& param : func.generics) {
        if (!param.is_const && !param.is_lifetime) {
            func_type_params.push_back(param.name);
            // Extract lifetime bound if present (e.g., T: life static)
            if (param.lifetime_bound.has_value()) {
                lifetime_bounds[param.name] = param.lifetime_bound.value();
            }
        }
    }

    // Extract const generic parameters
    std::vector<ConstGenericParam> func_const_params = extract_const_params(func.generics);

    // Extract FFI module namespace from @link
    std::optional<std::string> ffi_module = std::nullopt;
    if (!func.link_libs.empty()) {
        ffi_module = extract_ffi_module_name(func.link_libs[0]);
    }

    env_.define_func(FuncSig{.name = func.name,
                             .params = std::move(params),
                             .return_type = std::move(ret),
                             .type_params = std::move(func_type_params),
                             .is_async = func.is_async,
                             .span = func.span,
                             .stability = StabilityLevel::Unstable,
                             .deprecated_message = "",
                             .since_version = "",
                             .where_constraints = std::move(where_constraints),
                             .is_lowlevel = false,
                             .extern_abi = func.extern_abi,
                             .extern_name = func.extern_name,
                             .link_libs = func.link_libs,
                             .ffi_module = ffi_module,
                             .const_params = std::move(func_const_params),
                             .lifetime_bounds = std::move(lifetime_bounds)});
}

void TypeChecker::check_func_body(const parser::FuncDecl& func) {
    // Skip @extern functions - they have no body to check
    if (func.extern_abi.has_value()) {
        return;
    }

    TML_DEBUG_LN("[DEBUG] check_func_body called for function: " << func.name);
    env_.push_scope();

    // S014: Save and reset read_vars_ for this function scope
    auto saved_read_vars = std::move(read_vars_);
    read_vars_.clear();

    current_return_type_ = func.return_type ? resolve_type(**func.return_type) : make_unit();

    // Set async context flag for await expression checking
    bool was_async = in_async_func_;
    in_async_func_ = func.is_async;

    // Extract and store where constraints for method lookup on bounded generics
    // NOTE: We do NOT clear current_where_constraints_ here — impl-level constraints
    // (e.g., I: Iterator from impl[I: Iterator]) must remain visible inside methods.
    // We save and restore so function-level constraints are scoped properly.
    auto saved_where_constraints = current_where_constraints_;
    // Keep existing constraints (from impl block) and add function-level ones on top

    // First, process inline bounds from generic parameters (e.g., [T: Addable])
    for (const auto& generic : func.generics) {
        if (!generic.bounds.empty()) {
            std::vector<std::string> behavior_names;
            std::vector<BoundConstraint> parameterized_bounds;

            for (const auto& bound : generic.bounds) {
                if (bound->is<parser::NamedType>()) {
                    const auto& named = bound->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        std::string behavior_name = named.path.segments.back();
                        if (named.generics && !named.generics->args.empty()) {
                            std::vector<TypePtr> type_args;
                            for (const auto& arg : named.generics->args) {
                                if (arg.is_type()) {
                                    type_args.push_back(resolve_type(*arg.as_type()));
                                }
                            }
                            parameterized_bounds.push_back(
                                BoundConstraint{behavior_name, std::move(type_args)});
                        } else {
                            behavior_names.push_back(behavior_name);
                        }
                    }
                }
            }

            if (!behavior_names.empty() || !parameterized_bounds.empty()) {
                current_where_constraints_.push_back(WhereConstraint{
                    generic.name, std::move(behavior_names), std::move(parameterized_bounds)});
            }
        }
    }

    // Then, process explicit where clause constraints
    if (func.where_clause) {
        for (const auto& [type_ptr, behaviors] : func.where_clause->constraints) {
            std::string type_param_name;
            std::string assoc_type_name; // For I::Item, this is "Item"
            if (type_ptr->is<parser::NamedType>()) {
                const auto& named = type_ptr->as<parser::NamedType>();
                if (!named.path.segments.empty()) {
                    type_param_name = named.path.segments[0];
                    if (named.path.segments.size() > 1) {
                        assoc_type_name = named.path.segments.back();
                    }
                }
            }

            std::vector<std::string> behavior_names;
            std::vector<BoundConstraint> parameterized_bounds;
            for (const auto& behavior_type : behaviors) {
                if (behavior_type->is<parser::NamedType>()) {
                    const auto& named = behavior_type->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        std::string behavior_name = named.path.segments.back();
                        if (named.generics && !named.generics->args.empty()) {
                            std::vector<TypePtr> type_args;
                            for (const auto& arg : named.generics->args) {
                                if (arg.is_type()) {
                                    type_args.push_back(resolve_type(*arg.as_type()));
                                }
                            }
                            parameterized_bounds.push_back(
                                BoundConstraint{behavior_name, std::move(type_args)});
                        } else {
                            behavior_names.push_back(behavior_name);
                        }
                    }
                }
            }

            if (!type_param_name.empty() &&
                (!behavior_names.empty() || !parameterized_bounds.empty())) {
                // For associated type paths like I::Item, also register the
                // constraint under the last segment (e.g., "Item") so method
                // lookups on associated types can find the bound.
                if (!assoc_type_name.empty()) {
                    current_where_constraints_.push_back(
                        WhereConstraint{assoc_type_name, behavior_names, parameterized_bounds});
                }
                current_where_constraints_.push_back(WhereConstraint{
                    type_param_name, std::move(behavior_names), std::move(parameterized_bounds)});
            }
        }
    }

    // Add parameters to scope (supports all pattern types including tuple destructuring)
    for (const auto& p : func.params) {
        auto param_type = resolve_type(*p.type);
        bind_pattern(*p.pattern, param_type);
    }

    // Type-check contract clauses (pre/post conditions)
    for (const auto& contract : func.contracts) {
        if (!contract.is_pre && contract.result_binding.has_value()) {
            // Post-condition with result binding: bind the result variable
            env_.push_scope();
            auto ret_type = func.return_type ? resolve_type(**func.return_type) : make_unit();
            env_.current_scope()->define(*contract.result_binding, ret_type, false, contract.span);
            auto cond_type = check_expr(*contract.condition);
            if (!cond_type->is<PrimitiveType>() ||
                cond_type->as<PrimitiveType>().kind != PrimitiveKind::Bool) {
                error("Post-condition expression must return Bool, got " +
                          type_to_string(cond_type),
                      contract.span, "T090");
            }
            env_.pop_scope();
        } else {
            // Pre-condition or post-condition without result binding
            auto cond_type = check_expr(*contract.condition);
            if (!cond_type->is<PrimitiveType>() ||
                cond_type->as<PrimitiveType>().kind != PrimitiveKind::Bool) {
                error(std::string(contract.is_pre ? "Pre" : "Post") +
                          "-condition expression must return Bool, got " +
                          type_to_string(cond_type),
                      contract.span, "T090");
            }
        }
    }

    if (func.body) {
        auto body_type = check_block(*func.body);

        // Check if function with explicit non-Unit return type has return statement
        if (func.return_type) {
            auto return_type = resolve_type(**func.return_type);
            // Only require return if return type is not Unit
            if (!return_type->is<PrimitiveType>() ||
                return_type->as<PrimitiveType>().kind != PrimitiveKind::Unit) {

                TML_DEBUG_LN("[DEBUG] Checking function '" << func.name
                                                           << "' for return statement");
                bool has_ret = block_has_return(*func.body);
                TML_DEBUG_LN("[DEBUG] Has return: " << (has_ret ? "yes" : "no"));

                if (!has_ret) {
                    error("Function '" + func.name + "' with return type " +
                              type_to_string(return_type) +
                              " must have an explicit return statement",
                          func.span, "T029");
                }
            }
        }

        // Check return type compatibility (simplified for now)
        (void)body_type;
    }

    // S014: Check for unused parameters (local variables are checked in check_block)
    for (const auto& p : func.params) {
        if (p.pattern->is<parser::IdentPattern>()) {
            const auto& ident = p.pattern->as<parser::IdentPattern>();
            // Skip parameters starting with _ (intentionally unused)
            if (!ident.name.empty() && ident.name[0] != '_') {
                if (read_vars_.find(ident.name) == read_vars_.end()) {
                    warning("Unused variable '" + ident.name + "'", ident.span, "S014");
                }
            }
        }
    }

    // Restore read_vars_ from before this function
    read_vars_ = std::move(saved_read_vars);

    env_.pop_scope();
    current_return_type_ = nullptr;
    in_async_func_ = was_async;
    current_where_constraints_ = std::move(saved_where_constraints);
}

void TypeChecker::check_const_decl(const parser::ConstDecl& const_decl) {
    // Resolve the declared type
    TypePtr declared_type = resolve_type(*const_decl.type);

    // Type-check the initializer expression with expected type for literal inference
    TypePtr init_type = check_expr(*const_decl.value, declared_type);

    // Verify the types match (should always match now due to literal type inference)
    if (!types_equal(init_type, declared_type)) {
        error("Type mismatch in const initializer: expected " + type_to_string(declared_type) +
                  ", found " + type_to_string(init_type),
              const_decl.value->span, "T055");
        return;
    }

    // Try to evaluate the const value at compile time
    auto const_value = evaluate_const_expr(*const_decl.value, declared_type);
    if (const_value) {
        // Store the evaluated const value for use in const expressions
        const_values_[const_decl.name] = *const_value;
    }

    // Define the const in the global scope (as a variable that's immutable)
    env_.current_scope()->define(const_decl.name, declared_type, false, const_decl.span);
}

void TypeChecker::check_impl_decl(const parser::ImplDecl& impl) {
    // Get the type name from self_type
    // For generic impl blocks (impl[T] Container[T]), use just the base type name
    // so that method lookup works (Container::get, not Container[T]::get)
    auto resolved_self = resolve_type(*impl.self_type);
    std::string type_name;
    std::string specialized_type_name; // Discriminated key for specialized impls
    if (resolved_self->is<NamedType>()) {
        const auto& named_self = resolved_self->as<NamedType>();
        type_name = named_self.name;
        // For specialized impls like impl[T] Pin[Heap[T]], build a discriminated
        // key (e.g., "Pin[Heap]") to avoid collisions with other impls on the
        // same base type (e.g., impl[T] Pin[ref T] → "Pin[ref]").
        // The discriminator is the first non-bare-param type arg's wrapper name.
        if (!named_self.type_args.empty() && !impl.generics.empty()) {
            std::set<std::string> impl_params;
            for (const auto& gp : impl.generics) {
                impl_params.insert(gp.name);
            }
            for (const auto& ta : named_self.type_args) {
                if (ta->is<NamedType>()) {
                    const auto& n = ta->as<NamedType>();
                    if (impl_params.count(n.name) && n.type_args.empty()) {
                        continue; // Bare type param
                    }
                    specialized_type_name = type_name + "[" + n.name + "]";
                    break;
                } else if (ta->is<RefType>()) {
                    const auto& r = ta->as<RefType>();
                    specialized_type_name = type_name + "[" + (r.is_mut ? "mut_ref" : "ref") + "]";
                    break;
                }
            }
        }
    } else {
        type_name = type_to_string(resolved_self);
    }

    // Collect method names that impl provides
    std::set<std::string> impl_method_names;
    for (const auto& method : impl.methods) {
        impl_method_names.insert(method.name);
    }

    // Collect impl block's generic parameters (e.g., T in impl[T] Container[T])
    std::vector<std::string> impl_type_params;
    for (const auto& param : impl.generics) {
        impl_type_params.push_back(param.name);
    }

    // Set up current_self_type_ and current_associated_types_ before resolving method types
    // This allows types like This::Item to be resolved correctly
    current_self_type_ = resolved_self;
    current_associated_types_.clear();
    for (const auto& binding : impl.type_bindings) {
        current_associated_types_[binding.name] = resolve_type(*binding.type);
    }

    // Also record where-clause type equalities (e.g., `where I::Item = ref T`)
    // so method signature resolution can see them.
    if (impl.where_clause) {
        for (const auto& [lhs, rhs] : impl.where_clause->type_equalities) {
            if (!lhs || !rhs || !lhs->is<parser::NamedType>())
                continue;
            const auto& lhs_named = lhs->as<parser::NamedType>();
            if (lhs_named.path.segments.size() >= 2) {
                const std::string& param = lhs_named.path.segments[0];
                const std::string& assoc = lhs_named.path.segments.back();
                current_associated_types_[param + "::" + assoc] = resolve_type(*rhs);
            }
        }
    }

    // Register all constants in the impl block
    for (const auto& const_decl : impl.constants) {
        std::string qualified_name = type_name + "::" + const_decl.name;
        TypePtr const_type = resolve_type(*const_decl.type);
        // Register as a constant (immutable variable with qualified name)
        env_.current_scope()->define(qualified_name, const_type, false, const_decl.span);
    }

    // Extract impl self-type args for specialized impls like impl[T] Pin[ref T].
    // These patterns are needed to correctly map type params at call sites.
    std::vector<TypePtr> impl_self_type_args;
    if (resolved_self->is<NamedType>()) {
        impl_self_type_args = resolved_self->as<NamedType>().type_args;
    }

    // Register all methods in the impl block
    for (const auto& method : impl.methods) {
        std::string qualified_name = type_name + "::" + method.name;
        std::vector<TypePtr> params;
        params.reserve(method.params.size());
        for (const auto& p : method.params) {
            params.push_back(resolve_type(*p.type));
        }
        TypePtr ret = method.return_type ? resolve_type(**method.return_type) : make_unit();

        // Collect type parameters: impl-level + method-level
        // This supports generic methods on non-generic types (e.g., func identity[T](x: T) -> T)
        std::vector<std::string> method_type_params = impl_type_params;
        for (const auto& param : method.generics) {
            method_type_params.push_back(param.name);
        }

        env_.define_func(FuncSig{.name = qualified_name,
                                 .params = params,
                                 .return_type = ret,
                                 .type_params = method_type_params,
                                 .is_async = method.is_async,
                                 .span = method.span,
                                 .impl_self_type_args = impl_self_type_args});

        // For specialized impls (e.g., impl[T] Pin[Heap[T]]), also register
        // under the full type string key to avoid collision with other impls
        // on the same base type. The call site will try the specialized key first.
        if (!specialized_type_name.empty()) {
            std::string spec_qualified = specialized_type_name + "::" + method.name;
            env_.define_func(FuncSig{.name = spec_qualified,
                                     .params = std::move(params),
                                     .return_type = std::move(ret),
                                     .type_params = method_type_params,
                                     .is_async = method.is_async,
                                     .span = method.span,
                                     .impl_self_type_args = impl_self_type_args});
        }
    }

    // Register default implementations from the behavior
    // Extract behavior name from trait_type (TypePtr -> NamedType -> path.segments.back())
    std::string behavior_name;
    if (impl.trait_type && impl.trait_type->is<parser::NamedType>()) {
        const auto& named = impl.trait_type->as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            behavior_name = named.path.segments.back();
        }
    }
    if (!behavior_name.empty()) {

        // Register that this type implements this behavior (for where clause checking)
        env_.register_impl(type_name, behavior_name);

        auto behavior_def = env_.lookup_behavior(behavior_name);

        // If not found, try loading the behavior's module from GlobalModuleCache.
        // This handles behaviors like Iterator that are defined in library modules
        // not explicitly imported by user code.
        // If behavior not found, try loading its definition from binary
        // cache on-demand. This handles behaviors like Iterator that are
        // defined in library modules not explicitly imported by user code.
        // We load ONLY the behavior definition, not the full module, to
        // avoid pulling in all library code during codegen.
        if (!behavior_def) {
            static const std::unordered_map<std::string, std::string> behavior_modules = {
                {"Iterator", "core::iter::traits::iterator"},
                {"IntoIterator", "core::iter::traits::into_iterator"},
                {"FromIterator", "core::iter::traits::from_iterator"},
                {"Display", "core::fmt::traits"},
                {"Debug", "core::fmt::traits"},
                {"Duplicate", "core::clone"},
                {"Hash", "core::hash"},
                {"Default", "core::default"},
                {"Error", "core::error"},
                {"From", "core::convert"},
                {"Into", "core::convert"},
                {"TryFrom", "core::convert"},
                {"TryInto", "core::convert"},
                {"PartialEq", "core::cmp"},
                {"Eq", "core::cmp"},
                {"PartialOrd", "core::cmp"},
                {"Ord", "core::cmp"},
            };
            auto mod_it = behavior_modules.find(behavior_name);
            if (mod_it != behavior_modules.end()) {
                auto cached_mod = types::load_module_from_cache(mod_it->second);
                if (cached_mod) {
                    auto bit = cached_mod->behaviors.find(behavior_name);
                    if (bit != cached_mod->behaviors.end()) {
                        // Register behavior directly in the env
                        env_.define_behavior(bit->second);
                        behavior_def = env_.lookup_behavior(behavior_name);
                    }
                }
            }
        }

        if (behavior_def) {
            // Build substitution map for This and associated types
            // e.g., {"This": Counter3, "This::Item": I32, "Item": I32}
            auto self_type_ptr = std::make_shared<types::Type>();
            self_type_ptr->kind = NamedType{type_name, "", {}};
            std::unordered_map<std::string, TypePtr> assoc_subs;
            assoc_subs["This"] = self_type_ptr;
            assoc_subs["Self"] = self_type_ptr;
            for (const auto& binding : impl.type_bindings) {
                if (binding.type) {
                    TypePtr resolved = resolve_type(*binding.type);
                    assoc_subs["This::" + binding.name] = resolved;
                    assoc_subs[binding.name] = resolved;
                }
            }

            for (const auto& behavior_method : behavior_def->methods) {
                // Skip if impl provides this method
                if (impl_method_names.count(behavior_method.name) > 0)
                    continue;

                // Skip if this method doesn't have a default implementation
                if (behavior_def->methods_with_defaults.count(behavior_method.name) == 0)
                    continue;

                // Register default implementation
                std::string qualified_name = type_name + "::" + behavior_method.name;

                // Substitute 'This', 'This::Item', etc. in parameter types
                std::vector<TypePtr> params;
                for (const auto& p : behavior_method.params) {
                    params.push_back(substitute_type(p, assoc_subs));
                }

                // Substitute 'This', 'This::Item', etc. in return type
                TypePtr ret = substitute_type(behavior_method.return_type, assoc_subs);

                env_.define_func(FuncSig{.name = qualified_name,
                                         .params = std::move(params),
                                         .return_type = ret,
                                         .type_params = {},
                                         .is_async = behavior_method.is_async,
                                         .span = behavior_method.span});
            }
        }
    }
}

void TypeChecker::check_impl_body(const parser::ImplDecl& impl) {
    // Set current_self_type_ so 'This' resolves correctly
    current_self_type_ = resolve_type(*impl.self_type);

    // Collect associated type bindings (e.g., type Owned = I32)
    current_associated_types_.clear();
    for (const auto& binding : impl.type_bindings) {
        current_associated_types_[binding.name] = resolve_type(*binding.type);
    }

    // Collect generic type parameter names (e.g., T in impl[T] ...)
    current_type_params_.clear();
    current_const_params_.clear();
    for (const auto& param : impl.generics) {
        if (param.is_const) {
            // Const generic param (e.g., const N: I64) — register so N can be used as a value
            TypePtr value_type = param.const_type ? resolve_type(**param.const_type) : make_i64();
            current_const_params_[param.name] = ConstGenericParam{param.name, value_type};
        }
        // For now, map generic params to a placeholder type
        auto type_var = std::make_shared<Type>();
        type_var->kind = NamedType{param.name, "", {}};
        current_type_params_[param.name] = type_var;
    }

    // Extract where constraints from impl block's generic bounds
    // so method bodies can resolve calls like I.next() via Iterator bound
    current_where_constraints_.clear();
    for (const auto& generic : impl.generics) {
        if (!generic.bounds.empty()) {
            std::vector<std::string> behavior_names;
            std::vector<BoundConstraint> parameterized_bounds;

            for (const auto& bound : generic.bounds) {
                if (bound->is<parser::NamedType>()) {
                    const auto& named = bound->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        std::string behavior_name = named.path.segments.back();
                        if (named.generics && !named.generics->args.empty()) {
                            std::vector<TypePtr> type_args;
                            for (const auto& arg : named.generics->args) {
                                if (arg.is_type()) {
                                    type_args.push_back(resolve_type(*arg.as_type()));
                                }
                            }
                            parameterized_bounds.push_back(
                                BoundConstraint{behavior_name, std::move(type_args)});
                        } else {
                            behavior_names.push_back(behavior_name);
                        }
                    }
                }
            }

            if (!behavior_names.empty() || !parameterized_bounds.empty()) {
                current_where_constraints_.push_back(WhereConstraint{
                    generic.name, std::move(behavior_names), std::move(parameterized_bounds)});
            }
        }
    }

    // Also process explicit where clause on impl block
    if (impl.where_clause) {
        for (const auto& [type_ptr, behaviors] : impl.where_clause->constraints) {
            std::string type_param_name;
            std::string assoc_type_name;
            if (type_ptr->is<parser::NamedType>()) {
                const auto& named = type_ptr->as<parser::NamedType>();
                if (!named.path.segments.empty()) {
                    type_param_name = named.path.segments[0];
                    if (named.path.segments.size() > 1) {
                        assoc_type_name = named.path.segments.back();
                    }
                }
            }

            std::vector<std::string> behavior_names;
            std::vector<BoundConstraint> parameterized_bounds;
            for (const auto& behavior_type : behaviors) {
                if (behavior_type->is<parser::NamedType>()) {
                    const auto& named = behavior_type->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        std::string behavior_name = named.path.segments.back();
                        if (named.generics && !named.generics->args.empty()) {
                            std::vector<TypePtr> type_args;
                            for (const auto& arg : named.generics->args) {
                                if (arg.is_type()) {
                                    type_args.push_back(resolve_type(*arg.as_type()));
                                }
                            }
                            parameterized_bounds.push_back(
                                BoundConstraint{behavior_name, std::move(type_args)});
                        } else {
                            behavior_names.push_back(behavior_name);
                        }
                    }
                }
            }

            if (!type_param_name.empty() &&
                (!behavior_names.empty() || !parameterized_bounds.empty())) {
                if (!assoc_type_name.empty()) {
                    current_where_constraints_.push_back(
                        WhereConstraint{assoc_type_name, behavior_names, parameterized_bounds});
                }
                current_where_constraints_.push_back(WhereConstraint{
                    type_param_name, std::move(behavior_names), std::move(parameterized_bounds)});
            }
        }

        // Process where clause type equalities (e.g., `where I::Item = ref T`).
        // For each equality, record the mapping so that references to the associated
        // type path (e.g., I::Item) can resolve to the RHS type within the impl body.
        for (const auto& [lhs, rhs] : impl.where_clause->type_equalities) {
            if (!lhs || !rhs || !lhs->is<parser::NamedType>())
                continue;
            const auto& lhs_named = lhs->as<parser::NamedType>();
            if (lhs_named.path.segments.size() >= 2) {
                // Two-segment path like I::Item — store as associated type binding
                // so resolve_type_path can find it when checking method bodies.
                const std::string& param = lhs_named.path.segments[0];
                const std::string& assoc = lhs_named.path.segments.back();
                std::string key = param + "::" + assoc;
                TypePtr resolved_rhs = resolve_type(*rhs);
                current_associated_types_[key] = resolved_rhs;
            }
        }
    }

    // Check constant initializers
    for (const auto& const_decl : impl.constants) {
        TypePtr declared_type = resolve_type(*const_decl.type);
        TypePtr init_type = check_expr(*const_decl.value);

        if (!types_equal(init_type, declared_type)) {
            error("Type mismatch in const initializer: expected " + type_to_string(declared_type) +
                      ", found " + type_to_string(init_type),
                  const_decl.value->span, "T055");
        }
    }

    for (const auto& method : impl.methods) {
        check_func_body(method);
    }

    current_self_type_ = nullptr;
    current_associated_types_.clear();
    current_type_params_.clear();
    current_where_constraints_.clear();
}

// Note: OOP type checking (interface/class registration, validation,
// class body checking, visibility) moved to core_oop.cpp

} // namespace tml::types
