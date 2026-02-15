//! # LLVM IR Generator - Type Resolution & Substitution
//!
//! This file implements type resolution, substitution, unification,
//! and associated type lookup for generic instantiation.
//!
//! ## Sections
//!
//! | Section                        | Purpose                                   |
//! |--------------------------------|-------------------------------------------|
//! | `resolve_parser_type_with_subs`| Convert parser::Type to types::TypePtr     |
//! | `apply_type_substitutions`     | Apply generic subs to semantic types       |
//! | `contains_unresolved_generic`  | Check for uninstantiated type params       |
//! | `unify_types`                  | Extract type bindings from pattern match   |
//! | `semantic_type_from_llvm`      | Convert LLVM type string to semantic type  |
//! | `lookup_associated_type`       | Find associated types in impl blocks       |

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"

namespace tml::codegen {

// ============ Parser Type to Semantic Type with Substitution ============
// Converts parser::Type to types::TypePtr, applying generic substitutions

auto LLVMIRGen::resolve_parser_type_with_subs(
    const parser::Type& type, const std::unordered_map<std::string, types::TypePtr>& subs)
    -> types::TypePtr {
    return std::visit(
        [this, &subs](const auto& t) -> types::TypePtr {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, parser::NamedType>) {
                // Handle associated types like This::Item or Self::Item
                // Path will have segments ["This", "Item"] or ["Self", "Item"]
                if (t.path.segments.size() == 2) {
                    const std::string& first = t.path.segments[0];
                    const std::string& second = t.path.segments[1];
                    if (first == "This" || first == "Self") {
                        // Look up in current associated types
                        auto assoc_it = current_associated_types_.find(second);
                        if (assoc_it != current_associated_types_.end()) {
                            return assoc_it->second;
                        }
                    }
                    // Handle T::AssociatedType where T is a generic param
                    // Example: I::Item where I -> RangeIterI64 should resolve to I64
                    auto param_it = subs.find(first);
                    if (param_it != subs.end() && param_it->second) {
                        // Get the concrete type that T was substituted to
                        const auto& concrete_type = param_it->second;
                        if (concrete_type->is<types::NamedType>()) {
                            const auto& named = concrete_type->as<types::NamedType>();
                            // Look up the associated type of this concrete type
                            auto assoc_type = lookup_associated_type(named.name, second);
                            if (assoc_type) {
                                return assoc_type;
                            }
                        } else if (concrete_type->is<types::PrimitiveType>()) {
                            // For primitive types with associated types (e.g., T::Owned where T:
                            // ToOwned) Most primitives have Self as their Owned type
                            if (second == "Owned") {
                                // For ToOwned, Owned = Self for all primitive types
                                return concrete_type;
                            }
                            // For other associated types on primitives, look up by primitive name
                            auto prim_kind = concrete_type->as<types::PrimitiveType>().kind;
                            std::string prim_name = types::primitive_kind_to_string(prim_kind);
                            auto assoc_type = lookup_associated_type(prim_name, second);
                            if (assoc_type) {
                                return assoc_type;
                            }
                        }
                        // Fallback: check current_associated_types_
                        auto assoc_it = current_associated_types_.find(second);
                        if (assoc_it != current_associated_types_.end()) {
                            return assoc_it->second;
                        }
                    }
                }

                // Get the type name
                std::string name;
                if (!t.path.segments.empty()) {
                    name = t.path.segments.back();
                }

                // Check if it's a generic parameter that needs substitution
                auto it = subs.find(name);
                if (it != subs.end()) {
                    return it->second; // Return substituted type
                }

                // Check for primitive types
                static const std::unordered_map<std::string, types::PrimitiveKind> primitives = {
                    {"I8", types::PrimitiveKind::I8},     {"I16", types::PrimitiveKind::I16},
                    {"I32", types::PrimitiveKind::I32},   {"I64", types::PrimitiveKind::I64},
                    {"I128", types::PrimitiveKind::I128}, {"U8", types::PrimitiveKind::U8},
                    {"U16", types::PrimitiveKind::U16},   {"U32", types::PrimitiveKind::U32},
                    {"U64", types::PrimitiveKind::U64},   {"U128", types::PrimitiveKind::U128},
                    {"F32", types::PrimitiveKind::F32},   {"F64", types::PrimitiveKind::F64},
                    {"Bool", types::PrimitiveKind::Bool}, {"Char", types::PrimitiveKind::Char},
                    {"Str", types::PrimitiveKind::Str},   {"String", types::PrimitiveKind::Str},
                    {"Unit", types::PrimitiveKind::Unit}, {"Usize", types::PrimitiveKind::U64},
                    {"Isize", types::PrimitiveKind::I64},
                };

                auto prim_it = primitives.find(name);
                if (prim_it != primitives.end()) {
                    return types::make_primitive(prim_it->second);
                }

                // Handle Ptr[T] - convert to PtrType for proper dereference handling
                if (name == "Ptr" && t.generics.has_value() && !t.generics->args.empty()) {
                    auto inner =
                        resolve_parser_type_with_subs(*t.generics->args[0].as_type(), subs);
                    auto result = std::make_shared<types::Type>();
                    result->kind = types::PtrType{false, inner};
                    return result;
                }

                // Check if it's a class type (non-generic)
                auto class_def = env_.lookup_class(name);
                if (class_def.has_value()) {
                    auto result = std::make_shared<types::Type>();
                    // Process generic arguments for generic classes
                    std::vector<types::TypePtr> class_type_args;
                    if (t.generics.has_value()) {
                        for (const auto& arg : t.generics->args) {
                            if (arg.is_type()) {
                                class_type_args.push_back(
                                    resolve_parser_type_with_subs(*arg.as_type(), subs));
                            }
                        }
                    }
                    result->kind = types::ClassType{name, "", std::move(class_type_args)};
                    return result;
                }

                // Check if it's a pending generic class (e.g., Box[T] before instantiation)
                auto pending_it = pending_generic_classes_.find(name);
                if (pending_it != pending_generic_classes_.end()) {
                    auto result = std::make_shared<types::Type>();
                    // Process generic arguments
                    std::vector<types::TypePtr> class_type_args;
                    if (t.generics.has_value()) {
                        for (const auto& arg : t.generics->args) {
                            if (arg.is_type()) {
                                class_type_args.push_back(
                                    resolve_parser_type_with_subs(*arg.as_type(), subs));
                            }
                        }
                    }
                    result->kind = types::ClassType{name, "", std::move(class_type_args)};
                    return result;
                }

                // Named type - process generic arguments if present
                std::vector<types::TypePtr> type_args;
                if (t.generics.has_value()) {
                    for (const auto& arg : t.generics->args) {
                        if (arg.is_type()) {
                            type_args.push_back(
                                resolve_parser_type_with_subs(*arg.as_type(), subs));
                        }
                    }
                }

                // Look up module_path from registry - necessary for method resolution
                // when library code is re-parsed during generic instantiation
                std::string module_path = "";
                if (env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        if (mod.structs.find(name) != mod.structs.end() ||
                            mod.enums.find(name) != mod.enums.end()) {
                            module_path = mod_name;
                            break;
                        }
                    }
                }

                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{name, module_path, std::move(type_args)};
                return result;
            } else if constexpr (std::is_same_v<T, parser::RefType>) {
                auto inner = resolve_parser_type_with_subs(*t.inner, subs);
                auto result = std::make_shared<types::Type>();
                result->kind =
                    types::RefType{.is_mut = t.is_mut, .inner = inner, .lifetime = t.lifetime};
                return result;
            } else if constexpr (std::is_same_v<T, parser::PtrType>) {
                auto inner = resolve_parser_type_with_subs(*t.inner, subs);
                auto result = std::make_shared<types::Type>();
                result->kind = types::PtrType{t.is_mut, inner};
                return result;
            } else if constexpr (std::is_same_v<T, parser::ArrayType>) {
                auto element = resolve_parser_type_with_subs(*t.element, subs);
                // parser::ArrayType::size is an ExprPtr, need to evaluate it
                // For now, use a default size of 0 (will be computed elsewhere if needed)
                size_t arr_size = 0;
                if (t.size && t.size->template is<parser::LiteralExpr>()) {
                    const auto& lit = t.size->template as<parser::LiteralExpr>();
                    if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                        arr_size = static_cast<size_t>(lit.token.int_value().value);
                    }
                }
                auto result = std::make_shared<types::Type>();
                result->kind = types::ArrayType{element, arr_size};
                return result;
            } else if constexpr (std::is_same_v<T, parser::SliceType>) {
                auto element = resolve_parser_type_with_subs(*t.element, subs);
                auto result = std::make_shared<types::Type>();
                result->kind = types::SliceType{element};
                return result;
            } else if constexpr (std::is_same_v<T, parser::TupleType>) {
                std::vector<types::TypePtr> elements;
                for (const auto& elem : t.elements) {
                    elements.push_back(resolve_parser_type_with_subs(*elem, subs));
                }
                return types::make_tuple(std::move(elements));
            } else if constexpr (std::is_same_v<T, parser::FuncType>) {
                std::vector<types::TypePtr> params;
                for (const auto& param : t.params) {
                    params.push_back(resolve_parser_type_with_subs(*param, subs));
                }
                types::TypePtr ret = types::make_unit();
                if (t.return_type) {
                    ret = resolve_parser_type_with_subs(*t.return_type, subs);
                }
                return types::make_func(std::move(params), ret);
            } else if constexpr (std::is_same_v<T, parser::DynType>) {
                // dyn Behavior[T] - convert to DynBehaviorType
                std::string behavior_name;
                if (!t.behavior.segments.empty()) {
                    behavior_name = t.behavior.segments.back();
                }

                // Process type arguments if present (e.g., dyn Processor[I32])
                std::vector<types::TypePtr> type_args;
                if (t.generics.has_value()) {
                    for (const auto& arg : t.generics->args) {
                        if (arg.is_type()) {
                            type_args.push_back(
                                resolve_parser_type_with_subs(*arg.as_type(), subs));
                        }
                    }
                }

                auto result = std::make_shared<types::Type>();
                result->kind =
                    types::DynBehaviorType{behavior_name, std::move(type_args), t.is_mut};
                return result;
            } else if constexpr (std::is_same_v<T, parser::ImplBehaviorType>) {
                // impl Behavior[T] - convert to ImplBehaviorType
                std::string behavior_name;
                if (!t.behavior.segments.empty()) {
                    behavior_name = t.behavior.segments.back();
                }

                // Process type arguments if present (e.g., impl Iterator[Item=I32])
                std::vector<types::TypePtr> type_args;
                if (t.generics.has_value()) {
                    for (const auto& arg : t.generics->args) {
                        if (arg.is_type()) {
                            type_args.push_back(
                                resolve_parser_type_with_subs(*arg.as_type(), subs));
                        }
                    }
                }

                auto result = std::make_shared<types::Type>();
                result->kind = types::ImplBehaviorType{behavior_name, std::move(type_args)};
                return result;
            } else if constexpr (std::is_same_v<T, parser::InferType>) {
                // Infer type - return a type variable or Unit as placeholder
                return types::make_unit();
            } else {
                // Default: return Unit
                return types::make_unit();
            }
        },
        type.kind);
}

