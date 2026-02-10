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
#include "types/builtins_cache.hpp"
#include "types/checker.hpp"

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
    if (!type)
        return 8; // Default pointer size

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
                    return 0;
                case PrimitiveKind::Never:
                    return 0;
                case PrimitiveKind::Str:
                    return 24; // Str is typically ptr + len + capacity
                }
                return 8; // Default for any unknown primitives
            } else if constexpr (std::is_same_v<T, PtrType> || std::is_same_v<T, RefType>) {
                return 8; // Pointer size
            } else if constexpr (std::is_same_v<T, ClassType>) {
                return 8; // Class instances are stored by reference (pointer)
            } else if constexpr (std::is_same_v<T, NamedType>) {
                return 8; // Conservative estimate - actual size computed during codegen
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
    if (RESERVED_BEHAVIOR_NAMES.count(decl.name) > 0) {
        error("Cannot redefine builtin behavior '" + decl.name +
                  "'. Use the builtin behavior instead of defining your own.",
              decl.span, "T038");
        return;
    }

    std::vector<FuncSig> methods;
    std::set<std::string> methods_with_defaults;

    for (const auto& method : decl.methods) {
        std::vector<TypePtr> params;
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
    if (RESERVED_TYPE_NAMES.count(decl.name) > 0) {
        error("Cannot redefine builtin type '" + decl.name +
                  "'. Use the builtin type instead of defining your own.",
              decl.span, "T038");
        return;
    }

    std::vector<std::string> generic_params;
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
        if (i > 0)
            module_path += "::";
        module_path += use_decl.path.segments[i];
    }

    // Handle glob imports: use std::math::*
    if (use_decl.is_glob) {
        // Load the module
        env_.load_native_module(module_path, /*silent=*/true);
        auto module_opt = env_.get_module(module_path);

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
                    if (needs_load)
                        break;
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
            if (i > 0)
                base_module_path += "::";
            base_module_path += use_decl.path.segments[i];
        }

        env_.load_native_module(base_module_path, /*silent=*/true);
        module_opt = env_.get_module(base_module_path);

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
    current_return_type_ = func.return_type ? resolve_type(**func.return_type) : make_unit();

    // Set async context flag for await expression checking
    bool was_async = in_async_func_;
    in_async_func_ = func.is_async;

    // Extract and store where constraints for method lookup on bounded generics
    auto saved_where_constraints = current_where_constraints_;
    current_where_constraints_.clear();

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
            if (type_ptr->is<parser::NamedType>()) {
                const auto& named = type_ptr->as<parser::NamedType>();
                if (!named.path.segments.empty()) {
                    type_param_name = named.path.segments[0];
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
              const_decl.value->span, "T001");
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
    if (resolved_self->is<NamedType>()) {
        type_name = resolved_self->as<NamedType>().name;
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

    // Register all constants in the impl block
    for (const auto& const_decl : impl.constants) {
        std::string qualified_name = type_name + "::" + const_decl.name;
        TypePtr const_type = resolve_type(*const_decl.type);
        // Register as a constant (immutable variable with qualified name)
        env_.current_scope()->define(qualified_name, const_type, false, const_decl.span);
    }

    // Register all methods in the impl block
    for (const auto& method : impl.methods) {
        std::string qualified_name = type_name + "::" + method.name;
        std::vector<TypePtr> params;
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
                                 .params = std::move(params),
                                 .return_type = std::move(ret),
                                 .type_params = method_type_params,
                                 .is_async = method.is_async,
                                 .span = method.span});
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
        if (behavior_def) {
            for (const auto& behavior_method : behavior_def->methods) {
                // Skip if impl provides this method
                if (impl_method_names.count(behavior_method.name) > 0)
                    continue;

                // Skip if this method doesn't have a default implementation
                if (behavior_def->methods_with_defaults.count(behavior_method.name) == 0)
                    continue;

                // Register default implementation
                std::string qualified_name = type_name + "::" + behavior_method.name;

                // Substitute 'This' type with the concrete type
                std::vector<TypePtr> params;
                for (const auto& p : behavior_method.params) {
                    TypePtr param_type = p;
                    // Handle 'This' type substitution
                    if (param_type->is<NamedType>() && param_type->as<NamedType>().name == "This") {
                        auto self_type = std::make_shared<types::Type>();
                        self_type->kind = NamedType{type_name, "", {}};
                        params.push_back(self_type);
                    } else {
                        params.push_back(param_type);
                    }
                }

                TypePtr ret = behavior_method.return_type;
                // Handle 'This' return type substitution
                if (ret->is<NamedType>() && ret->as<NamedType>().name == "This") {
                    auto self_type = std::make_shared<types::Type>();
                    self_type->kind = NamedType{type_name, "", {}};
                    ret = self_type;
                }

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
    for (const auto& param : impl.generics) {
        // For now, map generic params to a placeholder type
        auto type_var = std::make_shared<Type>();
        type_var->kind = NamedType{param.name, "", {}};
        current_type_params_[param.name] = type_var;
    }

    // Check constant initializers
    for (const auto& const_decl : impl.constants) {
        TypePtr declared_type = resolve_type(*const_decl.type);
        TypePtr init_type = check_expr(*const_decl.value);

        if (!types_equal(init_type, declared_type)) {
            error("Type mismatch in const initializer: expected " + type_to_string(declared_type) +
                      ", found " + type_to_string(init_type),
                  const_decl.value->span, "T001");
        }
    }

    for (const auto& method : impl.methods) {
        check_func_body(method);
    }

    current_self_type_ = nullptr;
    current_associated_types_.clear();
    current_type_params_.clear();
}

// ============================================================================
// OOP Type Checking - Interface Registration
// ============================================================================

void TypeChecker::register_interface_decl(const parser::InterfaceDecl& decl) {
    // Check if the type name is reserved
    if (RESERVED_TYPE_NAMES.count(decl.name) > 0) {
        error("Cannot redefine builtin type '" + decl.name + "'", decl.span, "T038");
        return;
    }

    // Build InterfaceDef
    InterfaceDef def;
    def.name = decl.name;
    def.span = decl.span;

    // Collect type parameters
    for (const auto& param : decl.generics) {
        if (!param.is_const) {
            def.type_params.push_back(param.name);
        } else if (param.const_type.has_value()) {
            def.const_params.push_back(
                ConstGenericParam{param.name, resolve_type(*param.const_type.value())});
        }
    }

    // Collect extended interfaces
    for (const auto& ext : decl.extends) {
        if (!ext.segments.empty()) {
            def.extends.push_back(ext.segments.back());
        }
    }

    // Collect methods
    for (const auto& method : decl.methods) {
        InterfaceMethodDef method_def;
        method_def.is_static = method.is_static;
        method_def.has_default = method.default_body.has_value();

        // Build signature
        FuncSig sig;
        sig.name = method.name;
        sig.is_async = false;
        sig.span = method.span;

        for (const auto& param : method.params) {
            // Skip 'this' parameter - it's the implicit receiver
            if (param.pattern && param.pattern->is<parser::IdentPattern>() &&
                param.pattern->as<parser::IdentPattern>().name == "this") {
                continue;
            }
            if (param.type) {
                sig.params.push_back(resolve_type(*param.type));
            }
        }

        if (method.return_type.has_value()) {
            sig.return_type = resolve_type(*method.return_type.value());
        } else {
            sig.return_type = make_unit();
        }

        method_def.sig = sig;
        def.methods.push_back(method_def);
    }

    env_.define_interface(std::move(def));
}

// ============================================================================
// OOP Type Checking - Class Registration
// ============================================================================

void TypeChecker::register_class_decl(const parser::ClassDecl& decl) {
    // Check if the type name is reserved
    if (RESERVED_TYPE_NAMES.count(decl.name) > 0) {
        error("Cannot redefine builtin type '" + decl.name + "'", decl.span, "T038");
        return;
    }

    // Build ClassDef
    ClassDef def;
    def.name = decl.name;
    def.is_abstract = decl.is_abstract;
    def.is_sealed = decl.is_sealed;
    def.span = decl.span;

    // Check for @value and @pool decorators
    def.is_value = false;
    def.is_pooled = false;
    for (const auto& deco : decl.decorators) {
        if (deco.name == "value") {
            def.is_value = true;
            // @value implies sealed
            def.is_sealed = true;
        } else if (deco.name == "pool") {
            def.is_pooled = true;
        }
    }

    // Collect type parameters
    for (const auto& param : decl.generics) {
        if (!param.is_const) {
            def.type_params.push_back(param.name);
        } else if (param.const_type.has_value()) {
            def.const_params.push_back(
                ConstGenericParam{param.name, resolve_type(*param.const_type.value())});
        }
    }

    // Collect base class
    if (decl.extends.has_value()) {
        const auto& base = decl.extends.value();
        if (!base.segments.empty()) {
            def.base_class = base.segments.back();
        }
    }

    // Collect implemented interfaces (supports generic interfaces like IEquatable[T])
    for (const auto& iface_type : decl.implements) {
        if (auto* named = std::get_if<parser::NamedType>(&iface_type->kind)) {
            if (!named->path.segments.empty()) {
                def.interfaces.push_back(named->path.segments.back());
            }
        }
    }

    // Collect fields
    for (const auto& field : decl.fields) {
        ClassFieldDef field_def;
        field_def.name = field.name;
        field_def.type = resolve_type(*field.type);
        field_def.is_static = field.is_static;
        field_def.vis = static_cast<MemberVisibility>(field.vis);
        def.fields.push_back(field_def);
    }

    // Collect methods
    for (const auto& method : decl.methods) {
        ClassMethodDef method_def;
        method_def.is_static = method.is_static;
        method_def.is_virtual = method.is_virtual;
        method_def.is_override = method.is_override;
        method_def.is_abstract = method.is_abstract;
        method_def.is_final = method.is_final;
        method_def.vis = static_cast<MemberVisibility>(method.vis);
        method_def.vtable_index = 0; // Will be assigned during codegen

        // Build signature
        FuncSig sig;
        sig.name = method.name;
        sig.is_async = false;
        sig.span = method.span;

        // Collect method's type parameters (for generic methods)
        for (const auto& gp : method.generics) {
            if (!gp.is_const) {
                sig.type_params.push_back(gp.name);
            }
        }

        // Collect parameters (skip 'this' to match module loading behavior)
        for (const auto& param : method.params) {
            // Skip 'this' parameter
            if (param.pattern && param.pattern->is<parser::IdentPattern>() &&
                param.pattern->as<parser::IdentPattern>().name == "this") {
                continue;
            }
            if (param.type) {
                sig.params.push_back(resolve_type(*param.type));
            }
        }

        if (method.return_type.has_value()) {
            // Check if return type is the class being registered (self-referential)
            // Since the class isn't registered yet, we need to handle this case specially
            const auto& ret_type = *method.return_type.value();
            if (auto* named = std::get_if<parser::NamedType>(&ret_type.kind)) {
                if (!named->path.segments.empty() && named->path.segments.back() == decl.name) {
                    // Return type is the class itself - create ClassType directly
                    auto class_type = std::make_shared<Type>();
                    class_type->kind = ClassType{decl.name, "", {}};
                    sig.return_type = class_type;
                } else {
                    sig.return_type = resolve_type(ret_type);
                }
            } else {
                sig.return_type = resolve_type(ret_type);
            }
        } else {
            sig.return_type = make_unit();
        }

        method_def.sig = sig;
        def.methods.push_back(method_def);
    }

    // Collect properties
    for (const auto& prop : decl.properties) {
        PropertyDef prop_def;
        prop_def.name = prop.name;
        prop_def.type = resolve_type(*prop.type);
        prop_def.is_static = prop.is_static;
        prop_def.vis = static_cast<MemberVisibility>(prop.vis);
        prop_def.has_getter = prop.has_getter;
        prop_def.has_setter = prop.has_setter;
        def.properties.push_back(prop_def);
    }

    // Collect constructors
    for (const auto& ctor : decl.constructors) {
        ConstructorDef ctor_def;
        ctor_def.vis = static_cast<MemberVisibility>(ctor.vis);
        ctor_def.calls_base = ctor.base_args.has_value();

        for (const auto& param : ctor.params) {
            if (param.type) {
                ctor_def.params.push_back(resolve_type(*param.type));
            }
        }

        def.constructors.push_back(ctor_def);
    }

    // ========================================================================
    // Compute stack allocation eligibility metadata
    // ========================================================================

    // Calculate inheritance depth
    def.inheritance_depth = 0;
    if (def.base_class.has_value()) {
        std::string current_base = *def.base_class;
        while (!current_base.empty()) {
            def.inheritance_depth++;
            auto base_def = env_.lookup_class(current_base);
            if (base_def.has_value() && base_def->base_class.has_value()) {
                current_base = *base_def->base_class;
            } else {
                break;
            }
        }
    }

    // Calculate estimated size:
    // - vtable pointer (8 bytes) for non-@value classes
    // - inherited fields (from base class)
    // - own fields
    def.estimated_size = 0;

    // vtable pointer (8 bytes) for non-@value classes
    if (!def.is_value) {
        def.estimated_size += 8;
    }

    // Add inherited field sizes
    if (def.base_class.has_value()) {
        auto base_def = env_.lookup_class(*def.base_class);
        if (base_def.has_value()) {
            // Include base class size (minus vtable since we already counted it)
            def.estimated_size += base_def->estimated_size;
            if (!base_def->is_value) {
                // Don't double-count vtable pointer
                def.estimated_size -= 8;
            }
        }
    }

    // Add own field sizes
    for (const auto& field : def.fields) {
        if (!field.is_static) {
            def.estimated_size += estimate_type_size(field.type);
        }
    }

    // Determine stack allocation eligibility:
    // A class is stack-allocatable if:
    // 1. It's a @value class (no vtable, no virtual methods), OR
    // 2. It's sealed (no subclasses) and small enough
    // AND:
    // 3. It's not abstract
    // 4. Its estimated size is within the threshold
    // 5. It doesn't contain unsized types
    def.stack_allocatable = false;

    if (!def.is_abstract && def.estimated_size <= MAX_STACK_CLASS_SIZE) {
        if (def.is_value) {
            // @value classes are always eligible if small enough
            def.stack_allocatable = true;
        } else if (def.is_sealed) {
            // Sealed classes with known type can be stack-allocated
            // (escape analysis determines actual placement at call sites)
            def.stack_allocatable = true;
        }
    }

    env_.define_class(std::move(def));
}

// ============================================================================
// OOP Type Checking - Interface Validation (Pass 2)
// ============================================================================

void TypeChecker::check_interface_decl(const parser::InterfaceDecl& iface) {
    // Verify extended interfaces exist
    for (const auto& ext : iface.extends) {
        if (!ext.segments.empty()) {
            const std::string& name = ext.segments.back();
            if (!env_.lookup_interface(name).has_value()) {
                error("Interface '" + name + "' not found", iface.span, "T047");
            }
        }
    }
}

// ============================================================================
// OOP Type Checking - Class Validation (Pass 2)
// ============================================================================

void TypeChecker::check_class_decl(const parser::ClassDecl& cls) {
    // Run all validation checks
    validate_inheritance(cls);
    validate_interface_impl(cls);

    // Check override methods
    for (const auto& method : cls.methods) {
        if (method.is_override) {
            validate_override(cls, method);
        }
    }

    // Check abstract methods are implemented (non-abstract classes only)
    if (!cls.is_abstract && cls.extends.has_value()) {
        validate_abstract_methods(cls);
    }

    // Validate @value class constraints
    validate_value_class(cls);

    // Validate @pool class constraints
    validate_pool_class(cls);
}

void TypeChecker::validate_value_class(const parser::ClassDecl& cls) {
    // Check if class has @value decorator
    bool is_value = false;
    for (const auto& deco : cls.decorators) {
        if (deco.name == "value") {
            is_value = true;
            break;
        }
    }

    if (!is_value) {
        return; // Not a value class, no validation needed
    }

    // @value classes cannot be abstract
    if (cls.is_abstract) {
        error("@value class '" + cls.name + "' cannot be abstract", cls.span, "T043");
    }

    // @value classes cannot have virtual methods
    for (const auto& method : cls.methods) {
        if (method.is_virtual || method.is_abstract) {
            error("@value class '" + cls.name + "' cannot have virtual method '" + method.name +
                      "'. Value classes use direct dispatch only.",
                  method.span, "T042");
        }
    }

    // @value classes cannot extend non-value classes
    if (cls.extends.has_value()) {
        const auto& base_path = cls.extends.value();
        if (!base_path.segments.empty()) {
            const auto& base_name = base_path.segments.back();
            auto base_def = env_.lookup_class(base_name);
            if (base_def.has_value() && !base_def->is_value) {
                error("@value class '" + cls.name + "' cannot extend non-value class '" +
                          base_name + "'. Base class must also be @value.",
                      cls.span, "T041");
            }
        }
    }

    // Note: @value classes CAN implement interfaces, so no check needed there
}

void TypeChecker::validate_pool_class(const parser::ClassDecl& cls) {
    // Check if class has @pool decorator
    bool is_pooled = false;
    bool is_value = false;
    for (const auto& deco : cls.decorators) {
        if (deco.name == "pool") {
            is_pooled = true;
        } else if (deco.name == "value") {
            is_value = true;
        }
    }

    if (!is_pooled) {
        return; // Not a pooled class, no validation needed
    }

    // @pool and @value are mutually exclusive
    if (is_value) {
        error("@pool and @value are mutually exclusive on class '" + cls.name +
                  "'. Use one or the other.",
              cls.span, "T044");
    }

    // @pool classes cannot be abstract
    if (cls.is_abstract) {
        error("@pool class '" + cls.name + "' cannot be abstract", cls.span, "T040");
    }

    // @pool classes should not be sealed (pooling benefits from inheritance)
    // But we don't enforce this - just a note that sealed pools are unusual
}

void TypeChecker::validate_abstract_methods(const parser::ClassDecl& cls) {
    // Collect all abstract methods from inheritance chain
    std::vector<std::pair<std::string, std::string>>
        abstract_methods; // (method_name, declaring_class)
    std::string current = cls.extends.value().segments.back();

    while (!current.empty()) {
        auto parent = env_.lookup_class(current);
        if (!parent.has_value())
            break;

        for (const auto& method : parent->methods) {
            if (method.is_abstract) {
                abstract_methods.emplace_back(method.sig.name, current);
            }
        }

        if (parent->base_class.has_value()) {
            current = parent->base_class.value();
        } else {
            break;
        }
    }

    // Check each abstract method has an implementation
    for (const auto& [method_name, declaring_class] : abstract_methods) {
        bool implemented = false;

        // Check if this class implements it
        for (const auto& method : cls.methods) {
            if (method.name == method_name && (method.is_override || !method.is_abstract)) {
                implemented = true;
                break;
            }
        }

        // Check if any intermediate class implements it
        if (!implemented) {
            current = cls.extends.value().segments.back();
            while (current != declaring_class && !current.empty()) {
                auto parent = env_.lookup_class(current);
                if (!parent.has_value())
                    break;

                for (const auto& method : parent->methods) {
                    if (method.sig.name == method_name && method.is_override) {
                        implemented = true;
                        break;
                    }
                }

                if (implemented)
                    break;

                if (parent->base_class.has_value()) {
                    current = parent->base_class.value();
                } else {
                    break;
                }
            }
        }

        if (!implemented) {
            error("Non-abstract class '" + cls.name + "' does not implement abstract method '" +
                      method_name + "' from '" + declaring_class + "'",
                  cls.span, "T045");
        }
    }
}

void TypeChecker::validate_inheritance(const parser::ClassDecl& cls) {
    if (!cls.extends.has_value()) {
        return; // No inheritance to validate
    }

    const auto& base_path = cls.extends.value();
    if (base_path.segments.empty()) {
        return;
    }

    const std::string& base_name = base_path.segments.back();

    // Check base class exists
    auto base_def = env_.lookup_class(base_name);
    if (!base_def.has_value()) {
        error("Base class '" + base_name + "' not found", cls.span, "T046");
        return;
    }

    // Check sealed class not extended (unless both are @value classes)
    if (base_def->is_sealed) {
        // @value classes can extend other @value classes
        bool this_is_value = false;
        for (const auto& deco : cls.decorators) {
            if (deco.name == "value") {
                this_is_value = true;
                break;
            }
        }
        // Only allow if both classes are @value
        if (!this_is_value || !base_def->is_value) {
            error("Cannot extend sealed class '" + base_name + "'", cls.span, "T041");
            return;
        }
    }

    // Check for circular inheritance
    std::unordered_set<std::string> visited;
    std::string current = base_name;

    while (!current.empty()) {
        if (visited.count(current) > 0) {
            error("Circular inheritance detected involving class '" + cls.name + "'", cls.span,
                  "T039");
            return;
        }
        visited.insert(current);

        auto parent = env_.lookup_class(current);
        if (!parent.has_value() || !parent->base_class.has_value()) {
            break;
        }
        current = parent->base_class.value();
    }

    // Check if current class would create a cycle
    if (visited.count(cls.name) > 0) {
        error("Circular inheritance: class '" + cls.name + "' cannot extend itself", cls.span,
              "T039");
    }
}

void TypeChecker::validate_override(const parser::ClassDecl& cls,
                                    const parser::ClassMethod& method) {
    if (!cls.extends.has_value()) {
        error("Cannot override method '" + method.name + "': class has no base class", method.span,
              "T006");
        return;
    }

    const std::string& base_name = cls.extends.value().segments.back();
    auto base_def = env_.lookup_class(base_name);

    if (!base_def.has_value()) {
        return; // Error already reported
    }

    // Search for method in base class hierarchy
    bool found = false;
    bool is_virtual = false;
    std::string current = base_name;

    while (!current.empty()) {
        auto parent = env_.lookup_class(current);
        if (!parent.has_value()) {
            break;
        }

        for (const auto& parent_method : parent->methods) {
            if (parent_method.sig.name == method.name) {
                found = true;
                is_virtual = parent_method.is_virtual || parent_method.is_abstract;

                // Verify method is virtual/abstract
                if (!is_virtual && !parent_method.is_override) {
                    error("Cannot override non-virtual method '" + method.name + "' from '" +
                              current + "'",
                          method.span, "T006");
                    return;
                }

                // Check signature match - return type
                TypePtr override_ret = method.return_type.has_value()
                                           ? resolve_type(**method.return_type)
                                           : make_unit();
                TypePtr parent_ret =
                    parent_method.sig.return_type ? parent_method.sig.return_type : make_unit();

                if (!types_equal(override_ret, parent_ret)) {
                    error("Override method '" + method.name +
                              "' has different return type than base method",
                          method.span, "T016");
                    return;
                }

                // Check parameter count (excluding implicit 'this' from both sides)
                size_t override_params = method.params.size();
                size_t parent_params = parent_method.sig.params.size();
                // Exclude 'this' from override method params if present
                if (override_params > 0 && method.params[0].pattern &&
                    method.params[0].pattern->is<parser::IdentPattern>() &&
                    method.params[0].pattern->as<parser::IdentPattern>().name == "this") {
                    override_params -= 1;
                }
                // Note: parent_method.sig.params already excludes 'this' when loaded from module

                if (override_params != parent_params) {
                    error("Override method '" + method.name + "' has " +
                              std::to_string(override_params) +
                              " parameters, but base method has " + std::to_string(parent_params),
                          method.span, "T004");
                    return;
                }

                // Check parameter types match
                // Note: override params are at method.params[1], [2], etc. (index 0 is 'this')
                // parent params are at sig.params[0], [1], etc. ('this' already excluded)
                for (size_t i = 0; i < override_params; ++i) {
                    // Override param: skip 'this' by adding 1 to index
                    size_t override_idx = i + 1; // method.params[0] is 'this'
                    TypePtr override_param_type =
                        (override_idx < method.params.size() && method.params[override_idx].type)
                            ? resolve_type(*method.params[override_idx].type)
                            : make_unit();
                    // Parent param: 'this' already excluded from sig.params
                    TypePtr parent_param_type = parent_method.sig.params.size() > i
                                                    ? parent_method.sig.params[i]
                                                    : make_unit();

                    if (!types_equal(override_param_type, parent_param_type)) {
                        error("Override method '" + method.name + "' parameter " +
                                  std::to_string(i + 1) + " has different type than base method",
                              method.span, "T001");
                        return;
                    }
                }

                break;
            }
        }

        if (found)
            break;

        if (parent->base_class.has_value()) {
            current = parent->base_class.value();
        } else {
            break;
        }
    }

    if (!found) {
        error("Method '" + method.name + "' marked as override but not found in any base class",
              method.span, "T006");
    }
}

void TypeChecker::validate_interface_impl(const parser::ClassDecl& cls) {
    for (const auto& iface_type : cls.implements) {
        // Extract interface name from the type (supports generic interfaces)
        auto* named = std::get_if<parser::NamedType>(&iface_type->kind);
        if (!named || named->path.segments.empty()) {
            continue;
        }

        const std::string& iface_name = named->path.segments.back();
        auto iface_def = env_.lookup_interface(iface_name);

        if (!iface_def.has_value()) {
            error("Interface '" + iface_name + "' not found", cls.span, "T047");
            continue;
        }

        // Check all interface methods are implemented
        for (const auto& iface_method : iface_def->methods) {
            if (iface_method.has_default) {
                continue; // Has default implementation
            }

            bool implemented = false;
            for (const auto& cls_method : cls.methods) {
                if (cls_method.name == iface_method.sig.name) {
                    implemented = true;

                    // Check signature match
                    // 1. Check return type
                    TypePtr expected_return =
                        iface_method.sig.return_type ? iface_method.sig.return_type : make_unit();
                    TypePtr actual_return = cls_method.return_type
                                                ? resolve_type(**cls_method.return_type)
                                                : make_unit();
                    if (!types_equal(expected_return, actual_return)) {
                        error("Method '" + cls_method.name + "' in class '" + cls.name +
                                  "' has incompatible return type with interface '" + iface_name +
                                  "'. Expected '" + type_to_string(expected_return) +
                                  "' but got '" + type_to_string(actual_return) + "'",
                              cls_method.span, "T016");
                    }

                    // 2. Check parameter count (excluding 'this')
                    size_t expected_params = iface_method.sig.params.size();
                    size_t actual_params = 0;
                    for (const auto& p : cls_method.params) {
                        if (!p.pattern || !p.pattern->is<parser::IdentPattern>() ||
                            p.pattern->as<parser::IdentPattern>().name != "this") {
                            actual_params++;
                        }
                    }
                    if (expected_params != actual_params) {
                        error("Method '" + cls_method.name + "' in class '" + cls.name +
                                  "' has wrong number of parameters. Interface '" + iface_name +
                                  "' expects " + std::to_string(expected_params) +
                                  " parameters but got " + std::to_string(actual_params),
                              cls_method.span, "T004");
                    } else {
                        // 3. Check parameter types
                        size_t param_idx = 0;
                        for (const auto& cls_param : cls_method.params) {
                            if (cls_param.pattern &&
                                cls_param.pattern->is<parser::IdentPattern>() &&
                                cls_param.pattern->as<parser::IdentPattern>().name == "this") {
                                continue; // Skip 'this' parameter
                            }
                            if (param_idx < expected_params && cls_param.type) {
                                TypePtr expected_param_type = iface_method.sig.params[param_idx];
                                TypePtr actual_param_type = resolve_type(*cls_param.type);
                                if (!types_equal(expected_param_type, actual_param_type)) {
                                    error("Parameter " + std::to_string(param_idx + 1) +
                                              " of method '" + cls_method.name + "' in class '" +
                                              cls.name +
                                              "' has incompatible type with interface '" +
                                              iface_name + "'. Expected '" +
                                              type_to_string(expected_param_type) + "' but got '" +
                                              type_to_string(actual_param_type) + "'",
                                          cls_method.span, "T001");
                                }
                            }
                            param_idx++;
                        }
                    }
                    break;
                }
            }

            if (!implemented) {
                error("Class '" + cls.name + "' does not implement method '" +
                          iface_method.sig.name + "' from interface '" + iface_name + "'",
                      cls.span, "T026");
            }
        }
    }
}

// ============================================================================
// OOP Type Checking - Class Body Checking (Pass 3)
// ============================================================================

void TypeChecker::check_class_body(const parser::ClassDecl& cls) {
    // Set up self type for 'this' references
    auto class_type = std::make_shared<Type>();
    class_type->kind = ClassType{cls.name, "", {}};
    current_self_type_ = class_type;

    // Check constructor bodies
    for (const auto& ctor : cls.constructors) {
        if (ctor.body.has_value()) {
            // Push a new scope
            env_.push_scope();

            // Bind 'this' in scope
            auto this_type = make_ref(current_self_type_, true); // mut ref to class
            env_.current_scope()->define("this", this_type, false, ctor.span);

            // Bind constructor parameters
            for (const auto& param : ctor.params) {
                if (param.type) {
                    TypePtr param_type = resolve_type(*param.type);
                    bool is_mut = false;
                    std::string name;

                    if (param.pattern->is<parser::IdentPattern>()) {
                        const auto& ident = param.pattern->as<parser::IdentPattern>();
                        name = ident.name;
                        is_mut = ident.is_mut;
                    }

                    if (!name.empty()) {
                        env_.current_scope()->define(name, param_type, is_mut, param.span);
                    }
                }
            }

            // Check constructor body
            check_block(ctor.body.value());

            env_.pop_scope();
        }
    }

    // Check method bodies
    for (const auto& method : cls.methods) {
        if (method.body.has_value()) {
            // Set return type
            if (method.return_type.has_value()) {
                current_return_type_ = resolve_type(*method.return_type.value());
            } else {
                current_return_type_ = make_unit();
            }

            // Push a new scope
            env_.push_scope();

            // Bind 'this' for non-static methods
            if (!method.is_static) {
                auto this_type = make_ref(current_self_type_, true); // mut ref
                env_.current_scope()->define("this", this_type, false, method.span);
            }

            // Bind parameters
            for (const auto& param : method.params) {
                if (param.type) {
                    TypePtr param_type = resolve_type(*param.type);
                    bool is_mut = false;
                    std::string name;

                    if (param.pattern->is<parser::IdentPattern>()) {
                        const auto& ident = param.pattern->as<parser::IdentPattern>();
                        name = ident.name;
                        is_mut = ident.is_mut;
                    }

                    if (!name.empty()) {
                        env_.current_scope()->define(name, param_type, is_mut, param.span);
                    }
                }
            }

            // Check method body
            check_block(method.body.value());

            env_.pop_scope();
            current_return_type_ = nullptr;
        }
    }

    current_self_type_ = nullptr;
}

// ============================================================================
// Visibility Checking
// ============================================================================

bool TypeChecker::is_subclass_of(const std::string& derived_class, const std::string& base_class) {
    if (derived_class == base_class) {
        return true;
    }

    std::string current = derived_class;
    std::set<std::string> visited;

    while (!current.empty()) {
        if (visited.count(current))
            break; // Prevent infinite loop
        visited.insert(current);

        auto class_def = env_.lookup_class(current);
        if (!class_def.has_value())
            break;

        if (class_def->base_class.has_value()) {
            if (class_def->base_class.value() == base_class) {
                return true;
            }
            current = class_def->base_class.value();
        } else {
            break;
        }
    }

    return false;
}

bool TypeChecker::check_member_visibility(MemberVisibility vis, const std::string& defining_class,
                                          const std::string& member_name, SourceSpan span) {
    // Public members are always accessible
    if (vis == MemberVisibility::Public) {
        return true;
    }

    // Get current class context (if any)
    std::string current_class_name;
    if (current_self_type_ && current_self_type_->is<ClassType>()) {
        current_class_name = current_self_type_->as<ClassType>().name;
    }

    // Private: only accessible within the defining class
    if (vis == MemberVisibility::Private) {
        if (current_class_name != defining_class) {
            error("Cannot access private member '" + member_name + "' of class '" + defining_class +
                      "' from " +
                      (current_class_name.empty() ? "outside any class"
                                                  : "class '" + current_class_name + "'"),
                  span, "T048");
            return false;
        }
        return true;
    }

    // Protected: accessible within the class and subclasses
    if (vis == MemberVisibility::Protected) {
        if (current_class_name.empty()) {
            error("Cannot access protected member '" + member_name + "' of class '" +
                      defining_class + "' from outside any class",
                  span, "T048");
            return false;
        }

        if (!is_subclass_of(current_class_name, defining_class)) {
            error("Cannot access protected member '" + member_name + "' of class '" +
                      defining_class + "' from class '" + current_class_name +
                      "' which is not a subclass",
                  span, "T048");
            return false;
        }
        return true;
    }

    return true;
}

} // namespace tml::types
