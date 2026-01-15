//! # HIR Builder - Core Implementation
//!
//! This file implements the main `HirBuilder` class responsible for lowering
//! the Abstract Syntax Tree (AST) to High-level Intermediate Representation (HIR).
//!
//! ## Overview
//!
//! The HIR builder performs "lowering" - transforming parser AST nodes into
//! a simpler, more explicit representation suitable for type checking and
//! code generation. Key transformations include:
//!
//! - **Desugaring**: `for` loops → iterator protocol, `?` → explicit match
//! - **Name resolution**: Identifiers linked to declarations
//! - **Type resolution**: Type annotations resolved to concrete types
//! - **Generic instantiation**: Monomorphization of generic types/functions
//!
//! ## Architecture
//!
//! ```text
//! ┌─────────────┐    ┌────────────┐    ┌─────────────┐
//! │  Parser AST │ -> │ HirBuilder │ -> │ HIR Module  │
//! └─────────────┘    └────────────┘    └─────────────┘
//!                          │
//!                          ├── lower_expr()  (hir_builder_expr.cpp)
//!                          ├── lower_stmt()  (hir_builder_stmt.cpp)
//!                          └── lower_pattern() (hir_builder_pattern.cpp)
//! ```
//!
//! ## Monomorphization
//!
//! Generic types and functions are instantiated at each call site with
//! concrete type arguments. The `MonomorphizationCache` ensures each
//! unique instantiation is created only once.
//!
//! Example: `Vec[I32]` and `Vec[String]` become separate types with
//! mangled names like `Vec_I32` and `Vec_String`.
//!
//! ## Scope Management
//!
//! The builder maintains a scope stack to track variable bindings:
//! - `push_scope()` / `pop_scope()` for block boundaries
//! - Variables are added to the current scope during let/var lowering
//! - Name resolution walks the scope stack from innermost to outermost
//!
//! ## See Also
//!
//! - `hir_builder.hpp` - Class declarations and interface
//! - `hir_builder_expr.cpp` - Expression lowering
//! - `hir_builder_stmt.cpp` - Statement lowering
//! - `hir_builder_pattern.cpp` - Pattern lowering

#include "hir/hir_builder.hpp"

#include "lexer/token.hpp"

