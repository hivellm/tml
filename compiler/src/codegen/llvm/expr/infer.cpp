TML_MODULE("codegen_x86")

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

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "types/module.hpp"

#include <functional>
#include <iostream>
#include <map>
#include <unordered_set>

namespace tml::codegen {

// Helper: Recursively search a parser field type for a generic parameter name,
// and extract the corresponding concrete type from the inferred argument type.
// For example, if field_type is Heap[UnaryTree[T]] and arg_type is
// Heap[UnaryTree[I32]], this finds T at generics[0].generics[0] and returns I32.
static types::TypePtr extract_generic_from_nested_type(const parser::TypePtr& field_type,
                                                       const std::string& generic_name,
                                                       const types::TypePtr& arg_type) {

    if (!field_type || !arg_type)
        return nullptr;

    if (field_type->is<parser::NamedType>()) {
        const auto& named = field_type->as<parser::NamedType>();

        // Direct match: this field type IS the generic parameter
        if (!named.path.segments.empty() && named.path.segments.back() == generic_name &&
            !named.generics.has_value()) {
            return arg_type;
        }

        // Recurse into generic arguments
        if (named.generics.has_value() && arg_type->is<types::NamedType>()) {
            const auto& arg_named = arg_type->as<types::NamedType>();
            const auto& gen_args = named.generics->args;
            for (size_t i = 0; i < gen_args.size() && i < arg_named.type_args.size(); ++i) {
                if (gen_args[i].is_type()) {
                    const auto& inner_field = std::get<parser::TypePtr>(gen_args[i].value);
                    auto result = extract_generic_from_nested_type(inner_field, generic_name,
                                                                   arg_named.type_args[i]);
                    if (result)
                        return result;
                }
            }
        }
    }

    return nullptr;
}

// Helper: Extract a generic parameter from a nested field type by looking at
// the actual argument expression. When infer_expr_type on the argument returns
// a type without type_args (e.g., Heap with no [T] resolved), this function
// unwraps constructor calls to look at inner arguments.
// For example: field_type=Heap[UnaryTree[T]], arg_expr=Heap::new(UnaryTree::Leaf(10))
// → unwraps Heap::new(...) to get UnaryTree::Leaf(10), matches field_type's
// generic arg UnaryTree[T] against inferred type UnaryTree[I32], extracts T=I32.
static types::TypePtr
extract_generic_from_call_expr(const parser::TypePtr& field_type, const std::string& generic_name,
                               const parser::Expr& arg_expr,
                               std::function<types::TypePtr(const parser::Expr&)> infer_fn) {

    if (!field_type)
        return nullptr;

    // Only handle NamedType with generics (like Heap[UnaryTree[T]])
    if (!field_type->is<parser::NamedType>())
        return nullptr;
    const auto& named = field_type->as<parser::NamedType>();
    if (!named.generics.has_value())
        return nullptr;

    // If the argument is a call expr like Heap::new(X), and the field type
    // outer name matches (Heap), then try to match field type's generic args
    // against the arguments of the call
    if (arg_expr.is<parser::CallExpr>()) {
        const auto& call = arg_expr.as<parser::CallExpr>();
        // Check if callee is Type::method (PathExpr with 2 segments)
        if (call.callee && call.callee->is<parser::PathExpr>()) {
            const auto& path = call.callee->as<parser::PathExpr>();
            if (path.path.segments.size() == 2) {
                const std::string& callee_type = path.path.segments[0];
                // Check if outer type matches field type's outer name
                if (!named.path.segments.empty() && named.path.segments.back() == callee_type) {
                    // Match: field is Heap[...] and arg is Heap::method(...)
                    // The constructor's first arg should correspond to the
                    // first generic parameter. Try matching field's generic args
                    // against the call's arguments.
                    const auto& gen_args = named.generics->args;
                    for (size_t i = 0; i < gen_args.size() && i < call.args.size(); ++i) {
                        if (gen_args[i].is_type()) {
                            const auto& inner_field = std::get<parser::TypePtr>(gen_args[i].value);
                            auto inner_inferred = infer_fn(*call.args[i]);
                            auto result = extract_generic_from_nested_type(
                                inner_field, generic_name, inner_inferred);
                            if (result)
                                return result;
                        }
                    }
                }
            }
        }
    }

    return nullptr;
}

// Member function: Extract a generic parameter from a field type pattern by
// matching against an inferred argument type. Combines nested type walking
// with constructor call unwrapping for robust generic inference.
types::TypePtr LLVMIRGen::extract_generic_from_type(const parser::TypePtr& field_type,
                                                    const std::string& generic_name,
                                                    const types::TypePtr& arg_type) {
    return extract_generic_from_nested_type(field_type, generic_name, arg_type);
}

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

