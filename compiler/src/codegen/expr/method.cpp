//! # LLVM IR Generator - Method Call Dispatcher
//!
//! This file is the main entry point for method call code generation.
//! It delegates to specialized handlers based on receiver type.
//!
//! ## Dispatch Order
//!
//! 1. Static methods: `Type::method()` → method_static.cpp
//! 2. Primitive methods: `.to_string()`, `.abs()` → method_primitive.cpp
//! 3. Collection methods: `.push()`, `.get()` → method_collection.cpp
//! 4. Slice methods: `.len()`, `.get()` → method_slice.cpp
//! 5. Maybe methods: `.unwrap()`, `.map()` → method_maybe.cpp
//! 6. Outcome methods: `.unwrap()`, `.ok()` → method_outcome.cpp
//! 7. Array methods: `.len()`, `.get()` → method_array.cpp
//! 8. User-defined methods: Look up in impl blocks
//!
//! ## Specialized Files
//!
//! | File                    | Handles                        |
//! |-------------------------|--------------------------------|
//! | method_static.cpp       | `Type::method()` static calls  |
//! | method_primitive.cpp    | Integer, Float, Bool methods   |
//! | method_collection.cpp   | List, HashMap, Buffer methods  |
//! | method_slice.cpp        | Slice, MutSlice methods        |
//! | method_maybe.cpp        | Maybe[T] methods               |
//! | method_outcome.cpp      | Outcome[T,E] methods           |
//! | method_array.cpp        | Array[T; N] methods            |

#include "codegen/llvm_ir_gen.hpp"
#include "types/module.hpp"

#include <iostream>
#include <unordered_set>

