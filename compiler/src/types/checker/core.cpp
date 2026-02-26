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
    for (const auto& param : impl.generics) {
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