// ============ Semantic Type Substitution ============
// Apply type substitutions to a semantic type

auto LLVMIRGen::apply_type_substitutions(
    const types::TypePtr& type, const std::unordered_map<std::string, types::TypePtr>& subs)
    -> types::TypePtr {
    if (!type)
        return type;

    // Check if it's a named type that might need substitution
    if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();

        // Check if the name itself is a substitution target (e.g., T -> I64)
        auto it = subs.find(named.name);
        if (it != subs.end() && it->second) {
            return it->second;
        }

        // Handle unresolved associated types like "T::Owned" that were deferred from type checking
        // These are stored as a single name string "T::Owned" by the type checker
        auto colon_pos = named.name.find("::");
        if (colon_pos != std::string::npos) {
            std::string first_part = named.name.substr(0, colon_pos);
            std::string second_part = named.name.substr(colon_pos + 2);

            // Try to resolve the first part (e.g., "T" -> I32)
            auto param_it = subs.find(first_part);
            if (param_it != subs.end() && param_it->second) {
                const auto& concrete_type = param_it->second;
                // For primitives with "Owned" associated type, return the primitive itself
                if (second_part == "Owned" && concrete_type->is<types::PrimitiveType>()) {
                    return concrete_type;
                }
                // For named types, look up the associated type
                if (concrete_type->is<types::NamedType>()) {
                    const auto& concrete_named = concrete_type->as<types::NamedType>();
                    auto assoc_type = lookup_associated_type(concrete_named.name, second_part);
                    if (assoc_type) {
                        return assoc_type;
                    }
                }
            }
        }

        // If it has type args, recursively apply substitutions to them
        if (!named.type_args.empty()) {
            std::vector<types::TypePtr> new_args;
            bool changed = false;
            for (const auto& arg : named.type_args) {
                auto new_arg = apply_type_substitutions(arg, subs);
                if (new_arg != arg)
                    changed = true;
                new_args.push_back(new_arg);
            }
            if (changed) {
                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{named.name, named.module_path, std::move(new_args)};
                return result;
            }
        }
    } else if (type->is<types::RefType>()) {
        const auto& ref = type->as<types::RefType>();
        auto new_inner = apply_type_substitutions(ref.inner, subs);
        if (new_inner != ref.inner) {
            return types::make_ref(new_inner, ref.is_mut);
        }
    } else if (type->is<types::PtrType>()) {
        const auto& ptr = type->as<types::PtrType>();
        auto new_inner = apply_type_substitutions(ptr.inner, subs);
        if (new_inner != ptr.inner) {
            return types::make_ptr(new_inner, ptr.is_mut);
        }
    } else if (type->is<types::ArrayType>()) {
        const auto& arr = type->as<types::ArrayType>();
        auto new_elem = apply_type_substitutions(arr.element, subs);
        if (new_elem != arr.element) {
            return types::make_array(new_elem, arr.size);
        }
    } else if (type->is<types::SliceType>()) {
        const auto& slice = type->as<types::SliceType>();
        auto new_elem = apply_type_substitutions(slice.element, subs);
        if (new_elem != slice.element) {
            return types::make_slice(new_elem);
        }
    } else if (type->is<types::TupleType>()) {
        const auto& tuple = type->as<types::TupleType>();
        std::vector<types::TypePtr> new_elems;
        bool changed = false;
        for (const auto& elem : tuple.elements) {
            auto new_elem = apply_type_substitutions(elem, subs);
            if (new_elem != elem)
                changed = true;
            new_elems.push_back(new_elem);
        }
        if (changed) {
            return types::make_tuple(std::move(new_elems));
        }
    } else if (type->is<types::GenericType>()) {
        // Handle uninstantiated generic type parameters (e.g., T in Mutex[T])
        // Look up the substitution for this generic type parameter
        const auto& generic = type->as<types::GenericType>();
        auto it = subs.find(generic.name);
        if (it != subs.end() && it->second) {
            return it->second;
        }
    }

    return type;
}