namespace tml::codegen {

// Static helper to parse mangled type strings like "Mutex__I32" into proper TypePtr
// This is used for nested generic type inference and avoids expensive std::function lambdas
static types::TypePtr parse_mangled_type_string(const std::string& s) {
    // Primitives
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

    // Check for pointer prefix (e.g., ptr_ChannelNode__I32 -> Ptr[ChannelNode[I32]])
    // Must be checked BEFORE the __ delim check to properly handle nested types
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

    // Check for nested generic (e.g., Mutex__I32)
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

    // Simple struct type
    auto t = std::make_shared<types::Type>();
    t->kind = types::NamedType{s, "", {}};
    return t;
}

auto LLVMIRGen::gen_method_call(const parser::MethodCallExpr& call) -> std::string {
    // Clear expected literal type context - it should only apply within explicit type annotations
    // (like "let x: F64 = 5") and not leak into method call arguments
    expected_literal_type_.clear();
    expected_literal_is_unsigned_ = false;

    const std::string& method = call.method;
    TML_DEBUG_LN("[METHOD] gen_method_call: " << method << " where_constraints.size="
                                              << current_where_constraints_.size());

    // =========================================================================
    // 1. Check for static method calls (Type::method)
    // =========================================================================
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

                    // Generate call to static method
                    // Only use suite prefix for test-local methods, not library methods
                    std::string prefix =
                        is_library_method(type_name, method) ? "" : get_suite_prefix();
                    std::string func_name =
                        "@tml_" + prefix + type_name + mangled_type_suffix + "_" + method;

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
        // NOTE: Exclude runtime-managed collection types (List, HashMap, Buffer, HashMapIter)
        // as they have special handling in gen_static_method_call
        bool is_runtime_collection = type_name == "List" || type_name == "HashMap" ||
                                     type_name == "Buffer" || type_name == "HashMapIter";
        bool is_generic_struct =
            !is_runtime_collection &&
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
        // DEBUG: always print type_name when handling generic struct calls
        if (type_name == "Range" || type_name == "RangeInclusive") {
            std::cerr << "[DEBUG] type_name=" << type_name
                      << " is_local_generic=" << is_local_generic
                      << " is_runtime_collection=" << is_runtime_collection
                      << " has_registry=" << (env_.module_registry() ? "yes" : "no") << "\n";
        }
        if (!is_local_generic && !is_runtime_collection && env_.module_registry()) {
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
                        if (arg.is_type()) {
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

                // Build type substitutions - try local impls first, then imported type params
                std::vector<std::string> generic_names;
                auto impl_it = pending_generic_impls_.find(type_name);
                if (impl_it != pending_generic_impls_.end()) {
                    for (const auto& g : impl_it->second->generics) {
                        generic_names.push_back(g.name);
                    }
                } else if (!imported_type_params.empty()) {
                    generic_names = imported_type_params;
                }

                // For simple cases like Range__I64, extract the type arg
                std::string suffix = mangled_type_name.substr(type_name.length());
                if (suffix.starts_with("__") && generic_names.size() == 1) {
                    std::string type_arg_str = suffix.substr(2);
                    types::TypePtr type_arg = str_to_type(type_arg_str);
                    if (type_arg && !generic_names.empty()) {
                        type_subs[generic_names[0]] = type_arg;
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

            // Request impl method instantiation if needed
            // This must be done regardless of func_sig to handle local generic structs
            std::string mangled_method_name = "tml_" + mangled_type_name + "_" + method;
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
                    // Fallback: only use suite prefix for test-local functions
                    std::string prefix = is_imported ? "" : get_suite_prefix();
                    fn_name = "@tml_" + prefix + mangled_type_name + "_" + method;
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
                    typed_args.push_back({arg_type, val});
                }

                auto return_type = func_sig->return_type;
                if (!type_subs.empty()) {
                    return_type = types::substitute_type(return_type, type_subs);
                }
                std::string ret_type = llvm_type_from_semantic(return_type);

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
                                std::string prefix = is_imported ? "" : get_suite_prefix();
                                fn_name = "@tml_" + prefix + mangled_type_name + "_" + method;
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
                    std::string fn_name = "@tml_" + mangled_type_name + "_" + method;

                    // Generate arguments - infer types from expressions
                    std::vector<std::pair<std::string, std::string>> typed_args;
                    for (const auto& arg : call.args) {
                        std::string val = gen_expr(*arg);
                        std::string arg_type = last_expr_type_;
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

        bool is_type_name =
            struct_types_.count(type_name) > 0 || type_name == "List" || type_name == "HashMap" ||
            type_name == "Buffer" || type_name == "File" || type_name == "Path" ||
            type_name == "I8" || type_name == "I16" || type_name == "I32" || type_name == "I64" ||
            type_name == "I128" || type_name == "U8" || type_name == "U16" || type_name == "U32" ||
            type_name == "U64" || type_name == "U128" || type_name == "F32" || type_name == "F64" ||
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
                            std::string method_key = "tml_" + mangled_type_name + "_" + method;
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
                        expected_type = llvm_type_from_semantic(param_type);
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
                    behavior_suffix = "_" + arg_tml_types[0];
                }

                // Look up in functions_ for the correct LLVM name
                std::string method_lookup_key = mangled_type_name + "_" + method + behavior_suffix;
                auto method_it = functions_.find(method_lookup_key);
                std::string fn_name;
                if (method_it != functions_.end()) {
                    fn_name = method_it->second.llvm_name;
                } else {
                    // Fallback - only use suite prefix for test-local methods
                    std::string prefix =
                        is_library_method(type_name, method) ? "" : get_suite_prefix();
                    fn_name = "@tml_" + prefix + mangled_type_name + "_" + method + behavior_suffix;
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

            report_error("Unknown static method: " + type_name + "." + method, call.span, "C006");
            return "0";
        }
    }

    // =========================================================================
    // 2. Check for array methods first (before generating receiver)
    // =========================================================================
    auto array_result = gen_array_method(call, method);
    if (array_result.has_value()) {
        return *array_result;
    }

    // =========================================================================
    // 2b. Check for SliceType [T] methods (before generating receiver)
    // =========================================================================
    auto slice_type_result = gen_slice_type_method(call, method);
    if (slice_type_result.has_value()) {
        return *slice_type_result;
    }

    // =========================================================================
    // 3. Generate receiver and get receiver pointer
    // =========================================================================
    std::string receiver;
    std::string receiver_ptr;

    // Special handling for FieldExpr receiver: we need the pointer to the field,
    // not a loaded copy, so that mutations inside the method are persisted.
    TML_DEBUG_LN("[METHOD_CALL] receiver is FieldExpr: "
                 << (call.receiver->is<parser::FieldExpr>() ? "yes" : "no"));
    if (call.receiver->is<parser::FieldExpr>()) {
        const auto& field_expr = call.receiver->as<parser::FieldExpr>();

        // Generate the base object expression
        std::string base_ptr;
        types::TypePtr base_type;

        if (field_expr.object->is<parser::IdentExpr>()) {
            const auto& ident = field_expr.object->as<parser::IdentExpr>();
            if (ident.name == "this") {
                base_ptr = "%this";
                // For 'this' in impl blocks, use current_impl_type_ if infer fails
                base_type = infer_expr_type(*field_expr.object);
                if (!base_type && !current_impl_type_.empty()) {
                    // Create a NamedType for current_impl_type_
                    auto result = std::make_shared<types::Type>();
                    result->kind = types::NamedType{current_impl_type_, "", {}};
                    base_type = result;
                }
            } else {
                auto it = locals_.find(ident.name);
                if (it != locals_.end()) {
                    base_ptr = it->second.reg;
                    base_type = it->second.semantic_type;
                }
                if (!base_type) {
                    base_type = infer_expr_type(*field_expr.object);
                }
            }
        } else if (field_expr.object->is<parser::FieldExpr>()) {
            // Handle nested field access: this.inner.field
            // Generate the nested field access - gen_expr will return the loaded value
            // but we need the semantic type to determine the struct type
            std::string nested_val = gen_expr(*field_expr.object);
            base_type = infer_expr_type(*field_expr.object);

            // For struct types, gen_expr returns a loaded value but we need a pointer
            // The field is stored in memory, so we can use the value as if it were a pointer
            // Actually, for struct fields that are themselves structs, gen_field returns
            // a loaded value. We need the pointer to access sub-fields.
            // Re-generate using gen_field_ptr if available, or allocate temp storage
            if (last_expr_type_.starts_with("%struct.") || last_expr_type_ == "ptr") {
                // For pointer types, the loaded value IS the pointer
                if (last_expr_type_ == "ptr") {
                    base_ptr = nested_val;
                } else {
                    // For struct values, we need to store to a temp alloca
                    std::string temp_ptr = fresh_reg();
                    emit_line("  " + temp_ptr + " = alloca " + last_expr_type_);
                    emit_line("  store " + last_expr_type_ + " " + nested_val + ", ptr " +
                              temp_ptr);
                    base_ptr = temp_ptr;
                }
            } else {
                // Primitive field - use value directly but we may need its address for method calls
                base_ptr = nested_val;
            }
        } else if (field_expr.object->is<parser::UnaryExpr>()) {
            // Handle dereferenced pointer field access: (*ptr).field
            const auto& unary = field_expr.object->as<parser::UnaryExpr>();
            if (unary.op == parser::UnaryOp::Deref) {
                // Generate the pointer value - this becomes our base_ptr
                base_ptr = gen_expr(*unary.operand);

                // Infer the pointee type
                types::TypePtr ptr_type = infer_expr_type(*unary.operand);
                if (ptr_type) {
                    if (ptr_type->is<types::PtrType>()) {
                        base_type = ptr_type->as<types::PtrType>().inner;
                    } else if (ptr_type->is<types::RefType>()) {
                        base_type = ptr_type->as<types::RefType>().inner;
                    } else if (ptr_type->is<types::NamedType>()) {
                        // Handle TML's Ptr[T] type (NamedType wrapper)
                        const auto& named = ptr_type->as<types::NamedType>();
                        if ((named.name == "Ptr" || named.name == "RawPtr") &&
                            !named.type_args.empty()) {
                            base_type = named.type_args[0];
                            TML_DEBUG_LN("[FIELD_MUTATION] NamedType Ptr inner: "
                                         << types::type_to_string(base_type));
                        }
                    }

                    // Apply type substitutions for generic types
                    if (base_type && !current_type_subs_.empty()) {
                        base_type = apply_type_substitutions(base_type, current_type_subs_);
                    }
                }
            }
        }

        if (!base_ptr.empty() && base_type) {
            TML_DEBUG_LN("[FIELD_MUTATION] base_type exists: " << (base_type ? "yes" : "no"));
            if (base_type) {
                TML_DEBUG_LN("[FIELD_MUTATION] base_type is NamedType: "
                             << (base_type->is<types::NamedType>() ? "yes" : "no"));
            }
            if (base_type->is<types::NamedType>()) {
                const auto& base_named = base_type->as<types::NamedType>();
                std::string base_type_name = base_named.name;

                // Get the mangled struct type name if it has type args
                std::string struct_type_name = base_type_name;
                if (!base_named.type_args.empty()) {
                    // Ensure generic struct is instantiated so fields are registered
                    // Use the return value which handles UNRESOLVED cases properly
                    struct_type_name =
                        require_struct_instantiation(base_type_name, base_named.type_args);
                }
                std::string llvm_struct_type = "%struct." + struct_type_name;

                // Check for auto-deref: if field not found on base type but base type implements
                // Deref
                types::TypePtr deref_target = get_deref_target_type(base_type);
                if (deref_target && !struct_has_field(struct_type_name, field_expr.field)) {
                    TML_DEBUG_LN("[METHOD_CALL] Auto-deref for FieldExpr: "
                                 << base_type_name << " -> "
                                 << types::type_to_string(deref_target));

                    // Generate deref code for Arc[T] or Box[T]
                    auto sep_pos = base_type_name.find("__");
                    std::string smart_ptr_name = (sep_pos != std::string::npos)
                                                     ? base_type_name.substr(0, sep_pos)
                                                     : base_type_name;

                    if (smart_ptr_name == "Arc" || smart_ptr_name == "Shared" ||
                        smart_ptr_name == "Rc") {
                        // Arc layout: { ptr: Ptr[ArcInner[T]] }
                        // ArcInner layout: { strong, weak, data }
                        std::string arc_ptr_field = fresh_reg();
                        emit_line("  " + arc_ptr_field + " = getelementptr " + llvm_struct_type +
                                  ", ptr " + base_ptr + ", i32 0, i32 0");
                        std::string inner_ptr = fresh_reg();
                        emit_line("  " + inner_ptr + " = load ptr, ptr " + arc_ptr_field);

                        // Get ArcInner type
                        std::string arc_inner_mangled = mangle_struct_name(
                            "ArcInner", std::vector<types::TypePtr>{deref_target});
                        std::string arc_inner_type = "%struct." + arc_inner_mangled;

                        // GEP to data field (index 2)
                        std::string data_ptr = fresh_reg();
                        emit_line("  " + data_ptr + " = getelementptr " + arc_inner_type +
                                  ", ptr " + inner_ptr + ", i32 0, i32 2");

                        // Update base_ptr and types to point to inner struct
                        base_ptr = data_ptr;
                        if (deref_target->is<types::NamedType>()) {
                            const auto& inner_named = deref_target->as<types::NamedType>();
                            if (!inner_named.type_args.empty()) {
                                // Use return value to handle UNRESOLVED cases
                                struct_type_name = require_struct_instantiation(
                                    inner_named.name, inner_named.type_args);
                            } else {
                                struct_type_name = inner_named.name;
                            }
                            llvm_struct_type = "%struct." + struct_type_name;
                        }
                    } else if (smart_ptr_name == "Box" || smart_ptr_name == "Heap") {
                        // Box layout: { ptr: Ptr[T] }
                        std::string box_ptr_field = fresh_reg();
                        emit_line("  " + box_ptr_field + " = getelementptr " + llvm_struct_type +
                                  ", ptr " + base_ptr + ", i32 0, i32 0");
                        std::string inner_ptr = fresh_reg();
                        emit_line("  " + inner_ptr + " = load ptr, ptr " + box_ptr_field);

                        // Update base_ptr and types
                        base_ptr = inner_ptr;
                        if (deref_target->is<types::NamedType>()) {
                            const auto& inner_named = deref_target->as<types::NamedType>();
                            if (!inner_named.type_args.empty()) {
                                // Use return value to handle UNRESOLVED cases
                                struct_type_name = require_struct_instantiation(
                                    inner_named.name, inner_named.type_args);
                            } else {
                                struct_type_name = inner_named.name;
                            }
                            llvm_struct_type = "%struct." + struct_type_name;
                        }
                    }
                }

                // Get field index
                int field_idx = get_field_index(struct_type_name, field_expr.field);
                if (field_idx >= 0) {
                    std::string field_type = get_field_type(struct_type_name, field_expr.field);

                    // Generate getelementptr to get pointer to the field
                    std::string field_ptr = fresh_reg();
                    emit_line("  " + field_ptr + " = getelementptr " + llvm_struct_type + ", ptr " +
                              base_ptr + ", i32 0, i32 " + std::to_string(field_idx));

                    // Load the field value for method calls - structs, primitives, and pointers
                    // all need to be loaded from the field pointer before use.
                    // The receiver_ptr is kept for methods that mutate the receiver.
                    receiver_ptr = field_ptr;
                    std::string loaded = fresh_reg();
                    emit_line("  " + loaded + " = load " + field_type + ", ptr " + field_ptr);
                    receiver = loaded;
                    last_expr_type_ = field_type;
                }
            }
        }

        // Fallback if we couldn't get the field pointer
        if (receiver.empty()) {
            receiver = gen_expr(*call.receiver);
        }
    } else {
        receiver = gen_expr(*call.receiver);
        if (call.receiver->is<parser::IdentExpr>()) {
            const auto& ident = call.receiver->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                receiver_ptr = it->second.reg;
            }
        }
    }

    // =========================================================================
    // 4. Get receiver type info
    // =========================================================================
    types::TypePtr receiver_type = infer_expr_type(*call.receiver);

    // For FieldExpr receivers in generic impl blocks, try to get field type from
    // pending_generic_structs_ This handles cases where infer_expr_type returns an incorrect
    // fallback type
    if (call.receiver->is<parser::FieldExpr>()) {
        const auto& field_expr = call.receiver->as<parser::FieldExpr>();
        if (field_expr.object->is<parser::IdentExpr>()) {
            const auto& ident = field_expr.object->as<parser::IdentExpr>();
            if (ident.name == "this" && !current_impl_type_.empty()) {
                // Parse the impl type to get base name and type args
                std::string base_name = current_impl_type_;
                std::vector<types::TypePtr> type_args;

                auto sep_pos = current_impl_type_.find("__");
                if (sep_pos != std::string::npos) {
                    base_name = current_impl_type_.substr(0, sep_pos);

                    // Parse type args from mangled suffix using recursive parser
                    // This properly handles nested types like ptr_ChannelNode__I32 as
                    // Ptr[ChannelNode[I32]] For most generic types (Mutex, MutexGuard, Arc, etc.)
                    // which have a single type parameter, this is the complete type arg.
                    std::string args_str = current_impl_type_.substr(sep_pos + 2);
                    auto parsed_type = parse_mangled_type_string(args_str);
                    if (parsed_type) {
                        type_args.push_back(parsed_type);
                    }
                }

                // Look up the generic struct definition
                auto generic_it = pending_generic_structs_.find(base_name);
                if (generic_it != pending_generic_structs_.end()) {
                    const parser::StructDecl* decl = generic_it->second;

                    // Build type substitution map
                    std::unordered_map<std::string, types::TypePtr> subs;
                    for (size_t i = 0; i < decl->generics.size() && i < type_args.size(); ++i) {
                        subs[decl->generics[i].name] = type_args[i];
                    }

                    // Find the field
                    for (const auto& decl_field : decl->fields) {
                        if (decl_field.name == field_expr.field && decl_field.type) {
                            receiver_type = resolve_parser_type_with_subs(*decl_field.type, subs);
                            break;
                        }
                    }
                }

                // Also try module registry for imported structs
                // Check if receiver_type is valid (not a primitive fallback type like Str)
                bool needs_registry_lookup =
                    !receiver_type ||
                    (receiver_type->is<types::PrimitiveType>() &&
                     receiver_type->as<types::PrimitiveType>().kind == types::PrimitiveKind::Str);
                if (needs_registry_lookup && env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto struct_it = mod.structs.find(base_name);
                        if (struct_it != mod.structs.end()) {
                            const auto& struct_def = struct_it->second;

                            // Build type substitution map
                            std::unordered_map<std::string, types::TypePtr> subs;
                            for (size_t i = 0;
                                 i < struct_def.type_params.size() && i < type_args.size(); ++i) {
                                subs[struct_def.type_params[i]] = type_args[i];
                            }

                            // Find the field
                            for (const auto& [fname, ftype] : struct_def.fields) {
                                if (fname == field_expr.field && ftype) {
                                    if (!subs.empty()) {
                                        receiver_type = types::substitute_type(ftype, subs);
                                    } else {
                                        receiver_type = ftype;
                                    }
                                    break;
                                }
                            }
                            if (receiver_type)
                                break;
                        }
                    }
                }
            }
        }
    }

    // Apply type substitutions to the receiver type.
    // This handles both simple type parameters (T -> I32) AND nested generic types
    // like AtomicPtr[Node[T]] -> AtomicPtr[Node[I32]]
    if (receiver_type && !current_type_subs_.empty()) {
        receiver_type = apply_type_substitutions(receiver_type, current_type_subs_);
    }

    // If receiver type is a reference, unwrap it for method dispatch
    // Methods are dispatched on the inner type, not the reference type
    // Track whether we unwrapped a reference, as the receiver value will be a pointer
    // Note: Type substitutions were already applied above, so inner is already concrete
    bool receiver_was_ref = false;
    if (receiver_type && receiver_type->is<types::RefType>()) {
        const auto& ref = receiver_type->as<types::RefType>();
        if (ref.inner) {
            receiver_type = ref.inner;
            receiver_was_ref = true;
        }
    }

    std::string receiver_type_name;
    if (receiver_type) {
        if (receiver_type->is<types::ClassType>()) {
            receiver_type_name = receiver_type->as<types::ClassType>().name;
        } else if (receiver_type->is<types::NamedType>()) {
            receiver_type_name = receiver_type->as<types::NamedType>().name;
        } else if (receiver_type->is<types::PrimitiveType>()) {
            // Convert primitive type to name for method dispatch
            const auto& prim = receiver_type->as<types::PrimitiveType>();
            switch (prim.kind) {
            case types::PrimitiveKind::I8:
                receiver_type_name = "I8";
                break;
            case types::PrimitiveKind::I16:
                receiver_type_name = "I16";
                break;
            case types::PrimitiveKind::I32:
                receiver_type_name = "I32";
                break;
            case types::PrimitiveKind::I64:
                receiver_type_name = "I64";
                break;
            case types::PrimitiveKind::I128:
                receiver_type_name = "I128";
                break;
            case types::PrimitiveKind::U8:
                receiver_type_name = "U8";
                break;
            case types::PrimitiveKind::U16:
                receiver_type_name = "U16";
                break;
            case types::PrimitiveKind::U32:
                receiver_type_name = "U32";
                break;
            case types::PrimitiveKind::U64:
                receiver_type_name = "U64";
                break;
            case types::PrimitiveKind::U128:
                receiver_type_name = "U128";
                break;
            case types::PrimitiveKind::F32:
                receiver_type_name = "F32";
                break;
            case types::PrimitiveKind::F64:
                receiver_type_name = "F64";
                break;
            case types::PrimitiveKind::Bool:
                receiver_type_name = "Bool";
                break;
            case types::PrimitiveKind::Char:
                receiver_type_name = "Char";
                break;
            case types::PrimitiveKind::Str:
                receiver_type_name = "Str";
                break;
            default:
                break;
            }
        }
    }

    // =========================================================================
    // 4b. Handle method calls on bounded generics (e.g., C: Container[T])
    // =========================================================================
    // When the receiver is a type parameter with behavior bounds from where clauses,
    // we need to dispatch to the concrete impl method for the substituted type.
    TML_DEBUG_LN("[METHOD 4b] method=" << method
                                       << " where_constraints=" << current_where_constraints_.size()
                                       << " type_subs=" << current_type_subs_.size());
    if (!current_where_constraints_.empty() && !current_type_subs_.empty()) {
        // For bounded generics, the type checker has already validated that the method exists
        // We need to find the concrete type and dispatch to its impl method

        // Debug: dump current_type_subs_
        for (const auto& [key, val] : current_type_subs_) {
            TML_DEBUG_LN("[METHOD 4b] type_subs: " << key << " -> is_NamedType="
                                                   << val->is<types::NamedType>());
        }

        // Iterate through all where constraints to find one with a behavior that has this method
        for (const auto& constraint : current_where_constraints_) {
            TML_DEBUG_LN("[METHOD 4b] checking constraint for type_param="
                         << constraint.type_param
                         << " parameterized_bounds=" << constraint.parameterized_bounds.size());

            // Get the concrete type name from the type parameter substitution
            std::string concrete_type_name;
            auto sub_it = current_type_subs_.find(constraint.type_param);
            if (sub_it != current_type_subs_.end()) {
                auto sub_type = sub_it->second;
                TML_DEBUG_LN("[METHOD 4b] sub_type for "
                             << constraint.type_param
                             << " is_NamedType=" << sub_type->is<types::NamedType>()
                             << " is_PrimitiveType=" << sub_type->is<types::PrimitiveType>());
                if (sub_type->is<types::NamedType>()) {
                    concrete_type_name = sub_type->as<types::NamedType>().name;
                } else if (sub_type->is<types::PrimitiveType>()) {
                    // Handle primitive types (I8, I16, I32, I64, etc.)
                    const auto& prim = sub_type->as<types::PrimitiveType>();
                    concrete_type_name = types::primitive_kind_to_string(prim.kind);
                }
            }
            TML_DEBUG_LN("[METHOD 4b] concrete_type_name=" << concrete_type_name);

            // Look through parameterized bounds for a behavior with this method
            for (const auto& bound : constraint.parameterized_bounds) {
                TML_DEBUG_LN("[METHOD 4b] checking bound.behavior_name=" << bound.behavior_name);
                auto behavior_def = env_.lookup_behavior(bound.behavior_name);
                if (behavior_def) {
                    TML_DEBUG_LN("[METHOD 4b] found behavior_def with "
                                 << behavior_def->methods.size() << " methods");
                    for (const auto& bmethod : behavior_def->methods) {
                        TML_DEBUG_LN("[METHOD 4b] checking bmethod.name="
                                     << bmethod.name << " vs method=" << method);
                        if (bmethod.name == method) {
                            // Found the method in the behavior!
                            // Now dispatch to the concrete impl for the substituted type
                            TML_DEBUG_LN("[METHOD 4b] FOUND method! concrete_type_name="
                                         << concrete_type_name);

                            // For primitive types with intrinsic methods (duplicate, to_owned,
                            // etc.), delegate to gen_primitive_method instead of generating a
                            // function call
                            if (sub_it != current_type_subs_.end() &&
                                sub_it->second->is<types::PrimitiveType>()) {
                                // Check if this is an intrinsic primitive method
                                static const std::unordered_set<std::string> primitive_intrinsics =
                                    {"duplicate",   "to_owned",     "borrow", "borrow_mut",
                                     "to_string",   "debug_string", "hash",   "cmp",
                                     "partial_cmp", "add",          "sub",    "mul",
                                     "div",         "rem",          "neg",    "abs",
                                     "eq",          "ne",           "lt",     "le",
                                     "gt",          "ge",           "min",    "max",
                                     "clamp",       "is_zero",      "is_one"};
                                if (primitive_intrinsics.count(method)) {
                                    TML_DEBUG_LN("[METHOD 4b] Delegating primitive method to "
                                                 "gen_primitive_method");
                                    // If receiver was originally a ref T, we need to dereference
                                    // to get the primitive value for methods like to_owned,
                                    // duplicate
                                    std::string actual_receiver = receiver;
                                    if (receiver_was_ref) {
                                        std::string prim_ty =
                                            llvm_type_from_semantic(sub_it->second);
                                        actual_receiver = fresh_reg();
                                        emit_line("  " + actual_receiver + " = load " + prim_ty +
                                                  ", ptr " + receiver);
                                    }
                                    auto prim_result = gen_primitive_method(
                                        call, actual_receiver, receiver_ptr, sub_it->second);
                                    if (prim_result) {
                                        return *prim_result;
                                    }
                                    // If gen_primitive_method didn't handle it, fall through
                                }
                            }

                            // Build substitution map from behavior type params to bound's type args
                            std::unordered_map<std::string, types::TypePtr> behavior_subs;
                            if (!bound.type_args.empty() && !behavior_def->type_params.empty()) {
                                for (size_t i = 0; i < behavior_def->type_params.size() &&
                                                   i < bound.type_args.size();
                                     ++i) {
                                    behavior_subs[behavior_def->type_params[i]] =
                                        bound.type_args[i];
                                }
                            }
                            // Also substitute Self with the concrete type
                            if (sub_it != current_type_subs_.end()) {
                                behavior_subs["Self"] = sub_it->second;
                            }

                            // Look up the impl method: ConcreteType::method
                            std::string qualified_name = concrete_type_name + "::" + method;
                            auto func_sig = env_.lookup_func(qualified_name);
                            bool is_lib_type = this->is_library_method(concrete_type_name, method);

                            // If not found locally, search modules
                            if (!func_sig && env_.module_registry()) {
                                const auto& all_modules = env_.module_registry()->get_all_modules();
                                for (const auto& [mod_name, mod] : all_modules) {
                                    auto func_it = mod.functions.find(qualified_name);
                                    if (func_it != mod.functions.end()) {
                                        func_sig = func_it->second;
                                        is_lib_type = true;
                                        break;
                                    }
                                }
                            }

                            if (func_sig) {
                                // Generate the call to the concrete impl method
                                // Only use suite prefix for test-local methods, not library methods
                                std::string prefix = is_lib_type ? "" : get_suite_prefix();
                                std::string fn_name =
                                    "@tml_" + prefix + concrete_type_name + "_" + method;

                                // Build arguments
                                std::vector<std::pair<std::string, std::string>> typed_args;

                                // First argument is 'this' (the receiver)
                                std::string this_type = "ptr";
                                std::string this_val = receiver;

                                // For structs, we need a pointer to the struct
                                // For ref types (ptr), we already have the pointer value
                                if (call.receiver->is<parser::IdentExpr>()) {
                                    const auto& ident = call.receiver->as<parser::IdentExpr>();
                                    auto it = locals_.find(ident.name);
                                    if (it != locals_.end()) {
                                        if (it->second.type == "ptr") {
                                            // Ref type: use loaded value (receiver) which is the
                                            // ptr
                                            this_val = receiver;
                                        } else {
                                            // Struct type: use the alloca which is a ptr to struct
                                            this_val = it->second.reg;
                                        }
                                    }
                                } else if (call.receiver->is<parser::FieldExpr>()) {
                                    // For field expressions:
                                    // - If receiver_ptr is set, use it (pointer to field)
                                    // - Otherwise, spill the struct to stack
                                    if (last_expr_type_ == "ptr") {
                                        this_val = receiver;
                                    } else if (!receiver_ptr.empty()) {
                                        this_val = receiver_ptr;
                                    } else if (last_expr_type_.starts_with("%struct.")) {
                                        // Spill struct to stack for method call
                                        std::string tmp = fresh_reg();
                                        emit_line("  " + tmp + " = alloca " + last_expr_type_);
                                        emit_line("  store " + last_expr_type_ + " " + receiver +
                                                  ", ptr " + tmp);
                                        this_val = tmp;
                                    }
                                }

                                typed_args.push_back({this_type, this_val});

                                // Add remaining arguments with type substitution
                                for (size_t i = 0; i < call.args.size(); ++i) {
                                    std::string val = gen_expr(*call.args[i]);
                                    std::string arg_type = "i32";
                                    if (func_sig && i + 1 < func_sig->params.size()) {
                                        auto param_type = func_sig->params[i + 1];
                                        if (!behavior_subs.empty()) {
                                            param_type =
                                                types::substitute_type(param_type, behavior_subs);
                                        }
                                        arg_type = llvm_type_from_semantic(param_type);
                                    }
                                    typed_args.push_back({arg_type, val});
                                }

                                // Determine return type with substitution
                                auto return_type = bmethod.return_type;
                                if (!behavior_subs.empty()) {
                                    return_type =
                                        types::substitute_type(return_type, behavior_subs);
                                }
                                std::string ret_type = llvm_type_from_semantic(return_type);

                                // Build args string
                                std::string args_str;
                                for (size_t i = 0; i < typed_args.size(); ++i) {
                                    if (i > 0)
                                        args_str += ", ";
                                    args_str += typed_args[i].first + " " + typed_args[i].second;
                                }

                                // Generate the call
                                std::string result = fresh_reg();
                                if (ret_type == "void") {
                                    emit_line("  call void " + fn_name + "(" + args_str + ")");
                                    last_expr_type_ = "void";
                                    return "void";
                                } else {
                                    emit_line("  " + result + " = call " + ret_type + " " +
                                              fn_name + "(" + args_str + ")");
                                    last_expr_type_ = ret_type;
                                    return result;
                                }
                            }
                        }
                    }
                }
            }

            // Also check simple (non-parameterized) behavior bounds
            TML_DEBUG_LN("[METHOD 4b] checking required_behaviors.size="
                         << constraint.required_behaviors.size());
            for (const auto& behavior_name : constraint.required_behaviors) {
                TML_DEBUG_LN("[METHOD 4b] checking required_behavior=" << behavior_name
                                                                       << " for method=" << method);

                // For primitive types with intrinsic methods, delegate to gen_primitive_method
                if (sub_it != current_type_subs_.end() &&
                    sub_it->second->is<types::PrimitiveType>()) {
                    static const std::unordered_set<std::string> primitive_intrinsics = {
                        "duplicate",    "to_owned", "borrow", "borrow_mut",  "to_string",
                        "debug_string", "hash",     "cmp",    "partial_cmp", "add",
                        "sub",          "mul",      "div",    "rem",         "neg",
                        "abs",          "eq",       "ne",     "lt",          "le",
                        "gt",           "ge",       "min",    "max",         "clamp",
                        "is_zero",      "is_one"};
                    if (primitive_intrinsics.count(method)) {
                        TML_DEBUG_LN("[METHOD 4b] Delegating primitive method to "
                                     "gen_primitive_method (required_behaviors)");
                        // If receiver was originally a ref T, we need to dereference
                        // to get the primitive value for methods like to_owned, duplicate
                        std::string actual_receiver = receiver;
                        if (receiver_was_ref) {
                            std::string prim_ty = llvm_type_from_semantic(sub_it->second);
                            actual_receiver = fresh_reg();
                            emit_line("  " + actual_receiver + " = load " + prim_ty + ", ptr " +
                                      receiver);
                        }
                        auto prim_result = gen_primitive_method(call, actual_receiver, receiver_ptr,
                                                                sub_it->second);
                        if (prim_result) {
                            return *prim_result;
                        }
                    }
                }

                // First, try to directly dispatch to ConcreteType::method
                // This handles cases where the behavior definition isn't loaded but the impl exists
                std::string qualified_name = concrete_type_name + "::" + method;
                TML_DEBUG_LN("[METHOD 4b] trying direct qualified_name=" << qualified_name);
                auto func_sig = env_.lookup_func(qualified_name);
                // Check if type is from a library module (works for impl methods too)
                bool is_from_library = this->is_library_method(concrete_type_name, method);

                // If not found locally, search modules
                if (!func_sig && env_.module_registry()) {
                    const auto& all_modules = env_.module_registry()->get_all_modules();
                    for (const auto& [mod_name, mod] : all_modules) {
                        auto func_it = mod.functions.find(qualified_name);
                        if (func_it != mod.functions.end()) {
                            func_sig = func_it->second;
                            is_from_library = true;
                            break;
                        }
                    }
                }

                TML_DEBUG_LN("[METHOD 4b] func_sig found: " << (func_sig ? "yes" : "no"));

                if (func_sig) {
                    // Only use suite prefix for test-local functions, not library methods
                    std::string prefix = is_from_library ? "" : get_suite_prefix();
                    std::string fn_name = "@tml_" + prefix + concrete_type_name + "_" + method;

                    std::vector<std::pair<std::string, std::string>> typed_args;
                    std::string this_val = receiver;
                    if (call.receiver->is<parser::IdentExpr>()) {
                        const auto& ident = call.receiver->as<parser::IdentExpr>();
                        auto it = locals_.find(ident.name);
                        if (it != locals_.end()) {
                            if (it->second.type == "ptr") {
                                this_val = receiver;
                            } else {
                                this_val = it->second.reg;
                            }
                        }
                    } else if (call.receiver->is<parser::FieldExpr>()) {
                        // For field expressions:
                        // - If receiver_ptr is set, use it (pointer to field)
                        // - Otherwise, spill the struct to stack
                        if (last_expr_type_ == "ptr") {
                            this_val = receiver;
                        } else if (!receiver_ptr.empty()) {
                            this_val = receiver_ptr;
                        } else if (last_expr_type_.starts_with("%struct.")) {
                            // Spill struct to stack for method call
                            std::string tmp = fresh_reg();
                            emit_line("  " + tmp + " = alloca " + last_expr_type_);
                            emit_line("  store " + last_expr_type_ + " " + receiver + ", ptr " +
                                      tmp);
                            this_val = tmp;
                        }
                    }

                    // Determine 'this' type based on func_sig first param
                    // For instance methods (self/this), struct types are always passed as ptr
                    // Only primitives are passed by value
                    std::string this_type = "ptr";
                    if (!func_sig->params.empty()) {
                        auto first_param_type = func_sig->params[0];
                        std::string llvm_first = llvm_type_from_semantic(first_param_type);
                        // Primitives (i8, i16, i32, etc.) are passed by value
                        // Structs/classes (%struct.X, %class.X) are passed by ptr
                        if (llvm_first[0] != '%') {
                            this_type = llvm_first; // primitive
                        }
                        // else keep as "ptr" for structs
                    }
                    typed_args.push_back({this_type, this_val});

                    for (size_t i = 0; i < call.args.size(); ++i) {
                        std::string val = gen_expr(*call.args[i]);
                        std::string arg_type = "i32";
                        if (i + 1 < func_sig->params.size()) {
                            arg_type = llvm_type_from_semantic(func_sig->params[i + 1]);
                        }
                        typed_args.push_back({arg_type, val});
                    }

                    // Get return type with Self substitution
                    std::string ret_type = "void";
                    if (func_sig->return_type) {
                        auto return_type = func_sig->return_type;
                        // Substitute Self with the concrete type
                        if (sub_it != current_type_subs_.end()) {
                            std::unordered_map<std::string, types::TypePtr> self_subs;
                            self_subs["Self"] = sub_it->second;
                            return_type = types::substitute_type(return_type, self_subs);
                        }
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
                        emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" +
                                  args_str + ")");
                        last_expr_type_ = ret_type;
                        return result;
                    }
                }
            }
        }
    }

