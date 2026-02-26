TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Type Inference (Method Calls & Remaining Expressions)
//!
//! This file is the continuation of infer.cpp, handling:
//! - Method call expressions (MethodCallExpr)
//! - Tuple, array, index, and cast expressions
//! - Deref coercion helpers
//! - Struct field lookup

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "types/module.hpp"

#include <iostream>
#include <unordered_set>

namespace tml::codegen {

// Helper: infer semantic type from method call, tuple, array, index, cast expressions
auto LLVMIRGen::infer_expr_type_continued(const parser::Expr& expr) -> types::TypePtr {
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

        // Auto-deref: unwrap RefType for method dispatch (ref T -> T)
        if (receiver_type && receiver_type->is<types::RefType>()) {
            receiver_type = receiver_type->as<types::RefType>().inner;
        }

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

            // Shared[T] / Sync[T] get_mut returns Maybe[mut ref T]
            if ((named.name == "Shared" || named.name == "Sync" || named.name == "Arc") &&
                !named.type_args.empty() && call.method == "get_mut") {
                auto mut_ref = std::make_shared<types::Type>();
                mut_ref->kind = types::RefType{
                    .is_mut = true, .inner = named.type_args[0], .lifetime = std::nullopt};
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"Maybe", "", {mut_ref}};
                return result;
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

            // Str.parse_* methods return Maybe[T]
            if (kind == types::PrimitiveKind::Str) {
                auto make_maybe = [](types::TypePtr inner) -> types::TypePtr {
                    auto result = std::make_shared<types::Type>();
                    result->kind = types::NamedType{"Maybe", "", {std::move(inner)}};
                    return result;
                };

                if (call.method == "parse_i8")
                    return make_maybe(types::make_primitive(types::PrimitiveKind::I8));
                if (call.method == "parse_i16")
                    return make_maybe(types::make_primitive(types::PrimitiveKind::I16));
                if (call.method == "parse_i32")
                    return make_maybe(types::make_i32());
                if (call.method == "parse_i64")
                    return make_maybe(types::make_i64());
                if (call.method == "parse_i128")
                    return make_maybe(types::make_primitive(types::PrimitiveKind::I128));
                if (call.method == "parse_u8")
                    return make_maybe(types::make_primitive(types::PrimitiveKind::U8));
                if (call.method == "parse_u16")
                    return make_maybe(types::make_primitive(types::PrimitiveKind::U16));
                if (call.method == "parse_u32")
                    return make_maybe(types::make_primitive(types::PrimitiveKind::U32));
                if (call.method == "parse_u64")
                    return make_maybe(types::make_primitive(types::PrimitiveKind::U64));
                if (call.method == "parse_u128")
                    return make_maybe(types::make_primitive(types::PrimitiveKind::U128));
                if (call.method == "parse_f32")
                    return make_maybe(types::make_primitive(types::PrimitiveKind::F32));
                if (call.method == "parse_f64")
                    return make_maybe(types::make_f64());
                if (call.method == "parse_bool")
                    return make_maybe(types::make_bool());
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

            // as_slice returns Slice[T] (SliceType)
            if (call.method == "as_slice") {
                auto result = std::make_shared<types::Type>();
                result->kind = types::SliceType{elem_type};
                return result;
            }

            // as_mut_slice returns MutSlice[T]
            if (call.method == "as_mut_slice") {
                std::vector<types::TypePtr> type_args = {elem_type};
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{"MutSlice", "", std::move(type_args)};
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
                        if (struct_it == mod.structs.end()) {
                            // Also check internal_structs
                            struct_it = mod.internal_structs.find(named.name);
                            if (struct_it == mod.internal_structs.end()) {
                                continue;
                            }
                        }
                        if (!struct_it->second.type_params.empty()) {
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
        for (const auto& fld : struct_def->fields) {
            if (fld.name == field_name) {
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
                for (const auto& fld : mod_struct_it->second.fields) {
                    if (fld.name == field_name) {
                        return true;
                    }
                }
            }
            // Also check internal_structs
            auto internal_it = mod.internal_structs.find(struct_name);
            if (internal_it != mod.internal_structs.end()) {
                for (const auto& fld : internal_it->second.fields) {
                    if (fld.name == field_name) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

} // namespace tml::codegen