// ============ Unresolved Generic Check ============
// Check if a type contains any unresolved generic type parameters
// This is used to avoid premature struct instantiation with incomplete types

auto LLVMIRGen::contains_unresolved_generic(const types::TypePtr& type) -> bool {
    if (!type)
        return false;

    if (type->is<types::GenericType>()) {
        // Found an unresolved generic parameter
        return true;
    }

    if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();

        // Check for unresolved associated types like "T::Owned"
        // These are stored as a single name string by the type checker
        if (named.name.find("::") != std::string::npos) {
            // This is an unresolved associated type
            return true;
        }

        // Check if this is a known generic struct being used without type arguments
        // e.g., ChannelNode (which requires T) being used without [I32]
        if (named.type_args.empty()) {
            // Check if this struct requires type parameters
            auto pgs_it = pending_generic_structs_.find(named.name);
            if (pgs_it != pending_generic_structs_.end() && !pgs_it->second->generics.empty()) {
                // This is a generic struct being used without type args - treat as unresolved
                return true;
            }
            // Also check module registry for imported generic structs
            if (env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    auto struct_it = mod.structs.find(named.name);
                    if (struct_it != mod.structs.end() && !struct_it->second.type_params.empty()) {
                        // Imported generic struct being used without type args
                        return true;
                    }
                }
            }
        }

        // Check all type arguments recursively
        for (const auto& arg : named.type_args) {
            if (contains_unresolved_generic(arg)) {
                return true;
            }
        }
        return false;
    }

    if (type->is<types::RefType>()) {
        return contains_unresolved_generic(type->as<types::RefType>().inner);
    }

    if (type->is<types::PtrType>()) {
        return contains_unresolved_generic(type->as<types::PtrType>().inner);
    }

    if (type->is<types::ArrayType>()) {
        return contains_unresolved_generic(type->as<types::ArrayType>().element);
    }

    if (type->is<types::SliceType>()) {
        return contains_unresolved_generic(type->as<types::SliceType>().element);
    }

    if (type->is<types::TupleType>()) {
        const auto& tuple = type->as<types::TupleType>();
        for (const auto& elem : tuple.elements) {
            if (contains_unresolved_generic(elem)) {
                return true;
            }
        }
        return false;
    }

    if (type->is<types::FuncType>()) {
        const auto& func = type->as<types::FuncType>();
        for (const auto& param : func.params) {
            if (contains_unresolved_generic(param)) {
                return true;
            }
        }
        return contains_unresolved_generic(func.return_type);
    }

    return false;
}

