// LLVM IR generator - Type conversion and mangling
// Handles: llvm_type_name, llvm_type, llvm_type_ptr, llvm_type_from_semantic
// Handles: mangle_type, mangle_type_args, mangle_struct_name, mangle_func_name
// Handles: resolve_parser_type_with_subs, unify_types

#include "tml/codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::llvm_type_name(const std::string& name) -> std::string {
    // Primitive types
    if (name == "I8")
        return "i8";
    if (name == "I16")
        return "i16";
    if (name == "I32")
        return "i32";
    if (name == "I64")
        return "i64";
    if (name == "I128")
        return "i128";
    if (name == "U8")
        return "i8";
    if (name == "U16")
        return "i16";
    if (name == "U32")
        return "i32";
    if (name == "U64")
        return "i64";
    if (name == "U128")
        return "i128";
    if (name == "F32")
        return "float";
    if (name == "F64")
        return "double";
    if (name == "Bool")
        return "i1";
    if (name == "Char")
        return "i32";
    if (name == "Str" || name == "String")
        return "ptr"; // String is a pointer to struct
    if (name == "Unit")
        return "void";

    // Collection types - all are pointers to runtime structs
    if (name == "List" || name == "Vec" || name == "Array")
        return "ptr";
    if (name == "HashMap" || name == "Map" || name == "Dict")
        return "ptr";
    if (name == "Buffer")
        return "ptr";
    if (name == "Channel")
        return "ptr";
    if (name == "Mutex")
        return "ptr";
    if (name == "WaitGroup")
        return "ptr";

    // User-defined type - return struct type
    return "%struct." + name;
}

auto LLVMIRGen::llvm_type(const parser::Type& type) -> std::string {
    if (type.is<parser::NamedType>()) {
        const auto& named = type.as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            std::string base_name = named.path.segments.back();

            // Check if this is a generic type with type arguments
            if (named.generics.has_value() && !named.generics->args.empty()) {
                // Check if this is a known generic struct/enum
                auto it = pending_generic_structs_.find(base_name);
                if (it != pending_generic_structs_.end()) {
                    // Convert parser type args to semantic types
                    std::vector<types::TypePtr> type_args;
                    for (const auto& arg : named.generics->args) {
                        types::TypePtr semantic_type = resolve_parser_type_with_subs(*arg, {});
                        type_args.push_back(semantic_type);
                    }
                    // Get mangled name and ensure instantiation
                    std::string mangled = require_struct_instantiation(base_name, type_args);
                    return "%struct." + mangled;
                }
                // Check enum
                auto enum_it = pending_generic_enums_.find(base_name);
                if (enum_it != pending_generic_enums_.end()) {
                    std::vector<types::TypePtr> type_args;
                    for (const auto& arg : named.generics->args) {
                        types::TypePtr semantic_type = resolve_parser_type_with_subs(*arg, {});
                        type_args.push_back(semantic_type);
                    }
                    std::string mangled = require_enum_instantiation(base_name, type_args);
                    return "%struct." + mangled;
                }
            }

            return llvm_type_name(base_name);
        }
    } else if (type.is<parser::RefType>()) {
        return "ptr";
    } else if (type.is<parser::PtrType>()) {
        return "ptr";
    } else if (type.is<parser::ArrayType>()) {
        return "ptr";
    } else if (type.is<parser::FuncType>()) {
        // Function types are pointers in LLVM
        return "ptr";
    } else if (type.is<parser::DynType>()) {
        // Dyn types are fat pointers: { data_ptr, vtable_ptr }
        const auto& dyn = type.as<parser::DynType>();
        std::string behavior_name;
        if (!dyn.behavior.segments.empty()) {
            behavior_name = dyn.behavior.segments.back();
        }
        return "%dyn." + behavior_name;
    }
    return "i32"; // Default
}

auto LLVMIRGen::llvm_type_ptr(const parser::TypePtr& type) -> std::string {
    if (!type)
        return "void";
    return llvm_type(*type);
}

auto LLVMIRGen::llvm_type_from_semantic(const types::TypePtr& type, bool for_data) -> std::string {
    if (!type)
        return for_data ? "{}" : "void";

    if (type->is<types::PrimitiveType>()) {
        const auto& prim = type->as<types::PrimitiveType>();
        switch (prim.kind) {
        case types::PrimitiveKind::I8:
            return "i8";
        case types::PrimitiveKind::I16:
            return "i16";
        case types::PrimitiveKind::I32:
            return "i32";
        case types::PrimitiveKind::I64:
            return "i64";
        case types::PrimitiveKind::I128:
            return "i128";
        case types::PrimitiveKind::U8:
            return "i8";
        case types::PrimitiveKind::U16:
            return "i16";
        case types::PrimitiveKind::U32:
            return "i32";
        case types::PrimitiveKind::U64:
            return "i64";
        case types::PrimitiveKind::U128:
            return "i128";
        case types::PrimitiveKind::F32:
            return "float";
        case types::PrimitiveKind::F64:
            return "double";
        case types::PrimitiveKind::Bool:
            return "i1";
        case types::PrimitiveKind::Char:
            return "i32";
        case types::PrimitiveKind::Str:
            return "ptr";
        // Unit: use "{}" (empty struct) when used as data, "void" for return types
        case types::PrimitiveKind::Unit:
            return for_data ? "{}" : "void";
        }
    } else if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();

        // If it has type arguments, need to use mangled name and ensure instantiation
        if (!named.type_args.empty()) {
            // Check if it's a generic enum (like Maybe, Outcome)
            auto enum_it = pending_generic_enums_.find(named.name);
            if (enum_it != pending_generic_enums_.end()) {
                std::string mangled = require_enum_instantiation(named.name, named.type_args);
                return "%struct." + mangled;
            }
            // Otherwise try as struct
            std::string mangled = require_struct_instantiation(named.name, named.type_args);
            return "%struct." + mangled;
        }

        return "%struct." + named.name;
    } else if (type->is<types::GenericType>()) {
        // Uninstantiated generic type - this shouldn't happen in codegen normally
        // Return a placeholder (will cause error if actually used)
        return "i32";
    } else if (type->is<types::RefType>() || type->is<types::PtrType>()) {
        return "ptr";
    } else if (type->is<types::FuncType>()) {
        // Function types are pointers in LLVM
        return "ptr";
    } else if (type->is<types::DynBehaviorType>()) {
        // Trait objects are fat pointers: { data_ptr, vtable_ptr }
        // We use a struct type: %dyn.BehaviorName
        const auto& dyn = type->as<types::DynBehaviorType>();
        return "%dyn." + dyn.behavior_name;
    }

    return "i32"; // Default
}

