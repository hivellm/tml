TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Reflection Intrinsics (Extended)
//!
//! This file implements reflection intrinsics and OOP reflection intrinsics.
//! Extracted from intrinsics_extended.cpp for maintainability.
//!
//! ## Sections in this file
//!
//! - Reflection Intrinsics (field_count, variant_count, field_name, field_type_id,
//!                          type_name, field_offset)
//! - OOP Reflection Intrinsics (base_class, is_abstract, is_sealed, method_count,
//!                              method_name, is_virtual, is_override, is_static_method)

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

/// Handles reflection and OOP reflection intrinsics.
/// Called from try_gen_intrinsic_extended() after the math section.
auto LLVMIRGen::try_gen_intrinsic_extended_reflect(const std::string& intrinsic_name,
                                                   const std::string& fn_name,
                                                   const parser::CallExpr& call)
    -> std::optional<std::string> {

    (void)fn_name; // unused — present for consistency with other helper signatures

    // ============================================================================
    // Reflection Intrinsics
    // ============================================================================

    // field_count[T]() -> USize
    // Returns the number of fields in a struct type, 0 for primitives
    if (intrinsic_name == "field_count") {
        std::string type_name;

        // Extract type argument from PathExpr generics (e.g., field_count[MyStruct]())
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    if (resolved->is<types::NamedType>()) {
                        type_name = resolved->as<types::NamedType>().name;
                    }
                }
            }
        }

        // Look up struct fields
        size_t count = 0;
        if (!type_name.empty()) {
            auto it = struct_fields_.find(type_name);
            if (it != struct_fields_.end()) {
                count = it->second.size();
            }
        }

        last_expr_type_ = "i64";
        return std::to_string(count);
    }

    // variant_count[T]() -> USize
    // Returns the number of variants in an enum type, 0 for structs/primitives
    if (intrinsic_name == "variant_count") {
        std::string type_name;

        // Extract type argument from PathExpr generics
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    if (resolved->is<types::NamedType>()) {
                        type_name = resolved->as<types::NamedType>().name;
                    }
                }
            }
        }

        // Count variants by looking for enum_variants_ entries with this prefix
        size_t count = 0;
        if (!type_name.empty()) {
            std::string prefix = type_name + "::";
            for (const auto& [key, _] : enum_variants_) {
                if (key.starts_with(prefix)) {
                    count++;
                }
            }
        }

        last_expr_type_ = "i64";
        return std::to_string(count);
    }

    // field_name[T](index: USize) -> Str
    // Returns the name of the field at the given index as a string literal
    if (intrinsic_name == "field_name") {
        std::string type_name;

        // Extract type argument from PathExpr generics
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    if (resolved->is<types::NamedType>()) {
                        type_name = resolved->as<types::NamedType>().name;
                    }
                }
            }
        }

        // Get the index argument (must be a compile-time constant or comptime loop variable)
        size_t index = 0;
        bool has_index = false;
        if (!call.args.empty()) {
            if (call.args[0]->is<parser::LiteralExpr>()) {
                const auto& lit = call.args[0]->as<parser::LiteralExpr>();
                if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                    index = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
                    has_index = true;
                }
            } else if (call.args[0]->is<parser::IdentExpr>()) {
                // Check if this is the compile-time loop variable
                const auto& ident = call.args[0]->as<parser::IdentExpr>();
                if (!comptime_loop_var_.empty() && ident.name == comptime_loop_var_) {
                    index = static_cast<size_t>(comptime_loop_value_);
                    has_index = true;
                }
            }
        }

        // Look up the field name
        std::string field_name = "";
        if (!type_name.empty() && has_index) {
            auto it = struct_fields_.find(type_name);
            if (it != struct_fields_.end()) {
                if (index < it->second.size()) {
                    field_name = it->second[index].name;
                } else {
                    report_error("field_name: index " + std::to_string(index) +
                                     " is out of bounds for type '" + type_name + "' with " +
                                     std::to_string(it->second.size()) + " field(s)",
                                 call.span, "R001");
                }
            }
        }

        // Return as string literal
        std::string str_const = add_string_literal(field_name);
        last_expr_type_ = "ptr";
        return str_const;
    }

    // field_type_id[T](index: USize) -> U64
    // Returns the type ID of the field at the given index
    if (intrinsic_name == "field_type_id") {
        std::string type_name;

        // Extract type argument from PathExpr generics
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    if (resolved->is<types::NamedType>()) {
                        type_name = resolved->as<types::NamedType>().name;
                    }
                }
            }
        }

        // Get the index argument (must be a compile-time constant or comptime loop variable)
        size_t index = 0;
        bool has_index = false;
        if (!call.args.empty()) {
            if (call.args[0]->is<parser::LiteralExpr>()) {
                const auto& lit = call.args[0]->as<parser::LiteralExpr>();
                if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                    index = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
                    has_index = true;
                }
            } else if (call.args[0]->is<parser::IdentExpr>()) {
                // Check if this is the compile-time loop variable
                const auto& ident = call.args[0]->as<parser::IdentExpr>();
                if (!comptime_loop_var_.empty() && ident.name == comptime_loop_var_) {
                    index = static_cast<size_t>(comptime_loop_value_);
                    has_index = true;
                }
            }
        }

        // Look up the field's semantic type and compute its type ID
        uint64_t type_id = 0;
        if (!type_name.empty() && has_index) {
            auto it = struct_fields_.find(type_name);
            if (it != struct_fields_.end()) {
                if (index < it->second.size()) {
                    const auto& field = it->second[index];
                    if (field.semantic_type) {
                        // Compute FNV-1a hash of the mangled type name
                        std::string mangled = mangle_type(field.semantic_type);
                        type_id = 14695981039346656037ULL;
                        for (char c : mangled) {
                            type_id ^= static_cast<uint64_t>(c);
                            type_id *= 1099511628211ULL;
                        }
                    }
                } else {
                    report_error("field_type_id: index " + std::to_string(index) +
                                     " is out of bounds for type '" + type_name + "' with " +
                                     std::to_string(it->second.size()) + " field(s)",
                                 call.span, "R001");
                }
            }
        }

        last_expr_type_ = "i64";
        return std::to_string(type_id);
    }

    // type_name[T]() -> Str
    // Returns the name of the type as a string literal
    if (intrinsic_name == "type_name") {
        std::string type_name = "unknown";

        // Extract type argument from PathExpr generics (e.g., type_name[I32]())
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    // Get a human-readable type name
                    if (resolved->is<types::NamedType>()) {
                        const auto& named = resolved->as<types::NamedType>();
                        type_name = named.name;
                        // Include type arguments for generic types
                        if (!named.type_args.empty()) {
                            type_name += "[";
                            for (size_t i = 0; i < named.type_args.size(); ++i) {
                                if (i > 0)
                                    type_name += ", ";
                                type_name += types::type_to_string(named.type_args[i]);
                            }
                            type_name += "]";
                        }
                    } else if (resolved->is<types::PrimitiveType>()) {
                        const auto& prim = resolved->as<types::PrimitiveType>();
                        switch (prim.kind) {
                        case types::PrimitiveKind::I8:
                            type_name = "I8";
                            break;
                        case types::PrimitiveKind::I16:
                            type_name = "I16";
                            break;
                        case types::PrimitiveKind::I32:
                            type_name = "I32";
                            break;
                        case types::PrimitiveKind::I64:
                            type_name = "I64";
                            break;
                        case types::PrimitiveKind::I128:
                            type_name = "I128";
                            break;
                        case types::PrimitiveKind::U8:
                            type_name = "U8";
                            break;
                        case types::PrimitiveKind::U16:
                            type_name = "U16";
                            break;
                        case types::PrimitiveKind::U32:
                            type_name = "U32";
                            break;
                        case types::PrimitiveKind::U64:
                            type_name = "U64";
                            break;
                        case types::PrimitiveKind::U128:
                            type_name = "U128";
                            break;
                        case types::PrimitiveKind::F32:
                            type_name = "F32";
                            break;
                        case types::PrimitiveKind::F64:
                            type_name = "F64";
                            break;
                        case types::PrimitiveKind::Bool:
                            type_name = "Bool";
                            break;
                        case types::PrimitiveKind::Char:
                            type_name = "Char";
                            break;
                        case types::PrimitiveKind::Str:
                            type_name = "Str";
                            break;
                        case types::PrimitiveKind::Unit:
                            type_name = "Unit";
                            break;
                        default:
                            type_name = "unknown";
                            break;
                        }
                    } else if (resolved->is<types::PtrType>()) {
                        type_name =
                            "*" + types::type_to_string(resolved->as<types::PtrType>().inner);
                    } else if (resolved->is<types::RefType>()) {
                        const auto& ref = resolved->as<types::RefType>();
                        type_name =
                            (ref.is_mut ? "mut ref " : "ref ") + types::type_to_string(ref.inner);
                    }
                }
            }
        }

        // Return as string literal
        std::string str_const = add_string_literal(type_name);
        last_expr_type_ = "ptr";
        return str_const;
    }

    // field_offset[T](index: USize) -> USize
    // Returns the byte offset of the field at the given index
    if (intrinsic_name == "field_offset") {
        std::string type_name;
        std::string llvm_type;

        // Extract type argument from PathExpr generics
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    llvm_type = llvm_type_from_semantic(resolved);
                    if (resolved->is<types::NamedType>()) {
                        type_name = resolved->as<types::NamedType>().name;
                    }
                }
            }
        }

        // Get the index argument (must be a compile-time constant or comptime loop variable)
        size_t index = 0;
        if (!call.args.empty()) {
            if (call.args[0]->is<parser::LiteralExpr>()) {
                const auto& lit = call.args[0]->as<parser::LiteralExpr>();
                if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                    index = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
                }
            } else if (call.args[0]->is<parser::IdentExpr>()) {
                // Check if this is the compile-time loop variable
                const auto& ident = call.args[0]->as<parser::IdentExpr>();
                if (!comptime_loop_var_.empty() && ident.name == comptime_loop_var_) {
                    index = static_cast<size_t>(comptime_loop_value_);
                }
            }
        }

        // Validate index at compile time
        if (!type_name.empty()) {
            auto it = struct_fields_.find(type_name);
            if (it != struct_fields_.end() && index >= it->second.size()) {
                report_error("field_offset: index " + std::to_string(index) +
                                 " is out of bounds for type '" + type_name + "' with " +
                                 std::to_string(it->second.size()) + " field(s)",
                             call.span, "R001");
            }
        }

        // Use GEP trick to compute field offset at compile time
        if (!llvm_type.empty() &&
            (llvm_type.starts_with("%struct.") || llvm_type.starts_with("%class."))) {
            std::string offset_ptr = fresh_reg();
            std::string offset_val = fresh_reg();
            emit_line("  " + offset_ptr + " = getelementptr inbounds " + llvm_type +
                      ", ptr null, i32 0, i32 " + std::to_string(index));
            emit_line("  " + offset_val + " = ptrtoint ptr " + offset_ptr + " to i64");
            last_expr_type_ = "i64";
            return offset_val;
        }

        last_expr_type_ = "i64";
        return "0";
    }

    // ========================================================================
    // OOP Reflection Intrinsics
    // ========================================================================

    // Helper: extract type name from generic parameter [T]
    auto extract_class_type_name = [&]() -> std::string {
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    if (resolved->is<types::NamedType>()) {
                        return resolved->as<types::NamedType>().name;
                    }
                }
            }
        }
        return "";
    };

    // base_class[T]() -> Str
    // Returns the base class name, or "" if no base class
    if (intrinsic_name == "base_class") {
        std::string type_name = extract_class_type_name();
        std::string base;
        if (!type_name.empty()) {
            auto class_def = env_.lookup_class(type_name);
            if (class_def.has_value() && class_def->base_class.has_value()) {
                base = *class_def->base_class;
            }
        }
        if (base.empty()) {
            last_expr_type_ = "ptr";
            return "null";
        }
        // Emit string constant for the base class name
        std::string str_const = add_string_literal(base);
        last_expr_type_ = "ptr";
        return str_const;
    }

    // is_abstract[T]() -> Bool
    if (intrinsic_name == "is_abstract") {
        std::string type_name = extract_class_type_name();
        bool result = false;
        if (!type_name.empty()) {
            auto class_def = env_.lookup_class(type_name);
            if (class_def.has_value()) {
                result = class_def->is_abstract;
            }
        }
        last_expr_type_ = "i1";
        return result ? "true" : "false";
    }

    // is_sealed[T]() -> Bool
    if (intrinsic_name == "is_sealed") {
        std::string type_name = extract_class_type_name();
        bool result = false;
        if (!type_name.empty()) {
            auto class_def = env_.lookup_class(type_name);
            if (class_def.has_value()) {
                result = class_def->is_sealed;
            }
        }
        last_expr_type_ = "i1";
        return result ? "true" : "false";
    }

    // method_count[T]() -> I64
    // Returns the number of methods (including inherited) in a class
    if (intrinsic_name == "method_count") {
        std::string type_name = extract_class_type_name();
        size_t count = 0;
        if (!type_name.empty()) {
            auto class_def = env_.lookup_class(type_name);
            if (class_def.has_value()) {
                count = class_def->methods.size();
            }
        }
        last_expr_type_ = "i64";
        return std::to_string(count);
    }

    // method_name[T](index: I64) -> Str
    // Returns the name of the method at the given index
    if (intrinsic_name == "method_name") {
        std::string type_name = extract_class_type_name();
        size_t index = 0;
        if (!call.args.empty() && call.args[0]->is<parser::LiteralExpr>()) {
            const auto& lit = call.args[0]->as<parser::LiteralExpr>();
            if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                index = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
            }
        }
        std::string name;
        if (!type_name.empty()) {
            auto class_def = env_.lookup_class(type_name);
            if (class_def.has_value() && index < class_def->methods.size()) {
                name = class_def->methods[index].sig.name;
            }
        }
        if (name.empty()) {
            last_expr_type_ = "ptr";
            return "null";
        }
        std::string str_const = add_string_literal(name);
        last_expr_type_ = "ptr";
        return str_const;
    }

    // is_virtual[T](index: I64) -> Bool
    if (intrinsic_name == "is_virtual") {
        std::string type_name = extract_class_type_name();
        size_t index = 0;
        if (!call.args.empty() && call.args[0]->is<parser::LiteralExpr>()) {
            const auto& lit = call.args[0]->as<parser::LiteralExpr>();
            if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                index = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
            }
        }
        bool result = false;
        if (!type_name.empty()) {
            auto class_def = env_.lookup_class(type_name);
            if (class_def.has_value() && index < class_def->methods.size()) {
                result = class_def->methods[index].is_virtual;
            }
        }
        last_expr_type_ = "i1";
        return result ? "true" : "false";
    }

    // is_override[T](index: I64) -> Bool
    if (intrinsic_name == "is_override") {
        std::string type_name = extract_class_type_name();
        size_t index = 0;
        if (!call.args.empty() && call.args[0]->is<parser::LiteralExpr>()) {
            const auto& lit = call.args[0]->as<parser::LiteralExpr>();
            if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                index = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
            }
        }
        bool result = false;
        if (!type_name.empty()) {
            auto class_def = env_.lookup_class(type_name);
            if (class_def.has_value() && index < class_def->methods.size()) {
                result = class_def->methods[index].is_override;
            }
        }
        last_expr_type_ = "i1";
        return result ? "true" : "false";
    }

    // is_static_method[T](index: I64) -> Bool
    if (intrinsic_name == "is_static_method") {
        std::string type_name = extract_class_type_name();
        size_t index = 0;
        if (!call.args.empty() && call.args[0]->is<parser::LiteralExpr>()) {
            const auto& lit = call.args[0]->as<parser::LiteralExpr>();
            if (std::holds_alternative<lexer::IntValue>(lit.token.value)) {
                index = static_cast<size_t>(std::get<lexer::IntValue>(lit.token.value).value);
            }
        }
        bool result = false;
        if (!type_name.empty()) {
            auto class_def = env_.lookup_class(type_name);
            if (class_def.has_value() && index < class_def->methods.size()) {
                result = class_def->methods[index].is_static;
            }
        }
        last_expr_type_ = "i1";
        return result ? "true" : "false";
    }

    return std::nullopt;
}

} // namespace tml::codegen