// ============ Type Unification ============
// Unify a parser type pattern with a semantic type to extract type bindings.
// For example: unify(Maybe[T], Maybe[I32], {T}) -> {T: I32}

void LLVMIRGen::unify_types(const parser::Type& pattern, const types::TypePtr& concrete,
                            const std::unordered_set<std::string>& generics,
                            std::unordered_map<std::string, types::TypePtr>& bindings) {
    if (!concrete)
        return;

    std::visit(
        [this, &concrete, &generics, &bindings](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, parser::NamedType>) {
                // Get the pattern's name
                std::string pattern_name;
                if (!p.path.segments.empty()) {
                    pattern_name = p.path.segments.back();
                }

                // Check if this is a generic parameter we're looking for
                if (generics.count(pattern_name) > 0) {
                    // Found a binding: T = concrete
                    // Check if we already have a binding
                    auto existing = bindings.find(pattern_name);
                    if (existing != bindings.end()) {
                        // Prefer existing non-Unit binding over Unit
                        bool existing_is_unit = existing->second->is<types::PrimitiveType>() &&
                                                existing->second->as<types::PrimitiveType>().kind ==
                                                    types::PrimitiveKind::Unit;
                        bool new_is_unit =
                            concrete->is<types::PrimitiveType>() &&
                            concrete->as<types::PrimitiveType>().kind == types::PrimitiveKind::Unit;
                        if (!existing_is_unit && new_is_unit) {
                            // Keep existing non-Unit binding
                            return;
                        }
                    }
                    bindings[pattern_name] = concrete;
                    return;
                }

                // Not a generic param - try to match structurally
                if (auto* named = std::get_if<types::NamedType>(&concrete->kind)) {
                    // If both are the same named type (e.g., Maybe), match type args
                    if (named->name == pattern_name && p.generics.has_value()) {
                        const auto& pattern_args = p.generics->args;
                        const auto& concrete_args = named->type_args;

                        size_t concrete_idx = 0;
                        for (size_t i = 0;
                             i < pattern_args.size() && concrete_idx < concrete_args.size(); ++i) {
                            // Only process type arguments (skip const generics for now)
                            if (pattern_args[i].is_type()) {
                                unify_types(*pattern_args[i].as_type(), concrete_args[concrete_idx],
                                            generics, bindings);
                                ++concrete_idx;
                            }
                        }
                    }
                }
            } else if constexpr (std::is_same_v<T, parser::RefType>) {
                if (auto* ref = std::get_if<types::RefType>(&concrete->kind)) {
                    unify_types(*p.inner, ref->inner, generics, bindings);
                }
            } else if constexpr (std::is_same_v<T, parser::PtrType>) {
                if (auto* ptr = std::get_if<types::PtrType>(&concrete->kind)) {
                    unify_types(*p.inner, ptr->inner, generics, bindings);
                }
            } else if constexpr (std::is_same_v<T, parser::ArrayType>) {
                if (auto* arr = std::get_if<types::ArrayType>(&concrete->kind)) {
                    unify_types(*p.element, arr->element, generics, bindings);
                }
            } else if constexpr (std::is_same_v<T, parser::SliceType>) {
                if (auto* slice = std::get_if<types::SliceType>(&concrete->kind)) {
                    unify_types(*p.element, slice->element, generics, bindings);
                }
            } else if constexpr (std::is_same_v<T, parser::TupleType>) {
                if (auto* tup = std::get_if<types::TupleType>(&concrete->kind)) {
                    for (size_t i = 0; i < p.elements.size() && i < tup->elements.size(); ++i) {
                        unify_types(*p.elements[i], tup->elements[i], generics, bindings);
                    }
                }
            } else if constexpr (std::is_same_v<T, parser::FuncType>) {
                if (auto* func = std::get_if<types::FuncType>(&concrete->kind)) {
                    for (size_t i = 0; i < p.params.size() && i < func->params.size(); ++i) {
                        unify_types(*p.params[i], func->params[i], generics, bindings);
                    }
                    if (p.return_type && func->return_type) {
                        unify_types(*p.return_type, func->return_type, generics, bindings);
                    }
                }
            }
        },
        pattern.kind);
}

