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
#include <unordered_set>

namespace tml::codegen {

// Helper: Parse a mangled type string back into a semantic type
// e.g., "ptr_ChannelNode__I32" -> PtrType{inner=NamedType{name="ChannelNode", type_args=[I32]}}
static types::TypePtr parse_mangled_type_string(const std::string& s) {
    // Handle primitive types
    if (s == "I8")
        return types::make_primitive(types::PrimitiveKind::I8);
    if (s == "I16")
        return types::make_primitive(types::PrimitiveKind::I16);
    if (s == "I32")
        return types::make_i32();
    if (s == "I64")
        return types::make_i64();
    if (s == "I128")
        return types::make_primitive(types::PrimitiveKind::I128);
    if (s == "U8")
        return types::make_primitive(types::PrimitiveKind::U8);
    if (s == "U16")
        return types::make_primitive(types::PrimitiveKind::U16);
    if (s == "U32")
        return types::make_primitive(types::PrimitiveKind::U32);
    if (s == "U64")
        return types::make_primitive(types::PrimitiveKind::U64);
    if (s == "U128")
        return types::make_primitive(types::PrimitiveKind::U128);
    if (s == "F32")
        return types::make_primitive(types::PrimitiveKind::F32);
    if (s == "F64")
        return types::make_f64();
    if (s == "Bool")
        return types::make_bool();
    if (s == "Str")
        return types::make_str();
    if (s == "Unit")
        return types::make_unit();

    // Check for pointer prefix (e.g., ptr_ChannelNode__I32 -> Ptr[ChannelNode[I32]])
    if (s.size() > 4 && s.substr(0, 4) == "ptr_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = false, .inner = inner};
            return t;
        }
    }