namespace tml::hir {

// ============================================================================
// MonomorphizationCache
// ============================================================================
//
// The monomorphization cache tracks instantiations of generic types and
// functions with concrete type arguments. Each unique combination of
// base name + type args produces a unique mangled name.
//
// Example:
//   Vec[I32]    -> "Vec_I32"
//   Vec[String] -> "Vec_String"
//   map[I32, String] -> "map_I32_String"

auto MonomorphizationCache::has_type(const std::string& key) const -> bool {
    return type_instances.find(key) != type_instances.end();
}

auto MonomorphizationCache::has_func(const std::string& key) const -> bool {
    return func_instances.find(key) != func_instances.end();
}

auto MonomorphizationCache::get_or_create_type(const std::string& base_name,
                                               const std::vector<HirType>& type_args)
    -> std::string {
    std::string key = mangle_name(base_name, type_args);
    auto it = type_instances.find(key);
    if (it != type_instances.end()) {
        return it->second;
    }
    type_instances[key] = key;
    return key;
}

auto MonomorphizationCache::get_or_create_func(const std::string& base_name,
                                               const std::vector<HirType>& type_args)
    -> std::string {
    std::string key = mangle_name(base_name, type_args);
    auto it = func_instances.find(key);
    if (it != func_instances.end()) {
        return it->second;
    }
    func_instances[key] = key;
    return key;
}

auto MonomorphizationCache::mangle_name(const std::string& base, const std::vector<HirType>& args)
    -> std::string {
    if (args.empty()) {
        return base;
    }

    std::string result = base;
    for (const auto& arg : args) {
        result += "_";
        if (arg) {
            result += types::type_to_string(arg);
        } else {
            result += "?";
        }
    }
    // Sanitize: replace invalid characters
    for (char& c : result) {
        if (c == '[' || c == ']' || c == ',' || c == ' ' || c == '<' || c == '>') {
            c = '_';
        }
    }
    return result;
}

// ============================================================================
// HirBuilder Constructor
// ============================================================================

HirBuilder::HirBuilder(types::TypeEnv& type_env) : type_env_(type_env) {}

// ============================================================================
// Module Lowering
// ============================================================================
//
// Entry point for lowering an entire AST module to HIR. Processes all
// top-level declarations in order:
// 1. Functions -> HirFunction
// 2. Structs -> HirStruct
// 3. Enums -> HirEnum
// 4. Traits -> HirBehavior
// 5. Impls -> HirImpl
// 6. Constants -> HirConst
// 7. Use statements -> imports list
//
// After processing all declarations, pending monomorphization requests
// are resolved to ensure all generic instantiations are available.

auto HirBuilder::lower_module(const parser::Module& ast_module) -> HirModule {
    HirModule module;
    module.name = ast_module.name;
    // source_path is not available in parser::Module - use span start file if available
    module.source_path = std::string(ast_module.span.start.file);

    current_module_ = &module;

    // Lower all declarations
    for (const auto& decl : ast_module.decls) {
        std::visit(
            [&](const auto& d) {
                using T = std::decay_t<decltype(d)>;

                if constexpr (std::is_same_v<T, parser::FuncDecl>) {
                    module.functions.push_back(lower_function(d));
                } else if constexpr (std::is_same_v<T, parser::StructDecl>) {
                    module.structs.push_back(lower_struct(d));
                } else if constexpr (std::is_same_v<T, parser::EnumDecl>) {
                    module.enums.push_back(lower_enum(d));
                } else if constexpr (std::is_same_v<T, parser::TraitDecl>) {
                    module.behaviors.push_back(lower_behavior(d));
                } else if constexpr (std::is_same_v<T, parser::ImplDecl>) {
                    module.impls.push_back(lower_impl(d));
                } else if constexpr (std::is_same_v<T, parser::ConstDecl>) {
                    module.constants.push_back(lower_const(d));
                } else if constexpr (std::is_same_v<T, parser::UseDecl>) {
                    // Process import
                    std::string import_path;
                    for (size_t i = 0; i < d.path.segments.size(); ++i) {
                        if (i > 0) {
                            import_path += "::";
                        }
                        import_path += d.path.segments[i];
                    }
                    module.imports.push_back(import_path);
                }
                // TypeAliasDecl and ModDecl are handled during type checking
            },
            decl->kind);
    }

    // Process pending monomorphizations
    process_monomorphizations();

    current_module_ = nullptr;
    return module;
}

// ============================================================================
// Function Lowering
// ============================================================================
//
// Lowers a function declaration to HIR. Key steps:
// 1. Create HirFunction with metadata (name, visibility, async, extern)
// 2. Lower parameters (pattern -> HirParam with name, type, mutability)
// 3. Lower return type annotation
// 4. Lower function body in a new scope with parameters bound
//
// For generic functions, the mangled_name is updated based on type arguments
// during monomorphization.

auto HirBuilder::lower_function(const parser::FuncDecl& func) -> HirFunction {
    HirFunction hir_func;
    hir_func.id = fresh_id();
    hir_func.name = func.name;
    hir_func.mangled_name = func.name; // Will be updated for generic functions
    hir_func.is_public = func.vis == parser::Visibility::Public;
    hir_func.is_async = func.is_async;
    hir_func.is_extern = func.extern_abi.has_value();
    hir_func.extern_abi = func.extern_abi;
    hir_func.span = func.span;

    // Extract attributes
    for (const auto& decorator : func.decorators) {
        hir_func.attributes.push_back(decorator.name);
    }

    // Store current function context
    current_func_name_ = func.name;

    // Lower parameters
    for (const auto& param : func.params) {
        HirParam hir_param;
        // Extract name from pattern
        if (param.pattern->is<parser::IdentPattern>()) {
            const auto& ident = param.pattern->as<parser::IdentPattern>();
            hir_param.name = ident.name;
            hir_param.is_mut = ident.is_mut;
        } else {
            hir_param.name = "_"; // Anonymous parameter
            hir_param.is_mut = false;
        }
        hir_param.type = resolve_type(*param.type);
        hir_param.span = param.span;
        hir_func.params.push_back(std::move(hir_param));
    }

    // Lower return type
    if (func.return_type) {
        hir_func.return_type = resolve_type(**func.return_type);
    } else {
        hir_func.return_type = types::make_unit();
    }
    current_return_type_ = hir_func.return_type;

    // Lower body
    if (func.body) {
        // Create scope for function body
        scopes_.emplace_back();
        type_env_.push_scope(); // Also push type environment scope for variable types
        for (const auto& param : hir_func.params) {
            scopes_.back().insert(param.name);
            // Define parameter in type environment so we can look up its type later
            type_env_.current_scope()->define(param.name, param.type, param.is_mut, param.span);
        }

        hir_func.body = lower_block(*func.body);

        type_env_.pop_scope(); // Pop type environment scope
        scopes_.pop_back();
    }

    current_func_name_.clear();
    current_return_type_ = nullptr;

    return hir_func;
}

// ============================================================================
// Struct Lowering
// ============================================================================

auto HirBuilder::lower_struct(const parser::StructDecl& struct_decl) -> HirStruct {
    HirStruct hir_struct;
    hir_struct.id = fresh_id();
    hir_struct.name = struct_decl.name;
    hir_struct.mangled_name = struct_decl.name;
    hir_struct.is_public = struct_decl.vis == parser::Visibility::Public;
    hir_struct.span = struct_decl.span;

    for (const auto& field : struct_decl.fields) {
        HirField hir_field;
        hir_field.name = field.name;
        hir_field.type = resolve_type(*field.type);
        hir_field.is_public = field.vis == parser::Visibility::Public;
        hir_field.span = field.span;
        hir_struct.fields.push_back(std::move(hir_field));
    }

    return hir_struct;
}

// ============================================================================
// Enum Lowering
// ============================================================================

auto HirBuilder::lower_enum(const parser::EnumDecl& enum_decl) -> HirEnum {
    HirEnum hir_enum;
    hir_enum.id = fresh_id();
    hir_enum.name = enum_decl.name;
    hir_enum.mangled_name = enum_decl.name;
    hir_enum.is_public = enum_decl.vis == parser::Visibility::Public;
    hir_enum.span = enum_decl.span;

    int index = 0;
    for (const auto& variant : enum_decl.variants) {
        HirVariant hir_variant;
        hir_variant.name = variant.name;
        hir_variant.index = index++;
        hir_variant.span = variant.span;

        if (variant.tuple_fields) {
            for (const auto& field_type : *variant.tuple_fields) {
                hir_variant.payload_types.push_back(resolve_type(*field_type));
            }
        }

        hir_enum.variants.push_back(std::move(hir_variant));
    }

    return hir_enum;
}

// ============================================================================
// Behavior Lowering
// ============================================================================

auto HirBuilder::lower_behavior(const parser::TraitDecl& trait_decl) -> HirBehavior {
    HirBehavior hir_behavior;
    hir_behavior.id = fresh_id();
    hir_behavior.name = trait_decl.name;
    hir_behavior.is_public = trait_decl.vis == parser::Visibility::Public;
    hir_behavior.span = trait_decl.span;

    // Super behaviors
    for (const auto& super_trait : trait_decl.super_traits) {
        hir_behavior.super_behaviors.push_back(types::type_to_string(resolve_type(*super_trait)));
    }

    // Methods
    for (const auto& method : trait_decl.methods) {
        HirBehaviorMethod hir_method;
        hir_method.name = method.name;
        hir_method.has_default_impl = method.body.has_value();
        hir_method.span = method.span;

        // Lower parameters
        for (const auto& param : method.params) {
            HirParam hir_param;
            if (param.pattern->is<parser::IdentPattern>()) {
                const auto& ident = param.pattern->as<parser::IdentPattern>();
                hir_param.name = ident.name;
                hir_param.is_mut = ident.is_mut;
            } else {
                hir_param.name = "_";
                hir_param.is_mut = false;
            }
            hir_param.type = resolve_type(*param.type);
            hir_param.span = param.span;
            hir_method.params.push_back(std::move(hir_param));
        }

        // Return type
        if (method.return_type) {
            hir_method.return_type = resolve_type(**method.return_type);
        } else {
            hir_method.return_type = types::make_unit();
        }

        // Default body
        if (method.body) {
            scopes_.emplace_back();
            for (const auto& p : hir_method.params) {
                scopes_.back().insert(p.name);
            }
            hir_method.default_body = lower_block(*method.body);
            scopes_.pop_back();
        }

        hir_behavior.methods.push_back(std::move(hir_method));
    }

    return hir_behavior;
}

// ============================================================================
// Impl Lowering
// ============================================================================

auto HirBuilder::lower_impl(const parser::ImplDecl& impl_decl) -> HirImpl {
    HirImpl hir_impl;
    hir_impl.id = fresh_id();
    hir_impl.span = impl_decl.span;

    // Trait being implemented
    if (impl_decl.trait_type) {
        hir_impl.behavior_name = types::type_to_string(resolve_type(*impl_decl.trait_type));
    }

    // Self type
    hir_impl.self_type = resolve_type(*impl_decl.self_type);
    hir_impl.type_name = types::type_to_string(hir_impl.self_type);

    // Set the context for resolving 'This'/'Self' types in method parameters
    current_impl_self_type_ = hir_impl.self_type;

    // Methods
    for (const auto& method : impl_decl.methods) {
        hir_impl.methods.push_back(lower_function(method));
    }

    // Clear the context
    current_impl_self_type_ = std::nullopt;

    return hir_impl;
}

// ============================================================================
// Const Lowering
// ============================================================================

auto HirBuilder::lower_const(const parser::ConstDecl& const_decl) -> HirConst {
    HirConst hir_const;
    hir_const.id = fresh_id();
    hir_const.name = const_decl.name;
    hir_const.type = resolve_type(*const_decl.type);
    hir_const.value = lower_expr(*const_decl.value);
    hir_const.is_public = const_decl.vis == parser::Visibility::Public;
    hir_const.span = const_decl.span;
    return hir_const;
}

// ============================================================================
// Helper Methods
// ============================================================================
//
// Utility functions used throughout the lowering process:
// - fresh_id(): Generate unique HIR node identifiers
// - resolve_type(): Convert parser::Type to HirType (types::TypePtr)
// - get_expr_type(): Infer type from expression structure
// - convert_*_op(): Map parser operators to HIR operators
// - is_captured() / collect_captures(): Closure capture analysis

auto HirBuilder::fresh_id() -> HirId {
    return id_gen_.next();
}

auto HirBuilder::current_return_type() const -> HirType {
    return current_return_type_;
}

void HirBuilder::request_monomorphization(const std::string& func_name,
                                          const std::vector<HirType>& type_args) {
    mono_requests_.push_back({func_name, type_args});
}

void HirBuilder::process_monomorphizations() {
    // Process pending monomorphization requests.
    // Each request represents a call to a generic function with concrete type arguments.
    // For now, we just track the mangled names - actual instantiation will happen
    // during codegen when the full function body is needed.
    for (const auto& request : mono_requests_) {
        mono_cache_.get_or_create_func(request.func_name, request.type_args);
    }
    mono_requests_.clear();
}

auto HirBuilder::resolve_type(const parser::Type& type) -> HirType {
    // Delegate to type checker's resolution
    // For now, create a basic type mapping
    if (type.is<parser::NamedType>()) {
        const auto& named = type.as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            const std::string& name = named.path.segments.back();

            // Handle primitive types
            if (name == "I8")
                return types::make_primitive(types::PrimitiveKind::I8);
            if (name == "I16")
                return types::make_primitive(types::PrimitiveKind::I16);
            if (name == "I32")
                return types::make_i32();
            if (name == "I64")
                return types::make_i64();
            if (name == "U8")
                return types::make_primitive(types::PrimitiveKind::U8);
            if (name == "U16")
                return types::make_primitive(types::PrimitiveKind::U16);
            if (name == "U32")
                return types::make_primitive(types::PrimitiveKind::U32);
            if (name == "U64")
                return types::make_primitive(types::PrimitiveKind::U64);
            if (name == "F32")
                return types::make_primitive(types::PrimitiveKind::F32);
            if (name == "F64")
                return types::make_f64();
            if (name == "Bool")
                return types::make_bool();
            if (name == "Str")
                return types::make_str();
            if (name == "Unit" || name == "()")
                return types::make_unit();

            // Handle 'This' and 'Self' types in impl context
            // These refer to the impl's self type (e.g., 'Number' in impl Addable for Number)
            if ((name == "This" || name == "Self") && current_impl_self_type_.has_value()) {
                return current_impl_self_type_.value();
            }

            // Check if this is a class type
            if (auto class_def = type_env_.lookup_class(name)) {
                auto result = std::make_shared<types::Type>();
                result->kind = types::ClassType{name};
                return result;
            }

            // For user-defined types (structs, enums), create a NamedType
            auto result = std::make_shared<types::Type>();
            result->kind = types::NamedType{name, "", {}};
            return result;
        }
    } else if (type.is<parser::RefType>()) {
        const auto& ref = type.as<parser::RefType>();
        return types::make_ref(resolve_type(*ref.inner), ref.is_mut);
    } else if (type.is<parser::ArrayType>()) {
        const auto& arr = type.as<parser::ArrayType>();
        // Try to evaluate size as a constant expression
        size_t size = 0;
        if (arr.size && arr.size->is<parser::LiteralExpr>()) {
            const auto& lit = arr.size->as<parser::LiteralExpr>();
            if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                size = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
            }
        }
        return types::make_array(resolve_type(*arr.element), size);
    } else if (type.is<parser::SliceType>()) {
        const auto& slice = type.as<parser::SliceType>();
        return types::make_slice(resolve_type(*slice.element));
    } else if (type.is<parser::TupleType>()) {
        const auto& tuple = type.as<parser::TupleType>();
        std::vector<types::TypePtr> elements;
        for (const auto& elem : tuple.elements) {
            elements.push_back(resolve_type(*elem));
        }
        return types::make_tuple(std::move(elements));
    }

    return types::make_unit();
}