    // Nested generic: treat the entire suffix after the first "__" as a single
    // (possibly nested) type argument.  This handles cases like
    // "Shared__PromiseState__I32" -> Shared[PromiseState[I32]] correctly.
    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);
        auto inner = parse_mangled_type_string(arg_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::NamedType{base, "", {inner}};
            return t;
        }
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
    // Template literals (`...{expr}...`) produce a Text value. The type
    // checker already records this; the codegen-side inference was
    // missing it, causing `.as_str()` and other Text methods to fail
    // with "Unknown method" when called directly on a template literal.
    if (expr.is<parser::TemplateLiteralExpr>()) {
        return std::make_shared<types::Type>(
            types::Type{types::NamedType{"Text", "", {}}});
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
            // When current_type_subs_ is available (from gen_impl_method's type sub setup),
            // use it with the struct definition to build the correct semantic type.
            // This is the most accurate path since it uses the actual type substitutions.
            auto sep_pos = current_impl_type_.find("__");
            if (sep_pos != std::string::npos && !current_type_subs_.empty()) {
                std::string base_name = current_impl_type_.substr(0, sep_pos);
                auto struct_def = env_.lookup_struct(base_name);
                if (struct_def.has_value() && !struct_def->type_params.empty()) {
                    std::vector<types::TypePtr> type_args;
                    for (const auto& param : struct_def->type_params) {
                        auto it = current_type_subs_.find(param);
                        if (it != current_type_subs_.end()) {
                            type_args.push_back(it->second);
                        }
                    }
                    if (!type_args.empty()) {
                        auto result = std::make_shared<types::Type>();
                        result->kind = types::NamedType{base_name, "", std::move(type_args)};
                        return result;
                    }
                }
            }
            // Fallback: use parse_mangled_type_string which handles nested
            // generics correctly (e.g., Shared__PromiseState__I32 -> Shared[PromiseState[I32]])
            auto parsed = parse_mangled_type_string(current_impl_type_);
            if (parsed) {
                return parsed;
            }
            auto result = std::make_shared<types::Type>();
            result->kind = types::NamedType{current_impl_type_, "", {}};
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
                return nullptr; // Unknown ptr — do not assume Str (mut ref params are also ptr)
            // For tuple types like "{ i32, i32 }", parse element types
            if (ty.starts_with("{ ") && ty.back() == '}') {
                // Parse "{ i32, i32 }" -> TupleType{I32, I32}
                std::string inner = ty.substr(2, ty.size() - 4); // strip "{ " and " }"
                std::vector<types::TypePtr> elements;
                size_t pos = 0;
                while (pos < inner.size()) {
                    // Skip whitespace
                    while (pos < inner.size() && inner[pos] == ' ')
                        pos++;
                    // Find next comma or end
                    auto comma = inner.find(',', pos);
                    std::string elem = (comma == std::string::npos)
                                           ? inner.substr(pos)
                                           : inner.substr(pos, comma - pos);
                    // Trim trailing whitespace
                    while (!elem.empty() && elem.back() == ' ')
                        elem.pop_back();
                    // Map to semantic type
                    types::TypePtr elem_type;
                    if (elem == "i32")
                        elem_type = types::make_i32();
                    else if (elem == "i64")
                        elem_type = types::make_i64();
                    else if (elem == "i8")
                        elem_type = types::make_primitive(types::PrimitiveKind::I8);
                    else if (elem == "i16")
                        elem_type = types::make_primitive(types::PrimitiveKind::I16);
                    else if (elem == "i1")
                        elem_type = types::make_bool();
                    else if (elem == "float")
                        elem_type = types::make_primitive(types::PrimitiveKind::F32);
                    else if (elem == "double")
                        elem_type = types::make_f64();
                    else if (elem == "ptr")
                        elem_type = types::make_str();
                    else
                        elem_type = types::make_i32(); // fallback
                    elements.push_back(elem_type);
                    if (comma == std::string::npos)
                        break;
                    pos = comma + 1;
                }
                auto result = std::make_shared<types::Type>();
                result->kind = types::TupleType{std::move(elements)};
                return result;
            }
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

        // Check if this is a function reference
        auto func_sig = env_.lookup_func(ident.name);
        if (func_sig.has_value()) {
            // Return a FuncType representing the function's signature
            auto result = std::make_shared<types::Type>();
            result->kind = types::FuncType{func_sig->params, func_sig->return_type};
            return result;
        }
    }
    if (expr.is<parser::BinaryExpr>()) {
        const auto& bin = expr.as<parser::BinaryExpr>();
        switch (bin.op) {
        // Comparison and logical operators return Bool
        case parser::BinaryOp::Eq:
        case parser::BinaryOp::Ne:
        case parser::BinaryOp::Lt:
        case parser::BinaryOp::Gt:
        case parser::BinaryOp::Le:
        case parser::BinaryOp::Ge:
        case parser::BinaryOp::And:
        case parser::BinaryOp::Or:
            return types::make_bool();
        // Assignment operators return Unit (void)
        case parser::BinaryOp::Assign:
        case parser::BinaryOp::AddAssign:
        case parser::BinaryOp::SubAssign:
        case parser::BinaryOp::MulAssign:
        case parser::BinaryOp::DivAssign:
        case parser::BinaryOp::ModAssign:
        case parser::BinaryOp::BitAndAssign:
        case parser::BinaryOp::BitOrAssign:
        case parser::BinaryOp::BitXorAssign:
        case parser::BinaryOp::ShlAssign:
        case parser::BinaryOp::ShrAssign:
            return types::make_unit();
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

        // For Ref operation, wrap operand type in RefType
        // This is needed for proper type unification in generic function calls
        if (unary.op == parser::UnaryOp::Ref && operand_type) {
            auto result = std::make_shared<types::Type>();
            result->kind = types::RefType{.is_mut = false, .inner = operand_type, .lifetime = ""};
            return result;
        }

        // For RefMut operation, wrap operand type in RefType with is_mut = true
        if (unary.op == parser::UnaryOp::RefMut && operand_type) {
            auto result = std::make_shared<types::Type>();
            result->kind = types::RefType{.is_mut = true, .inner = operand_type, .lifetime = ""};
            return result;
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
    // Field access, block, closure, conditional, and call expressions — see infer_types.cpp
    if (auto extended = infer_expr_type_extended(expr)) {
        return *extended;
    }
    // Remaining expression types handled in infer_methods.cpp
    return infer_expr_type_continued(expr);
}

} // namespace tml::codegen