// ============ Generic Type Mangling ============
// Converts type to mangled string for LLVM IR names
// e.g., I32 -> "I32", List[I32] -> "List__I32", HashMap[Str, Bool] -> "HashMap__Str__Bool"

auto LLVMIRGen::mangle_type(const types::TypePtr& type) -> std::string {
    if (!type)
        return "void";

    if (type->is<types::PrimitiveType>()) {
        auto kind = type->as<types::PrimitiveType>().kind;
        // Special handling for Unit and Never - symbols invalid in LLVM identifiers
        if (kind == types::PrimitiveKind::Unit)
            return "Unit";
        if (kind == types::PrimitiveKind::Never)
            return "Never";
        return types::primitive_kind_to_string(kind);
    } else if (type->is<types::NamedType>()) {
        const auto& named = type->as<types::NamedType>();
        if (named.type_args.empty()) {
            return named.name;
        }
        // Mangle with type arguments: List[I32] -> List__I32
        return named.name + "__" + mangle_type_args(named.type_args);
    } else if (type->is<types::RefType>()) {
        const auto& ref = type->as<types::RefType>();
        return (ref.is_mut ? "mutref_" : "ref_") + mangle_type(ref.inner);
    } else if (type->is<types::PtrType>()) {
        const auto& ptr = type->as<types::PtrType>();
        return (ptr.is_mut ? "mutptr_" : "ptr_") + mangle_type(ptr.inner);
    } else if (type->is<types::DynBehaviorType>()) {
        const auto& dyn = type->as<types::DynBehaviorType>();
        if (dyn.type_args.empty()) {
            return "dyn_" + dyn.behavior_name;
        }
        return "dyn_" + dyn.behavior_name + "__" + mangle_type_args(dyn.type_args);
    } else if (type->is<types::ArrayType>()) {
        const auto& arr = type->as<types::ArrayType>();
        return "arr_" + mangle_type(arr.element) + "_" + std::to_string(arr.size);
    } else if (type->is<types::GenericType>()) {
        // Uninstantiated generic - shouldn't reach codegen normally
        return type->as<types::GenericType>().name;
    }

    return "unknown";
}

auto LLVMIRGen::mangle_type_args(const std::vector<types::TypePtr>& args) -> std::string {
    std::string result;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0)
            result += "__";
        result += mangle_type(args[i]);
    }
    return result;
}

auto LLVMIRGen::mangle_struct_name(const std::string& base_name,
                                   const std::vector<types::TypePtr>& type_args) -> std::string {
    if (type_args.empty()) {
        return base_name;
    }
    return base_name + "__" + mangle_type_args(type_args);
}

auto LLVMIRGen::mangle_func_name(const std::string& base_name,
                                 const std::vector<types::TypePtr>& type_args) -> std::string {
    if (type_args.empty()) {
        return base_name;
    }
    return base_name + "__" + mangle_type_args(type_args);
}

// ============ Parser Type to Semantic Type with Substitution ============
// Converts parser::Type to types::TypePtr, applying generic substitutions

auto LLVMIRGen::resolve_parser_type_with_subs(
    const parser::Type& type, const std::unordered_map<std::string, types::TypePtr>& subs)
    -> types::TypePtr {
    return std::visit(
        [this, &subs](const auto& t) -> types::TypePtr {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, parser::NamedType>) {
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
                    {"Unit", types::PrimitiveKind::Unit},
                };

                auto prim_it = primitives.find(name);
                if (prim_it != primitives.end()) {
                    return types::make_primitive(prim_it->second);
                }

                // Named type - process generic arguments if present
                std::vector<types::TypePtr> type_args;
                if (t.generics.has_value()) {
                    for (const auto& arg : t.generics->args) {
                        type_args.push_back(resolve_parser_type_with_subs(*arg, subs));
                    }
                }

                auto result = std::make_shared<types::Type>();
                result->kind = types::NamedType{name, "", std::move(type_args)};
                return result;
            } else if constexpr (std::is_same_v<T, parser::RefType>) {
                auto inner = resolve_parser_type_with_subs(*t.inner, subs);
                auto result = std::make_shared<types::Type>();
                result->kind = types::RefType{t.is_mut, inner};
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
                if (t.size && t.size->is<parser::LiteralExpr>()) {
                    const auto& lit = t.size->as<parser::LiteralExpr>();
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

                        for (size_t i = 0; i < pattern_args.size() && i < concrete_args.size();
                             ++i) {
                            unify_types(*pattern_args[i], concrete_args[i], generics, bindings);
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

} // namespace tml::codegen