auto HirBuilder::get_expr_type(const parser::Expr& expr) -> HirType {
    // Infer type from expression structure.
    // The type checker has already validated types, so we reconstruct them here.
    return std::visit(
        [&](const auto& e) -> HirType {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::LiteralExpr>) {
                switch (e.token.kind) {
                case lexer::TokenKind::IntLiteral:
                    if (std::holds_alternative<lexer::IntValue>(e.token.value)) {
                        auto int_val = std::get<lexer::IntValue>(e.token.value);
                        if (!int_val.suffix.empty()) {
                            if (int_val.suffix == "i8")
                                return types::make_primitive(types::PrimitiveKind::I8);
                            if (int_val.suffix == "i16")
                                return types::make_primitive(types::PrimitiveKind::I16);
                            if (int_val.suffix == "i32")
                                return types::make_i32();
                            if (int_val.suffix == "i64")
                                return types::make_i64();
                            if (int_val.suffix == "u8")
                                return types::make_primitive(types::PrimitiveKind::U8);
                            if (int_val.suffix == "u16")
                                return types::make_primitive(types::PrimitiveKind::U16);
                            if (int_val.suffix == "u32")
                                return types::make_primitive(types::PrimitiveKind::U32);
                            if (int_val.suffix == "u64")
                                return types::make_primitive(types::PrimitiveKind::U64);
                        }
                    }
                    // Default to I32 (like most languages)
                    return types::make_i32();
                case lexer::TokenKind::FloatLiteral:
                    return types::make_f64();
                case lexer::TokenKind::StringLiteral:
                    return types::make_str();
                case lexer::TokenKind::BoolLiteral:
                    return types::make_bool();
                case lexer::TokenKind::CharLiteral:
                    return types::make_primitive(types::PrimitiveKind::Char);
                default:
                    return types::make_unit();
                }
            } else if constexpr (std::is_same_v<T, parser::IdentExpr>) {
                // Look up variable type in current scope
                auto scope = type_env_.current_scope();
                if (scope) {
                    if (auto var = scope->lookup(e.name)) {
                        return type_env_.resolve(var->type);
                    }
                }
                return types::make_unit();
            } else if constexpr (std::is_same_v<T, parser::CallExpr>) {
                // Look up function return type
                std::string func_name;
                if (e.callee->is<parser::IdentExpr>()) {
                    func_name = e.callee->as<parser::IdentExpr>().name;
                } else if (e.callee->is<parser::PathExpr>()) {
                    const auto& path = e.callee->as<parser::PathExpr>();
                    if (!path.path.segments.empty()) {
                        func_name = path.path.segments.back();
                    }
                }
                if (auto sig = type_env_.lookup_func(func_name)) {
                    return type_env_.resolve(sig->return_type);
                }
                return types::make_unit();
            } else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
                // Get field type from struct definition
                HirType object_type = get_expr_type(*e.object);
                if (object_type && object_type->is<types::NamedType>()) {
                    const auto& named = object_type->as<types::NamedType>();
                    if (auto struct_def = type_env_.lookup_struct(named.name)) {
                        for (const auto& field : struct_def->fields) {
                            if (field.first == e.field) {
                                return type_env_.resolve(field.second);
                            }
                        }
                    }
                }
                return types::make_unit();
            } else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
                // Get element type from array/slice
                HirType container_type = get_expr_type(*e.object);
                if (container_type) {
                    if (container_type->is<types::ArrayType>()) {
                        return container_type->as<types::ArrayType>().element;
                    } else if (container_type->is<types::SliceType>()) {
                        return container_type->as<types::SliceType>().element;
                    }
                }
                return types::make_unit();
            } else if constexpr (std::is_same_v<T, parser::StructExpr>) {
                // Get struct type from definition
                std::string struct_name;
                if (!e.path.segments.empty()) {
                    struct_name = e.path.segments.back();
                }
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{struct_name, "", {}};
                return result;
            } else if constexpr (std::is_same_v<T, parser::PathExpr>) {
                // Check for enum variant or variable
                if (e.path.segments.size() >= 2) {
                    std::string enum_name = e.path.segments[e.path.segments.size() - 2];
                    if (auto enum_def = type_env_.lookup_enum(enum_name)) {
                        auto result = std::make_shared<types::Type>();
                        result->kind = types::NamedType{enum_name, "", {}};
                        return result;
                    }
                }
                // Try variable lookup
                if (!e.path.segments.empty()) {
                    auto scope = type_env_.current_scope();
                    if (scope) {
                        if (auto var = scope->lookup(e.path.segments.back())) {
                            return type_env_.resolve(var->type);
                        }
                    }
                }
                return types::make_unit();
            } else {
                return types::make_unit();
            }
        },
        expr.kind);
}

