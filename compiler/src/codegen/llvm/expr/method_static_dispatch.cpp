TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Static Method Dispatch
//!
//! This file handles Section 1 of gen_method_call: dispatching Type::method()
//! static method calls. This includes class static methods, primitive type
//! static methods, generic struct static methods, and imported type methods.
//!
//! Extracted from method.cpp to reduce file size.

#include "codegen/llvm/llvm_ir_gen.hpp"
#include "types/module.hpp"

#include <iostream>
#include <unordered_set>

namespace tml::codegen {

// Static helper to parse mangled type strings like "Mutex__I32" into proper TypePtr
// Duplicated from method.cpp as it's needed in both translation units
static types::TypePtr parse_mangled_type_string(const std::string& s) {
    if (s == "I64")
        return types::make_i64();
    if (s == "I32")
        return types::make_i32();
    if (s == "I8") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I8};
        return t;
    }
    if (s == "I16") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I16};
        return t;
    }
    if (s == "U8") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U8};
        return t;
    }
    if (s == "U16") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U16};
        return t;
    }
    if (s == "U32") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U32};
        return t;
    }
    if (s == "U64") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U64};
        return t;
    }
    if (s == "Usize") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::U64};
        return t;
    }
    if (s == "Isize") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::I64};
        return t;
    }
    if (s == "F32") {
        auto t = std::make_shared<types::Type>();
        t->kind = types::PrimitiveType{types::PrimitiveKind::F32};
        return t;
    }
    if (s == "F64")
        return types::make_f64();
    if (s == "Bool")
        return types::make_bool();
    if (s == "Str")
        return types::make_str();
    if (s.size() > 4 && s.substr(0, 4) == "ptr_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = false, .inner = inner};
            return t;
        }
    }
    if (s.size() > 7 && s.substr(0, 7) == "mutptr_") {
        std::string inner_str = s.substr(7);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.is_mut = true, .inner = inner};
            return t;
        }
    }
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
    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