// ============ LLVM Type to Semantic Type ============
// Converts common LLVM type strings back to semantic types

auto LLVMIRGen::semantic_type_from_llvm(const std::string& llvm_type) -> types::TypePtr {
    // Primitive types
    if (llvm_type == "i8")
        return types::make_primitive(types::PrimitiveKind::I8);
    if (llvm_type == "i16")
        return types::make_primitive(types::PrimitiveKind::I16);
    if (llvm_type == "i32")
        return types::make_primitive(types::PrimitiveKind::I32);
    if (llvm_type == "i64")
        return types::make_primitive(types::PrimitiveKind::I64);
    if (llvm_type == "i128")
        return types::make_primitive(types::PrimitiveKind::I128);
    if (llvm_type == "float")
        return types::make_primitive(types::PrimitiveKind::F32);
    if (llvm_type == "double")
        return types::make_primitive(types::PrimitiveKind::F64);
    if (llvm_type == "i1")
        return types::make_primitive(types::PrimitiveKind::Bool);
    if (llvm_type == "ptr")
        return types::make_primitive(types::PrimitiveKind::Str);
    if (llvm_type == "void" || llvm_type == "{}")
        return types::make_unit();

    // For struct types like %struct.TypeName, extract the type name
    if (llvm_type.substr(0, 8) == "%struct.") {
        std::string type_name = llvm_type.substr(8);
        // Check if it's a mangled generic type (contains "__")
        // For now, just create a simple NamedType
        auto result = std::make_shared<types::Type>();
        result->kind = types::NamedType{type_name, "", {}};
        return result;
    }

    // Default: return I32
    return types::make_primitive(types::PrimitiveKind::I32);
}