auto HirBuilder::get_field_index(const std::string& struct_name, const std::string& field_name)
    -> int {
    if (current_module_) {
        if (auto* s = current_module_->find_struct(struct_name)) {
            for (size_t i = 0; i < s->fields.size(); ++i) {
                if (s->fields[i].name == field_name) {
                    return static_cast<int>(i);
                }
            }
        }
    }
    return -1;
}

auto HirBuilder::get_variant_index(const std::string& enum_name, const std::string& variant_name)
    -> int {
    if (current_module_) {
        if (auto* e = current_module_->find_enum(enum_name)) {
            for (const auto& v : e->variants) {
                if (v.name == variant_name) {
                    return v.index;
                }
            }
        }
    }
    return -1;
}

auto HirBuilder::convert_binary_op(parser::BinaryOp op) -> HirBinOp {
    switch (op) {
    case parser::BinaryOp::Add:
        return HirBinOp::Add;
    case parser::BinaryOp::Sub:
        return HirBinOp::Sub;
    case parser::BinaryOp::Mul:
        return HirBinOp::Mul;
    case parser::BinaryOp::Div:
        return HirBinOp::Div;
    case parser::BinaryOp::Mod:
        return HirBinOp::Mod;
    case parser::BinaryOp::Eq:
        return HirBinOp::Eq;
    case parser::BinaryOp::Ne:
        return HirBinOp::Ne;
    case parser::BinaryOp::Lt:
        return HirBinOp::Lt;
    case parser::BinaryOp::Le:
        return HirBinOp::Le;
    case parser::BinaryOp::Gt:
        return HirBinOp::Gt;
    case parser::BinaryOp::Ge:
        return HirBinOp::Ge;
    case parser::BinaryOp::And:
        return HirBinOp::And;
    case parser::BinaryOp::Or:
        return HirBinOp::Or;
    case parser::BinaryOp::BitAnd:
        return HirBinOp::BitAnd;
    case parser::BinaryOp::BitOr:
        return HirBinOp::BitOr;
    case parser::BinaryOp::BitXor:
        return HirBinOp::BitXor;
    case parser::BinaryOp::Shl:
        return HirBinOp::Shl;
    case parser::BinaryOp::Shr:
        return HirBinOp::Shr;
    default:
        return HirBinOp::Add; // Fallback
    }
}

