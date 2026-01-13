//! # LLVM IR Generator - Type Inference
//!
//! This file implements expression type inference for codegen.
//!
//! ## Purpose
//!
//! `infer_expr_type()` infers the semantic type of an expression.
//! This is used during monomorphization to determine concrete types
//! for generic instantiation.
//!
//! ## Inference Rules
//!
//! | Expression | Inferred Type                   |
//! |------------|---------------------------------|
//! | Int lit    | I32 (default)                   |
//! | Float lit  | F64 (default)                   |
//! | Bool lit   | Bool                            |
//! | String lit | Str                             |
//! | Identifier | Look up in locals/globals       |
//! | Call       | Return type of function         |
//! | Field      | Type of struct field            |

#include "codegen/llvm_ir_gen.hpp"
#include "types/module.hpp"

#include <iostream>

namespace tml::codegen {

// Helper: infer semantic type from expression for generics instantiation
auto LLVMIRGen::infer_expr_type(const parser::Expr& expr) -> types::TypePtr {
    if (expr.is<parser::LiteralExpr>()) {
        const auto& lit = expr.as<parser::LiteralExpr>();
        switch (lit.token.kind) {
        case lexer::TokenKind::IntLiteral:
            return types::make_i32();
        case lexer::TokenKind::FloatLiteral:
            return types::make_f64();
        case lexer::TokenKind::BoolLiteral:
            return types::make_bool();
        case lexer::TokenKind::StringLiteral:
            return types::make_str();
        case lexer::TokenKind::CharLiteral:
            return types::make_primitive(types::PrimitiveKind::Char);
        case lexer::TokenKind::NullLiteral:
            return types::make_ptr(types::make_unit());
        default:
            return types::make_i32();
        }
    }
    if (expr.is<parser::IdentExpr>()) {
        const auto& ident = expr.as<parser::IdentExpr>();

        // Special handling for 'this' in impl methods
        if (ident.name == "this" && !current_impl_type_.empty()) {
            auto result = std::make_shared<types::Type>();
            // current_impl_type_ might be a mangled name like Maybe__I32
            // Check if it contains __ separator (indicates generic type)
            auto sep_pos = current_impl_type_.find("__");
            if (sep_pos != std::string::npos) {
                // Parse mangled name: Maybe__I32 -> Maybe[I32]
                std::string base_name = current_impl_type_.substr(0, sep_pos);
                std::string type_args_str = current_impl_type_.substr(sep_pos + 2);

                // Split type args by __ and create nested types
                std::vector<types::TypePtr> type_args;
                size_t pos = 0;
                while (pos < type_args_str.size()) {
                    auto next_sep = type_args_str.find("__", pos);
                    std::string arg = (next_sep == std::string::npos)
                                          ? type_args_str.substr(pos)
                                          : type_args_str.substr(pos, next_sep - pos);

                    // Create type for this arg
                    types::TypePtr arg_type;
                    if (arg == "I32")
                        arg_type = types::make_i32();
                    else if (arg == "I64")
                        arg_type = types::make_i64();
                    else if (arg == "Bool")
                        arg_type = types::make_bool();
                    else if (arg == "Str")
                        arg_type = types::make_str();
                    else if (arg == "F32")
                        arg_type = types::make_primitive(types::PrimitiveKind::F32);
                    else if (arg == "F64")
                        arg_type = types::make_f64();
                    else if (arg.starts_with("tuple_")) {
                        // Parse tuple type: tuple_Layout_I64 -> TupleType{Layout, I64}
                        std::string tuple_args = arg.substr(6); // Remove "tuple_"
                        std::vector<types::TypePtr> elements;
                        size_t tuple_pos = 0;
                        while (tuple_pos < tuple_args.size()) {
                            auto next_underscore = tuple_args.find('_', tuple_pos);
                            std::string elem_name =
                                (next_underscore == std::string::npos)
                                    ? tuple_args.substr(tuple_pos)
                                    : tuple_args.substr(tuple_pos, next_underscore - tuple_pos);

                            // Create type for this element
                            types::TypePtr elem_type;
                            if (elem_name == "I32")
                                elem_type = types::make_i32();
                            else if (elem_name == "I64")
                                elem_type = types::make_i64();
                            else if (elem_name == "Bool")
                                elem_type = types::make_bool();
                            else if (elem_name == "Str")
                                elem_type = types::make_str();
                            else if (elem_name == "F32")
                                elem_type = types::make_primitive(types::PrimitiveKind::F32);
                            else if (elem_name == "F64")
                                elem_type = types::make_f64();
                            else {
                                // Named type (struct)
                                auto et = std::make_shared<types::Type>();
                                et->kind = types::NamedType{elem_name, "", {}};
                                elem_type = et;
                            }
                            elements.push_back(elem_type);

                            if (next_underscore == std::string::npos)
                                break;
                            tuple_pos = next_underscore + 1;
                        }
                        arg_type = types::make_tuple(std::move(elements));
                    } else {
                        // Unknown type, use as named type
                        auto t = std::make_shared<types::Type>();
                        t->kind = types::NamedType{arg, "", {}};
                        arg_type = t;
                    }
                    type_args.push_back(arg_type);

                    if (next_sep == std::string::npos)
                        break;
                    pos = next_sep + 2;
                }

                result->kind = types::NamedType{base_name, "", std::move(type_args)};
            } else {
                // Non-generic type
                result->kind = types::NamedType{current_impl_type_, "", {}};
            }
            return result;
        }

        auto it = locals_.find(ident.name);
        if (it != locals_.end()) {
            // Use semantic type if available (for complex types like Ptr[T], FuncType)
            if (it->second.semantic_type) {
                return it->second.semantic_type;
            }
            // Map LLVM type back to semantic type
            const std::string& ty = it->second.type;
            if (ty == "i32")
                return types::make_i32();
            if (ty == "i64")
                return types::make_i64();
            if (ty == "i1")
                return types::make_bool();
            if (ty == "float")
                return types::make_primitive(types::PrimitiveKind::F32);
            if (ty == "double")
                return types::make_f64();
            if (ty == "ptr")
                return types::make_str(); // Assume string for now
            // For struct types, try to extract and demangle generic types
            if (ty.starts_with("%struct.")) {
                std::string mangled = ty.substr(8);

                // Check if this is a generic type (contains __ separator)
                auto sep_pos = mangled.find("__");
                if (sep_pos != std::string::npos) {
                    // Parse mangled name: Maybe__I32 -> Maybe[I32]
                    std::string base_name = mangled.substr(0, sep_pos);
                    std::string type_args_str = mangled.substr(sep_pos + 2);

                    // Split type args by __ and create nested types
                    std::vector<types::TypePtr> type_args;
                    size_t pos = 0;
                    while (pos < type_args_str.size()) {
                        auto next_sep = type_args_str.find("__", pos);
                        std::string arg = (next_sep == std::string::npos)
                                              ? type_args_str.substr(pos)
                                              : type_args_str.substr(pos, next_sep - pos);

                        // Create type for this arg
                        types::TypePtr arg_type;
                        if (arg == "I32")
                            arg_type = types::make_i32();
                        else if (arg == "I64")
                            arg_type = types::make_i64();
                        else if (arg == "Bool")
                            arg_type = types::make_bool();
                        else if (arg == "Str")
                            arg_type = types::make_str();
                        else if (arg == "F32")
                            arg_type = types::make_primitive(types::PrimitiveKind::F32);
                        else if (arg == "F64")
                            arg_type = types::make_f64();
                        else if (arg == "Unit")
                            arg_type = types::make_unit();
                        else if (arg.starts_with("tuple_")) {
                            // Parse tuple type: tuple_Layout_I64 -> TupleType{Layout, I64}
                            std::string tuple_args = arg.substr(6); // Remove "tuple_"
                            std::vector<types::TypePtr> elements;
                            size_t tuple_pos = 0;
                            while (tuple_pos < tuple_args.size()) {
                                auto next_underscore = tuple_args.find('_', tuple_pos);
                                std::string elem_name =
                                    (next_underscore == std::string::npos)
                                        ? tuple_args.substr(tuple_pos)
                                        : tuple_args.substr(tuple_pos, next_underscore - tuple_pos);

                                // Create type for this element
                                types::TypePtr elem_type;
                                if (elem_name == "I32")
                                    elem_type = types::make_i32();
                                else if (elem_name == "I64")
                                    elem_type = types::make_i64();
                                else if (elem_name == "Bool")
                                    elem_type = types::make_bool();
                                else if (elem_name == "Str")
                                    elem_type = types::make_str();
                                else if (elem_name == "F32")
                                    elem_type = types::make_primitive(types::PrimitiveKind::F32);
                                else if (elem_name == "F64")
                                    elem_type = types::make_f64();
                                else if (elem_name == "Unit")
                                    elem_type = types::make_unit();
                                else {
                                    // Named type (struct)
                                    auto et = std::make_shared<types::Type>();
                                    et->kind = types::NamedType{elem_name, "", {}};
                                    elem_type = et;
                                }
                                elements.push_back(elem_type);

                                if (next_underscore == std::string::npos)
                                    break;
                                tuple_pos = next_underscore + 1;
                            }
                            arg_type = types::make_tuple(std::move(elements));
                        } else {
                            // Named type without generics
                            auto t = std::make_shared<types::Type>();
                            t->kind = types::NamedType{arg, "", {}};
                            arg_type = t;
                        }
                        type_args.push_back(arg_type);

                        if (next_sep == std::string::npos)
                            break;
                        pos = next_sep + 2;
                    }

                    auto result = std::make_shared<types::Type>();
                    result->kind = types::NamedType{base_name, "", std::move(type_args)};
                    return result;
                }

                // Non-generic struct type
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{mangled, "", {}};
                return result;
            }
        }

        // Check global constants
        auto const_it = global_constants_.find(ident.name);
        if (const_it != global_constants_.end()) {
            // Global constants are currently stored without explicit type info
            // but the values stored are strings of numeric literals
            // For now, assume I64 for large constants (like FNV hashes)
            // We could store type info alongside constants in the future
            return types::make_i64();
        }
    }
    if (expr.is<parser::BinaryExpr>()) {
        const auto& bin = expr.as<parser::BinaryExpr>();
        // Comparison and logical operators return Bool
        switch (bin.op) {
        case parser::BinaryOp::Eq:
        case parser::BinaryOp::Ne:
        case parser::BinaryOp::Lt:
        case parser::BinaryOp::Gt:
        case parser::BinaryOp::Le:
        case parser::BinaryOp::Ge:
        case parser::BinaryOp::And:
        case parser::BinaryOp::Or:
            return types::make_bool();
        default:
            // Arithmetic/other operators: infer from left operand
            return infer_expr_type(*bin.left);
        }
    }
    if (expr.is<parser::UnaryExpr>()) {
        const auto& unary = expr.as<parser::UnaryExpr>();
        return infer_expr_type(*unary.operand);
    }
    if (expr.is<parser::StructExpr>()) {
        const auto& s = expr.as<parser::StructExpr>();
        if (!s.path.segments.empty()) {
            std::string base_name = s.path.segments.back();

            // Check if this is a generic struct
            auto generic_it = pending_generic_structs_.find(base_name);
            if (generic_it != pending_generic_structs_.end() && !s.fields.empty()) {
                // Infer type arguments from field values
                const parser::StructDecl* decl = generic_it->second;
                std::vector<types::TypePtr> type_args;
                std::unordered_map<std::string, types::TypePtr> inferred_generics;

                for (const auto& gp : decl->generics) {
                    inferred_generics[gp.name] = nullptr;
                }

                for (size_t fi = 0; fi < s.fields.size() && fi < decl->fields.size(); ++fi) {
                    const auto& field_decl = decl->fields[fi];
                    if (field_decl.type && field_decl.type->is<parser::NamedType>()) {
                        const auto& ftype = field_decl.type->as<parser::NamedType>();
                        std::string ft_name =
                            ftype.path.segments.empty() ? "" : ftype.path.segments.back();
                        auto gen_it = inferred_generics.find(ft_name);
                        if (gen_it != inferred_generics.end() && !gen_it->second) {
                            gen_it->second = infer_expr_type(*s.fields[fi].second);
                        }
                    }
                }

                for (const auto& gp : decl->generics) {
                    auto inf = inferred_generics[gp.name];
                    type_args.push_back(inf ? inf : types::make_i32());
                }

                // Return NamedType with type_args
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{base_name, "", std::move(type_args)};
                return result;
            }

            // Non-generic struct
            auto result = std::make_shared<types::Type>();
            result->kind = types::NamedType{base_name, "", {}};
            return result;
        }
    }
    // Handle path expressions (enum variants like Ordering::Less)
    if (expr.is<parser::PathExpr>()) {
        const auto& path = expr.as<parser::PathExpr>();
        if (path.path.segments.size() >= 2) {
            // First segment is the enum type name
            std::string enum_name = path.path.segments[0];
            auto result = std::make_shared<types::Type>();
            result->kind = types::NamedType{enum_name, "", {}};
            return result;
        }
    }
    // Handle field access expressions
    if (expr.is<parser::FieldExpr>()) {
        const auto& field = expr.as<parser::FieldExpr>();
        // Get the type of the object
        types::TypePtr obj_type = infer_expr_type(*field.object);
        if (obj_type && obj_type->is<types::NamedType>()) {
            const auto& named = obj_type->as<types::NamedType>();
            // Look up field type in struct definition
            // For generic types, use the mangled name (e.g., Take__RangeIterI64)
            std::string lookup_name = named.name;
            if (!named.type_args.empty()) {
                lookup_name = mangle_struct_name(named.name, named.type_args);
            }
            std::string field_llvm_type = get_field_type(lookup_name, field.field);
            // Convert LLVM type back to semantic type
            if (field_llvm_type == "i32")
                return types::make_i32();
            if (field_llvm_type == "i64")
                return types::make_i64();
            if (field_llvm_type == "i1")
                return types::make_bool();
            if (field_llvm_type == "float")
                return types::make_primitive(types::PrimitiveKind::F32);
            if (field_llvm_type == "double")
                return types::make_f64();
            if (field_llvm_type == "ptr")
                return types::make_str();
            if (field_llvm_type.starts_with("%struct.")) {
                std::string mangled = field_llvm_type.substr(8);

                // Check if this is a generic type (contains __ separator)
                auto sep_pos = mangled.find("__");
                if (sep_pos != std::string::npos) {
                    // Parse mangled name: Maybe__Str -> Maybe[Str]
                    std::string base_name = mangled.substr(0, sep_pos);
                    std::string type_args_str = mangled.substr(sep_pos + 2);

                    // Split type args by __ and create nested types
                    std::vector<types::TypePtr> type_args;
                    size_t pos = 0;
                    while (pos < type_args_str.size()) {
                        auto next_sep = type_args_str.find("__", pos);
                        std::string arg = (next_sep == std::string::npos)
                                              ? type_args_str.substr(pos)
                                              : type_args_str.substr(pos, next_sep - pos);

                        // Create type for this arg
                        types::TypePtr arg_type;
                        if (arg == "I32")
                            arg_type = types::make_i32();
                        else if (arg == "I64")
                            arg_type = types::make_i64();
                        else if (arg == "Bool")
                            arg_type = types::make_bool();
                        else if (arg == "Str")
                            arg_type = types::make_str();
                        else if (arg == "F32")
                            arg_type = types::make_primitive(types::PrimitiveKind::F32);
                        else if (arg == "F64")
                            arg_type = types::make_f64();
                        else if (arg == "Unit")
                            arg_type = types::make_unit();
                        else if (arg.starts_with("ref_") || arg.starts_with("mutref_")) {
                            // Reference type: ref_X or mutref_X -> RefType
                            bool is_mut = arg.starts_with("mutref_");
                            std::string inner_name = is_mut ? arg.substr(7) : arg.substr(4);

                            // Check if inner is a dyn type
                            types::TypePtr inner_type;
                            if (inner_name.starts_with("dyn_")) {
                                // dyn_Error -> DynBehaviorType
                                std::string behavior = inner_name.substr(4);
                                auto dyn_t = std::make_shared<types::Type>();
                                dyn_t->kind = types::DynBehaviorType{behavior, {}};
                                inner_type = dyn_t;
                            } else {
                                // Regular named type
                                auto inner_t = std::make_shared<types::Type>();
                                inner_t->kind = types::NamedType{inner_name, "", {}};
                                inner_type = inner_t;
                            }

                            auto ref_t = std::make_shared<types::Type>();
                            ref_t->kind = types::RefType{is_mut, inner_type};
                            arg_type = ref_t;
                        } else if (arg.starts_with("dyn_")) {
                            // Dynamic trait type: dyn_Error -> DynBehaviorType
                            std::string behavior = arg.substr(4);
                            auto dyn_t = std::make_shared<types::Type>();
                            dyn_t->kind = types::DynBehaviorType{behavior, {}};
                            arg_type = dyn_t;
                        } else {
                            // Named type without generics
                            auto t = std::make_shared<types::Type>();
                            t->kind = types::NamedType{arg, "", {}};
                            arg_type = t;
                        }
                        type_args.push_back(arg_type);

                        if (next_sep == std::string::npos)
                            break;
                        pos = next_sep + 2;
                    }

                    auto result = std::make_shared<types::Type>();
                    result->kind = types::NamedType{base_name, "", std::move(type_args)};
                    return result;
                }

                // Non-generic struct type
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{mangled, "", {}};
                return result;
            }
            // Handle slice type: { ptr, i64 }
            if (field_llvm_type == "{ ptr, i64 }") {
                // This is a slice - we need to look up the actual element type
                // For now, default to U8 for byte slices
                auto elem_type = types::make_primitive(types::PrimitiveKind::U8);
                auto result = std::make_shared<types::Type>();
                result->kind = types::SliceType{elem_type};
                return result;
            }
        }
        // Also try to look up field type from type checker environment
        if (obj_type && obj_type->is<types::NamedType>()) {
            const auto& named = obj_type->as<types::NamedType>();
            auto struct_def = env_.lookup_struct(named.name);
            if (struct_def) {
                for (const auto& [fname, ftype] : struct_def->fields) {
                    if (fname == field.field) {
                        return ftype;
                    }
                }
            }
        }
    }
    // Handle closure expressions
    if (expr.is<parser::ClosureExpr>()) {
        const auto& closure = expr.as<parser::ClosureExpr>();

        // Build parameter types
        std::vector<types::TypePtr> param_types;
        for (const auto& [pattern, type_opt] : closure.params) {
            if (type_opt.has_value()) {
                // Use explicit type annotation
                param_types.push_back(resolve_parser_type_with_subs(**type_opt, {}));
            } else {
                // No type annotation - use I32 as default
                param_types.push_back(types::make_i32());
            }
        }

        // Determine return type
        types::TypePtr return_type;
        if (closure.return_type.has_value()) {
            // Use explicit return type
            return_type = resolve_parser_type_with_subs(**closure.return_type, {});
        } else {
            // Infer from body expression
            return_type = infer_expr_type(*closure.body);
        }

        // Create FuncType
        auto result = std::make_shared<types::Type>();
        result->kind = types::FuncType{std::move(param_types), return_type, false};
        return result;
    }
    // Handle ternary expressions (condition ? true_value : false_value): infer from true_value
    // branch
    if (expr.is<parser::TernaryExpr>()) {
        const auto& ternary = expr.as<parser::TernaryExpr>();
        return infer_expr_type(*ternary.true_value);
    }
    // Handle if expressions (if condition then expr else expr): infer from then branch
    if (expr.is<parser::IfExpr>()) {
        const auto& if_expr = expr.as<parser::IfExpr>();
        return infer_expr_type(*if_expr.then_branch);
    }
    // Handle when expressions: infer from first arm's body
    if (expr.is<parser::WhenExpr>()) {
        const auto& when = expr.as<parser::WhenExpr>();
        if (!when.arms.empty()) {
            return infer_expr_type(*when.arms[0].body);
        }
        return types::make_unit();
    }
    // Handle call expressions (including enum constructors like Just, Ok, Err)
    if (expr.is<parser::CallExpr>()) {
        const auto& call = expr.as<parser::CallExpr>();
        if (call.callee->is<parser::IdentExpr>()) {
            const auto& callee_ident = call.callee->as<parser::IdentExpr>();
            // Check if it's a generic enum constructor
            for (const auto& [enum_name, enum_decl] : pending_generic_enums_) {
                for (size_t var_idx = 0; var_idx < enum_decl->variants.size(); ++var_idx) {
                    const auto& variant = enum_decl->variants[var_idx];
                    if (variant.name == callee_ident.name) {
                        // Found enum constructor
                        // Build type args for each generic parameter
                        std::vector<types::TypePtr> type_args;

                        // For each generic parameter, check if this variant uses it
                        for (size_t g = 0; g < enum_decl->generics.size(); ++g) {
                            const std::string& generic_name = enum_decl->generics[g].name;
                            types::TypePtr inferred_type = nullptr;

                            // Check if variant's tuple_fields reference this generic
                            if (variant.tuple_fields.has_value()) {
                                for (size_t f = 0;
                                     f < variant.tuple_fields->size() && f < call.args.size();
                                     ++f) {
                                    const auto& field_type = (*variant.tuple_fields)[f];
                                    // Check if this field is the generic parameter
                                    if (field_type->is<parser::NamedType>()) {
                                        const auto& named = field_type->as<parser::NamedType>();
                                        if (!named.path.segments.empty() &&
                                            named.path.segments.back() == generic_name) {
                                            // This field uses the generic - infer from argument
                                            inferred_type = infer_expr_type(*call.args[f]);
                                            break;
                                        }
                                    }
                                }
                            }

                            // If we couldn't infer this generic, use Unit as placeholder
                            if (!inferred_type) {
                                inferred_type = types::make_unit();
                            }
                            type_args.push_back(inferred_type);
                        }

                        auto result = std::make_shared<types::Type>();
                        result->kind = types::NamedType{enum_name, "", std::move(type_args)};
                        return result;
                    }
                }
            }

            // Check if it's a regular function call - look up recorded return type
            auto ret_it = func_return_types_.find(callee_ident.name);
            if (ret_it != func_return_types_.end()) {
                return ret_it->second;
            }
        }
    }
    // Handle method call expressions (need to know return type of methods)
    if (expr.is<parser::MethodCallExpr>()) {
        const auto& call = expr.as<parser::MethodCallExpr>();

        // Check for static method calls on primitive types (e.g., I32::default())
        if (call.receiver->is<parser::IdentExpr>()) {
            const auto& type_name = call.receiver->as<parser::IdentExpr>().name;
            if (call.method == "default") {
                if (type_name == "I8")
                    return types::make_primitive(types::PrimitiveKind::I8);
                if (type_name == "I16")
                    return types::make_primitive(types::PrimitiveKind::I16);
                if (type_name == "I32")
                    return types::make_i32();
                if (type_name == "I64")
                    return types::make_i64();
                if (type_name == "I128")
                    return types::make_primitive(types::PrimitiveKind::I128);
                if (type_name == "U8")
                    return types::make_primitive(types::PrimitiveKind::U8);
                if (type_name == "U16")
                    return types::make_primitive(types::PrimitiveKind::U16);
                if (type_name == "U32")
                    return types::make_primitive(types::PrimitiveKind::U32);
                if (type_name == "U64")
                    return types::make_primitive(types::PrimitiveKind::U64);
                if (type_name == "U128")
                    return types::make_primitive(types::PrimitiveKind::U128);
                if (type_name == "F32")
                    return types::make_primitive(types::PrimitiveKind::F32);
                if (type_name == "F64")
                    return types::make_primitive(types::PrimitiveKind::F64);
                if (type_name == "Bool")
                    return types::make_bool();
                if (type_name == "Str")
                    return types::make_str();
            }
        }

        types::TypePtr receiver_type = infer_expr_type(*call.receiver);

        // Check for Ordering methods
        if (receiver_type && receiver_type->is<types::NamedType>()) {
            const auto& named = receiver_type->as<types::NamedType>();
            if (named.name == "Ordering") {
                // is_less, is_equal, is_greater return Bool
                if (call.method == "is_less" || call.method == "is_equal" ||
                    call.method == "is_greater") {
                    return types::make_bool();
                }
                // reverse, then_cmp return Ordering
                if (call.method == "reverse" || call.method == "then_cmp") {
                    auto result = std::make_shared<types::Type>();
                    result->kind = types::NamedType{"Ordering", "", {}};
                    return result;
                }
            }

            // Outcome[T, E] methods that return T
            if (named.name == "Outcome" && !named.type_args.empty()) {
                if (call.method == "unwrap" || call.method == "unwrap_or" ||
                    call.method == "unwrap_or_else" || call.method == "expect") {
                    return named.type_args[0]; // Return T
                }
                // is_ok, is_err return Bool
                if (call.method == "is_ok" || call.method == "is_err") {
                    return types::make_bool();
                }
            }

            // Maybe[T] methods that return T
            if (named.name == "Maybe" && !named.type_args.empty()) {
                if (call.method == "unwrap" || call.method == "unwrap_or" ||
                    call.method == "unwrap_or_else" || call.method == "expect") {
                    return named.type_args[0]; // Return T
                }
                // is_just, is_nothing return Bool
                if (call.method == "is_just" || call.method == "is_nothing") {
                    return types::make_bool();
                }
            }
        }

        // Check for primitive type methods
        if (receiver_type && receiver_type->is<types::PrimitiveType>()) {
            const auto& prim = receiver_type->as<types::PrimitiveType>();
            auto kind = prim.kind;

            bool is_numeric =
                (kind == types::PrimitiveKind::I8 || kind == types::PrimitiveKind::I16 ||
                 kind == types::PrimitiveKind::I32 || kind == types::PrimitiveKind::I64 ||
                 kind == types::PrimitiveKind::I128 || kind == types::PrimitiveKind::U8 ||
                 kind == types::PrimitiveKind::U16 || kind == types::PrimitiveKind::U32 ||
                 kind == types::PrimitiveKind::U64 || kind == types::PrimitiveKind::U128 ||
                 kind == types::PrimitiveKind::F32 || kind == types::PrimitiveKind::F64);

            // cmp returns Ordering
            if (is_numeric && call.method == "cmp") {
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"Ordering", "", {}};
                return result;
            }

            // max, min return the same type
            if (is_numeric && (call.method == "max" || call.method == "min")) {
                return receiver_type;
            }

            // Arithmetic methods return the same type
            if (is_numeric &&
                (call.method == "add" || call.method == "sub" || call.method == "mul" ||
                 call.method == "div" || call.method == "rem" || call.method == "neg")) {
                return receiver_type;
            }

            // negate returns Bool
            if (kind == types::PrimitiveKind::Bool && call.method == "negate") {
                return receiver_type;
            }

            // duplicate returns the same type (copy semantics)
            if (call.method == "duplicate") {
                return receiver_type;
            }

            // to_string returns Str (Display behavior)
            if (call.method == "to_string") {
                return types::make_str();
            }

            // hash returns I64
            if (call.method == "hash") {
                return types::make_i64();
            }

            // to_owned returns the same type (ToOwned behavior)
            if (call.method == "to_owned") {
                return receiver_type;
            }

            // borrow returns ref T (Borrow behavior)
            if (call.method == "borrow") {
                auto ref_type = std::make_shared<types::Type>();
                ref_type->kind = types::RefType{false, receiver_type};
                return ref_type;
            }

            // borrow_mut returns mut ref T (BorrowMut behavior)
            if (call.method == "borrow_mut") {
                auto ref_type = std::make_shared<types::Type>();
                ref_type->kind = types::RefType{true, receiver_type};
                return ref_type;
            }
        }

        // Check for array methods
        if (receiver_type && receiver_type->is<types::ArrayType>()) {
            const auto& arr_type = receiver_type->as<types::ArrayType>();
            types::TypePtr elem_type = arr_type.element;

            // len returns I64
            if (call.method == "len") {
                return types::make_i64();
            }

            // is_empty returns Bool
            if (call.method == "is_empty") {
                return types::make_bool();
            }

            // get, first, last return Maybe[ref T]
            if (call.method == "get" || call.method == "first" || call.method == "last") {
                auto ref_type = std::make_shared<types::Type>();
                ref_type->kind = types::RefType{false, elem_type};
                std::vector<types::TypePtr> type_args = {ref_type};
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"Maybe", "", std::move(type_args)};
                return result;
            }

            // map returns the same array type (simplified)
            if (call.method == "map") {
                return receiver_type;
            }

            // eq, ne return Bool
            if (call.method == "eq" || call.method == "ne") {
                return types::make_bool();
            }

            // cmp returns Ordering
            if (call.method == "cmp") {
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"Ordering", "", {}};
                return result;
            }

