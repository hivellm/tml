TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Type Inference (Field, Block, Closure, Call Expressions)
//!
//! This file is the second continuation of infer.cpp, handling:
//! - Field access expressions (FieldExpr) — struct field type lookup with registry fallback
//! - Block expressions (BlockExpr) — trailing-expression type propagation
//! - Closure expressions (ClosureExpr) — FuncType construction
//! - Ternary, if, and when expressions — branch type propagation
//! - Call expressions (CallExpr) — return-type lookup for functions and enum constructors
//!
//! Called from infer_expr_type() in infer.cpp when the expression is not a
//! literal, identifier, unary/binary op, struct literal, or path expression.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "types/module.hpp"

#include <functional>
#include <iostream>
#include <map>
#include <unordered_set>

namespace tml::codegen {

// Static helpers duplicated from infer.cpp (file-static, needed by this TU)
static types::TypePtr extract_generic_from_nested_type(const parser::TypePtr& field_type,
                                                       const std::string& generic_name,
                                                       const types::TypePtr& arg_type) {
    if (!field_type || !arg_type)
        return nullptr;
    if (field_type->is<parser::NamedType>()) {
        const auto& named = field_type->as<parser::NamedType>();
        if (!named.path.segments.empty() && named.path.segments.back() == generic_name &&
            !named.generics.has_value())
            return arg_type;
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

static types::TypePtr
extract_generic_from_call_expr(const parser::TypePtr& field_type, const std::string& generic_name,
                               const parser::Expr& arg_expr,
                               std::function<types::TypePtr(const parser::Expr&)> infer_fn) {
    if (!field_type)
        return nullptr;
    if (!field_type->is<parser::NamedType>())
        return nullptr;
    const auto& named = field_type->as<parser::NamedType>();
    if (!named.generics.has_value())
        return nullptr;
    if (arg_expr.is<parser::CallExpr>()) {
        const auto& call = arg_expr.as<parser::CallExpr>();
        if (call.callee && call.callee->is<parser::PathExpr>()) {
            const auto& path = call.callee->as<parser::PathExpr>();
            if (path.path.segments.size() == 2) {
                const std::string& callee_type = path.path.segments[0];
                if (!named.path.segments.empty() && named.path.segments.back() == callee_type) {
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

static types::TypePtr parse_mangled_type_string(const std::string& s) {
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
    if (s.size() > 4 && s.substr(0, 4) == "ptr_") {
        auto inner = parse_mangled_type_string(s.substr(4));
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = false, .inner = inner};
            return t;
        }
    }
    if (s.size() > 7 && s.substr(0, 7) == "mutptr_") {
        auto inner = parse_mangled_type_string(s.substr(7));
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = true, .inner = inner};
            return t;
        }
    }
    if (s.size() > 4 && s.substr(0, 4) == "ref_") {
        auto inner = parse_mangled_type_string(s.substr(4));
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::RefType{.is_mut = false, .inner = inner};
            return t;
        }
    }
    if (s.size() > 7 && s.substr(0, 7) == "mutref_") {
        auto inner = parse_mangled_type_string(s.substr(7));
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::RefType{.is_mut = true, .inner = inner};
            return t;
        }
    }
    auto delim = s.find("__");
    if (delim != std::string::npos) {
        std::string base = s.substr(0, delim);
        std::string arg_str = s.substr(delim + 2);
        std::vector<types::TypePtr> type_args;
        size_t pos = 0;
        while (pos < arg_str.size()) {
            auto next_delim = arg_str.find("__", pos);
            std::string arg_part = (next_delim == std::string::npos)
                                       ? arg_str.substr(pos)
                                       : arg_str.substr(pos, next_delim - pos);
            pos = (next_delim == std::string::npos) ? arg_str.size() : next_delim + 2;
            auto arg_type = parse_mangled_type_string(arg_part);
            if (arg_type) {
                type_args.push_back(arg_type);
            } else {
                auto t = std::make_shared<types::Type>();
                t->kind = types::NamedType{arg_part, "", {}};
                type_args.push_back(t);
            }
        }
        auto t = std::make_shared<types::Type>();
        t->kind = types::NamedType{base, "", std::move(type_args)};
        return t;
    }
    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

/// Infer semantic type for field access, block, closure, conditional, and call expressions.
/// Called by infer_expr_type() for expression kinds not handled in infer.cpp.
auto LLVMIRGen::infer_expr_type_extended(const parser::Expr& expr)
    -> std::optional<types::TypePtr> {
    // Handle field access expressions
    if (expr.is<parser::FieldExpr>()) {
        const auto& field = expr.as<parser::FieldExpr>();
        // Get the type of the object
        types::TypePtr obj_type = infer_expr_type(*field.object);

        // Auto-deref: unwrap RefType for field access (ref T → T)
        if (obj_type && obj_type->is<types::RefType>()) {
            obj_type = obj_type->as<types::RefType>().inner;
        }

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

            // If the local lookup found a struct but it doesn't have the field we need,
            // clear it so we search the module registry for a better match.
            // This handles name collisions (e.g., core::iter::Take vs core::async_iter::Take).
            if (struct_def) {
                bool local_has_field = false;
                for (const auto& fld : struct_def->fields) {
                    if (fld.name == field.field) {
                        local_has_field = true;
                        break;
                    }
                }
                if (!local_has_field) {
                    struct_def = std::nullopt; // try module registry instead
                }
            }

            // If not found locally, search all modules in the registry
            // When multiple modules define a struct with the same name (e.g., core::iter::Take
            // vs core::async_iter::Take), prefer the one that has the field we're looking for.
            if (!struct_def && env_.module_registry()) {
                std::optional<types::StructDef> first_match;
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    std::optional<types::StructDef> candidate;
                    auto mod_struct_it = mod.structs.find(named.name);
                    if (mod_struct_it != mod.structs.end()) {
                        candidate = mod_struct_it->second;
                    }
                    if (!candidate) {
                        auto internal_it = mod.internal_structs.find(named.name);
                        if (internal_it != mod.internal_structs.end()) {
                            candidate = internal_it->second;
                        }
                    }
                    if (candidate) {
                        // Check if this candidate has the field we're looking for
                        bool has_field = false;
                        for (const auto& fld : candidate->fields) {
                            if (fld.name == field.field) {
                                has_field = true;
                                break;
                            }
                        }
                        if (has_field) {
                            struct_def = candidate;
                            break;
                        }
                        if (!first_match) {
                            first_match = candidate;
                        }
                    }
                }
                // Fall back to first match if no candidate had the field
                if (!struct_def && first_match) {
                    struct_def = first_match;
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
                        // Also check internal_structs with prefix matching
                        if (!struct_def) {
                            for (const auto& [struct_name, sdef] : mod.internal_structs) {
                                if (struct_name == named.name ||
                                    (struct_name.size() > named.name.size() + 2 &&
                                     struct_name.substr(struct_name.size() - named.name.size()) ==
                                         named.name &&
                                     struct_name[struct_name.size() - named.name.size() - 2] ==
                                         ':')) {
                                    struct_def = sdef;
                                    break;
                                }
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
                        types::StructFieldDef fdef;
                        fdef.name = field_decl.name;
                        fdef.type = field_type;
                        temp_struct_def.fields.push_back(std::move(fdef));
                    }
                    struct_def = temp_struct_def;
                }
            }

            if (struct_def) {
                TML_DEBUG_LN("[INFER] struct_def found, searching for field: " << field.field);
                for (const auto& fld : struct_def->fields) {
                    TML_DEBUG_LN("[INFER]   field: "
                                 << fld.name << " type: "
                                 << (fld.type ? types::type_to_string(fld.type) : "null"));
                    if (fld.name == field.field && fld.type) {
                        TML_DEBUG_LN(
                            "[INFER] Returning field type: " << types::type_to_string(fld.type));
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
                            auto substituted = types::substitute_type(fld.type, subs);
                            TML_DEBUG_LN("[INFER] After substitution: "
                                         << types::type_to_string(substituted));
                            return substituted;
                        }
                        return fld.type;
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
                                // Also check internal_structs for module-internal types
                                auto internal_it = mod.internal_structs.find(inner_named.name);
                                if (internal_it != mod.internal_structs.end()) {
                                    inner_struct_def = internal_it->second;
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
                                    types::StructFieldDef fdef;
                                    fdef.name = field_decl.name;
                                    fdef.type = ft;
                                    temp_struct_def.fields.push_back(std::move(fdef));
                                }
                                inner_struct_def = temp_struct_def;
                            }
                        }
                        if (inner_struct_def) {
                            TML_DEBUG_LN("[INFER] Found inner struct: " << inner_named.name);
                            for (const auto& inner_fld : inner_struct_def->fields) {
                                if (inner_fld.name == field.field && inner_fld.type) {
                                    TML_DEBUG_LN(
                                        "[INFER] Found field via auto-deref: " << inner_fld.name);
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
                                        return types::substitute_type(inner_fld.type, subs);
                                    }
                                    return inner_fld.type;
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
                            auto internal_it = mod.internal_structs.find(base_name);
                            if (internal_it != mod.internal_structs.end()) {
                                expected_type_params = internal_it->second.type_params.size();
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
                for (const auto& fld : struct_def->fields) {
                    if (fld.name == field.field) {
                        return fld.type;
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
    // Handle block expressions
    if (expr.is<parser::BlockExpr>()) {
        const auto& block = expr.as<parser::BlockExpr>();
        if (block.expr.has_value()) {
            // Block has trailing expression — infer from that
            return infer_expr_type(*block.expr.value());
        }
        // Block has no trailing expression (only statements) — returns Unit
        return types::make_unit();
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

        // Handle PathExpr callee (like Builder::create or I32::try_from)
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
                            auto ret = m.sig.return_type;
                            // For generic classes (e.g., Heap[T]::new), substitute
                            // type params with inferred arg types
                            if (!class_def->type_params.empty() && ret &&
                                ret->is<types::NamedType>()) {
                                const auto& ret_named = ret->as<types::NamedType>();
                                if (ret_named.name == type_name &&
                                    ret_named.type_args.size() == class_def->type_params.size()) {
                                    // Build substitution map from method params
                                    std::map<std::string, types::TypePtr> subst;
                                    for (size_t pi = 0;
                                         pi < m.sig.params.size() && pi < call.args.size(); ++pi) {
                                        auto param_type = m.sig.params[pi];
                                        if (param_type && param_type->is<types::NamedType>()) {
                                            const auto& pn = param_type->as<types::NamedType>();
                                            // Check if param is a type param directly
                                            for (const auto& tp : class_def->type_params) {
                                                if (pn.name == tp && pn.type_args.empty()) {
                                                    subst[tp] = infer_expr_type(*call.args[pi]);
                                                }
                                            }
                                        }
                                    }
                                    // Apply substitution to return type args
                                    if (!subst.empty()) {
                                        std::vector<types::TypePtr> new_type_args;
                                        for (const auto& ta : ret_named.type_args) {
                                            if (ta && ta->is<types::NamedType>()) {
                                                const auto& tan = ta->as<types::NamedType>();
                                                auto sit = subst.find(tan.name);
                                                if (sit != subst.end() && tan.type_args.empty()) {
                                                    new_type_args.push_back(sit->second);
                                                    continue;
                                                }
                                            }
                                            new_type_args.push_back(ta);
                                        }
                                        auto result = std::make_shared<types::Type>();
                                        result->kind = types::NamedType{type_name, "",
                                                                        std::move(new_type_args)};
                                        return result;
                                    }
                                }
                            }
                            return ret;
                        }
                    }
                }

                // Check for impl static methods (like I32::try_from)
                // Look up qualified name in TypeEnv
                std::string qualified_name = type_name + "::" + method_name;
                auto func_sig = env_.lookup_func(qualified_name);
                if (func_sig.has_value()) {
                    return func_sig->return_type;
                }

                // Check module registry for library impl methods
                if (env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto func_it = mod.functions.find(qualified_name);
                        if (func_it != mod.functions.end()) {
                            return func_it->second.return_type;
                        }
                    }
                }

                // Check if it's an enum variant constructor (e.g., Expr::Num(1))
                // For generic enums, prefer the pending_generic_enums_ path which
                // correctly infers type arguments. Only use the simple path for
                // truly non-generic enums.
                {
                    bool is_generic_enum =
                        pending_generic_enums_.find(type_name) != pending_generic_enums_.end();
                    if (!is_generic_enum) {
                        // Non-generic enums: look up in TypeEnv
                        auto enum_def = env_.lookup_enum(type_name);
                        if (enum_def.has_value()) {
                            for (const auto& v : enum_def->variants) {
                                if (v.first == method_name) {
                                    auto result = std::make_shared<types::Type>();
                                    result->kind = types::NamedType{type_name, "", {}};
                                    return result;
                                }
                            }
                        }
                        // Also check all modules for enums
                        if (env_.module_registry()) {
                            const auto& all_modules = env_.module_registry()->get_all_modules();
                            for (const auto& [mod_name, mod] : all_modules) {
                                auto enum_it = mod.enums.find(type_name);
                                if (enum_it != mod.enums.end()) {
                                    for (const auto& v : enum_it->second.variants) {
                                        if (v.first == method_name) {
                                            auto result = std::make_shared<types::Type>();
                                            result->kind = types::NamedType{type_name, "", {}};
                                            return result;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                // Generic enums: check pending_generic_enums_
                auto gen_enum_it = pending_generic_enums_.find(type_name);
                if (gen_enum_it != pending_generic_enums_.end()) {
                    const auto& gen_enum_decl = *gen_enum_it->second;
                    for (const auto& variant : gen_enum_decl.variants) {
                        if (variant.name == method_name) {
                            // Infer type args from arguments
                            std::vector<types::TypePtr> type_args;
                            for (size_t g = 0; g < gen_enum_decl.generics.size(); ++g) {
                                const std::string& generic_name = gen_enum_decl.generics[g].name;
                                types::TypePtr inferred_type = nullptr;
                                if (variant.tuple_fields.has_value()) {
                                    for (size_t f = 0;
                                         f < variant.tuple_fields->size() && f < call.args.size();
                                         ++f) {
                                        const auto& field_type = (*variant.tuple_fields)[f];
                                        auto arg_inferred = infer_expr_type(*call.args[f]);
                                        auto extracted = extract_generic_from_nested_type(
                                            field_type, generic_name, arg_inferred);
                                        if (extracted) {
                                            inferred_type = extracted;
                                            break;
                                        }
                                        // Fallback: unwrap constructor calls to match
                                        // inner arguments against nested field type
                                        auto infer_fn = [this](const parser::Expr& e) {
                                            return infer_expr_type(e);
                                        };
                                        extracted = extract_generic_from_call_expr(
                                            field_type, generic_name, *call.args[f], infer_fn);
                                        if (extracted) {
                                            inferred_type = extracted;
                                            break;
                                        }
                                    }
                                }
                                if (!inferred_type) {
                                    inferred_type = types::make_unit();
                                }
                                type_args.push_back(inferred_type);
                            }
                            auto result = std::make_shared<types::Type>();
                            result->kind = types::NamedType{type_name, "", std::move(type_args)};
                            return result;
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
                                    auto arg_inferred = infer_expr_type(*call.args[f]);
                                    auto extracted = extract_generic_from_nested_type(
                                        field_type, generic_name, arg_inferred);
                                    if (extracted) {
                                        inferred_type = extracted;
                                        break;
                                    }
                                    // Fallback: unwrap constructor calls
                                    auto infer_fn = [this](const parser::Expr& e) {
                                        return infer_expr_type(e);
                                    };
                                    extracted = extract_generic_from_call_expr(
                                        field_type, generic_name, *call.args[f], infer_fn);
                                    if (extracted) {
                                        inferred_type = extracted;
                                        break;
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
                // For generic functions, the return type may contain unresolved
                // type parameters (stored as NamedType, not GenericType, by the
                // type checker). Infer concrete types from call arguments and
                // substitute into the return type. Without this, a variable
                // holding the return value of a generic function would have an
                // unresolved semantic type (e.g., List[Promise[T]] instead of
                // List[Promise[I32]]), causing downstream method dispatch to
                // emit wrong symbol names.
                if (!func_sig->type_params.empty()) {
                    auto gen_it = pending_generic_funcs_.find(callee_ident.name);
                    if (gen_it != pending_generic_funcs_.end()) {
                        const auto& gen_func = *gen_it->second;
                        std::unordered_set<std::string> generic_names;
                        for (const auto& g : gen_func.generics) {
                            generic_names.insert(g.name);
                        }
                        std::unordered_map<std::string, types::TypePtr> bindings;
                        for (size_t i = 0; i < call.args.size() && i < gen_func.params.size();
                             ++i) {
                            types::TypePtr arg_type = infer_expr_type(*call.args[i]);
                            if (arg_type && gen_func.params[i].type) {
                                unify_types(*gen_func.params[i].type, arg_type, generic_names,
                                            bindings);
                            }
                        }
                        if (!bindings.empty()) {
                            return types::substitute_type(func_sig->return_type, bindings);
                        }
                    }
                    // Inside a monomorphized generic function, apply
                    // current_type_subs_ to resolve type params from the
                    // enclosing scope.
                    if (!current_type_subs_.empty()) {
                        return apply_type_substitutions(func_sig->return_type, current_type_subs_);
                    }
                }
                return func_sig->return_type;
            }

            // Also check in module registry for qualified/aliased functions
            if (env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    auto func_it = mod.functions.find(callee_ident.name);
                    if (func_it != mod.functions.end()) {
                        // Same generic resolution for module registry lookups
                        if (!func_it->second.type_params.empty()) {
                            auto gen_it = pending_generic_funcs_.find(callee_ident.name);
                            if (gen_it != pending_generic_funcs_.end()) {
                                const auto& gen_func = *gen_it->second;
                                std::unordered_set<std::string> generic_names;
                                for (const auto& g : gen_func.generics) {
                                    generic_names.insert(g.name);
                                }
                                std::unordered_map<std::string, types::TypePtr> bindings;
                                for (size_t i = 0;
                                     i < call.args.size() && i < gen_func.params.size(); ++i) {
                                    types::TypePtr arg_type = infer_expr_type(*call.args[i]);
                                    if (arg_type && gen_func.params[i].type) {
                                        unify_types(*gen_func.params[i].type, arg_type,
                                                    generic_names, bindings);
                                    }
                                }
                                if (!bindings.empty()) {
                                    return types::substitute_type(func_it->second.return_type,
                                                                  bindings);
                                }
                            }
                            if (!current_type_subs_.empty()) {
                                return apply_type_substitutions(func_it->second.return_type,
                                                                current_type_subs_);
                            }
                        }
                        return func_it->second.return_type;
                    }
                }
            }
        }
    }
    return std::nullopt;
}

} // namespace tml::codegen