    // =========================================================================
    // 5. Handle Ptr[T] methods
    // =========================================================================
    if (receiver_type && receiver_type->is<types::PtrType>()) {
        const auto& ptr_type = receiver_type->as<types::PtrType>();
        std::string inner_llvm_type = llvm_type_from_semantic(ptr_type.inner);

        if (method == "read") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + inner_llvm_type + ", ptr " + receiver);
            last_expr_type_ = inner_llvm_type;
            return result;
        }
        if (method == "write") {
            if (call.args.empty()) {
                report_error("Ptr.write() requires a value argument", call.span, "C008");
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            emit_line("  store " + inner_llvm_type + " " + val + ", ptr " + receiver);
            return "void";
        }
        if (method == "offset") {
            if (call.args.empty()) {
                report_error("Ptr.offset() requires an offset argument", call.span, "C008");
                return receiver;
            }
            std::string offset = gen_expr(*call.args[0]);
            std::string offset_i64 = fresh_reg();
            emit_line("  " + offset_i64 + " = sext i32 " + offset + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr " + inner_llvm_type + ", ptr " + receiver +
                      ", i64 " + offset_i64);
            last_expr_type_ = "ptr";
            return result;
        }
        if (method == "is_null") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp eq ptr " + receiver + ", null");
            last_expr_type_ = "i1";
            return result;
        }
    }

    // =========================================================================
    // 6. Handle primitive type methods
    // =========================================================================
    auto prim_result = gen_primitive_method(call, receiver, receiver_ptr, receiver_type);
    if (prim_result) {
        return *prim_result;
    }

    // =========================================================================
    // 6b. Handle primitive type behavior methods (see method_prim_behavior.cpp)
    // =========================================================================
    if (auto prim_behavior_result = try_gen_primitive_behavior_method(
            call, receiver, receiver_type, receiver_type_name, receiver_was_ref)) {
        return *prim_behavior_result;
    }

    // =========================================================================
    // 7. Handle Ordering enum methods
    // =========================================================================
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        if (named.name == "Ordering") {
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue %struct.Ordering " + receiver + ", 0");

            if (method == "is_less") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 0");
                last_expr_type_ = "i1";
                return result;
            }
            if (method == "is_equal") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 1");
                last_expr_type_ = "i1";
                return result;
            }
            if (method == "is_greater") {
                std::string result = fresh_reg();
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 2");
                last_expr_type_ = "i1";
                return result;
            }
            if (method == "reverse") {
                std::string is_less = fresh_reg();
                emit_line("  " + is_less + " = icmp eq i32 " + tag_val + ", 0");
                std::string is_greater = fresh_reg();
                emit_line("  " + is_greater + " = icmp eq i32 " + tag_val + ", 2");
                std::string sel1 = fresh_reg();
                emit_line("  " + sel1 + " = select i1 " + is_greater + ", i32 0, i32 1");
                std::string new_tag = fresh_reg();
                emit_line("  " + new_tag + " = select i1 " + is_less + ", i32 2, i32 " + sel1);
                std::string result = fresh_reg();
                emit_line("  " + result + " = insertvalue %struct.Ordering undef, i32 " + new_tag +
                          ", 0");
                last_expr_type_ = "%struct.Ordering";
                return result;
            }
            if (method == "then_cmp") {
                if (call.args.empty()) {
                    report_error("then_cmp() requires an argument", call.span, "C008");
                    return "0";
                }
                std::string other = gen_expr(*call.args[0]);
                std::string other_tag = fresh_reg();
                emit_line("  " + other_tag + " = extractvalue %struct.Ordering " + other + ", 0");
                std::string is_equal = fresh_reg();
                emit_line("  " + is_equal + " = icmp eq i32 " + tag_val + ", 1");
                std::string new_tag = fresh_reg();
                emit_line("  " + new_tag + " = select i1 " + is_equal + ", i32 " + other_tag +
                          ", i32 " + tag_val);
                std::string result = fresh_reg();
                emit_line("  " + result + " = insertvalue %struct.Ordering undef, i32 " + new_tag +
                          ", 0");
                last_expr_type_ = "%struct.Ordering";
                return result;
            }
            if (method == "to_string") {
                std::string less_str = add_string_literal("Less");
                std::string equal_str = add_string_literal("Equal");
                std::string greater_str = add_string_literal("Greater");
                std::string is_less = fresh_reg();
                emit_line("  " + is_less + " = icmp eq i32 " + tag_val + ", 0");
                std::string is_equal = fresh_reg();
                emit_line("  " + is_equal + " = icmp eq i32 " + tag_val + ", 1");
                std::string sel1 = fresh_reg();
                emit_line("  " + sel1 + " = select i1 " + is_equal + ", ptr " + equal_str +
                          ", ptr " + greater_str);
                std::string result = fresh_reg();
                emit_line("  " + result + " = select i1 " + is_less + ", ptr " + less_str +
                          ", ptr " + sel1);
                last_expr_type_ = "ptr";
                return result;
            }
            if (method == "debug_string") {
                std::string less_str = add_string_literal("Ordering::Less");
                std::string equal_str = add_string_literal("Ordering::Equal");
                std::string greater_str = add_string_literal("Ordering::Greater");
                std::string is_less = fresh_reg();
                emit_line("  " + is_less + " = icmp eq i32 " + tag_val + ", 0");
                std::string is_equal = fresh_reg();
                emit_line("  " + is_equal + " = icmp eq i32 " + tag_val + ", 1");
                std::string sel1 = fresh_reg();
                emit_line("  " + sel1 + " = select i1 " + is_equal + ", ptr " + equal_str +
                          ", ptr " + greater_str);
                std::string result = fresh_reg();
                emit_line("  " + result + " = select i1 " + is_less + ", ptr " + less_str +
                          ", ptr " + sel1);
                last_expr_type_ = "ptr";
                return result;
            }
        }

        // Handle Maybe[T] methods
        if (named.name == "Maybe") {
            std::string enum_type_name = llvm_type_from_semantic(receiver_type, true);

            // If receiver is from field access, it's a pointer - need to load first
            std::string maybe_val = receiver;
            if (call.receiver->is<parser::FieldExpr>() && enum_type_name.starts_with("%struct.")) {
                std::string loaded = fresh_reg();
                emit_line("  " + loaded + " = load " + enum_type_name + ", ptr " + receiver);
                maybe_val = loaded;
            }

            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue " + enum_type_name + " " + maybe_val +
                      ", 0");
            auto result = gen_maybe_method(call, maybe_val, enum_type_name, tag_val, named);
            if (result) {
                return *result;
            }
        }

        // Handle Outcome[T, E] methods
        if (named.name == "Outcome" && named.type_args.size() >= 2) {
            std::string enum_type_name = llvm_type_from_semantic(receiver_type, true);

            // If receiver is from field access, it's a pointer - need to load first
            std::string outcome_val = receiver;
            if (call.receiver->is<parser::FieldExpr>() && enum_type_name.starts_with("%struct.")) {
                std::string loaded = fresh_reg();
                emit_line("  " + loaded + " = load " + enum_type_name + ", ptr " + receiver);
                outcome_val = loaded;
            }

            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue " + enum_type_name + " " + outcome_val +
                      ", 0");
            auto result = gen_outcome_method(call, outcome_val, enum_type_name, tag_val, named);
            if (result) {
                return *result;
            }
        }
    }

    // Special case: handle is_ok/is_err on compare_exchange results when type inference failed
    // The receiver might be I32 due to fallback, but if the receiver is a compare_exchange call,
    // we know it returns Outcome and should handle is_ok/is_err accordingly
    if ((method == "is_ok" || method == "is_err") && call.receiver->is<parser::MethodCallExpr>()) {
        const auto& inner_call = call.receiver->as<parser::MethodCallExpr>();
        if (inner_call.method == "compare_exchange" ||
            inner_call.method == "compare_exchange_weak") {
            // The receiver is a compare_exchange call - assume Outcome type
            // For is_ok/is_err, we just need to check the tag field
            // Outcome is represented as { i32 tag, inner_type value }
            // tag 0 = Ok, tag 1 = Err
            std::string tag_val = fresh_reg();
            emit_line("  " + tag_val + " = extractvalue { i32, i32 } " + receiver + ", 0");
            std::string result = fresh_reg();
            if (method == "is_ok") {
                emit_line("  " + result + " = icmp eq i32 " + tag_val + ", 0");
            } else { // is_err
                emit_line("  " + result + " = icmp ne i32 " + tag_val + ", 0");
            }
            last_expr_type_ = "i1";
            return result;
        }
    }

    // =========================================================================
    // 8. Handle Slice/MutSlice methods
    // =========================================================================
    auto slice_result = gen_slice_method(call, receiver, receiver_type_name, receiver_type);
    if (slice_result) {
        return *slice_result;
    }

    // =========================================================================
    // 9. Handle collection methods (List, HashMap, Buffer)
    // =========================================================================
    auto coll_result = gen_collection_method(call, receiver, receiver_type_name, receiver_type);
    if (coll_result) {
        return *coll_result;
    }

    // =========================================================================
    // 10. Check for user-defined impl methods (see method_impl.cpp)
    // =========================================================================
    if (auto impl_result = try_gen_impl_method_call(call, receiver, receiver_ptr, receiver_type)) {
        return *impl_result;
    }

    // =========================================================================
    // 11. Try module lookup for impl methods (see method_impl.cpp)
    // =========================================================================
    if (auto module_impl_result =
            try_gen_module_impl_method_call(call, receiver, receiver_ptr, receiver_type)) {
        return *module_impl_result;
    }

    // =========================================================================
    // 12. Handle dyn dispatch (see method_dyn.cpp)
    // =========================================================================
    if (auto dyn_result = try_gen_dyn_dispatch_call(call, receiver, receiver_type)) {
        return *dyn_result;
    }

    // =========================================================================
    // 13. Handle Fn trait method calls on closures and function types
    // =========================================================================
    // Closures and function pointers implement Fn, FnMut, FnOnce
    // call(), call_mut(), call_once() invoke the callable
    if (method == "call" || method == "call_mut" || method == "call_once") {
        if (receiver_type) {
            // Handle ClosureType
            if (receiver_type->is<types::ClosureType>()) {
                const auto& closure_type = receiver_type->as<types::ClosureType>();

                // Generate arguments for the closure call
                std::string args_str;
                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string arg_val = gen_expr(*call.args[i]);
                    std::string arg_type = "i32"; // default
                    if (i < closure_type.params.size()) {
                        arg_type = llvm_type_from_semantic(closure_type.params[i]);
                    }
                    if (!args_str.empty())
                        args_str += ", ";
                    args_str += arg_type + " " + arg_val;
                }

                // Determine return type
                std::string ret_type = "i32";
                if (closure_type.return_type) {
                    ret_type = llvm_type_from_semantic(closure_type.return_type);
                }

                // Call the closure (receiver is function pointer)
                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + receiver + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result + " = call " + ret_type + " " + receiver + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }

            // Handle FuncType (function pointers)
            if (receiver_type->is<types::FuncType>()) {
                const auto& func_type = receiver_type->as<types::FuncType>();

                // Generate arguments for the function call
                std::string args_str;
                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string arg_val = gen_expr(*call.args[i]);
                    std::string arg_type = "i32"; // default
                    if (i < func_type.params.size()) {
                        arg_type = llvm_type_from_semantic(func_type.params[i]);
                    }
                    if (!args_str.empty())
                        args_str += ", ";
                    args_str += arg_type + " " + arg_val;
                }

                // Determine return type
                std::string ret_type = "void";
                if (func_type.return_type) {
                    ret_type = llvm_type_from_semantic(func_type.return_type);
                }

                // Call the function pointer
                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + receiver + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    emit_line("  " + result + " = call " + ret_type + " " + receiver + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        }
    }

    // =========================================================================
    // 14. Handle File instance methods
    // =========================================================================
    if (method == "is_open" || method == "read_line" || method == "write_str" || method == "size" ||
        method == "close") {
        std::string file_ptr = receiver_ptr;
        if (file_ptr.empty()) {
            file_ptr = fresh_reg();
            emit_line("  " + file_ptr + " = alloca %struct.File");
            emit_line("  store %struct.File " + receiver + ", ptr " + file_ptr);
        }

        std::string handle_field_ptr = fresh_reg();
        emit_line("  " + handle_field_ptr + " = getelementptr %struct.File, ptr " + file_ptr +
                  ", i32 0, i32 0");
        std::string handle = fresh_reg();
        emit_line("  " + handle + " = load ptr, ptr " + handle_field_ptr);

        if (method == "is_open") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @file_is_open(ptr " + handle + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "read_line") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @file_read_line(ptr " + handle + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        if (method == "write_str") {
            if (call.args.empty()) {
                report_error("write_str requires a content argument", call.span, "C008");
                return "0";
            }
            std::string content_arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @file_write_str(ptr " + handle + ", ptr " +
                      content_arg + ")");
            last_expr_type_ = "i1";
            return result;
        }
        if (method == "size") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @file_size(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "close") {
            emit_line("  call void @file_close(ptr " + handle + ")");
            return "void";
        }
    }

    // =========================================================================
    // 15-16. Handle class instance method calls (see method_class.cpp)
    // =========================================================================
    if (auto class_result =
            try_gen_class_instance_call(call, receiver, receiver_ptr, receiver_type)) {
        return *class_result;
    }

    // =========================================================================
    // 17. Handle function pointer field calls (e.g., vtable.call_fn(args))
    // =========================================================================
    {
        // Get the receiver type name - use receiver_type_name that was computed earlier
        std::string fn_field_type_name = receiver_type_name;

        // Look up the struct definition
        auto struct_def = env_.lookup_struct(fn_field_type_name);
        if (struct_def) {
            // Look for a field with the method name
            int field_idx = 0;
            for (const auto& [field_name, field_type] : struct_def->fields) {
                if (field_name == method) {
                    // Check if the field is a function type
                    if (field_type->is<types::FuncType>()) {
                        const auto& func = field_type->as<types::FuncType>();

                        // Get pointer to the field
                        std::string field_ptr = fresh_reg();
                        emit_line("  " + field_ptr + " = getelementptr inbounds %struct." +
                                  fn_field_type_name + ", ptr " + receiver_ptr + ", i32 0, i32 " +
                                  std::to_string(field_idx));

                        // Load the function pointer
                        std::string func_ptr = fresh_reg();
                        emit_line("  " + func_ptr + " = load ptr, ptr " + field_ptr);

                        // Generate arguments
                        std::vector<std::string> arg_values;
                        std::vector<std::string> arg_types;
                        for (size_t i = 0; i < call.args.size(); ++i) {
                            std::string arg = gen_expr(*call.args[i]);
                            arg_values.push_back(arg);
                            if (i < func.params.size()) {
                                arg_types.push_back(llvm_type_from_semantic(func.params[i]));
                            } else {
                                arg_types.push_back(last_expr_type_);
                            }
                        }

                        // Determine return type
                        std::string ret_type = llvm_type_from_semantic(func.return_type);

                        // Build argument list
                        std::string args_str;
                        for (size_t i = 0; i < arg_values.size(); ++i) {
                            if (i > 0)
                                args_str += ", ";
                            args_str += arg_types[i] + " " + arg_values[i];
                        }

                        // Emit the call
                        std::string result;
                        if (ret_type != "void") {
                            result = fresh_reg();
                            emit_line("  " + result + " = call " + ret_type + " " + func_ptr + "(" +
                                      args_str + ")");
                        } else {
                            emit_line("  call void " + func_ptr + "(" + args_str + ")");
                        }

                        last_expr_type_ = ret_type;
                        return ret_type == "void" ? "void" : result;
                    }
                    break;
                }
                field_idx++;
            }
        }
    }

    report_error("Unknown method: " + method, call.span, "C006");
    return "0";
}

} // namespace tml::codegen