    // Check for mutable pointer prefix
    if (s.size() > 7 && s.substr(0, 7) == "mutptr_") {
        std::string inner_str = s.substr(7);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = true, .inner = inner};
            return t;
        }
    }

    // Check for ref prefix
    if (s.size() > 4 && s.substr(0, 4) == "ref_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::RefType{.is_mut = false, .inner = inner};
            return t;
        }
    }

    // Check for mutable ref prefix
    if (s.size() > 7 && s.substr(0, 7) == "mutref_") {
        std::string inner_str = s.substr(7);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::RefType{.is_mut = true, .inner = inner};
            return t;
        }
    }

    // Check for nested generic (e.g., Mutex__I32, ChannelNode__I32)
    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);

        // Parse all type arguments (separated by __)
        std::vector<types::TypePtr> type_args;
        size_t pos = 0;
        while (pos < arg_str.size()) {
            // Find next __ delimiter
            auto next_delim = arg_str.find("__", pos);
            std::string arg_part;
            if (next_delim == std::string::npos) {
                arg_part = arg_str.substr(pos);
                pos = arg_str.size();
            } else {
                arg_part = arg_str.substr(pos, next_delim - pos);
                pos = next_delim + 2;
            }

            auto arg_type = parse_mangled_type_string(arg_part);
            if (arg_type) {
                type_args.push_back(arg_type);
            } else {
                // Fallback: create NamedType
                auto t = std::make_shared<types::Type>();
                t->kind = types::NamedType{arg_part, "", {}};
                type_args.push_back(t);
            }
        }

        auto t = std::make_shared<types::Type>();
        t->kind = types::NamedType{base, "", std::move(type_args)};
        return t;
    }

    // Simple struct type (no generics, no prefix)
    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

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

        // First check if there's a semantic type in locals (works for both 'this' and other vars)
        auto local_it = locals_.find(ident.name);
        if (local_it != locals_.end() && local_it->second.semantic_type) {
            TML_DEBUG_LN("[INFER] IdentExpr '"
                         << ident.name << "' found in locals, semantic_type="
                         << types::type_to_string(local_it->second.semantic_type));
            if (local_it->second.semantic_type->is<types::NamedType>()) {
                const auto& nt = local_it->second.semantic_type->as<types::NamedType>();
                TML_DEBUG_LN("[INFER]   NamedType: name=" << nt.name << " type_args.size="
                                                          << nt.type_args.size());
            }
            return local_it->second.semantic_type;
        }

        // Special handling for 'this' in impl methods when no semantic type is available
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
                                    // Named type (struct) - parse mangled name properly
                                    elem_type = parse_mangled_type_string(elem_name);
                                }
                                elements.push_back(elem_type);

                                if (next_underscore == std::string::npos)
                                    break;
                                tuple_pos = next_underscore + 1;
                            }
                            arg_type = types::make_tuple(std::move(elements));
                        } else {
                            // Named type without generics - parse mangled name properly
                            arg_type = parse_mangled_type_string(arg);
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

                // Non-generic struct type - parse mangled name properly
                return parse_mangled_type_string(mangled);
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
        auto operand_type = infer_expr_type(*unary.operand);

        // For dereference operations, unwrap the pointer/reference type
        if (unary.op == parser::UnaryOp::Deref && operand_type) {
            TML_DEBUG_LN(
                "[INFER] UnaryExpr Deref, operand_type=" << types::type_to_string(operand_type));
            types::TypePtr inner_type;
            if (operand_type->is<types::PtrType>()) {
                inner_type = operand_type->as<types::PtrType>().inner;
                TML_DEBUG_LN("[INFER]   PtrType inner="
                             << (inner_type ? types::type_to_string(inner_type) : "null"));
            } else if (operand_type->is<types::RefType>()) {
                inner_type = operand_type->as<types::RefType>().inner;
                TML_DEBUG_LN("[INFER]   RefType inner="
                             << (inner_type ? types::type_to_string(inner_type) : "null"));
            } else if (operand_type->is<types::NamedType>()) {
                // Handle TML's Ptr[T], RawPtr[T], and smart pointer types
                const auto& named = operand_type->as<types::NamedType>();
                if (!named.type_args.empty()) {
                    // Check for Ptr/RawPtr
                    if (named.name == "Ptr" || named.name == "RawPtr") {
                        inner_type = named.type_args[0];
                        TML_DEBUG_LN("[INFER]   NamedType Ptr inner="
                                     << (inner_type ? types::type_to_string(inner_type) : "null"));
                    }
                    // Check for smart pointer types that implement Deref
                    // These return their inner type T when dereferenced
                    static const std::unordered_set<std::string> deref_types = {
                        "Arc",
                        "Box",
                        "Heap",
                        "Rc",
                        "Shared",
                        "Weak",
                        "MutexGuard",
                        "RwLockReadGuard",
                        "RwLockWriteGuard",
                        "Ref",
                        "RefMut",
                    };
                    if (deref_types.count(named.name) > 0) {
                        inner_type = named.type_args[0];
                        TML_DEBUG_LN("[INFER]   NamedType "
                                     << named.name << " deref inner="
                                     << (inner_type ? types::type_to_string(inner_type) : "null"));
                    }
                }
            }

            // Apply type substitutions for generic types inside the pointer
            // E.g., Ptr[Node[T]] with T -> I32 becomes Node[I32]
            if (inner_type && !current_type_subs_.empty()) {
                inner_type = apply_type_substitutions(inner_type, current_type_subs_);
                TML_DEBUG_LN("[INFER]   After substitution=" << types::type_to_string(inner_type));
            }

            if (inner_type) {
                return inner_type;
            }
        }

        return operand_type;
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
    // Handle path expressions (enum variants like Ordering::Less or class static fields)
    if (expr.is<parser::PathExpr>()) {
        const auto& path = expr.as<parser::PathExpr>();
        if (path.path.segments.size() >= 2) {
            std::string type_name = path.path.segments[0];
            std::string member_name = path.path.segments[1];

            // Check for class static field access
            auto class_def = env_.lookup_class(type_name);
            if (class_def.has_value()) {
                for (const auto& field : class_def->fields) {
                    if (field.name == member_name && field.is_static) {
                        return field.type;
                    }
                }
            }

            // Otherwise assume enum type
            auto result = std::make_shared<types::Type>();
            result->kind = types::NamedType{type_name, "", {}};
            return result;
        }
    }
    // Handle field access expressions
    if (expr.is<parser::FieldExpr>()) {
        const auto& field = expr.as<parser::FieldExpr>();
        // Get the type of the object
        types::TypePtr obj_type = infer_expr_type(*field.object);

        // Handle tuple element access (tuple.0, tuple.1, etc.)
        if (obj_type && obj_type->is<types::TupleType>()) {
            const auto& tuple_type = obj_type->as<types::TupleType>();
            // Check if field name is a numeric index
            if (!field.field.empty() && std::isdigit(field.field[0])) {
                try {
                    size_t index = std::stoul(field.field);
                    if (index < tuple_type.elements.size()) {
                        return tuple_type.elements[index];
                    }
                } catch (...) {
                    // Not a valid index, fall through to other handling
                }
            }
        }

        if (obj_type && obj_type->is<types::NamedType>()) {
            const auto& named = obj_type->as<types::NamedType>();

            // First, try to look up the struct definition to get the semantic field type
            // This preserves complex types like Maybe[ref dyn Error]
            auto struct_def = env_.lookup_struct(named.name);

            // If not found locally, search all modules in the registry
            if (!struct_def && env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    auto mod_struct_it = mod.structs.find(named.name);
                    if (mod_struct_it != mod.structs.end()) {
                        struct_def = mod_struct_it->second;
                        break;
                    }
                }
            }

            // Also try searching by full module path prefix
            if (!struct_def && env_.module_registry()) {
                // Structs might be registered with module prefix (e.g., "core::error::ErrorChain")
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    // Try: mod_name::named.name (e.g., "core::error::ErrorChain")
                    std::string full_name = named.name;
                    auto mod_struct_it = mod.structs.find(full_name);
                    if (mod_struct_it == mod.structs.end()) {
                        // Try without module prefix - struct might be stored with just name
                        for (const auto& [struct_name, sdef] : mod.structs) {
                            // Check if struct_name ends with "::named.name" or equals named.name
                            if (struct_name == named.name ||
                                (struct_name.size() > named.name.size() + 2 &&
                                 struct_name.substr(struct_name.size() - named.name.size()) ==
                                     named.name &&
                                 struct_name[struct_name.size() - named.name.size() - 2] == ':')) {
                                struct_def = sdef;
                                break;
                            }
                        }
                    } else {
                        struct_def = mod_struct_it->second;
                    }
                    if (struct_def)
                        break;
                }
            }

            // Also check pending_generic_structs_ for internal generic structs (like Node[T] in
            // queue.tml)
            if (!struct_def) {
                auto pending_it = pending_generic_structs_.find(named.name);
                if (pending_it != pending_generic_structs_.end()) {
                    const parser::StructDecl* decl = pending_it->second;
                    // Build a struct definition from the parser StructDecl
                    types::StructDef temp_struct_def;
                    temp_struct_def.name = decl->name;
                    for (const auto& gp : decl->generics) {
                        temp_struct_def.type_params.push_back(gp.name);
                    }
                    for (const auto& field_decl : decl->fields) {
                        // Resolve the field type - use empty subs since we'll apply them later
                        auto field_type = resolve_parser_type_with_subs(*field_decl.type, {});
                        temp_struct_def.fields.push_back({field_decl.name, field_type});
                    }
                    struct_def = temp_struct_def;
                }
            }

            if (struct_def) {
                TML_DEBUG_LN("[INFER] struct_def found, searching for field: " << field.field);
                for (const auto& [field_name, field_type] : struct_def->fields) {
                    TML_DEBUG_LN("[INFER]   field: "
                                 << field_name << " type: "
                                 << (field_type ? types::type_to_string(field_type) : "null"));
                    if (field_name == field.field && field_type) {
                        TML_DEBUG_LN(
                            "[INFER] Returning field type: " << types::type_to_string(field_type));
                        TML_DEBUG_LN("[INFER] named.type_args.size="
                                     << named.type_args.size() << " struct_def->type_params.size="
                                     << struct_def->type_params.size());
                        // If the struct is generic, substitute type arguments
                        if (!named.type_args.empty() && !struct_def->type_params.empty()) {
                            std::unordered_map<std::string, types::TypePtr> subs;
                            for (size_t i = 0;
                                 i < struct_def->type_params.size() && i < named.type_args.size();
                                 ++i) {
                                subs[struct_def->type_params[i]] = named.type_args[i];
                                TML_DEBUG_LN("[INFER] Substituting "
                                             << struct_def->type_params[i] << " -> "
                                             << types::type_to_string(named.type_args[i]));
                            }
                            auto substituted = types::substitute_type(field_type, subs);
                            TML_DEBUG_LN("[INFER] After substitution: "
                                         << types::type_to_string(substituted));
                            return substituted;
                        }
                        return field_type;
                    }
                }

                // Field not found directly on struct - try auto-deref coercion
                // For smart pointers like Arc[T], access fields on the inner T
                types::TypePtr deref_target = get_deref_target_type(obj_type);
                if (deref_target) {
                    TML_DEBUG_LN("[INFER] Trying auto-deref: "
                                 << named.name << " -> " << types::type_to_string(deref_target));
                    // Create a synthetic FieldExpr on the deref'd type
                    // and recursively infer its type
                    if (deref_target->is<types::NamedType>()) {
                        const auto& inner_named = deref_target->as<types::NamedType>();
                        // Look up the inner struct definition
                        auto inner_struct_def = env_.lookup_struct(inner_named.name);
                        if (!inner_struct_def && env_.module_registry()) {
                            const auto& all_modules = env_.module_registry()->get_all_modules();
                            for (const auto& [mod_name, mod] : all_modules) {
                                auto mod_struct_it = mod.structs.find(inner_named.name);
                                if (mod_struct_it != mod.structs.end()) {
                                    inner_struct_def = mod_struct_it->second;
                                    break;
                                }
                            }
                        }
                        // Also check pending_generic_structs_
                        if (!inner_struct_def) {
                            auto pending_it = pending_generic_structs_.find(inner_named.name);
                            if (pending_it != pending_generic_structs_.end()) {
                                const parser::StructDecl* decl = pending_it->second;
                                types::StructDef temp_struct_def;
                                temp_struct_def.name = decl->name;
                                for (const auto& gp : decl->generics) {
                                    temp_struct_def.type_params.push_back(gp.name);
                                }
                                for (const auto& field_decl : decl->fields) {
                                    auto ft = resolve_parser_type_with_subs(*field_decl.type, {});
                                    temp_struct_def.fields.push_back({field_decl.name, ft});
                                }
                                inner_struct_def = temp_struct_def;
                            }
                        }
                        if (inner_struct_def) {
                            TML_DEBUG_LN("[INFER] Found inner struct: " << inner_named.name);
                            for (const auto& [fname, ftype] : inner_struct_def->fields) {
                                if (fname == field.field && ftype) {
                                    TML_DEBUG_LN("[INFER] Found field via auto-deref: " << fname);
                                    // Apply type substitutions from the inner type
                                    if (!inner_named.type_args.empty() &&
                                        !inner_struct_def->type_params.empty()) {
                                        std::unordered_map<std::string, types::TypePtr> subs;
                                        for (size_t i = 0;
                                             i < inner_struct_def->type_params.size() &&
                                             i < inner_named.type_args.size();
                                             ++i) {
                                            subs[inner_struct_def->type_params[i]] =
                                                inner_named.type_args[i];
                                        }
                                        return types::substitute_type(ftype, subs);
                                    }
                                    return ftype;
                                }
                            }
                        }
                    }
                }
            } else {
                TML_DEBUG_LN("[INFER] struct_def NOT found for: " << named.name);
            }

            // Fallback: look up field type in struct_fields_ registry
            // For generic types, use the mangled name (e.g., Take__RangeIterI64)
            std::string lookup_name = named.name;
            if (!named.type_args.empty()) {
                lookup_name = mangle_struct_name(named.name, named.type_args);
            }

            // First try to get the semantic type directly (preserves full type info)
            types::TypePtr field_sem_type = get_field_semantic_type(lookup_name, field.field);
            if (field_sem_type) {
                TML_DEBUG_LN("[INFER] Got semantic type from registry: "
                             << types::type_to_string(field_sem_type));
                return field_sem_type;
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
                return types::make_str(); // Legacy fallback for unknown ptr types
            if (field_llvm_type.starts_with("%struct.")) {
                std::string mangled = field_llvm_type.substr(8);

                // Check if this is a generic type (contains __ separator)
                auto sep_pos = mangled.find("__");
                if (sep_pos != std::string::npos) {
                    // Parse mangled name: Maybe__Str -> Maybe[Str]
                    std::string base_name = mangled.substr(0, sep_pos);
                    std::string type_args_str = mangled.substr(sep_pos + 2);

                    // Look up the struct to determine how many type parameters it has
                    // This is crucial for handling nested generics like AtomicPtr[Node[I32]]
                    // which is mangled as AtomicPtr__Node__I32
                    size_t expected_type_params = 0;
                    if (env_.module_registry()) {
                        const auto& all_modules = env_.module_registry()->get_all_modules();
                        for (const auto& [mod_name, mod] : all_modules) {
                            auto struct_it = mod.structs.find(base_name);
                            if (struct_it != mod.structs.end()) {
                                expected_type_params = struct_it->second.type_params.size();
                                break;
                            }
                        }
                    }
                    // Also check pending_generic_structs_
                    if (expected_type_params == 0) {
                        auto pgs_it = pending_generic_structs_.find(base_name);
                        if (pgs_it != pending_generic_structs_.end()) {
                            expected_type_params = pgs_it->second->generics.size();
                        }
                    }

                    // Helper function to parse a single mangled type argument (non-recursive)
                    auto parse_simple_type_arg = [](const std::string& arg) -> types::TypePtr {
                        if (arg == "I32")
                            return types::make_i32();
                        if (arg == "I64")
                            return types::make_i64();
                        if (arg == "Bool")
                            return types::make_bool();
                        if (arg == "Str")
                            return types::make_str();
                        if (arg == "F32")
                            return types::make_primitive(types::PrimitiveKind::F32);
                        if (arg == "F64")
                            return types::make_f64();
                        if (arg == "Unit")
                            return types::make_unit();
                        if (arg.starts_with("dyn_")) {
                            std::string behavior = arg.substr(4);
                            auto dyn_t = std::make_shared<types::Type>();
                            dyn_t->kind = types::DynBehaviorType{behavior, {}, false};
                            return dyn_t;
                        }
                        // Default: Named type without generics
                        auto t = std::make_shared<types::Type>();
                        t->kind = types::NamedType{arg, "", {}};
                        return t;
                    };

                    // Helper to parse potentially nested types
                    auto parse_type_arg =
                        [&parse_simple_type_arg](const std::string& arg) -> types::TypePtr {
                        if (arg.starts_with("ref_") || arg.starts_with("mutref_")) {
                            // Reference type: ref_X or mutref_X -> RefType
                            bool is_mut = arg.starts_with("mutref_");
                            std::string inner_name = is_mut ? arg.substr(7) : arg.substr(4);
                            types::TypePtr inner_type;
                            if (inner_name.starts_with("dyn_")) {
                                std::string behavior = inner_name.substr(4);
                                auto dyn_t = std::make_shared<types::Type>();
                                dyn_t->kind = types::DynBehaviorType{behavior, {}, false};
                                inner_type = dyn_t;
                            } else {
                                auto inner_t = std::make_shared<types::Type>();
                                inner_t->kind = types::NamedType{inner_name, "", {}};
                                inner_type = inner_t;
                            }
                            auto ref_t = std::make_shared<types::Type>();
                            ref_t->kind = types::RefType{
                                .is_mut = is_mut, .inner = inner_type, .lifetime = std::nullopt};
                            return ref_t;
                        }
                        if (arg.starts_with("ptr_")) {
                            // Pointer type: ptr_X -> PtrType
                            std::string inner_name = arg.substr(4);
                            types::TypePtr inner_type;
                            // Check if inner is a nested generic like Node__I32
                            auto inner_sep = inner_name.find("__");
                            if (inner_sep != std::string::npos) {
                                std::string inner_base = inner_name.substr(0, inner_sep);
                                std::string inner_args = inner_name.substr(inner_sep + 2);
                                auto inner_arg_type = parse_simple_type_arg(inner_args);
                                auto inner_t = std::make_shared<types::Type>();
                                inner_t->kind = types::NamedType{inner_base, "", {inner_arg_type}};
                                inner_type = inner_t;
                            } else {
                                inner_type = parse_simple_type_arg(inner_name);
                            }
                            auto ptr_t = std::make_shared<types::Type>();
                            ptr_t->kind = types::PtrType{false, inner_type};
                            return ptr_t;
                        }
                        return parse_simple_type_arg(arg);
                    };

                    // Parse type args respecting the expected number of type parameters
                    // For structs with 1 type param (like AtomicPtr[T]), combine all remaining
                    // args into a single nested type: AtomicPtr__Node__I32 -> AtomicPtr[Node[I32]]
                    std::vector<types::TypePtr> type_args;
                    if (expected_type_params == 1) {
                        // Single type param - treat all of type_args_str as a single nested type
                        // Check if it's a nested generic: Node__I32 -> Node[I32]
                        auto nested_sep = type_args_str.find("__");
                        if (nested_sep != std::string::npos) {
                            std::string nested_base = type_args_str.substr(0, nested_sep);
                            std::string nested_args = type_args_str.substr(nested_sep + 2);
                            auto nested_arg_type = parse_type_arg(nested_args);
                            auto t = std::make_shared<types::Type>();
                            t->kind = types::NamedType{nested_base, "", {nested_arg_type}};
                            type_args.push_back(t);
                        } else {
                            type_args.push_back(parse_type_arg(type_args_str));
                        }
                    } else {
                        // Multiple type params - split by __ as before
                        size_t pos = 0;
                        while (pos < type_args_str.size()) {
                            auto next_sep = type_args_str.find("__", pos);
                            std::string arg = (next_sep == std::string::npos)
                                                  ? type_args_str.substr(pos)
                                                  : type_args_str.substr(pos, next_sep - pos);
                            type_args.push_back(parse_type_arg(arg));
                            if (next_sep == std::string::npos)
                                break;
                            pos = next_sep + 2;
                        }
                    }

                    auto result = std::make_shared<types::Type>();
                    result->kind = types::NamedType{base_name, "", std::move(type_args)};
                    return result;
                }

                // Non-generic struct type - parse mangled name properly
                return parse_mangled_type_string(mangled);
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
        // Also check for class types
        if (obj_type && obj_type->is<types::ClassType>()) {
            const auto& class_type = obj_type->as<types::ClassType>();
            // Search in class hierarchy
            std::string current_class = class_type.name;
            while (!current_class.empty()) {
                auto class_def = env_.lookup_class(current_class);
                if (!class_def.has_value())
                    break;
                for (const auto& f : class_def->fields) {
                    if (f.name == field.field) {
                        return f.type;
                    }
                }
                // Move to parent class
                current_class = class_def->base_class.value_or("");
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

        // Handle PathExpr callee (like Builder::create)
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path = call.callee->as<parser::PathExpr>();
            // For Type::method() syntax, segments are [Type, method]
            if (path.path.segments.size() == 2) {
                const std::string& type_name = path.path.segments[0];
                const std::string& method_name = path.path.segments[1];

                // Check if it's a class static method
                auto class_def = env_.lookup_class(type_name);
                if (class_def.has_value()) {
                    for (const auto& m : class_def->methods) {
                        if (m.sig.name == method_name && m.is_static) {
                            return m.sig.return_type;
                        }
                    }
                }
            }
        }

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

            // Fall back to looking up in TypeEnv (for library functions)
            auto func_sig = env_.lookup_func(callee_ident.name);
            if (func_sig.has_value()) {
                return func_sig->return_type;
            }

            // Also check in module registry for qualified/aliased functions
            if (env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    auto func_it = mod.functions.find(callee_ident.name);
                    if (func_it != mod.functions.end()) {
                        return func_it->second.return_type;
                    }
                }
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

            // Check for static method calls on user-defined types (e.g., Request::builder())
            // First check if this type_name is a known struct/type (not a local variable)
            if (locals_.find(type_name) == locals_.end()) {
                std::string qualified_name = type_name + "::" + call.method;

                // Look up the static method in the environment
                auto func_sig = env_.lookup_func(qualified_name);

                // If not found locally, search all modules
                if (!func_sig && env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto func_it = mod.functions.find(qualified_name);
                        if (func_it != mod.functions.end()) {
                            func_sig = func_it->second;
                            break;
                        }
                    }
                }

                if (func_sig && func_sig->return_type) {
                    return func_sig->return_type;
                }

                // Check if type_name is a class and look up static method
                auto class_def = env_.lookup_class(type_name);
                if (class_def.has_value()) {
                    for (const auto& m : class_def->methods) {
                        if (m.sig.name == call.method && m.is_static) {
                            return m.sig.return_type;
                        }
                    }
                }
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

        // Check for class type methods
        if (receiver_type && receiver_type->is<types::ClassType>()) {
            const auto& class_type = receiver_type->as<types::ClassType>();
            // Search for the method in class hierarchy
            std::string current_class = class_type.name;
            while (!current_class.empty()) {
                auto class_def = env_.lookup_class(current_class);
                if (!class_def.has_value())
                    break;
                for (const auto& m : class_def->methods) {
                    if (m.sig.name == call.method && !m.is_static) {
                        return m.sig.return_type;
                    }
                }
                // Move to parent class
                current_class = class_def->base_class.value_or("");
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

            // debug_string returns Str (Debug behavior)
            if (call.method == "debug_string") {
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
                ref_type->kind = types::RefType{
                    .is_mut = false, .inner = receiver_type, .lifetime = std::nullopt};
                return ref_type;
            }

            // borrow_mut returns mut ref T (BorrowMut behavior)
            if (call.method == "borrow_mut") {
                auto ref_type = std::make_shared<types::Type>();
                ref_type->kind = types::RefType{
                    .is_mut = true, .inner = receiver_type, .lifetime = std::nullopt};
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
                ref_type->kind =
                    types::RefType{.is_mut = false, .inner = elem_type, .lifetime = std::nullopt};
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

            // Check if named type is a class and look up instance methods
            auto class_def = env_.lookup_class(named.name);
            if (class_def.has_value()) {
                std::string current_class = named.name;
                while (!current_class.empty()) {
                    auto cls = env_.lookup_class(current_class);
                    if (!cls.has_value())
                        break;
                    for (const auto& m : cls->methods) {
                        if (m.sig.name == call.method && !m.is_static) {
                            return m.sig.return_type;
                        }
                    }
                    // Move to parent class
                    current_class = cls->base_class.value_or("");
                }
            }

            // Look up methods in pending_generic_impls_ for generic types
            auto impl_it = pending_generic_impls_.find(named.name);
            if (impl_it != pending_generic_impls_.end()) {
                for (const auto& method : impl_it->second->methods) {
                    if (method.name == call.method) {
                        if (method.return_type.has_value()) {
                            // Convert parser type to semantic type with substitution
                            types::TypePtr ret_type = resolve_parser_type_with_subs(
                                *method.return_type.value(), type_subs);
                            return ret_type;
                        }
                        return types::make_unit();
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
    // Handle index expressions arr[i] or tuple.0
    if (expr.is<parser::IndexExpr>()) {
        const auto& idx = expr.as<parser::IndexExpr>();
        types::TypePtr obj_type = infer_expr_type(*idx.object);

        // If the object is an array, return element type
        if (obj_type && obj_type->is<types::ArrayType>()) {
            return obj_type->as<types::ArrayType>().element;
        }

        // If the object is a tuple, return the element type at the index
        if (obj_type && obj_type->is<types::TupleType>()) {
            const auto& tuple_type = obj_type->as<types::TupleType>();
            // Get the index value (tuple indices are literals like .0, .1, etc.)
            if (idx.index && idx.index->is<parser::LiteralExpr>()) {
                const auto& lit = idx.index->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                    size_t index = static_cast<size_t>(lit.token.int_value().value);
                    if (index < tuple_type.elements.size()) {
                        return tuple_type.elements[index];
                    }
                }
            }
            // If we can't determine the index, return the first element type as fallback
            if (!tuple_type.elements.empty()) {
                return tuple_type.elements[0];
            }
        }

        // Default: assume I32 for list element
        return types::make_i32();
    }

    // Handle cast expressions (x as I64)
    if (expr.is<parser::CastExpr>()) {
        const auto& cast = expr.as<parser::CastExpr>();
        // The type of a cast expression is its target type
        if (cast.target && cast.target->is<parser::NamedType>()) {
            const auto& named = cast.target->as<parser::NamedType>();
            if (!named.path.segments.empty()) {
                const std::string& type_name = named.path.segments.back();
                // Handle primitive types
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
                    return types::make_f64();
                if (type_name == "Bool")
                    return types::make_bool();
                if (type_name == "Str")
                    return types::make_str();
                if (type_name == "Char")
                    return types::make_primitive(types::PrimitiveKind::Char);
                // For other named types (classes, etc.), return a NamedType
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{type_name, "", {}};
                return result;
            }
        }
        // For pointer types in casts
        if (cast.target && cast.target->is<parser::PtrType>()) {
            const auto& ptr = cast.target->as<parser::PtrType>();
            auto inner = std::make_shared<types::Type>();
            if (ptr.inner && ptr.inner->is<parser::NamedType>()) {
                const auto& inner_named = ptr.inner->as<parser::NamedType>();
                if (!inner_named.path.segments.empty()) {
                    inner->kind = types::NamedType{inner_named.path.segments.back(), "", {}};
                }
            } else {
                inner = types::make_unit();
            }
            return types::make_ptr(inner);
        }
    }

    // Default: I32
    return types::make_i32();
}

// =============================================================================
// Deref Coercion Helpers
// =============================================================================

auto LLVMIRGen::get_deref_target_type(const types::TypePtr& type) -> types::TypePtr {
    if (!type || !type->is<types::NamedType>()) {
        return nullptr;
    }

    const auto& named = type->as<types::NamedType>();

    // Known smart pointer types that implement Deref
    // Arc[T] -> T (via deref)
    // Box[T] -> T (via deref)
    // Heap[T] -> T (via deref) - TML's name for Box
    // Rc[T] -> T (via deref)
    // Shared[T] -> T (via deref) - TML's name for Rc
    // Ptr[T] -> T (via deref in lowlevel blocks)
    // MutexGuard[T] -> T (via deref)

    static const std::unordered_set<std::string> deref_types = {
        "Arc",
        "Box",
        "Heap",
        "Rc",
        "Shared",
        "Weak",
        "Ptr",
        "MutexGuard",
        "RwLockReadGuard",
        "RwLockWriteGuard",
        "Ref",
        "RefMut",
    };

    if (deref_types.count(named.name) && !named.type_args.empty()) {
        // For these types, Deref::Target is the first type argument
        return named.type_args[0];
    }

    return nullptr;
}

auto LLVMIRGen::struct_has_field(const std::string& struct_name, const std::string& field_name)
    -> bool {
    // Check dynamic struct_fields_ registry first
    auto it = struct_fields_.find(struct_name);
    if (it != struct_fields_.end()) {
        for (const auto& field : it->second) {
            if (field.name == field_name) {
                return true;
            }
        }
    }

    // Check type environment
    auto struct_def = env_.lookup_struct(struct_name);
    if (struct_def) {
        for (const auto& [fname, ftype] : struct_def->fields) {
            if (fname == field_name) {
                return true;
            }
        }
    }

    // Search in module registry
    if (env_.module_registry()) {
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            auto mod_struct_it = mod.structs.find(struct_name);
            if (mod_struct_it != mod.structs.end()) {
                for (const auto& [fname, ftype] : mod_struct_it->second.fields) {
                    if (fname == field_name) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

} // namespace tml::codegen