// ============ Associated Type Lookup ============
// Finds an associated type for a concrete type by searching impl blocks.
// For example: lookup_associated_type("RangeIterI64", "Item") -> I64
//
// This searches:
// 1. Local pending_generic_impls_ (for impls in current module)
// 2. Imported modules via module_registry

auto LLVMIRGen::lookup_associated_type(const std::string& type_name, const std::string& assoc_name)
    -> types::TypePtr {
    // NOTE: We intentionally do NOT check current_associated_types_ here.
    // current_associated_types_ holds the CURRENT impl's associated type bindings
    // (e.g., MyEnum's "Item = (I64, I::Item)"), but this function is called to
    // look up a SPECIFIC type's associated type (e.g., Counter's "Item = I32").
    // Using current_associated_types_ here would return the wrong impl's binding
    // when inner iterator types shadow the outer adapter's associated type.
    // Callers that need the current scope fallback (like resolve_parser_type_with_subs)
    // already check current_associated_types_ after this function returns nullptr.

    // Check persistent per-type registry (populated from concrete impl blocks)
    auto registry_it = type_associated_types_.find(type_name + "::" + assoc_name);
    if (registry_it != type_associated_types_.end()) {
        return registry_it->second;
    }

    // Check local generic impl blocks
    auto impl_it = pending_generic_impls_.find(type_name);
    if (impl_it != pending_generic_impls_.end()) {
        const auto& impl = *impl_it->second;
        for (const auto& binding : impl.type_bindings) {
            if (binding.name == assoc_name && binding.type) {
                return resolve_parser_type_with_subs(*binding.type, {});
            }
        }
    }

    // Check imported modules
    if (env_.module_registry()) {
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            // Check if this module has the struct
            auto struct_it = mod.structs.find(type_name);
            if (struct_it == mod.structs.end())
                continue;

            // Re-parse the module source to get impl AST
            if (mod.source_code.empty())
                continue;

            auto source = lexer::Source::from_string(mod.source_code, mod.file_path);
            lexer::Lexer lex(source);
            auto tokens = lex.tokenize();
            if (lex.has_errors())
                continue;

            parser::Parser mod_parser(std::move(tokens));
            auto module_name_stem = mod.name;
            if (auto pos = module_name_stem.rfind("::"); pos != std::string::npos) {
                module_name_stem = module_name_stem.substr(pos + 2);
            }
            auto parse_result = mod_parser.parse_module(module_name_stem);
            if (!std::holds_alternative<parser::Module>(parse_result))
                continue;

            const auto& parsed_mod = std::get<parser::Module>(parse_result);

            // Find the impl block for this type that has the associated type
            for (const auto& decl : parsed_mod.decls) {
                if (!decl->is<parser::ImplDecl>())
                    continue;
                const auto& impl_decl = decl->as<parser::ImplDecl>();

                // Check if this impl is for our type
                if (!impl_decl.self_type)
                    continue;
                if (!impl_decl.self_type->is<parser::NamedType>())
                    continue;
                const auto& target = impl_decl.self_type->as<parser::NamedType>();
                if (target.path.segments.empty())
                    continue;
                if (target.path.segments.back() != type_name)
                    continue;

                // Look for the associated type binding
                for (const auto& binding : impl_decl.type_bindings) {
                    if (binding.name == assoc_name && binding.type) {
                        return resolve_parser_type_with_subs(*binding.type, {});
                    }
                }
            }
        }
    }

    // Not found
    return nullptr;
}

} // namespace tml::codegen