auto HirBuilder::convert_unary_op(parser::UnaryOp op) -> HirUnaryOp {
    switch (op) {
    case parser::UnaryOp::Neg:
        return HirUnaryOp::Neg;
    case parser::UnaryOp::Not:
        return HirUnaryOp::Not;
    case parser::UnaryOp::BitNot:
        return HirUnaryOp::BitNot;
    case parser::UnaryOp::Ref:
        return HirUnaryOp::Ref;
    case parser::UnaryOp::RefMut:
        return HirUnaryOp::RefMut;
    case parser::UnaryOp::Deref:
        return HirUnaryOp::Deref;
    default:
        return HirUnaryOp::Neg; // Fallback
    }
}

auto HirBuilder::is_captured(const std::string& name) const -> bool {
    // Check if variable is defined in any parent scope but not current
    if (scopes_.empty()) {
        return false;
    }

    // Check current scope
    if (scopes_.back().count(name) > 0) {
        return false; // Defined locally
    }

    // Check parent scopes
    for (size_t i = 0; i + 1 < scopes_.size(); ++i) {
        if (scopes_[i].count(name) > 0) {
            return true;
        }
    }

    return false;
}

// Helper to recursively collect free variables from an expression
namespace {
void collect_free_vars(const parser::Expr& expr, const std::set<std::string>& bound_vars,
                       std::set<std::string>& free_vars) {
    std::visit(
        [&](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::IdentExpr>) {
                // Variable reference - check if it's bound or free
                if (bound_vars.find(e.name) == bound_vars.end()) {
                    free_vars.insert(e.name);
                }
            } else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
                collect_free_vars(*e.left, bound_vars, free_vars);
                collect_free_vars(*e.right, bound_vars, free_vars);
            } else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
                collect_free_vars(*e.operand, bound_vars, free_vars);
            } else if constexpr (std::is_same_v<T, parser::CallExpr>) {
                collect_free_vars(*e.callee, bound_vars, free_vars);
                for (const auto& arg : e.args) {
                    collect_free_vars(*arg, bound_vars, free_vars);
                }
            } else if constexpr (std::is_same_v<T, parser::MethodCallExpr>) {
                collect_free_vars(*e.receiver, bound_vars, free_vars);
                for (const auto& arg : e.args) {
                    collect_free_vars(*arg, bound_vars, free_vars);
                }
            } else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
                collect_free_vars(*e.object, bound_vars, free_vars);
            } else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
                collect_free_vars(*e.object, bound_vars, free_vars);
                collect_free_vars(*e.index, bound_vars, free_vars);
            } else if constexpr (std::is_same_v<T, parser::IfExpr>) {
                collect_free_vars(*e.condition, bound_vars, free_vars);
                collect_free_vars(*e.then_branch, bound_vars, free_vars);
                if (e.else_branch) {
                    collect_free_vars(**e.else_branch, bound_vars, free_vars);
                }
            } else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
                // Create new scope for block
                std::set<std::string> block_vars = bound_vars;
                for (const auto& stmt : e.stmts) {
                    if (stmt->is<parser::LetStmt>()) {
                        const auto& let = stmt->as<parser::LetStmt>();
                        if (let.pattern->is<parser::IdentPattern>()) {
                            block_vars.insert(let.pattern->as<parser::IdentPattern>().name);
                        }
                        if (let.init) {
                            collect_free_vars(**let.init, block_vars, free_vars);
                        }
                    } else if (stmt->is<parser::ExprStmt>()) {
                        collect_free_vars(*stmt->as<parser::ExprStmt>().expr, block_vars,
                                          free_vars);
                    }
                }
                if (e.expr) {
                    collect_free_vars(**e.expr, block_vars, free_vars);
                }
            } else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
                for (const auto& elem : e.elements) {
                    collect_free_vars(*elem, bound_vars, free_vars);
                }
            } else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
                if (std::holds_alternative<std::vector<parser::ExprPtr>>(e.kind)) {
                    for (const auto& elem : std::get<std::vector<parser::ExprPtr>>(e.kind)) {
                        collect_free_vars(*elem, bound_vars, free_vars);
                    }
                } else {
                    const auto& [val, cnt] =
                        std::get<std::pair<parser::ExprPtr, parser::ExprPtr>>(e.kind);
                    collect_free_vars(*val, bound_vars, free_vars);
                    collect_free_vars(*cnt, bound_vars, free_vars);
                }
            } else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
                // Nested closure - parameters are bound within
                std::set<std::string> closure_vars = bound_vars;
                for (const auto& [pattern, _] : e.params) {
                    if (pattern->is<parser::IdentPattern>()) {
                        closure_vars.insert(pattern->as<parser::IdentPattern>().name);
                    }
                }
                collect_free_vars(*e.body, closure_vars, free_vars);
            }
            // LiteralExpr, PathExpr, etc. don't introduce free vars directly
        },
        expr.kind);
}
} // anonymous namespace

