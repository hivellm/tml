// Type checker core - module checking and declarations
// Handles: check_module, register_*, check_func_decl, check_impl_decl

#include "lexer/token.hpp"
#include "types/checker.hpp"

#include <algorithm>
#include <iostream>
#include <set>

namespace tml::types {

// Reserved type names - builtin types that cannot be redefined by user code
// This prevents accidental shadowing of core types like Maybe, Outcome, List, etc.
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
    // Core enums
    "Maybe",
    "Outcome",
    "Ordering",
    // Core collections (from std)
    "List",
    "HashMap",
    "HashSet",
    "Buffer",
    // Smart pointers
    "Heap",
    "Shared",
    "Sync",
    // Other core types
    "Range",
    "RangeInclusive",
};

// Forward declarations from helpers.cpp
bool is_integer_type(const TypePtr& type);
bool is_float_type(const TypePtr& type);
std::string extract_ffi_module_name(const std::string& link_path);
bool types_compatible(const TypePtr& expected, const TypePtr& actual);

TypeChecker::TypeChecker() = default;

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
        } else if (decl->is<parser::EnumDecl>()) {
            register_enum_decl(decl->as<parser::EnumDecl>());
        } else if (decl->is<parser::TraitDecl>()) {
            register_trait_decl(decl->as<parser::TraitDecl>());
        } else if (decl->is<parser::TypeAliasDecl>()) {
            register_type_alias(decl->as<parser::TypeAliasDecl>());
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
        }
    }

    // Third pass: check function bodies
    for (const auto& decl : module.decls) {
        if (decl->is<parser::FuncDecl>()) {
            check_func_body(decl->as<parser::FuncDecl>());
        } else if (decl->is<parser::ImplDecl>()) {
            check_impl_body(decl->as<parser::ImplDecl>());
        }
    }

    if (has_errors()) {
        return errors_;
    }
    return env_;
}

void TypeChecker::register_struct_decl(const parser::StructDecl& decl) {
    // Check if the type name is reserved (builtin type)
    if (RESERVED_TYPE_NAMES.count(decl.name) > 0) {
        error("Cannot redefine builtin type '" + decl.name +
                  "'. Use the builtin type instead of defining your own.",
              decl.span);
        return;
    }

    std::vector<std::pair<std::string, TypePtr>> fields;
    for (const auto& field : decl.fields) {
        fields.emplace_back(field.name, resolve_type(*field.type));
    }

    std::vector<std::string> type_params;
    for (const auto& param : decl.generics) {
        type_params.push_back(param.name);
    }

    env_.define_struct(StructDef{.name = decl.name,
                                 .type_params = std::move(type_params),
                                 .fields = std::move(fields),
                                 .span = decl.span});
}

void TypeChecker::register_enum_decl(const parser::EnumDecl& decl) {
    // Check if the type name is reserved (builtin type)
    if (RESERVED_TYPE_NAMES.count(decl.name) > 0) {
        error("Cannot redefine builtin type '" + decl.name +
                  "'. Use the builtin type instead of defining your own.",
              decl.span);
        return;
    }

    std::vector<std::pair<std::string, std::vector<TypePtr>>> variants;
    for (const auto& variant : decl.variants) {
        std::vector<TypePtr> types;
        if (variant.tuple_fields) {
            for (const auto& type : *variant.tuple_fields) {
                types.push_back(resolve_type(*type));
            }
        }
        variants.emplace_back(variant.name, std::move(types));
    }

    std::vector<std::string> type_params;
    for (const auto& param : decl.generics) {
        type_params.push_back(param.name);
    }

    env_.define_enum(EnumDef{.name = decl.name,
                             .type_params = std::move(type_params),
                             .variants = std::move(variants),
                             .span = decl.span});
}

void TypeChecker::register_trait_decl(const parser::TraitDecl& decl) {
    std::vector<FuncSig> methods;
    std::set<std::string> methods_with_defaults;

    for (const auto& method : decl.methods) {
        std::vector<TypePtr> params;
        for (const auto& p : method.params) {
            params.push_back(resolve_type(*p.type));
        }
        TypePtr ret = method.return_type ? resolve_type(**method.return_type) : make_unit();
        methods.push_back(FuncSig{.name = method.name,
                                  .params = std::move(params),
                                  .return_type = std::move(ret),
                                  .type_params = {},
                                  .is_async = method.is_async,
                                  .span = method.span});

        // Track methods with default implementations
        if (method.body.has_value()) {
            methods_with_defaults.insert(method.name);
        }
    }

    std::vector<std::string> type_params;
    for (const auto& param : decl.generics) {
        type_params.push_back(param.name);
    }

    // Collect associated type declarations
    std::vector<AssociatedTypeDef> associated_types;
    for (const auto& assoc : decl.associated_types) {
        std::vector<std::string> bounds;
        for (const auto& bound : assoc.bounds) {
            // Convert TypePath to string (just use the last segment for now)
            if (!bound.segments.empty()) {
                bounds.push_back(bound.segments.back());
            }
        }
        associated_types.push_back(AssociatedTypeDef{
            .name = assoc.name, .bounds = std::move(bounds), .default_type = std::nullopt});
    }

    env_.define_behavior(BehaviorDef{.name = decl.name,
                                     .type_params = std::move(type_params),
                                     .associated_types = std::move(associated_types),
                                     .methods = std::move(methods),
                                     .super_behaviors = {},
                                     .methods_with_defaults = std::move(methods_with_defaults),
                                     .span = decl.span});
}