            // iter returns ArrayIter
            if (call.method == "iter" || call.method == "into_iter") {
                std::vector<types::TypePtr> type_args = {elem_type};
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"ArrayIter", "", std::move(type_args)};
                return result;
            }

            // duplicate returns same type
            if (call.method == "duplicate") {
                return receiver_type;
            }

            // to_string returns Str
            if (call.method == "to_string" || call.method == "debug_string") {
                return types::make_str();
            }
        }

        // Check for user-defined struct methods by looking up function signature
        if (receiver_type && receiver_type->is<types::NamedType>()) {
            const auto& named = receiver_type->as<types::NamedType>();
            std::string qualified_name = named.name + "::" + call.method;

            // Build type substitution map from receiver's type args
            std::unordered_map<std::string, types::TypePtr> type_subs;
            std::vector<std::string> type_param_names;
            if (!named.type_args.empty()) {
                // Look up the struct/impl to get generic parameter names
                auto impl_it = pending_generic_impls_.find(named.name);
                if (impl_it != pending_generic_impls_.end()) {
                    for (size_t i = 0;
                         i < impl_it->second->generics.size() && i < named.type_args.size(); ++i) {
                        type_subs[impl_it->second->generics[i].name] = named.type_args[i];
                        type_param_names.push_back(impl_it->second->generics[i].name);
                    }
                } else if (env_.module_registry()) {
                    // Check imported structs for type params
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto struct_it = mod.structs.find(named.name);
                        if (struct_it != mod.structs.end() &&
                            !struct_it->second.type_params.empty()) {
                            for (size_t i = 0; i < struct_it->second.type_params.size() &&
                                               i < named.type_args.size();
                                 ++i) {
                                type_subs[struct_it->second.type_params[i]] = named.type_args[i];
                                type_param_names.push_back(struct_it->second.type_params[i]);
                            }
                            break;
                        }
                    }
                }

                // Also add associated type mappings (e.g., I::Item -> I64)
                for (size_t i = 0; i < type_param_names.size() && i < named.type_args.size(); ++i) {
                    const auto& arg = named.type_args[i];
                    if (arg && arg->is<types::NamedType>()) {
                        const auto& arg_named = arg->as<types::NamedType>();
                        // Look up Item associated type for this concrete type
                        auto item_type = lookup_associated_type(arg_named.name, "Item");
                        if (item_type) {
                            // Map both "I::Item" and "Item" to the concrete type
                            std::string assoc_key = type_param_names[i] + "::Item";
                            type_subs[assoc_key] = item_type;
                            type_subs["Item"] = item_type;
                        }
                    }
                }
            }

            // Look up function in environment
            auto func_sig = env_.lookup_func(qualified_name);
            if (func_sig) {
                if (!type_subs.empty()) {
                    return types::substitute_type(func_sig->return_type, type_subs);
                }
                return func_sig->return_type;
            }

            // Also check imported modules if the receiver has a module path
            if (!named.module_path.empty()) {
                auto module = env_.get_module(named.module_path);
                if (module) {
                    auto func_it = module->functions.find(qualified_name);
                    if (func_it != module->functions.end()) {
                        if (!type_subs.empty()) {
                            return types::substitute_type(func_it->second.return_type, type_subs);
                        }
                        return func_it->second.return_type;
                    }
                }
            }

            // Check via imported symbol resolution
            auto imported_path = env_.resolve_imported_symbol(named.name);
            if (imported_path.has_value()) {
                std::string module_path;
                size_t pos = imported_path->rfind("::");
                if (pos != std::string::npos) {
                    module_path = imported_path->substr(0, pos);
                }

                auto module = env_.get_module(module_path);
                if (module) {
                    auto func_it = module->functions.find(qualified_name);
                    if (func_it != module->functions.end()) {
                        if (!type_subs.empty()) {
                            return types::substitute_type(func_it->second.return_type, type_subs);
                        }
                        return func_it->second.return_type;
                    }
                }
            }
        }

        // Default: try to return receiver type
        return receiver_type ? receiver_type : types::make_i32();
    }
    // Handle tuple expressions
    if (expr.is<parser::TupleExpr>()) {
        const auto& tuple = expr.as<parser::TupleExpr>();
        std::vector<types::TypePtr> element_types;
        for (const auto& elem : tuple.elements) {
            element_types.push_back(infer_expr_type(*elem));
        }
        return types::make_tuple(std::move(element_types));
    }
    // Handle array expressions [elem1, elem2, ...] or [expr; count]
    if (expr.is<parser::ArrayExpr>()) {
        const auto& arr = expr.as<parser::ArrayExpr>();

        if (std::holds_alternative<std::vector<parser::ExprPtr>>(arr.kind)) {
            const auto& elements = std::get<std::vector<parser::ExprPtr>>(arr.kind);
            if (elements.empty()) {
                // Empty array - use I32 as default element type
                auto result = std::make_shared<types::Type>();
                result->kind = types::ArrayType{types::make_i32(), 0};
                return result;
            }
            // Infer element type from first element
            types::TypePtr elem_type = infer_expr_type(*elements[0]);
            auto result = std::make_shared<types::Type>();
            result->kind = types::ArrayType{elem_type, elements.size()};
            return result;
        } else {
            // [expr; count] form
            const auto& pair = std::get<std::pair<parser::ExprPtr, parser::ExprPtr>>(arr.kind);
            types::TypePtr elem_type = infer_expr_type(*pair.first);

            // Get the count - must be a compile-time constant
            size_t count = 0;
            if (pair.second->is<parser::LiteralExpr>()) {
                const auto& lit = pair.second->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    const auto& val = lit.token.int_value();
                    count = static_cast<size_t>(val.value);
                }
            }

            auto result = std::make_shared<types::Type>();
            result->kind = types::ArrayType{elem_type, count};
            return result;
        }
    }
    // Handle index expressions arr[i]
    if (expr.is<parser::IndexExpr>()) {
        const auto& idx = expr.as<parser::IndexExpr>();
        types::TypePtr obj_type = infer_expr_type(*idx.object);

        // If the object is an array, return element type
        if (obj_type && obj_type->is<types::ArrayType>()) {
            return obj_type->as<types::ArrayType>().element;
        }

        // Default: assume I32 for list element
        return types::make_i32();
    }
    // Default: I32
    return types::make_i32();
}

} // namespace tml::codegen