auto LLVMIRGen::gen_method_static_dispatch(const parser::MethodCallExpr& call,
                                           const std::string& method)
    -> std::optional<std::string> {
    std::string type_name;
    bool has_type_name = false;

    if (call.receiver->is<parser::IdentExpr>()) {
        type_name = call.receiver->as<parser::IdentExpr>().name;
        has_type_name = true;
    } else if (call.receiver->is<parser::PathExpr>()) {
        const auto& path_expr = call.receiver->as<parser::PathExpr>();
        if (path_expr.path.segments.size() == 1) {
            type_name = path_expr.path.segments[0];
            has_type_name = true;
        }
    }

    if (has_type_name) {
        // Substitute type parameter with concrete type (e.g., T -> I64)
        // This handles T::default() in generic contexts (for MethodCallExpr)
        auto type_sub_it = current_type_subs_.find(type_name);
        if (type_sub_it != current_type_subs_.end()) {
            type_name = types::type_to_string(type_sub_it->second);
        } else if (!current_type_subs_.empty()) {
            // Handle trait method dispatch in generic contexts.
            // When type_name is a trait name (e.g., "Default", "Clone") rather than a
            // concrete type or type parameter name, the call `Trait::method()` inside
            // `impl[T: Trait] Trait for Wrapper[T]` should resolve to `T::method()`.
            // Example: Default::default() in impl[T: Default] → T::default() → I32::default()
            static const std::unordered_set<std::string> TRAIT_NAMES = {
                "Default",
                "Clone",
                "Duplicate",
                "Display",
                "Debug",
                "PartialEq",
                "Eq",
                "PartialOrd",
                "Ord",
                "Hash",
                "Deref",
                "DerefMut",
                "From",
                "Into",
                "TryFrom",
                "TryInto",
                "Iterator",
                "IntoIterator",
                "ExactSizeIterator",
                "ToString",
                "FromStr",
                "Copy",
                "Sized",
            };
            if (TRAIT_NAMES.count(type_name) > 0 && current_type_subs_.size() == 1) {
                type_name = types::type_to_string(current_type_subs_.begin()->second);
            }
        }

        // Handle primitive type static methods FIRST - before class lookup
        // This handles F32::zero(), I32::one(), U8::min_value(), etc.
        {
            auto is_primitive = [](const std::string& name) {
                return name == "I8" || name == "I16" || name == "I32" || name == "I64" ||
                       name == "I128" || name == "U8" || name == "U16" || name == "U32" ||
                       name == "U64" || name == "U128" || name == "F32" || name == "F64" ||
                       name == "Bool";
            };

            if (is_primitive(type_name)) {
                if (method == "from" || method == "zero" || method == "one" ||
                    method == "min_value" || method == "max_value" || method == "default") {
                    auto result = gen_static_method_call(call, type_name);
                    if (result) {
                        return *result;
                    }
                }
            }
        }

        // Check for @flags enum static methods (none, all, from_bits)
        {
            auto flags_it = flags_enums_.find(type_name);
            if (flags_it != flags_enums_.end() &&
                (method == "none" || method == "all" || method == "from_bits")) {
                std::string struct_type = "%struct." + type_name;
                std::string fn_name = "@" + mangle_impl_method(type_name, method);

                if (method == "from_bits" && !call.args.empty()) {
                    std::string arg = gen_expr(*call.args[0]);
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call " + struct_type + " " + fn_name + "(" +
                              flags_it->second.underlying_llvm_type + " " + arg + ")");
                    last_expr_type_ = struct_type;
                    return result;
                }
                // none() and all() take no arguments
                std::string result = fresh_reg();
                emit_line("  " + result + " = call " + struct_type + " " + fn_name + "()");
                last_expr_type_ = struct_type;
                return result;
            }
        }

        // Check for class static method call (ClassName.staticMethod())
        auto class_def = env_.lookup_class(type_name);
        if (class_def.has_value()) {
            // Look for static method
            for (const auto& m : class_def->methods) {
                if (m.sig.name == method && m.is_static) {
                    // For generic classes, extract type arguments and apply substitution
                    std::unordered_map<std::string, types::TypePtr> type_subs_local;
                    std::string mangled_type_suffix;

                    // Extract type args from PathExpr generics (e.g., LinkedList[I64].create())
                    if (call.receiver->is<parser::PathExpr>()) {
                        const auto& pe = call.receiver->as<parser::PathExpr>();
                        if (pe.generics.has_value() && !pe.generics->args.empty()) {
                            for (size_t i = 0;
                                 i < pe.generics->args.size() && i < class_def->type_params.size();
                                 ++i) {
                                const auto& arg = pe.generics->args[i];
                                if (arg.is_type()) {
                                    auto resolved = resolve_parser_type_with_subs(
                                        *arg.as_type(), current_type_subs_);
                                    if (resolved) {
                                        type_subs_local[class_def->type_params[i]] = resolved;
                                        mangled_type_suffix += "__" + mangle_type(resolved);
                                    }
                                }
                            }
                        }
                    }

                    // Generate call to static method with proper mangling
                    std::string func_name =
                        "@" + mangle_impl_method(type_name + mangled_type_suffix, method);

                    // Apply type substitution to return type
                    types::TypePtr return_type = m.sig.return_type;
                    if (!type_subs_local.empty()) {
                        return_type = types::substitute_type(return_type, type_subs_local);
                    }
                    std::string ret_type = llvm_type_from_semantic(return_type);

                    // Generate arguments
                    std::vector<std::string> args;
                    std::vector<std::string> arg_types;
                    for (const auto& arg : call.args) {
                        args.push_back(gen_expr(*arg));
                        arg_types.push_back(last_expr_type_);
                    }

                    // Generate call
                    std::string result = fresh_reg();
                    std::string call_str =
                        "  " + result + " = call " + ret_type + " " + func_name + "(";
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (i > 0)
                            call_str += ", ";
                        call_str += arg_types[i] + " " + args[i];
                    }
                    call_str += ")";
                    emit_line(call_str);

                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        }

        // Handle primitive type static methods early - before generic struct handling
        // This intercepts calls like F32::zero(), I32::one(), I8::min_value(), etc.
        {
            auto is_primitive = [](const std::string& name) {
                return name == "I8" || name == "I16" || name == "I32" || name == "I64" ||
                       name == "I128" || name == "U8" || name == "U16" || name == "U32" ||
                       name == "U64" || name == "U128" || name == "F32" || name == "F64" ||
                       name == "Bool";
            };

            // Handle from(), zero(), one(), min_value(), max_value() for primitive types
            if (is_primitive(type_name)) {
                if (method == "from" || method == "zero" || method == "one" ||
                    method == "min_value" || method == "max_value" || method == "default") {
                    auto result = gen_static_method_call(call, type_name);
                    if (result) {
                        return *result;
                    }
                }
            }
        }

        // Check if this is a generic struct/enum from:
        // 1. Local pending_generic_structs_, pending_generic_enums_, or pending_generic_impls_
        // 2. Imported structs/enums from module registry with type_params
        // 3. Method call has explicit type arguments (e.g., StackNode::new[T])
        // NOTE: All collection types (List, HashMap, Buffer) are now pure TML
        bool is_generic_struct =
            (pending_generic_structs_.count(type_name) > 0 ||
             pending_generic_enums_.count(type_name) > 0 ||
             pending_generic_impls_.count(type_name) > 0 ||
             !call.type_args.empty()); // Also treat calls with explicit type args as generic

        // Also check for imported generic structs and enums (except runtime collections)
        // Note: We search module registry even when is_generic_struct is true due to explicit
        // type args, because we need to find the generic parameter names (e.g., T) for type_subs
        std::vector<std::string> imported_type_params;
        bool is_local_generic = pending_generic_structs_.count(type_name) > 0 ||
                                pending_generic_enums_.count(type_name) > 0 ||
                                pending_generic_impls_.count(type_name) > 0;
        // DEBUG: log type_name when handling generic struct calls for Range types
        if (type_name == "Range" || type_name == "RangeInclusive") {
            TML_LOG_TRACE("codegen",
                          "[DEBUG] type_name="
                              << type_name << " is_local_generic=" << is_local_generic
                              << " has_registry=" << (env_.module_registry() ? "yes" : "no"));
        }
        if (!is_local_generic && env_.module_registry()) {
            TML_DEBUG_LN("[STATIC_METHOD] Looking for " << type_name << " in module registry");

            // First, try to resolve via imported symbols to get the correct module path
            // This is crucial when multiple modules export the same type name (e.g., Range)
            std::string resolved_module_path;
            auto resolved = env_.resolve_imported_symbol(type_name);
            if (resolved) {
                // Full path like "core::ops::range::Range" -> module is "core::ops::range"
                resolved_module_path = *resolved;
                auto last_sep = resolved_module_path.rfind("::");
                if (last_sep != std::string::npos) {
                    resolved_module_path = resolved_module_path.substr(0, last_sep);
                }
                TML_DEBUG_LN("[STATIC_METHOD] Resolved " << type_name << " to module "
                                                         << resolved_module_path);
            }

            const auto& all_modules = env_.module_registry()->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                // If we resolved a specific module, only check that one
                if (!resolved_module_path.empty() && mod_name != resolved_module_path) {
                    continue;
                }

                // Check structs
                auto struct_it = mod.structs.find(type_name);
                if (struct_it != mod.structs.end()) {
                    TML_DEBUG_LN("[STATIC_METHOD] Found " << type_name << " in " << mod_name
                                                          << " with type_params.size="
                                                          << struct_it->second.type_params.size());
                    if (!struct_it->second.type_params.empty()) {
                        is_generic_struct = true;
                        imported_type_params = struct_it->second.type_params;
                        break;
                    }
                }
                // Check enums
                auto enum_it = mod.enums.find(type_name);
                if (enum_it != mod.enums.end()) {
                    TML_DEBUG_LN("[STATIC_METHOD] Found enum "
                                 << type_name << " in " << mod_name << " with type_params.size="
                                 << enum_it->second.type_params.size());
                    if (!enum_it->second.type_params.empty()) {
                        is_generic_struct = true;
                        imported_type_params = enum_it->second.type_params;
                        break;
                    }
                }
                // Check classes (pub type declarations)
                auto class_it = mod.classes.find(type_name);
                if (class_it != mod.classes.end()) {
                    TML_DEBUG_LN("[STATIC_METHOD] Found class "
                                 << type_name << " in " << mod_name << " with type_params.size="
                                 << class_it->second.type_params.size());
                    if (!class_it->second.type_params.empty()) {
                        is_generic_struct = true;
                        imported_type_params = class_it->second.type_params;
                        break;
                    }
                }
            }
        }

        // For generic struct static methods (like Range::new), use expected_enum_type_ for type
        // args. Also handle calls with explicit type args even if struct definition wasn't found.
        TML_DEBUG_LN("[STATIC_METHOD] type_name="
                     << type_name << " method=" << method << " is_generic_struct="
                     << is_generic_struct << " call.type_args.empty()=" << call.type_args.empty());
        if ((is_generic_struct || !call.type_args.empty()) && locals_.count(type_name) == 0) {
            // Look up the impl method and generate the monomorphized call
            std::string qualified_name = type_name + "::" + method;
            auto func_sig = env_.lookup_func(qualified_name);
            TML_DEBUG_LN("[STATIC_METHOD] qualified_name="
                         << qualified_name
                         << " func_sig=" << (func_sig.has_value() ? "found" : "null"));

            // If not found locally, search modules
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

            // Determine the type arguments from explicit generics on PathExpr or
            // expected_enum_type_ This is done regardless of func_sig so local generic structs also
            // get type_subs
            std::string mangled_type_name = type_name;
            std::unordered_map<std::string, types::TypePtr> type_subs;

            // Helper to create primitive types
            auto make_prim = [](types::PrimitiveKind kind) -> types::TypePtr {
                auto t = std::make_shared<types::Type>();
                t->kind = types::PrimitiveType{kind};
                return t;
            };

            // Helper to convert type arg string to TypePtr
            auto str_to_type = [&make_prim](const std::string& type_arg_str) -> types::TypePtr {
                if (type_arg_str == "I64")
                    return types::make_i64();
                if (type_arg_str == "I32")
                    return types::make_i32();
                if (type_arg_str == "I8")
                    return make_prim(types::PrimitiveKind::I8);
                if (type_arg_str == "I16")
                    return make_prim(types::PrimitiveKind::I16);
                if (type_arg_str == "U8")
                    return make_prim(types::PrimitiveKind::U8);
                if (type_arg_str == "U16")
                    return make_prim(types::PrimitiveKind::U16);
                if (type_arg_str == "U32")
                    return make_prim(types::PrimitiveKind::U32);
                if (type_arg_str == "U64")
                    return make_prim(types::PrimitiveKind::U64);
                if (type_arg_str == "F32")
                    return make_prim(types::PrimitiveKind::F32);
                if (type_arg_str == "F64")
                    return types::make_f64();
                if (type_arg_str == "Bool")
                    return types::make_bool();
                if (type_arg_str == "Str")
                    return types::make_str();
                // Handle mangled pointer types: ptr_I32 -> PtrType{I32}
                if (type_arg_str.starts_with("ptr_")) {
                    std::string inner_str = type_arg_str.substr(4);
                    // Recursively parse the inner type
                    types::TypePtr inner = nullptr;
                    // Check for primitives first
                    if (inner_str == "I32")
                        inner = types::make_i32();
                    else if (inner_str == "I64")
                        inner = types::make_i64();
                    else if (inner_str == "I8")
                        inner = make_prim(types::PrimitiveKind::I8);
                    else if (inner_str == "I16")
                        inner = make_prim(types::PrimitiveKind::I16);
                    else if (inner_str == "U8")
                        inner = make_prim(types::PrimitiveKind::U8);
                    else if (inner_str == "U16")
                        inner = make_prim(types::PrimitiveKind::U16);
                    else if (inner_str == "U32")
                        inner = make_prim(types::PrimitiveKind::U32);
                    else if (inner_str == "U64")
                        inner = make_prim(types::PrimitiveKind::U64);
                    else if (inner_str == "F32")
                        inner = make_prim(types::PrimitiveKind::F32);
                    else if (inner_str == "F64")
                        inner = types::make_f64();
                    else if (inner_str == "Bool")
                        inner = types::make_bool();
                    else if (inner_str == "Str")
                        inner = types::make_str();
                    else {
                        // For struct types as inner (including nested generics like
                        // ChannelNode__I32)
                        inner = parse_mangled_type_string(inner_str);
                    }
                    auto t = std::make_shared<types::Type>();
                    t->kind = types::PtrType{false, inner};
                    return t;
                }
                // Handle mangled mutable pointer types: mutptr_I32 -> PtrType{mut, I32}
                if (type_arg_str.starts_with("mutptr_")) {
                    std::string inner_str = type_arg_str.substr(7);
                    types::TypePtr inner = nullptr;
                    if (inner_str == "I32")
                        inner = types::make_i32();
                    else if (inner_str == "I64")
                        inner = types::make_i64();
                    else if (inner_str == "I8")
                        inner = make_prim(types::PrimitiveKind::I8);
                    else if (inner_str == "I16")
                        inner = make_prim(types::PrimitiveKind::I16);
                    else if (inner_str == "U8")
                        inner = make_prim(types::PrimitiveKind::U8);
                    else if (inner_str == "U16")
                        inner = make_prim(types::PrimitiveKind::U16);
                    else if (inner_str == "U32")
                        inner = make_prim(types::PrimitiveKind::U32);
                    else if (inner_str == "U64")
                        inner = make_prim(types::PrimitiveKind::U64);
                    else if (inner_str == "F32")
                        inner = make_prim(types::PrimitiveKind::F32);
                    else if (inner_str == "F64")
                        inner = types::make_f64();
                    else if (inner_str == "Bool")
                        inner = types::make_bool();
                    else if (inner_str == "Str")
                        inner = types::make_str();
                    else {
                        // For struct types as inner (including nested generics like
                        // ChannelNode__I32)
                        inner = parse_mangled_type_string(inner_str);
                    }
                    auto t = std::make_shared<types::Type>();
                    t->kind = types::PtrType{true, inner};
                    return t;
                }
                // Handle nested generic types like Mutex__I32 -> NamedType{name="Mutex",
                // type_args=[I32]} Use static helper function for efficiency (avoids std::function
                // overhead)
                auto delim_pos = type_arg_str.find("__");
                if (delim_pos != std::string::npos) {
                    std::string base_name = type_arg_str.substr(0, delim_pos);
                    std::string type_arg_suffix = type_arg_str.substr(delim_pos + 2);
                    auto inner_type = parse_mangled_type_string(type_arg_suffix);
                    if (inner_type) {
                        auto t = std::make_shared<types::Type>();
                        t->kind = types::NamedType{base_name, "", {inner_type}};
                        return t;
                    }
                }

                // For struct types, use parse_mangled_type_string for proper handling
                return parse_mangled_type_string(type_arg_str);
            };

            // First, try to extract type args from explicit generics on the PathExpr (e.g.,
            // List[I32].new())
            if (call.receiver->is<parser::PathExpr>()) {
                const auto& pe = call.receiver->as<parser::PathExpr>();
                if (pe.generics.has_value() && !pe.generics->args.empty()) {
                    // Build type substitutions from explicit type args
                    std::vector<std::string> generic_names;
                    auto impl_it = pending_generic_impls_.find(type_name);
                    if (impl_it != pending_generic_impls_.end()) {
                        for (const auto& g : impl_it->second->generics) {
                            generic_names.push_back(g.name);
                        }
                    }
                    // Also check pending_generic_structs_ for imported generic structs
                    if (generic_names.empty()) {
                        auto struct_it = pending_generic_structs_.find(type_name);
                        if (struct_it != pending_generic_structs_.end()) {
                            for (const auto& g : struct_it->second->generics) {
                                generic_names.push_back(g.name);
                            }
                        }
                    }
                    if (generic_names.empty() && !imported_type_params.empty()) {
                        generic_names = imported_type_params;
                    }

                    // Build mangled name and type_subs from explicit generics
                    for (size_t i = 0; i < pe.generics->args.size(); ++i) {
                        const auto& arg = pe.generics->args[i];
                        if (arg.is_const && arg.is_expr()) {
                            // Const generic argument (e.g., 3 in ArrayIter[I32, 3])
                            int64_t const_val = 0;
                            const auto& expr = arg.as_expr();
                            if (expr && expr->is<parser::LiteralExpr>()) {
                                const auto& lit = expr->as<parser::LiteralExpr>();
                                if (lit.token.kind == lexer::TokenKind::IntLiteral) {
                                    const_val = lit.token.int_value().value;
                                }
                            }
                            auto const_type = std::make_shared<types::Type>();
                            const_type->kind = types::ConstGenericType{
                                std::to_string(const_val), types::make_i64(), const_val};
                            mangled_type_name += "__" + std::to_string(const_val);
                            if (i < generic_names.size()) {
                                type_subs[generic_names[i]] = const_type;
                            }
                        } else if (arg.is_type()) {
                            // Resolve type argument using current_type_subs_ (handles T -> I32)
                            auto resolved =
                                resolve_parser_type_with_subs(*arg.as_type(), current_type_subs_);
                            if (resolved) {
                                std::string type_arg_str = mangle_type(resolved);
                                mangled_type_name += "__" + type_arg_str;
                                if (i < generic_names.size()) {
                                    type_subs[generic_names[i]] = resolved;
                                }
                            }
                        }
                    }
                }
            }

            // Handle method-level type arguments (e.g., StackNode::new[T])
            // Resolve type parameters using current_type_subs_
            if (!call.type_args.empty()) {
                // Get generic parameter names for this method
                std::vector<std::string> generic_names;
                auto impl_it = pending_generic_impls_.find(type_name);
                if (impl_it != pending_generic_impls_.end()) {
                    for (const auto& g : impl_it->second->generics) {
                        generic_names.push_back(g.name);
                    }
                } else if (!imported_type_params.empty()) {
                    generic_names = imported_type_params;
                }

                // Build mangled name and type_subs from method type args
                for (size_t i = 0; i < call.type_args.size(); ++i) {
                    // Resolve type argument using current_type_subs_ (handles T -> I32)
                    auto resolved =
                        resolve_parser_type_with_subs(*call.type_args[i], current_type_subs_);
                    if (resolved) {
                        std::string type_arg_str = mangle_type(resolved);
                        mangled_type_name += "__" + type_arg_str;
                        if (i < generic_names.size()) {
                            type_subs[generic_names[i]] = resolved;
                        } else {
                            // For unknown generic names (internal types), use positional
                            // placeholder This ensures type_subs is non-empty so the fallback code
                            // path works
                            type_subs["_T" + std::to_string(i)] = resolved;
                        }
                    }
                }
            }

            // Fall back to expected_enum_type_ if no explicit generics found
            TML_DEBUG_LN("[STATIC_METHOD] expected_enum_type_ check: type_name="
                         << type_name << " expected_enum_type_=" << expected_enum_type_
                         << " type_subs.empty()=" << type_subs.empty());
            if (type_subs.empty() && !expected_enum_type_.empty() &&
                expected_enum_type_.find("%struct." + type_name + "__") == 0) {
                // Extract type args from expected_enum_type_ like "%struct.Range__I64"
                mangled_type_name = expected_enum_type_.substr(8); // Remove "%struct."

                // Build type substitutions: count expected type args from the suffix,
                // then find the impl with the matching number of generics AND the method.
                std::string suffix = mangled_type_name.substr(type_name.length());
                size_t expected_params = 0;
                if (suffix.starts_with("__")) {
                    // Count "__" separators to determine number of type args
                    std::string args_str2 = suffix.substr(2);
                    expected_params = 1;
                    size_t pos2 = 0;
                    while (pos2 < args_str2.size()) {
                        size_t nxt = args_str2.find("__", pos2);
                        if (nxt == std::string::npos)
                            break;
                        expected_params++;
                        pos2 = nxt + 2;
                    }
                }

                std::vector<std::string> generic_names;
                // Search all impls for one that has the right method + param count
                auto all_it = pending_generic_impls_all_.find(type_name);
                if (all_it != pending_generic_impls_all_.end()) {
                    for (const auto* cand : all_it->second) {
                        if (expected_params > 0 && cand->generics.size() != expected_params)
                            continue;
                        // Prefer impls that contain the called method
                        bool has_method = false;
                        for (const auto& m : cand->methods) {
                            if (m.name == method) {
                                has_method = true;
                                break;
                            }
                        }
                        if (has_method || generic_names.empty()) {
                            generic_names.clear();
                            for (const auto& g : cand->generics)
                                generic_names.push_back(g.name);
                            if (has_method)
                                break;
                        }
                    }
                }
                if (generic_names.empty()) {
                    auto impl_it = pending_generic_impls_.find(type_name);
                    if (impl_it != pending_generic_impls_.end()) {
                        for (const auto& g : impl_it->second->generics)
                            generic_names.push_back(g.name);
                    }
                }
                if (generic_names.empty() && !imported_type_params.empty()) {
                    generic_names = imported_type_params;
                }
                if (suffix.starts_with("__") && !generic_names.empty()) {
                    std::string args_str = suffix.substr(2); // Drop leading "__"
                    if (generic_names.size() == 1) {
                        types::TypePtr type_arg = str_to_type(args_str);
                        if (type_arg) {
                            type_subs[generic_names[0]] = type_arg;
                        }
                    } else {
                        // Multiple type params — split on "__" and map positionally
                        std::vector<std::string> parts;
                        size_t pos = 0;
                        while (pos < args_str.size()) {
                            size_t next = args_str.find("__", pos);
                            if (next == std::string::npos) {
                                parts.push_back(args_str.substr(pos));
                                break;
                            }
                            parts.push_back(args_str.substr(pos, next - pos));
                            pos = next + 2;
                        }
                        for (size_t i = 0; i < generic_names.size() && i < parts.size(); ++i) {
                            types::TypePtr type_arg = str_to_type(parts[i]);
                            if (type_arg) {
                                type_subs[generic_names[i]] = type_arg;
                            }
                        }
                    }
                }
            }

            // Infer type arguments from actual arguments when type_subs is still empty
            // This handles cases like Mutex::new(42) where T should be inferred from arg type
            if (type_subs.empty() && func_sig && !call.args.empty()) {
                // Get generic parameter names for this type
                std::vector<std::string> generic_names;
                auto impl_it = pending_generic_impls_.find(type_name);
                if (impl_it != pending_generic_impls_.end()) {
                    for (const auto& g : impl_it->second->generics) {
                        generic_names.push_back(g.name);
                    }
                } else if (!imported_type_params.empty()) {
                    generic_names = imported_type_params;
                }

                // Also use method-level generic type params (e.g., sum[I: Iterator])
                // when type-level params didn't provide any names.
                if (generic_names.empty() && !func_sig->type_params.empty()) {
                    generic_names = func_sig->type_params;
                }

                // Helper to check if a type is a generic parameter
                auto is_generic_param = [&generic_names](const types::TypePtr& t) -> std::string {
                    if (!t)
                        return "";
                    // Check for explicit GenericType
                    if (t->is<types::GenericType>()) {
                        return t->as<types::GenericType>().name;
                    }
                    // Check for NamedType with name matching a generic param
                    // (generic params are often stored as NamedType with no type_args)
                    if (t->is<types::NamedType>()) {
                        const auto& named = t->as<types::NamedType>();
                        if (named.type_args.empty()) {
                            for (const auto& gname : generic_names) {
                                if (named.name == gname) {
                                    return gname;
                                }
                            }
                        }
                    }
                    return "";
                };

                // For each argument, check if its corresponding parameter is a generic type
                // If so, infer the type from the argument
                for (size_t i = 0; i < call.args.size() && i < func_sig->params.size(); ++i) {
                    auto param_type = func_sig->params[i];
                    std::string param_name = is_generic_param(param_type);
                    if (!param_name.empty() && type_subs.find(param_name) == type_subs.end()) {
                        // Infer type from argument expression
                        auto arg_type = infer_expr_type(*call.args[i]);
                        if (arg_type) {
                            type_subs[param_name] = arg_type;
                            mangled_type_name += "__" + mangle_type(arg_type);
                            TML_DEBUG_LN("[STATIC_METHOD] Inferred " << param_name << " = "
                                                                     << mangle_type(arg_type)
                                                                     << " from argument " << i);
                        }
                    }
                }

                // If still empty, try matching ref/mut ref NamedType[T] params against
                // ref/mut ref NamedType[X] args (e.g., ManuallyDrop::take(mut ref md))
                if (type_subs.empty() && !generic_names.empty()) {
                    for (size_t i = 0; i < call.args.size() && i < func_sig->params.size(); ++i) {
                        auto param_type = func_sig->params[i];
                        if (!param_type)
                            continue;

                        // Unwrap RefType from param
                        types::TypePtr param_inner = param_type;
                        if (param_type->is<types::RefType>()) {
                            param_inner = param_type->as<types::RefType>().inner;
                        }
                        if (!param_inner || !param_inner->is<types::NamedType>())
                            continue;
                        const auto& param_named = param_inner->as<types::NamedType>();

                        // Case 1: ref NamedType[T] params (e.g., ManuallyDrop::take(mut ref md))
                        if (!param_named.type_args.empty()) {
                            // Get arg type and unwrap RefType
                            auto arg_type = infer_expr_type(*call.args[i]);
                            if (!arg_type)
                                continue;
                            types::TypePtr arg_inner = arg_type;
                            if (arg_type->is<types::RefType>()) {
                                arg_inner = arg_type->as<types::RefType>().inner;
                            }
                            if (!arg_inner || !arg_inner->is<types::NamedType>())
                                continue;
                            const auto& arg_named = arg_inner->as<types::NamedType>();

                            if (arg_named.name == param_named.name &&
                                !arg_named.type_args.empty() &&
                                arg_named.type_args.size() == param_named.type_args.size()) {
                                for (size_t j = 0;
                                     j < generic_names.size() && j < arg_named.type_args.size();
                                     ++j) {
                                    type_subs[generic_names[j]] = arg_named.type_args[j];
                                }
                                if (!type_subs.empty()) {
                                    mangled_type_name =
                                        type_name + "__" + mangle_type(arg_named.type_args[0]);
                                }
                                break;
                            }
                            continue;
                        }

                        // Case 2: ref T or mut ref T where T is a bare generic param
                        // (e.g., Pin::new(pointer: mut ref T) -> Pin[mut ref T])
                        // Use func_sig->type_params for matching since the param names
                        // may differ from struct-level generic_names (T vs P).
                        if (param_type->is<types::RefType>() && func_sig) {
                            std::string matched_param;
                            for (const auto& tp : func_sig->type_params) {
                                if (param_named.name == tp) {
                                    matched_param = tp;
                                    break;
                                }
                            }
                            if (!matched_param.empty() &&
                                type_subs.find(matched_param) == type_subs.end()) {
                                auto arg_type = infer_expr_type(*call.args[i]);
                                if (!arg_type)
                                    continue;
                                types::TypePtr arg_inner = arg_type;
                                if (arg_type->is<types::RefType>()) {
                                    arg_inner = arg_type->as<types::RefType>().inner;
                                }
                                if (arg_inner) {
                                    type_subs[matched_param] = arg_inner;
                                    mangled_type_name += "__" + mangle_type(arg_inner);
                                    TML_DEBUG_LN("[STATIC_METHOD] Inferred "
                                                 << matched_param << " = " << mangle_type(arg_inner)
                                                 << " from ref-wrapped argument " << i);
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            // Fallback: If func_sig is null but we have arguments and a single type parameter,
            // infer the type from the first argument. This handles imported generic types
            // (especially enums) where the method signature isn't in the function registry.
            if (type_subs.empty() && !func_sig && !call.args.empty() &&
                imported_type_params.size() == 1) {
                // Infer from first argument
                auto arg_type = infer_expr_type(*call.args[0]);
                if (arg_type) {
                    type_subs[imported_type_params[0]] = arg_type;
                    mangled_type_name += "__" + mangle_type(arg_type);
                    TML_DEBUG_LN("[STATIC_METHOD] Fallback inferred "
                                 << imported_type_params[0] << " = " << mangle_type(arg_type)
                                 << " from first argument");
                }
            }

            // Determine if this is an imported library type (for suite prefix decisions)
            bool is_imported = !imported_type_params.empty();

            // Also check if this is a non-generic imported type (e.g., Text::from)
            // or an enum (e.g., AddressFamily::to_raw)
            if (!is_imported && env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();
                for (const auto& [mod_name, mod] : all_modules) {
                    auto struct_it = mod.structs.find(type_name);
                    if (struct_it != mod.structs.end()) {
                        is_imported = true;
                        break;
                    }
                    auto enum_it = mod.enums.find(type_name);
                    if (enum_it != mod.enums.end()) {
                        is_imported = true;
                        break;
                    }
                }
            }

            // Built-in generic enums (Maybe, Outcome, Ordering, etc.) are created
            // synthetically and don't appear in any module's enums map.
            // Recognize them as imported so their impl methods get instantiated.
            if (!is_imported && pending_generic_enums_.count(type_name) > 0) {
                is_imported = true;
            }

            // If the library already emitted methods using the unmangled base name
            // (e.g., tml_BTreeMap_create from gen_impl_method), use the base name
            // so user code calls the existing function instead of a non-existent mangled one.
            {
                std::string base_fn_check = "@" + mangle_impl_method(type_name, method);
                if (mangled_type_name != type_name &&
                    generated_functions_.count(base_fn_check) > 0) {
                    mangled_type_name = type_name;
                }
            }

            // Request impl method instantiation if needed
            // This must be done regardless of func_sig to handle local generic structs
            std::string mangled_method_name = mangle_impl_method(mangled_type_name, method);
            if (generated_impl_methods_.find(mangled_method_name) ==
                generated_impl_methods_.end()) {
                // For both local and imported generic impls, request instantiation
                auto impl_it = pending_generic_impls_.find(type_name);
                bool is_local = impl_it != pending_generic_impls_.end();
                if (is_local || is_imported) {
                    pending_impl_method_instantiations_.push_back(
                        PendingImplMethod{mangled_type_name, method, type_subs, type_name, "",
                                          /*is_library_type=*/is_imported});
                    generated_impl_methods_.insert(mangled_method_name);
                }
            }

            TML_DEBUG_LN("[STATIC_METHOD] Before func_sig check: mangled_type_name="
                         << mangled_type_name << " type_subs.size=" << type_subs.size()
                         << " call.type_args.size=" << call.type_args.size());
            if (func_sig) {
                TML_DEBUG_LN("[STATIC_METHOD] Using func_sig path");
                // Generate the static method call
                // Look up in functions_ to get the correct LLVM name (handles suite prefix
                // correctly)
                std::string method_lookup_key = mangled_type_name + "_" + method;
                auto method_it = functions_.find(method_lookup_key);
                std::string fn_name;
                if (method_it != functions_.end()) {
                    fn_name = method_it->second.llvm_name;
                } else {
                    fn_name = "@" + mangle_impl_method(mangled_type_name, method);
                }

                // Generate arguments (no receiver for static methods)
                std::vector<std::pair<std::string, std::string>> typed_args;
                for (size_t i = 0; i < call.args.size(); ++i) {
                    // Set expected type context before generating argument
                    // This helps with type inference for nested generic calls
                    std::string saved_expected_enum = expected_enum_type_;
                    if (func_sig && i < func_sig->params.size()) {
                        auto param_type = func_sig->params[i];
                        if (!type_subs.empty()) {
                            param_type = types::substitute_type(param_type, type_subs);
                        }
                        std::string llvm_param_type = llvm_type_from_semantic(param_type);
                        // Set expected type for generic struct arguments
                        if (llvm_param_type.find("%struct.") == 0 &&
                            llvm_param_type.find("__") != std::string::npos) {
                            expected_enum_type_ = llvm_param_type;
                        }
                    }

                    std::string val = gen_expr(*call.args[i]);
                    expected_enum_type_ = saved_expected_enum;
                    std::string arg_type = last_expr_type_;
                    if (func_sig && i < func_sig->params.size()) {
                        auto param_type = func_sig->params[i];
                        if (!type_subs.empty()) {
                            param_type = types::substitute_type(param_type, type_subs);
                        }
                        arg_type = llvm_type_from_semantic(param_type);
                    }
                    // Check if the function definition expects ptr but we have a
                    // struct/enum by value (ABI mismatch from impl.cpp receiver
                    // convention). Use the registered FuncInfo param types as source
                    // of truth, falling back to the impl.cpp:392-395 rule for the
                    // first non-self param when the FuncInfo is not yet registered
                    // (generic instantiations are queued during arg processing and
                    // registered only when `gen_impl_method_instantiation` runs).
                    bool is_aggregate_arg =
                        arg_type.find("%struct.") == 0 || arg_type.find("%enum.") == 0;
                    bool needs_struct_to_ptr = false;
                    if (method_it != functions_.end() && i < method_it->second.param_types.size()) {
                        needs_struct_to_ptr =
                            method_it->second.param_types[i] == "ptr" && is_aggregate_arg;
                    } else if (method_it == functions_.end() && i == 0 && is_aggregate_arg) {
                        // Mirror impl.cpp:392-395: first non-self struct/enum param
                        // is lowered to `ptr` in the LLVM definition.
                        needs_struct_to_ptr = true;
                    }
                    if (needs_struct_to_ptr) {
                        std::string temp = fresh_reg();
                        emit_line("  " + temp + " = alloca " + arg_type);
                        emit_line("  store " + arg_type + " " + val + ", ptr " + temp);
                        val = temp;
                        arg_type = "ptr";
                    }
                    typed_args.push_back({arg_type, val});
                }

                // Fill in default values for missing arguments.
                // This handles cases like List[T]::new() where the TML source
                // calls with 0 args but the definition has 1 param (initial_capacity).
                // Without this, the LLVM IR has mismatched call/definition signatures
                // which causes undefined behavior (rcx reuse on Win64).
                if (func_sig && typed_args.size() < func_sig->params.size()) {
                    for (size_t i = typed_args.size(); i < func_sig->params.size(); ++i) {
                        auto param_type = func_sig->params[i];
                        if (!type_subs.empty()) {
                            param_type = types::substitute_type(param_type, type_subs);
                        }
                        std::string llvm_type = llvm_type_from_semantic(param_type);
                        // Use zero/null as default for missing arguments
                        if (llvm_type == "ptr") {
                            typed_args.push_back({"ptr", "null"});
                        } else if (llvm_type == "i64") {
                            typed_args.push_back({"i64", "0"});
                        } else if (llvm_type == "i32") {
                            typed_args.push_back({"i32", "0"});
                        } else if (llvm_type == "double") {
                            typed_args.push_back({"double", "0.0"});
                        } else if (llvm_type == "i1") {
                            typed_args.push_back({"i1", "false"});
                        } else {
                            // For struct/enum types, use zeroinitializer
                            typed_args.push_back({llvm_type, "zeroinitializer"});
                        }
                    }
                }

                auto return_type = func_sig->return_type;
                if (!type_subs.empty()) {
                    return_type = types::substitute_type(return_type, type_subs);
                }
                std::string ret_type = llvm_type_from_semantic(return_type);
                // Store semantic return type for correct downstream type resolution
                // (e.g., let pinned = Pin::new(mut ref f) needs Pin[mut ref Ready[I32]])
                last_semantic_type_ = return_type;

                std::string args_str;
                for (size_t i = 0; i < typed_args.size(); ++i) {
                    if (i > 0)
                        args_str += ", ";
                    args_str += typed_args[i].first + " " + typed_args[i].second;
                }

                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + fn_name + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            } else {
                // func_sig is null but we have a local generic struct
                // Generate the call using inferred types from arguments
                auto impl_it = pending_generic_impls_.find(type_name);
                if (impl_it != pending_generic_impls_.end()) {
                    // Find the method in the impl block to get return type
                    for (const auto& m : impl_it->second->methods) {
                        if (m.name == method) {
                            // Get function name
                            std::string fn_name;
                            auto method_it = functions_.find(mangled_type_name + "_" + method);
                            if (method_it != functions_.end()) {
                                fn_name = method_it->second.llvm_name;
                            } else {
                                fn_name = "@" + mangle_impl_method(mangled_type_name, method);
                            }

                            // Generate arguments using inferred types
                            std::vector<std::pair<std::string, std::string>> typed_args;
                            for (size_t i = 0; i < call.args.size(); ++i) {
                                std::string val = gen_expr(*call.args[i]);
                                std::string arg_type = last_expr_type_;
                                // Apply type substitution to parameter type
                                if (i < m.params.size()) {
                                    auto param_type =
                                        resolve_parser_type_with_subs(*m.params[i].type, type_subs);
                                    arg_type = llvm_type_from_semantic(param_type);
                                }
                                // struct/enum → ptr ABI fix (see impl.cpp:282)
                                if (method_it != functions_.end() &&
                                    i < method_it->second.param_types.size()) {
                                    const auto& expected = method_it->second.param_types[i];
                                    if (expected == "ptr" && (arg_type.find("%struct.") == 0 ||
                                                              arg_type.find("%enum.") == 0)) {
                                        std::string temp = fresh_reg();
                                        emit_line("  " + temp + " = alloca " + arg_type);
                                        emit_line("  store " + arg_type + " " + val + ", ptr " +
                                                  temp);
                                        val = temp;
                                        arg_type = "ptr";
                                    }
                                }
                                typed_args.push_back({arg_type, val});
                            }

                            // Get return type with substitution
                            std::string ret_type = "void";
                            if (m.return_type.has_value()) {
                                auto return_type =
                                    resolve_parser_type_with_subs(**m.return_type, type_subs);
                                ret_type = llvm_type_from_semantic(return_type);
                            }

                            std::string args_str;
                            for (size_t i = 0; i < typed_args.size(); ++i) {
                                if (i > 0)
                                    args_str += ", ";
                                args_str += typed_args[i].first + " " + typed_args[i].second;
                            }

                            std::string result = fresh_reg();
                            if (ret_type == "void") {
                                emit_line("  call void " + fn_name + "(" + args_str + ")");
                                last_expr_type_ = "void";
                                return "void";
                            } else {
                                emit_line("  " + result + " = call " + ret_type + " " + fn_name +
                                          "(" + args_str + ")");
                                last_expr_type_ = ret_type;
                                return result;
                            }
                        }
                    }
                }

                // Fallback: If we have type_args but no func_sig or pending impl, still generate
                // type-mangled call. This handles internal types like StackNode from imported
                // modules.
                TML_DEBUG_LN("[STATIC_METHOD] Fallback check: call.type_args.empty()="
                             << call.type_args.empty()
                             << " type_subs.empty()=" << type_subs.empty());
                if (!call.type_args.empty() && !type_subs.empty()) {
                    TML_DEBUG_LN("[STATIC_METHOD] Using fallback path for " << type_name
                                                                            << "::" << method);
                    // mangled_type_name should already include the type suffix from the type_args
                    // handling above
                    std::string fn_name = "@" + mangle_impl_method(mangled_type_name, method);

                    // Generate arguments - infer types from expressions
                    std::vector<std::pair<std::string, std::string>> typed_args;
                    auto fallback_method_it = functions_.find(mangled_type_name + "_" + method);
                    for (size_t fi = 0; fi < call.args.size(); ++fi) {
                        std::string val = gen_expr(*call.args[fi]);
                        std::string arg_type = last_expr_type_;
                        // struct/enum → ptr ABI fix (see impl.cpp:282)
                        if (fallback_method_it != functions_.end() &&
                            fi < fallback_method_it->second.param_types.size()) {
                            const auto& expected = fallback_method_it->second.param_types[fi];
                            if (expected == "ptr" &&
                                (arg_type.find("%struct.") == 0 || arg_type.find("%enum.") == 0)) {
                                std::string temp = fresh_reg();
                                emit_line("  " + temp + " = alloca " + arg_type);
                                emit_line("  store " + arg_type + " " + val + ", ptr " + temp);
                                val = temp;
                                arg_type = "ptr";
                            }
                        }
                        typed_args.push_back({arg_type, val});
                    }

                    std::string args_str;
                    for (size_t i = 0; i < typed_args.size(); ++i) {
                        if (i > 0)
                            args_str += ", ";
                        args_str += typed_args[i].first + " " + typed_args[i].second;
                    }

                    // Assume ptr return type for constructor-like methods (new, etc.)
                    // This is a heuristic but covers most cases for internal structs
                    std::string ret_type = "ptr";
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
                TML_DEBUG_LN("[STATIC_METHOD] Falling through after fallback for "
                             << type_name << "::" << method);
            }
        }

        bool is_type_name = struct_types_.count(type_name) > 0 || type_name == "List" ||
                            type_name == "I8" || type_name == "I16" || type_name == "I32" ||
                            type_name == "I64" || type_name == "I128" || type_name == "U8" ||
                            type_name == "U16" || type_name == "U32" || type_name == "U64" ||
                            type_name == "U128" || type_name == "F32" || type_name == "F64" ||
                            type_name == "Bool" || type_name == "Str";

        // Also check for imported structs from module registry
        if (!is_type_name && env_.module_registry()) {
            const auto& all_modules = env_.module_registry()->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                if (mod.structs.count(type_name) > 0) {
                    is_type_name = true;
                    break;
                }
            }
        }

        if (is_type_name && locals_.count(type_name) == 0) {
            auto result = gen_static_method_call(call, type_name);
            if (result) {
                return *result;
            }

            // Try looking up user-defined static methods in the environment/modules
            std::string qualified_name = type_name + "::" + method;
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

            if (func_sig) {
                // Generate call to user-defined static method
                // Check if we have expected type context that tells us the generic instantiation
                std::string mangled_type_name = type_name;
                std::unordered_map<std::string, types::TypePtr> type_subs_fallback;

                // Use expected_enum_type_ to determine type substitutions for generic types
                if (!expected_enum_type_.empty() &&
                    expected_enum_type_.find("%struct." + type_name + "__") == 0) {
                    // Extract mangled name and infer type params
                    mangled_type_name = expected_enum_type_.substr(8); // Remove "%struct."
                    std::string suffix = mangled_type_name.substr(type_name.length());
                    if (suffix.starts_with("__")) {
                        std::string type_arg_str = suffix.substr(2);
                        // Use static helper function for efficiency (avoids std::function overhead)
                        types::TypePtr type_arg = parse_mangled_type_string(type_arg_str);
                        if (type_arg) {
                            type_subs_fallback["T"] = type_arg;
                            // Request impl method instantiation
                            std::string method_key = mangle_impl_method(mangled_type_name, method);
                            if (generated_impl_methods_.find(method_key) ==
                                generated_impl_methods_.end()) {
                                pending_impl_method_instantiations_.push_back(PendingImplMethod{
                                    mangled_type_name, method, type_subs_fallback, type_name, "",
                                    /*is_library_type=*/true});
                                generated_impl_methods_.insert(method_key);
                            }
                        }
                    }
                }

                // Detect method-level generic params (e.g., sum[I: Iterator]) and
                // infer their concrete types from arguments using infer_expr_type.
                // This handles cases like I32::sum(counter) where I → Counter.
                std::string method_level_suffix;
                if (!func_sig->type_params.empty() && !call.args.empty() &&
                    type_subs_fallback.empty()) {
                    for (size_t i = 0; i < call.args.size() && i < func_sig->params.size(); ++i) {
                        auto param_type = func_sig->params[i];
                        if (!param_type)
                            continue;
                        std::string generic_name;
                        if (param_type->is<types::GenericType>()) {
                            generic_name = param_type->as<types::GenericType>().name;
                        } else if (param_type->is<types::NamedType>()) {
                            const auto& named = param_type->as<types::NamedType>();
                            if (named.type_args.empty()) {
                                for (const auto& tp : func_sig->type_params) {
                                    if (named.name == tp) {
                                        generic_name = tp;
                                        break;
                                    }
                                }
                            }
                        }
                        if (!generic_name.empty() &&
                            type_subs_fallback.find(generic_name) == type_subs_fallback.end()) {
                            auto arg_type = infer_expr_type(*call.args[i]);
                            if (arg_type) {
                                type_subs_fallback[generic_name] = arg_type;
                                if (!method_level_suffix.empty())
                                    method_level_suffix += "__";
                                method_level_suffix += mangle_type(arg_type);
                            }
                        }
                    }
                    if (!type_subs_fallback.empty() && !method_level_suffix.empty()) {
                        std::string method_key = mangle_impl_method(
                            mangled_type_name, method + "__" + method_level_suffix);
                        if (generated_impl_methods_.find(method_key) ==
                            generated_impl_methods_.end()) {
                            pending_impl_method_instantiations_.push_back(PendingImplMethod{
                                mangled_type_name, method, type_subs_fallback, type_name,
                                method_level_suffix, /*is_library_type=*/true});
                            generated_impl_methods_.insert(method_key);
                        }
                    }
                }

                // Generate arguments FIRST to determine their types
                // Needed for behavior method overload resolution (e.g., TryFrom[I64])
                std::vector<std::pair<std::string, std::string>> typed_args;
                std::vector<std::string> arg_tml_types;
                for (size_t i = 0; i < call.args.size(); ++i) {
                    // Set expected type context before generating argument
                    // This helps with type inference for nested generic calls
                    std::string saved_expected_enum = expected_enum_type_;
                    if (i < func_sig->params.size()) {
                        auto param_type = func_sig->params[i];
                        if (!type_subs_fallback.empty()) {
                            param_type = types::substitute_type(param_type, type_subs_fallback);
                        }
                        std::string llvm_param_type = llvm_type_from_semantic(param_type);
                        // Set expected type for generic struct arguments
                        if (llvm_param_type.find("%struct.") == 0 &&
                            llvm_param_type.find("__") != std::string::npos) {
                            expected_enum_type_ = llvm_param_type;
                        }
                    }

                    std::string val = gen_expr(*call.args[i]);
                    expected_enum_type_ = saved_expected_enum;
                    std::string arg_type = last_expr_type_;

                    // Collect TML type name for behavior param lookup
                    std::string tml_type_name;
                    if (arg_type == "i8")
                        tml_type_name = last_expr_is_unsigned_ ? "U8" : "I8";
                    else if (arg_type == "i16")
                        tml_type_name = last_expr_is_unsigned_ ? "U16" : "I16";
                    else if (arg_type == "i32")
                        tml_type_name = last_expr_is_unsigned_ ? "U32" : "I32";
                    else if (arg_type == "i64")
                        tml_type_name = last_expr_is_unsigned_ ? "U64" : "I64";
                    else if (arg_type == "i128")
                        tml_type_name = last_expr_is_unsigned_ ? "U128" : "I128";
                    else if (arg_type == "float")
                        tml_type_name = "F32";
                    else if (arg_type == "double")
                        tml_type_name = "F64";
                    else if (arg_type == "i1")
                        tml_type_name = "Bool";
                    else if (arg_type == "ptr")
                        tml_type_name = "Str";
                    arg_tml_types.push_back(tml_type_name);

                    // For TryFrom/From on primitive types, DON'T coerce types - use actual arg type
                    // This is because func_sig might have wrong param type (only one overload
                    // registered)
                    auto is_primitive_target = [](const std::string& name) {
                        return name == "I8" || name == "I16" || name == "I32" || name == "I64" ||
                               name == "I128" || name == "U8" || name == "U16" || name == "U32" ||
                               name == "U64" || name == "U128" || name == "F32" || name == "F64" ||
                               name == "Bool";
                    };
                    bool skip_coercion = (method == "try_from" || method == "from") &&
                                         is_primitive_target(type_name);

                    std::string expected_type = arg_type;
                    if (!skip_coercion && i < func_sig->params.size()) {
                        auto param_type = func_sig->params[i];
                        if (!type_subs_fallback.empty()) {
                            param_type = types::substitute_type(param_type, type_subs_fallback);
                        }
                        expected_type = llvm_type_from_semantic(param_type, /*for_data=*/true);
                        // Type coercion if needed
                        if (arg_type != expected_type) {
                            bool is_int_actual = (arg_type[0] == 'i' && arg_type != "i1");
                            bool is_int_expected =
                                (expected_type[0] == 'i' && expected_type != "i1");
                            if (is_int_actual && is_int_expected) {
                                int actual_bits = std::stoi(arg_type.substr(1));
                                int expected_bits = std::stoi(expected_type.substr(1));
                                std::string coerced = fresh_reg();
                                if (expected_bits > actual_bits) {
                                    emit_line("  " + coerced + " = sext " + arg_type + " " + val +
                                              " to " + expected_type);
                                } else {
                                    emit_line("  " + coerced + " = trunc " + arg_type + " " + val +
                                              " to " + expected_type);
                                }
                                val = coerced;
                            }
                        }
                    }
                    typed_args.push_back({expected_type, val});
                }

                // Build function name with behavior type parameter suffix for overloaded methods
                // Only add suffix for PRIMITIVE types that have multiple TryFrom/From overloads
                // e.g., I32::try_from(I64) -> I32_try_from_I64
                // Custom types like Celsius::from(Fahrenheit) stay as Celsius_from
                std::string behavior_suffix = "";
                // DEBUG: emit comment to verify code path
                emit_line("  ; DEBUG: method.cpp path for " + type_name + "::" + method);
                auto is_primitive = [](const std::string& name) {
                    return name == "I8" || name == "I16" || name == "I32" || name == "I64" ||
                           name == "I128" || name == "U8" || name == "U16" || name == "U32" ||
                           name == "U64" || name == "U128" || name == "F32" || name == "F64" ||
                           name == "Bool";
                };
                if ((method == "try_from" || method == "from") && is_primitive(type_name) &&
                    !arg_tml_types.empty() && !arg_tml_types[0].empty()) {
                    behavior_suffix = "__" + arg_tml_types[0];
                }
                // Append method-level generic suffix (e.g., "__Counter" for sum[I=Counter])
                if (!method_level_suffix.empty() && behavior_suffix.empty()) {
                    behavior_suffix = "__" + method_level_suffix;
                }

                // Look up in functions_ for the correct LLVM name
                std::string method_lookup_key = mangled_type_name + "_" + method + behavior_suffix;
                auto method_it = functions_.find(method_lookup_key);
                std::string fn_name;
                if (method_it != functions_.end()) {
                    fn_name = method_it->second.llvm_name;
                } else {
                    fn_name = "@" + mangle_impl_method(mangled_type_name, method + behavior_suffix);
                }

                // struct/enum → ptr ABI fix (see impl.cpp:282)
                if (method_it != functions_.end()) {
                    for (size_t i = 0;
                         i < typed_args.size() && i < method_it->second.param_types.size(); ++i) {
                        const auto& expected = method_it->second.param_types[i];
                        auto& [at, av] = typed_args[i];
                        if (expected == "ptr" &&
                            (at.find("%struct.") == 0 || at.find("%enum.") == 0)) {
                            std::string temp = fresh_reg();
                            emit_line("  " + temp + " = alloca " + at);
                            emit_line("  store " + at + " " + av + ", ptr " + temp);
                            av = temp;
                            at = "ptr";
                        }
                    }
                }

                std::string args_str;
                for (size_t i = 0; i < typed_args.size(); ++i) {
                    if (i > 0)
                        args_str += ", ";
                    args_str += typed_args[i].first + " " + typed_args[i].second;
                }

                auto return_type = func_sig->return_type;
                if (!type_subs_fallback.empty()) {
                    return_type = types::substitute_type(return_type, type_subs_fallback);
                }
                std::string ret_type = return_type ? llvm_type_from_semantic(return_type) : "void";
                std::string result_reg = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + fn_name + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result_reg + " = call " + ret_type + " " + fn_name + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result_reg;
                }
            }

            report_error("Unknown static method: " + type_name + "." + method, call.span, "C035");
            return "0";
        }
    }

    return std::nullopt;
}

} // namespace tml::codegen
