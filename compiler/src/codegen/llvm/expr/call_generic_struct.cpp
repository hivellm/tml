//! # LLVM IR Generator - Generic Struct Static Method Calls
//!
//! Handles calls like Range::new(0, 10) where Range is a generic struct.
//! Split from call.cpp for file size management.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <cctype>

namespace tml::codegen {

// Static helper to parse mangled type strings like "Mutex__I32" into proper TypePtr
// Duplicated from call.cpp since both files need it and it's a pure helper function.
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

    if (s.substr(0, 4) == "ptr_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.inner = inner};
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

auto LLVMIRGen::gen_call_generic_struct_method(const parser::CallExpr& call,
                                               const std::string& fn_name)
    -> std::optional<std::string> {
    (void)fn_name; // Used for signature consistency with gen_call dispatch
    // ============ GENERIC STRUCT STATIC METHODS ============
    // Handle calls like Range::new(0, 10) where Range is a generic struct
    // These need type inference from expected_enum_type_ context

    if (call.callee->is<parser::PathExpr>()) {
        const auto& path_expr = call.callee->as<parser::PathExpr>();
        const auto& path = path_expr.path;
        if (path.segments.size() == 2) {
            const std::string& type_name = path.segments[0];
            const std::string& method = path.segments[1];

            // FIRST: Check for explicit type arguments like StackNode::new[T](...)
            // This handles internal (non-pub) generic structs that aren't in the module registry
            // We resolve these using current_type_subs_ regardless of whether we recognize the
            // struct
            if (path_expr.generics.has_value() && !path_expr.generics->args.empty() &&
                !current_type_subs_.empty()) {
                const auto& gen_args = path_expr.generics->args;
                std::vector<types::TypePtr> resolved_type_args;
                std::unordered_map<std::string, types::TypePtr> type_subs;

                for (size_t i = 0; i < gen_args.size(); ++i) {
                    if (gen_args[i].is_type()) {
                        // Resolve using current_type_subs_ to handle T -> I32
                        auto resolved = resolve_parser_type_with_subs(*gen_args[i].as_type(),
                                                                      current_type_subs_);
                        resolved_type_args.push_back(resolved);
                        // Use generic placeholder names since we don't know the struct's param
                        // names
                        type_subs["T" + std::to_string(i)] = resolved;
                    }
                }

                // If we resolved any type args, generate the monomorphized call
                if (!resolved_type_args.empty()) {
                    std::string mangled_type_name =
                        type_name + "__" + mangle_type_args(resolved_type_args);

                    // Look up the function signature
                    std::string qualified_name = type_name + "::" + method;
                    auto func_sig = env_.lookup_func(qualified_name);

                    // Also search in module registry
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

                    // Generate the monomorphized call
                    // Don't add suite prefix for library-internal types (they're defined in library
                    // code) If func_sig exists, it's an exported function - check if type is local
                    // If func_sig doesn't exist, it's a library-internal function - never use suite
                    // prefix
                    bool is_library_internal = !func_sig;
                    // A type is truly local only if its impl block exists in
                    // pending_generic_impls_ (defined in the current file). Having it in
                    // pending_generic_structs_ is not sufficient — it could have been
                    // registered from a library module's source during a previous
                    // instantiation pass.
                    bool has_local_impl = pending_generic_impls_.count(type_name) > 0;
                    bool is_local_type = !is_library_internal && has_local_impl;
                    std::string prefix = is_local_type ? get_suite_prefix() : "";
                    std::string fn_name_call = "@tml_" + prefix + mangled_type_name + "_" + method;

                    if (func_sig) {
                        // Request impl method instantiation
                        std::string mangled_method = "tml_" + mangled_type_name + "_" + method;
                        if (generated_impl_methods_.find(mangled_method) ==
                            generated_impl_methods_.end()) {
                            // Create type_subs using actual generic param names from func sig
                            std::unordered_map<std::string, types::TypePtr> actual_type_subs;
                            if (func_sig->type_params.size() == resolved_type_args.size()) {
                                for (size_t i = 0; i < func_sig->type_params.size(); ++i) {
                                    actual_type_subs[func_sig->type_params[i]] =
                                        resolved_type_args[i];
                                }
                            } else {
                                actual_type_subs = type_subs;
                            }

                            // Mark as library type if the impl is not defined locally.
                            // This ensures the module search in generic.cpp will check
                            // source code directly rather than requiring the struct to be
                            // in the public mod.structs map.
                            pending_impl_method_instantiations_.push_back(PendingImplMethod{
                                mangled_type_name, method, actual_type_subs, type_name, "",
                                /*is_library_type=*/!has_local_impl});
                            generated_impl_methods_.insert(mangled_method);
                        }

                        // Generate arguments
                        std::vector<std::pair<std::string, std::string>> typed_args;
                        for (size_t i = 0; i < call.args.size(); ++i) {
                            std::string val = gen_expr(*call.args[i]);
                            std::string actual_type = last_expr_type_;
                            std::string arg_type = actual_type;
                            if (i < func_sig->params.size()) {
                                auto param_type =
                                    types::substitute_type(func_sig->params[i], type_subs);
                                arg_type = llvm_type_from_semantic(param_type);
                                if (param_type->is<types::FuncType>()) {
                                    arg_type = "{ ptr, ptr }";
                                }
                            }
                            // ptr -> { ptr, ptr }: wrap bare function pointer in fat pointer
                            if (actual_type == "ptr" && arg_type == "{ ptr, ptr }") {
                                std::string fat1 = fresh_reg();
                                std::string fat2 = fresh_reg();
                                emit_line("  " + fat1 + " = insertvalue { ptr, ptr } undef, ptr " +
                                          val + ", 0");
                                emit_line("  " + fat2 + " = insertvalue { ptr, ptr } " + fat1 +
                                          ", ptr null, 1");
                                val = fat2;
                            }
                            typed_args.push_back({arg_type, val});
                        }

                        auto return_type = types::substitute_type(func_sig->return_type, type_subs);
                        std::string ret_type = llvm_type_from_semantic(return_type);

                        std::string args_str;
                        for (size_t i = 0; i < typed_args.size(); ++i) {
                            if (i > 0)
                                args_str += ", ";
                            args_str += typed_args[i].first + " " + typed_args[i].second;
                        }

                        std::string result = fresh_reg();
                        if (ret_type == "void") {
                            emit_line("  call void " + fn_name_call + "(" + args_str + ")");
                            last_expr_type_ = "void";
                            return "void";
                        } else {
                            emit_line("  " + result + " = call " + ret_type + " " + fn_name_call +
                                      "(" + args_str + ")");
                            last_expr_type_ = ret_type;
                            return result;
                        }
                    } else {
                        // No func_sig found - this is likely an internal (non-exported) function
                        // Request instantiation for internal library type
                        std::string mangled_method = "tml_" + mangled_type_name + "_" + method;
                        if (generated_impl_methods_.find(mangled_method) ==
                            generated_impl_methods_.end()) {
                            // Create type_subs with default generic name "T"
                            std::unordered_map<std::string, types::TypePtr> internal_type_subs;
                            for (size_t i = 0; i < resolved_type_args.size(); ++i) {
                                // Common generic param names
                                std::string param_name = (i == 0) ? "T" : "T" + std::to_string(i);
                                internal_type_subs[param_name] = resolved_type_args[i];
                            }

                            pending_impl_method_instantiations_.push_back(PendingImplMethod{
                                mangled_type_name, method, internal_type_subs, type_name, "",
                                /*is_library_type=*/true}); // Mark as library type for internal
                                                            // lookup
                            generated_impl_methods_.insert(mangled_method);
                        }

                        // Generate the call using just the argument types we can infer
                        std::vector<std::pair<std::string, std::string>> typed_args;
                        for (size_t i = 0; i < call.args.size(); ++i) {
                            std::string val = gen_expr(*call.args[i]);
                            // Function-typed args keep fat pointer { ptr, ptr } — no coercion
                            std::string arg_type = last_expr_type_;
                            typed_args.push_back({arg_type, val});
                        }

                        std::string args_str;
                        for (size_t i = 0; i < typed_args.size(); ++i) {
                            if (i > 0)
                                args_str += ", ";
                            args_str += typed_args[i].first + " " + typed_args[i].second;
                        }

                        // Try to look up return type from pending_generic_impls_
                        // This handles methods like Node::sentinel() that return Ptr[Node[T]]
                        std::string ret_type = "ptr"; // Default to ptr for static methods
                        auto impl_it = pending_generic_impls_.find(type_name);
                        if (impl_it != pending_generic_impls_.end()) {
                            for (const auto& m : impl_it->second->methods) {
                                if (m.name == method && m.return_type.has_value()) {
                                    // Create type_subs using resolved type args
                                    std::unordered_map<std::string, types::TypePtr>
                                        method_type_subs;
                                    for (size_t i = 0; i < impl_it->second->generics.size() &&
                                                       i < resolved_type_args.size();
                                         ++i) {
                                        method_type_subs[impl_it->second->generics[i].name] =
                                            resolved_type_args[i];
                                    }
                                    auto resolved_ret = resolve_parser_type_with_subs(
                                        **m.return_type, method_type_subs);
                                    ret_type = llvm_type_from_semantic(resolved_ret);
                                    break;
                                }
                            }
                        }

                        std::string result = fresh_reg();
                        if (ret_type == "void") {
                            emit_line("  call void " + fn_name_call + "(" + args_str + ")");
                            last_expr_type_ = "void";
                            return "void";
                        } else {
                            emit_line("  " + result + " = call " + ret_type + " " + fn_name_call +
                                      "(" + args_str + ")");
                            last_expr_type_ = ret_type;
                            return result;
                        }
                    }
                }
            }

            // Check if this is an imported generic struct or enum
            std::vector<std::string> imported_type_params;
            if (env_.module_registry()) {
                const auto& all_modules = env_.module_registry()->get_all_modules();

                // First, try to resolve via imported symbols to get the correct module path
                // This is important when multiple modules define types with the same name
                // (e.g., core::ops::range::Range vs core::range::Range)
                std::string resolved_module_path;
                auto resolved = env_.resolve_imported_symbol(type_name);
                if (resolved) {
                    resolved_module_path = *resolved;
                    // Remove the type name suffix to get just the module path
                    auto last_sep = resolved_module_path.rfind("::");
                    if (last_sep != std::string::npos) {
                        resolved_module_path = resolved_module_path.substr(0, last_sep);
                    }
                }

                // Search with resolved_module_path filter first, then without
                for (int pass = 0; pass < 2 && imported_type_params.empty(); ++pass) {
                    for (const auto& [mod_name, mod] : all_modules) {
                        // First pass: only check resolved module; second pass: check all
                        if (pass == 0 && !resolved_module_path.empty() &&
                            mod_name != resolved_module_path) {
                            continue;
                        }
                        // Skip second pass if we had no filter
                        if (pass == 1 && resolved_module_path.empty()) {
                            break;
                        }

                        // Check structs (public and internal)
                        auto struct_it = mod.structs.find(type_name);
                        if (struct_it != mod.structs.end() &&
                            !struct_it->second.type_params.empty()) {
                            imported_type_params = struct_it->second.type_params;
                            break;
                        }
                        auto internal_it = mod.internal_structs.find(type_name);
                        if (internal_it != mod.internal_structs.end() &&
                            !internal_it->second.type_params.empty()) {
                            imported_type_params = internal_it->second.type_params;
                            break;
                        }
                        // Check enums
                        auto enum_it = mod.enums.find(type_name);
                        if (enum_it != mod.enums.end() && !enum_it->second.type_params.empty()) {
                            imported_type_params = enum_it->second.type_params;
                            break;
                        }
                    }
                }
            }

            // Also check local generic structs and enums
            bool is_local_generic = pending_generic_structs_.count(type_name) > 0 ||
                                    pending_generic_enums_.count(type_name) > 0 ||
                                    pending_generic_impls_.count(type_name) > 0;

            if (!imported_type_params.empty() || is_local_generic) {
                // This is a generic struct static method - infer type args
                std::string mangled_type_name = type_name;
                std::unordered_map<std::string, types::TypePtr> type_subs;
                std::vector<std::string> generic_names;

                // Get generic parameter names
                auto impl_it = pending_generic_impls_.find(type_name);
                if (impl_it != pending_generic_impls_.end()) {
                    // First try impl-level generics (impl[T] Foo[T])
                    for (const auto& g : impl_it->second->generics) {
                        generic_names.push_back(g.name);
                    }
                    // If empty, extract from self_type generics (impl Foo[T])
                    if (generic_names.empty() &&
                        impl_it->second->self_type->kind.index() == 0) { // NamedType
                        const auto& named =
                            std::get<parser::NamedType>(impl_it->second->self_type->kind);
                        if (named.generics.has_value()) {
                            for (const auto& arg : named.generics->args) {
                                if (arg.is_type()) {
                                    const auto& t = arg.as_type();
                                    if (t->kind.index() == 0) { // NamedType
                                        const auto& inner_named =
                                            std::get<parser::NamedType>(t->kind);
                                        if (!inner_named.path.segments.empty()) {
                                            generic_names.push_back(
                                                inner_named.path.segments.back());
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else if (!imported_type_params.empty()) {
                    generic_names = imported_type_params;
                } else {
                    // Fallback: check pending_generic_structs_ for type params
                    auto struct_it = pending_generic_structs_.find(type_name);
                    if (struct_it != pending_generic_structs_.end()) {
                        for (const auto& g : struct_it->second->generics) {
                            generic_names.push_back(g.name);
                        }
                    }
                }

                // Check for explicit type arguments like StackNode::new[T](...)
                // This handles the case where the call has explicit generic args that
                // need to be resolved using current_type_subs_
                // (path_expr already declared above, reusing it)
                if (path_expr.generics.has_value() && !path_expr.generics->args.empty()) {
                    const auto& gen_args = path_expr.generics->args;
                    std::vector<types::TypePtr> resolved_type_args;

                    for (size_t i = 0; i < gen_args.size(); ++i) {
                        if (gen_args[i].is_type()) {
                            // Resolve using current_type_subs_ to handle T -> I32
                            auto resolved = resolve_parser_type_with_subs(*gen_args[i].as_type(),
                                                                          current_type_subs_);
                            resolved_type_args.push_back(resolved);

                            // Map to generic parameter name if available
                            if (i < generic_names.size()) {
                                type_subs[generic_names[i]] = resolved;
                            }
                        }
                    }

                    // Build mangled type name from resolved args
                    if (!resolved_type_args.empty()) {
                        mangled_type_name = type_name + "__" + mangle_type_args(resolved_type_args);
                    }
                }

                // Try to infer from expected_enum_type_ (only if we don't already have type_subs)
                if (type_subs.empty() && !expected_enum_type_.empty() &&
                    expected_enum_type_.find("%struct." + type_name + "__") == 0) {
                    mangled_type_name = expected_enum_type_.substr(8); // Remove "%struct."

                    // Extract type args from mangled name (e.g., Range__I64 -> I64)
                    std::string suffix = mangled_type_name.substr(type_name.length());
                    if (suffix.starts_with("__") && generic_names.size() == 1) {
                        std::string type_arg_str = suffix.substr(2);
                        types::TypePtr type_arg = nullptr;

                        auto make_prim = [](types::PrimitiveKind kind) -> types::TypePtr {
                            auto t = std::make_shared<types::Type>();
                            t->kind = types::PrimitiveType{kind};
                            return t;
                        };

                        if (type_arg_str == "I64")
                            type_arg = types::make_i64();
                        else if (type_arg_str == "I32")
                            type_arg = types::make_i32();
                        else if (type_arg_str == "I8")
                            type_arg = make_prim(types::PrimitiveKind::I8);
                        else if (type_arg_str == "I16")
                            type_arg = make_prim(types::PrimitiveKind::I16);
                        else if (type_arg_str == "U8")
                            type_arg = make_prim(types::PrimitiveKind::U8);
                        else if (type_arg_str == "U16")
                            type_arg = make_prim(types::PrimitiveKind::U16);
                        else if (type_arg_str == "U32")
                            type_arg = make_prim(types::PrimitiveKind::U32);
                        else if (type_arg_str == "U64")
                            type_arg = make_prim(types::PrimitiveKind::U64);
                        else if (type_arg_str == "F32")
                            type_arg = make_prim(types::PrimitiveKind::F32);
                        else if (type_arg_str == "F64")
                            type_arg = types::make_f64();
                        else if (type_arg_str == "Bool")
                            type_arg = types::make_bool();
                        else if (type_arg_str == "Str")
                            type_arg = types::make_str();
                        // Handle mangled pointer types: ptr_I32 -> PtrType{I32}
                        else if (type_arg_str.starts_with("ptr_")) {
                            std::string inner_str = type_arg_str.substr(4);
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
                                // For struct types, use parse_mangled_type_string for proper
                                // handling
                                inner = parse_mangled_type_string(inner_str);
                            }
                            type_arg = std::make_shared<types::Type>();
                            type_arg->kind = types::PtrType{false, inner};
                        }
                        // Handle mangled mutable pointer types: mutptr_I32 -> PtrType{mut, I32}
                        else if (type_arg_str.starts_with("mutptr_")) {
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
                                // For struct types, use parse_mangled_type_string for proper
                                // handling
                                inner = parse_mangled_type_string(inner_str);
                            }
                            type_arg = std::make_shared<types::Type>();
                            type_arg->kind = types::PtrType{true, inner};
                        } else {
                            // Handle nested generic types using static helper (avoids std::function
                            // overhead)
                            type_arg = parse_mangled_type_string(type_arg_str);
                        }

                        if (type_arg && !generic_names.empty()) {
                            type_subs[generic_names[0]] = type_arg;
                        }
                    }
                }

                // If expected_enum_type_ didn't give us type info, try current_type_subs_
                // This handles cases where we're inside a generic function and calling
                // a generic struct's static method (e.g., from_addr[T] calling RawPtr::from_addr)
                if (type_subs.empty() && !current_type_subs_.empty() && !generic_names.empty()) {
                    for (const auto& gname : generic_names) {
                        auto it = current_type_subs_.find(gname);
                        if (it != current_type_subs_.end()) {
                            type_subs[gname] = it->second;
                            mangled_type_name = type_name + "__" + mangle_type(it->second);
                        }
                    }
                }

                // Infer type args from argument types
                // E.g., ManuallyDrop::into_inner(md) where md: ManuallyDrop[I64] -> T = I64
                if (type_subs.empty() && !generic_names.empty() && !call.args.empty()) {
                    // Get the function signature to know param types
                    std::string qualified_name = type_name + "::" + method;
                    auto func_sig = env_.lookup_func(qualified_name);
                    if (!func_sig && env_.module_registry()) {
                        const auto& all_modules = env_.module_registry()->get_all_modules();
                        for (const auto& [mod_name, mod] : all_modules) {
                            auto func_it2 = mod.functions.find(qualified_name);
                            if (func_it2 != mod.functions.end()) {
                                func_sig = func_it2->second;
                                break;
                            }
                        }
                    }

                    if (func_sig) {
                        // Try to infer type args from argument types matching param types
                        for (size_t i = 0; i < call.args.size() && i < func_sig->params.size();
                             ++i) {
                            auto arg_type = infer_expr_type(*call.args[i]);
                            const auto& param_type = func_sig->params[i];

                            if (!arg_type || !param_type)
                                continue;

                            // Case 1: param is a bare generic type param (e.g., T)
                            // and arg is any concrete type (primitive, struct, etc.)
                            // E.g., Mutex::new(42) where param is T, arg is I32 -> T = I32
                            if (param_type->is<types::NamedType>()) {
                                const auto& param_named = param_type->as<types::NamedType>();
                                if (param_named.type_args.empty()) {
                                    // Check if param name matches a generic parameter
                                    for (const auto& gname : generic_names) {
                                        if (param_named.name == gname) {
                                            type_subs[gname] = arg_type;
                                            break;
                                        }
                                    }
                                }
                            }

                            // Case 2: param is GenericType (e.g., from local AST)
                            if (param_type->is<types::GenericType>()) {
                                const auto& param_generic = param_type->as<types::GenericType>();
                                for (const auto& gname : generic_names) {
                                    if (param_generic.name == gname) {
                                        type_subs[gname] = arg_type;
                                        break;
                                    }
                                }
                            }

                            // Case 3: arg is NamedType[X] and param is NamedType[T],
                            // map T -> X (e.g., ManuallyDrop::into_inner(md) where
                            // md: ManuallyDrop[I64])
                            if (arg_type->is<types::NamedType>() &&
                                param_type->is<types::NamedType>()) {
                                const auto& arg_named = arg_type->as<types::NamedType>();
                                const auto& param_named = param_type->as<types::NamedType>();
                                if (arg_named.name == param_named.name &&
                                    !arg_named.type_args.empty() &&
                                    arg_named.type_args.size() == param_named.type_args.size()) {
                                    // Map generic params to concrete types
                                    for (size_t j = 0;
                                         j < generic_names.size() && j < arg_named.type_args.size();
                                         ++j) {
                                        type_subs[generic_names[j]] = arg_named.type_args[j];
                                    }
                                }
                            }
                        }

                        // Update mangled type name from inferred type_subs
                        if (!type_subs.empty()) {
                            std::vector<types::TypePtr> type_args;
                            for (const auto& gname : generic_names) {
                                auto it = type_subs.find(gname);
                                if (it != type_subs.end()) {
                                    type_args.push_back(it->second);
                                }
                            }
                            if (!type_args.empty()) {
                                mangled_type_name = type_name + "__" + mangle_type_args(type_args);
                            }
                        }
                    }
                }

                // If we successfully inferred type args, generate the monomorphized call
                if (!type_subs.empty()) {
                    std::string qualified_name = type_name + "::" + method;
                    auto func_sig = env_.lookup_func(qualified_name);

                    // If not found locally, search modules
                    if (!func_sig && env_.module_registry()) {
                        const auto& all_modules = env_.module_registry()->get_all_modules();
                        for (const auto& [mod_name, mod] : all_modules) {
                            auto func_it2 = mod.functions.find(qualified_name);
                            if (func_it2 != mod.functions.end()) {
                                func_sig = func_it2->second;
                                break;
                            }
                        }
                    }

                    // Determine if this is an imported library type
                    bool is_imported = !imported_type_params.empty();

                    // For local generic impls, extract method signature from AST if not in env_
                    const parser::FuncDecl* local_method_decl = nullptr;
                    if (!func_sig && impl_it != pending_generic_impls_.end()) {
                        for (const auto& m : impl_it->second->methods) {
                            if (m.name == method) {
                                local_method_decl = &m;
                                break;
                            }
                        }
                    }

                    if (func_sig || local_method_decl) {
                        // Request impl method instantiation
                        std::string mangled_method = "tml_" + mangled_type_name + "_" + method;
                        if (generated_impl_methods_.find(mangled_method) ==
                            generated_impl_methods_.end()) {
                            bool is_local = impl_it != pending_generic_impls_.end();
                            if (is_local || is_imported) {
                                pending_impl_method_instantiations_.push_back(PendingImplMethod{
                                    mangled_type_name, method, type_subs, type_name, "",
                                    /*is_library_type=*/is_imported});
                                generated_impl_methods_.insert(mangled_method);
                            }
                        }

                        // Generate arguments with expected type context propagation
                        std::vector<std::pair<std::string, std::string>> typed_args;

                        // For local methods, determine param offset (skip 'this' if present)
                        size_t local_param_offset = 0;
                        if (local_method_decl && !local_method_decl->params.empty()) {
                            const auto& first_param = local_method_decl->params[0];
                            std::string first_name;
                            if (first_param.pattern &&
                                first_param.pattern->is<parser::IdentPattern>()) {
                                first_name = first_param.pattern->as<parser::IdentPattern>().name;
                            }
                            if (first_name == "this") {
                                local_param_offset = 1;
                            }
                        }

                        for (size_t i = 0; i < call.args.size(); ++i) {
                            // Set expected type context before generating argument
                            // This helps nested generic calls infer their type arguments
                            std::string saved_expected_enum = expected_enum_type_;

                            // Get param type from func_sig or local_method_decl
                            types::TypePtr param_semantic_type = nullptr;
                            if (func_sig && i < func_sig->params.size()) {
                                param_semantic_type =
                                    types::substitute_type(func_sig->params[i], type_subs);
                            } else if (local_method_decl) {
                                size_t param_idx = i + local_param_offset;
                                if (param_idx < local_method_decl->params.size()) {
                                    param_semantic_type = resolve_parser_type_with_subs(
                                        *local_method_decl->params[param_idx].type, type_subs);
                                }
                            }

                            if (param_semantic_type) {
                                std::string llvm_param_type =
                                    llvm_type_from_semantic(param_semantic_type);
                                // Set expected type for generic struct arguments
                                if (llvm_param_type.find("%struct.") == 0 &&
                                    llvm_param_type.find("__") != std::string::npos) {
                                    expected_enum_type_ = llvm_param_type;
                                }
                            }

                            std::string val = gen_expr(*call.args[i]);
                            expected_enum_type_ = saved_expected_enum;

                            std::string actual_type = last_expr_type_;
                            std::string arg_type = actual_type;
                            if (param_semantic_type) {
                                arg_type = llvm_type_from_semantic(param_semantic_type);
                                if (param_semantic_type->is<types::FuncType>()) {
                                    arg_type = "{ ptr, ptr }";
                                }
                            }
                            // ptr -> { ptr, ptr }: wrap bare function pointer in fat pointer
                            if (actual_type == "ptr" && arg_type == "{ ptr, ptr }") {
                                std::string fat1 = fresh_reg();
                                std::string fat2 = fresh_reg();
                                emit_line("  " + fat1 + " = insertvalue { ptr, ptr } undef, ptr " +
                                          val + ", 0");
                                emit_line("  " + fat2 + " = insertvalue { ptr, ptr } " + fat1 +
                                          ", ptr null, 1");
                                val = fat2;
                            }
                            typed_args.push_back({arg_type, val});
                        }

                        // Determine return type
                        std::string ret_type = "void";
                        if (func_sig) {
                            auto return_type =
                                types::substitute_type(func_sig->return_type, type_subs);
                            ret_type = llvm_type_from_semantic(return_type);
                        } else if (local_method_decl &&
                                   local_method_decl->return_type.has_value()) {
                            auto return_type = resolve_parser_type_with_subs(
                                **local_method_decl->return_type, type_subs);
                            ret_type = llvm_type_from_semantic(return_type);
                        }
                        // Look up in functions_ to get the correct LLVM name
                        std::string method_lookup_key = mangled_type_name + "_" + method;
                        auto method_it = functions_.find(method_lookup_key);
                        std::string fn_name_call;
                        if (method_it != functions_.end()) {
                            fn_name_call = method_it->second.llvm_name;
                        } else {
                            // Only use suite prefix for test-local functions, not library types
                            std::string prefix = is_imported ? "" : get_suite_prefix();
                            fn_name_call = "@tml_" + prefix + mangled_type_name + "_" + method;
                        }

                        std::string args_str;
                        for (size_t i = 0; i < typed_args.size(); ++i) {
                            if (i > 0)
                                args_str += ", ";
                            args_str += typed_args[i].first + " " + typed_args[i].second;
                        }

                        std::string result = fresh_reg();
                        if (ret_type == "void") {
                            emit_line("  call void " + fn_name_call + "(" + args_str + ")");
                            last_expr_type_ = "void";
                            return "void";
                        } else {
                            emit_line("  " + result + " = call " + ret_type + " " + fn_name_call +
                                      "(" + args_str + ")");
                            last_expr_type_ = ret_type;
                            return result;
                        }
                    }
                }
            }
        }
    }

    return std::nullopt;
}

} // namespace tml::codegen