void TypeChecker::register_type_alias(const parser::TypeAliasDecl& decl) {
    env_.define_type_alias(decl.name, resolve_type(*decl.type));
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
        env_.load_native_module(module_path);
        auto module_opt = env_.get_module(module_path);

        if (!module_opt.has_value()) {
            errors_.push_back(
                TypeError{"Module '" + module_path + "' not found", use_decl.span, {}});
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
        env_.load_native_module(module_path);
        auto module_opt = env_.get_module(module_path);

        if (!module_opt.has_value()) {
            errors_.push_back(
                TypeError{"Module '" + module_path + "' not found", use_decl.span, {}});
            return;
        }

        // Import each symbol individually
        for (const auto& symbol : symbols) {
            env_.import_symbol(module_path, symbol, std::nullopt);
        }
        return;
    }

    // Try first as complete module path
    env_.load_native_module(module_path);
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

        env_.load_native_module(base_module_path);
        module_opt = env_.get_module(base_module_path);

        if (module_opt.has_value()) {
            // Last segment is a symbol name - import only that symbol
            std::string symbol_name = use_decl.path.segments.back();
            env_.import_symbol(base_module_path, symbol_name, use_decl.alias);
            return;
        }
    }

    if (!module_opt.has_value()) {
        errors_.push_back(TypeError{"Module '" + module_path + "' not found", use_decl.span, {}});
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
                  func.span);
        }

        // @extern functions must not have a body
        if (func.body.has_value()) {
            error("@extern function '" + func.name + "' must not have a body", func.span);
        }
    }

    // Validate @link paths for security (no directory traversal)
    for (const auto& lib : func.link_libs) {
        if (lib.find("..") != std::string::npos) {
            error("@link path '" + lib +
                      "' contains '..' which is not allowed for security reasons",
                  func.span);
        }
    }

    std::vector<TypePtr> params;
    for (const auto& p : func.params) {
        params.push_back(resolve_type(*p.type));
    }
    TypePtr ret = func.return_type ? resolve_type(**func.return_type) : make_unit();

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

            // Extract behavior names from type pointers
            std::vector<std::string> behavior_names;
            for (const auto& behavior_type : behaviors) {
                if (behavior_type->is<parser::NamedType>()) {
                    const auto& named = behavior_type->as<parser::NamedType>();
                    if (!named.path.segments.empty()) {
                        behavior_names.push_back(named.path.segments.back());
                    }
                }
            }

            if (!type_param_name.empty() && !behavior_names.empty()) {
                where_constraints.push_back(WhereConstraint{type_param_name, behavior_names});
            }
        }
    }

    // Extract type parameter names from generics
    std::vector<std::string> func_type_params;
    for (const auto& param : func.generics) {
        func_type_params.push_back(param.name);
    }

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
                             .ffi_module = ffi_module});
}

void TypeChecker::check_func_body(const parser::FuncDecl& func) {
    // Skip @extern functions - they have no body to check
    if (func.extern_abi.has_value()) {
        return;
    }

    TML_DEBUG_LN("[DEBUG] check_func_body called for function: " << func.name);
    env_.push_scope();
    current_return_type_ = func.return_type ? resolve_type(**func.return_type) : make_unit();

    // Add parameters to scope
    for (const auto& p : func.params) {
        if (p.pattern->is<parser::IdentPattern>()) {
            auto& ident = p.pattern->as<parser::IdentPattern>();
            env_.current_scope()->define(ident.name, resolve_type(*p.type), ident.is_mut,
                                         p.pattern->span);
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
                          func.span);
                }
            }
        }

        // Check return type compatibility (simplified for now)
        (void)body_type;
    }

    env_.pop_scope();
    current_return_type_ = nullptr;
}

void TypeChecker::check_const_decl(const parser::ConstDecl& const_decl) {
    // Resolve the declared type
    TypePtr declared_type = resolve_type(*const_decl.type);

    // Type-check the initializer expression
    TypePtr init_type = check_expr(*const_decl.value);

    // Verify the types match
    if (!types_equal(init_type, declared_type)) {
        error("Type mismatch in const initializer: expected " + type_to_string(declared_type) +
                  ", found " + type_to_string(init_type),
              const_decl.value->span);
        return;
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
        env_.define_func(FuncSig{.name = qualified_name,
                                 .params = std::move(params),
                                 .return_type = std::move(ret),
                                 .type_params = impl_type_params,
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
                  const_decl.value->span);
        }
    }

    for (const auto& method : impl.methods) {
        check_func_body(method);
    }

    current_self_type_ = nullptr;
    current_associated_types_.clear();
    current_type_params_.clear();
}

} // namespace tml::types
