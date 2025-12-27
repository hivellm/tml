// LLVM IR generator - Type inference
// Handles: infer_expr_type for generics instantiation

#include "tml/codegen/llvm_ir_gen.hpp"
#include "tml/types/module.hpp"

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
        default:
            return types::make_i32();
        }
    }
    if (expr.is<parser::IdentExpr>()) {
        const auto& ident = expr.as<parser::IdentExpr>();

        // Special handling for 'this' in impl methods
        if (ident.name == "this" && !current_impl_type_.empty()) {
            auto result = std::make_shared<types::Type>();
            result->kind = types::NamedType{current_impl_type_, "", {}};
            return result;
        }

        auto it = locals_.find(ident.name);
        if (it != locals_.end()) {
            // Use semantic type if available (for complex types like Ptr[T])
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
                        else {
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
    // Handle field access expressions
    if (expr.is<parser::FieldExpr>()) {
        const auto& field = expr.as<parser::FieldExpr>();
        // Get the type of the object
        types::TypePtr obj_type = infer_expr_type(*field.object);
        if (obj_type && obj_type->is<types::NamedType>()) {
            const auto& named = obj_type->as<types::NamedType>();
            // Look up field type in struct definition
            std::string field_llvm_type = get_field_type(named.name, field.field);
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
                std::string struct_name = field_llvm_type.substr(8);
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{struct_name, "", {}};
                return result;
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
        }
    }
    // Default: I32
    return types::make_i32();
}

} // namespace tml::codegen