auto HirBuilder::collect_captures(const parser::ClosureExpr& closure) -> std::vector<HirCapture> {
    // Build set of bound variables (parameters + parent scope variables)
    std::set<std::string> bound_vars;

    // Add closure parameters
    for (const auto& [pattern, _] : closure.params) {
        if (pattern->is<parser::IdentPattern>()) {
            bound_vars.insert(pattern->as<parser::IdentPattern>().name);
        }
    }

    // Add variables from enclosing scopes (these are available, not captured)
    // We need to track what's in enclosing scopes so we know what to capture
    std::set<std::string> enclosing_vars;
    for (const auto& scope : scopes_) {
        for (const auto& var : scope) {
            enclosing_vars.insert(var);
        }
    }

    // Collect free variables from body
    std::set<std::string> free_vars;
    collect_free_vars(*closure.body, bound_vars, free_vars);

    // Filter to only variables from enclosing scopes (actual captures)
    std::vector<HirCapture> captures;
    for (const auto& var : free_vars) {
        if (enclosing_vars.find(var) != enclosing_vars.end()) {
            // Look up type from type environment
            HirType type = types::make_unit();
            auto scope = type_env_.current_scope();
            if (scope) {
                if (auto v = scope->lookup(var)) {
                    type = type_env_.resolve(v->type);
                }
            }
            captures.push_back(HirCapture{var, type, false}); // Assume immutable for now
        }
    }

    return captures;
}

} // namespace tml::hir
