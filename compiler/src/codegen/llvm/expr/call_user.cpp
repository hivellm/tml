TML_MODULE("codegen_x86")

//! # LLVM IR Generator - User-Defined Function Calls
//!
//! Handles calls to user-defined functions, @extern functions, and
//! includes V8-style inline optimizations for hot runtime functions.
//! Split from call.cpp for file size management.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <cctype>

namespace tml::codegen {

auto LLVMIRGen::gen_call_user_function(const parser::CallExpr& call, const std::string& fn_name)
    -> std::string {
    // ============ USER-DEFINED FUNCTIONS ============

    // Try to look up the function signature first
    auto func_sig = env_.lookup_func(fn_name);

    // If not found via env_ lookup and we have a bare function name (no ::),
    // search all modules for the function. This handles library-internal calls
    // like "alloc_global" when generating monomorphized library functions.
    if (!func_sig.has_value() && fn_name.find("::") == std::string::npos &&
        env_.module_registry()) {
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            auto func_it_mod = mod.functions.find(fn_name);
            if (func_it_mod != mod.functions.end()) {
                func_sig = func_it_mod->second;
                break;
            }
        }
    }

    // Check if this function was registered (includes @extern functions)
    auto func_it = functions_.find(fn_name);

    // If not found directly, check if it's a qualified FFI call (e.g., "SDL2::init")
    // In this case, the function is registered with just the bare name ("init")
    if (func_it == functions_.end() && func_sig.has_value() && func_sig->has_ffi_module()) {
        // Look up with just the function name (without module prefix)
        func_it = functions_.find(func_sig->name);
    }

    // If still not found and this is a submodule call (e.g., "unicode_data::func"),
    // try looking up with the current module prefix instead (e.g., "core::unicode::func")
    // This handles intra-module calls where "pub mod submod" re-exports functions
    if (func_it == functions_.end() && !current_module_prefix_.empty()) {
        size_t first_sep = fn_name.find("::");
        if (first_sep != std::string::npos) {
            // Check if first segment is a type name (Type::method) vs submodule name
            // Type names start with uppercase; submodules are lowercase
            std::string first_segment = fn_name.substr(0, first_sep);
            bool is_type_method = !first_segment.empty() && std::isupper(first_segment[0]);

            // Also check if this looks like a static method call (Type_method exists in functions_)
            std::string potential_method_name = first_segment + "_" + fn_name.substr(first_sep + 2);
            bool has_impl_method = functions_.find(potential_method_name) != functions_.end();

            // Skip submodule resolution for Type::method calls
            if (!is_type_method && !has_impl_method) {
                // Build qualified name: replace "submod::" with "module_prefix::"
                // First convert current_module_prefix_ back to :: format
                std::string module_path = current_module_prefix_;
                size_t pos = 0;
                while ((pos = module_path.find("_", pos)) != std::string::npos) {
                    module_path.replace(pos, 1, "::");
                    pos += 2;
                }
                // Get the function part after the submodule prefix
                std::string func_part = fn_name.substr(first_sep + 2);
                // Try looking up with full module path
                std::string qualified_name = module_path + "::" + func_part;
                func_it = functions_.find(qualified_name);
            }
        } else {
            // Bare function name (no ::) - try qualifying with current module
            // This handles calls to same-module private functions
            std::string module_path = current_module_prefix_;
            size_t pos = 0;
            while ((pos = module_path.find("_", pos)) != std::string::npos) {
                module_path.replace(pos, 1, "::");
                pos += 2;
            }
            std::string qualified_name = module_path + "::" + fn_name;
            func_it = functions_.find(qualified_name);
        }
    }

    // Sanitize function name (replace :: with _) to match how impl methods are registered
    std::string sanitized_name = fn_name;
    {
        size_t pos = 0;
        while ((pos = sanitized_name.find("::", pos)) != std::string::npos) {
            sanitized_name.replace(pos, 2, "_");
            pos += 1;
        }
    }

    // If not found with original name, try sanitized name (handles Type::method -> Type_method)
    if (func_it == functions_.end()) {
        func_it = functions_.find(sanitized_name);
    }

    // For primitive try_from/from calls, we MUST NOT use a cached func_it entry
    // because these methods have multiple overloads (e.g., I16_try_from could be
    // TryFrom[I32] or TryFrom[I64]) and the cached entry may be the wrong overload.
    // Force the special try_from handling below to determine the correct suffix.
    {
        size_t sep_pos = fn_name.find("::");
        if (sep_pos != std::string::npos && func_it != functions_.end()) {
            std::string type_name = fn_name.substr(0, sep_pos);
            std::string method = fn_name.substr(sep_pos + 2);
            auto is_primitive = [](const std::string& name) {
                return name == "I8" || name == "I16" || name == "I32" || name == "I64" ||
                       name == "I128" || name == "U8" || name == "U16" || name == "U32" ||
                       name == "U64" || name == "U128" || name == "F32" || name == "F64" ||
                       name == "Bool";
            };
            if ((method == "try_from" || method == "from") && is_primitive(type_name)) {
                func_it = functions_.end();
            }
        }
    }

    // If function not found in functions_ map but we have an @extern func_sig,
    // emit a late 'declare' and register it. This handles cases where module
    // re-parsing fails (e.g., due to 'use super::') but the function signature
    // is available from the module registry.
    if (func_it == functions_.end() && func_sig.has_value() && func_sig->is_extern() &&
        func_sig->return_type) {
        std::string symbol_name = func_sig->extern_name.value_or(func_sig->name);
        std::string ext_ret_type = llvm_type_from_semantic(func_sig->return_type);

        // For C ABI compatibility: C functions returning bool use i32, not i1
        bool promoted_bool = false;
        if (ext_ret_type == "i1") {
            ext_ret_type = "i32";
            promoted_bool = true;
        }

        // Build parameter types
        std::string param_types;
        std::vector<std::string> param_types_vec;
        for (size_t i = 0; i < func_sig->params.size(); ++i) {
            std::string pt =
                func_sig->params[i] ? llvm_type_from_semantic(func_sig->params[i]) : "i32";
            if (i > 0)
                param_types += ", ";
            param_types += pt;
            param_types_vec.push_back(pt);
        }

        // Emit declare if not already declared
        if (declared_externals_.find(symbol_name) == declared_externals_.end()) {
            declared_externals_.insert(symbol_name);
            emit_line("");
            emit_line("; @extern (late-emitted) " + func_sig->name);
            emit_line("declare " + ext_ret_type + " @" + symbol_name + "(" + param_types + ")");
        }

        // Register in functions_ map so future calls find it immediately
        std::string func_type = ext_ret_type + " (" + param_types + ")";
        functions_[fn_name] =
            FuncInfo{"@" + symbol_name, func_type,    ext_ret_type, param_types_vec, true,
                     func_sig->name,    promoted_bool};

        // Re-find in the map so the normal path picks it up
        func_it = functions_.find(fn_name);
    }

    std::string mangled;
    if (func_it != functions_.end()) {
        // Use the registered LLVM name (handles @extern functions correctly)
        mangled = func_it->second.llvm_name;
        TML_DEBUG_LN("[CALL] Found func_it for fn_name=" << fn_name << " -> llvm_name=" << mangled
                                                         << " ret=" << func_it->second.ret_type);
    } else {
        TML_DEBUG_LN("[CALL] NOT found func_it for fn_name="
                     << fn_name << " sanitized=" << sanitized_name
                     << " module_prefix=" << current_module_prefix_);

        // In suite mode, add suite prefix for test-local functions (forward references)
        // This handles mutual recursion where called function isn't yet in functions_ map
        // BUT: Don't add suite prefix for library functions (they don't have prefixes)
        bool is_library_function = false;
        if (env_.module_registry()) {
            const auto& all_modules = env_.module_registry()->get_all_modules();
            // Also extract the bare function name (last segment after ::) for module lookup.
            // Module registry stores functions by bare name (e.g., "arch" not "os::arch").
            std::string bare_fn_name;
            {
                size_t last_sep = fn_name.rfind("::");
                if (last_sep != std::string::npos) {
                    bare_fn_name = fn_name.substr(last_sep + 2);
                }
            }
            for (const auto& [mod_name, mod] : all_modules) {
                if (mod.functions.find(fn_name) != mod.functions.end() ||
                    mod.functions.find(sanitized_name) != mod.functions.end() ||
                    (!bare_fn_name.empty() &&
                     mod.functions.find(bare_fn_name) != mod.functions.end())) {
                    is_library_function = true;

                    // Queue instantiation for non-generic library static methods (e.g., Text::from)
                    // Only for Type::method calls (sanitized_name has no ::)
                    // Generic types are handled separately via expected_enum_type_ context
                    size_t sep_pos = fn_name.find("::");
                    if (sep_pos != std::string::npos) {
                        std::string type_name = fn_name.substr(0, sep_pos);
                        std::string method_name = fn_name.substr(sep_pos + 2);

                        // Check if this type exists in the module
                        bool is_type =
                            mod.structs.count(type_name) > 0 || mod.enums.count(type_name) > 0;

                        // Skip generic types - they need type substitutions from context
                        // and are handled via expected_enum_type_ in method.cpp
                        auto struct_it = mod.structs.find(type_name);
                        bool is_generic_struct = struct_it != mod.structs.end() &&
                                                 !struct_it->second.type_params.empty();
                        auto enum_it = mod.enums.find(type_name);
                        bool is_generic_enum =
                            enum_it != mod.enums.end() && !enum_it->second.type_params.empty();

                        if (is_type && !is_generic_struct && !is_generic_enum) {
                            std::string mangled_method = "tml_" + type_name + "_" + method_name;
                            if (generated_impl_methods_.find(mangled_method) ==
                                generated_impl_methods_.end()) {
                                pending_impl_method_instantiations_.push_back(
                                    PendingImplMethod{type_name,
                                                      method_name,
                                                      {},
                                                      type_name,
                                                      "",
                                                      /*is_library_type=*/true});
                                generated_impl_methods_.insert(mangled_method);
                            }
                        }
                    }
                    break;
                }
            }
        }
        // NOTE: Do NOT treat all functions with func_sig as library functions!
        // Local functions in test files also have signatures from the type checker.
        // Only functions found in the module registry are true library functions.
        std::string prefix = is_library_function ? "" : get_suite_prefix();

        // For primitive type try_from/from calls, we need behavior suffix
        // e.g., I32::try_from(value: I64) should call @tml_I32_try_from_I64
        std::string behavior_suffix = "";
        size_t sep_pos = fn_name.find("::");
        if (sep_pos != std::string::npos) {
            std::string type_name = fn_name.substr(0, sep_pos);
            std::string method = fn_name.substr(sep_pos + 2);

            auto is_primitive = [](const std::string& name) {
                return name == "I8" || name == "I16" || name == "I32" || name == "I64" ||
                       name == "I128" || name == "U8" || name == "U16" || name == "U32" ||
                       name == "U64" || name == "U128" || name == "F32" || name == "F64" ||
                       name == "Bool";
            };

            if ((method == "try_from" || method == "from") && is_primitive(type_name) &&
                !call.args.empty()) {
                // Generate first argument to determine its type
                std::string arg_val = gen_expr(*call.args[0]);
                std::string arg_llvm_type = last_expr_type_;

                // Convert LLVM type to TML type name for suffix
                std::string arg_tml_type;
                if (arg_llvm_type == "i8")
                    arg_tml_type = last_expr_is_unsigned_ ? "U8" : "I8";
                else if (arg_llvm_type == "i16")
                    arg_tml_type = last_expr_is_unsigned_ ? "U16" : "I16";
                else if (arg_llvm_type == "i32")
                    arg_tml_type = last_expr_is_unsigned_ ? "U32" : "I32";
                else if (arg_llvm_type == "i64")
                    arg_tml_type = last_expr_is_unsigned_ ? "U64" : "I64";
                else if (arg_llvm_type == "i128")
                    arg_tml_type = last_expr_is_unsigned_ ? "U128" : "I128";
                else if (arg_llvm_type == "float")
                    arg_tml_type = "F32";
                else if (arg_llvm_type == "double")
                    arg_tml_type = "F64";
                else if (arg_llvm_type == "i1")
                    arg_tml_type = "Bool";

                if (!arg_tml_type.empty()) {
                    // Use double underscore to match impl.cpp's convention for method_type_suffix
                    behavior_suffix = "__" + arg_tml_type;
                }

                // Build mangled name with suffix (uses __ separator)
                // e.g., I32_try_from__I64 for I32::try_from(I64 value)
                mangled = "@tml_" + prefix + sanitized_name + behavior_suffix;

                // Queue method instantiation with behavior suffix
                // The mangled_method key must match what impl.cpp generates
                // NOTE: Do NOT insert into generated_impl_methods_ here!
                // That set is for tracking ACTUALLY generated methods, not queued ones.
                // The queue processing in generic.cpp handles deduplication via
                // processed_impl_methods.
                std::string mangled_method = "tml_" + type_name + "_" + method + behavior_suffix;
                if (generated_impl_methods_.find(mangled_method) == generated_impl_methods_.end()) {
                    TML_DEBUG_LN("[IMPL_INST] Queueing " << type_name << "::" << method
                                                         << " suffix=" << arg_tml_type
                                                         << " mangled=" << mangled_method);
                    pending_impl_method_instantiations_.push_back(PendingImplMethod{
                        type_name,
                        method,
                        {},           // No type_subs for primitive impls
                        type_name,    // base_type_name
                        arg_tml_type, // Use as method_type_suffix (behavior param)
                        /*is_library_type=*/true});
                    // Don't insert into generated_impl_methods_ - that's done after actual
                    // generation
                }

                // Determine return type
                std::string ret_type = "i32"; // Default
                if (func_it != functions_.end()) {
                    ret_type = func_it->second.ret_type;
                } else if (func_sig.has_value()) {
                    ret_type = llvm_type_from_semantic(func_sig->return_type);
                }

                // Generate call with actual argument type (NO coercion)
                std::string result = fresh_reg();
                emit_line("  " + result + " = call " + ret_type + " " + mangled + "(" +
                          arg_llvm_type + " " + arg_val + ")");
                last_expr_type_ = ret_type;
                return result;
            }
        }

        mangled = "@tml_" + prefix + sanitized_name;
    }

    // For generic functions, build type substitution map from actual arguments.
    // This handles cases like mem::forget[T](value: T) where T needs to be
    // resolved to the concrete type (e.g., I32) from the actual argument.
    std::unordered_map<std::string, types::TypePtr> free_func_type_subs;
    if (func_sig.has_value() && !func_sig->type_params.empty()) {
        // Infer type params from arguments by looking at local variable types
        for (size_t i = 0; i < call.args.size() && i < func_sig->params.size(); ++i) {
            auto param_type = func_sig->params[i];
            if (!param_type)
                continue;
            // Check if this parameter uses a generic type directly
            if (param_type->is<types::GenericType>()) {
                const auto& generic = param_type->as<types::GenericType>();
                if (free_func_type_subs.count(generic.name))
                    continue;
                // Try to get semantic type from local variable
                if (call.args[i]->is<parser::IdentExpr>()) {
                    const auto& ident = call.args[i]->as<parser::IdentExpr>();
                    auto loc_it = locals_.find(ident.name);
                    if (loc_it != locals_.end() && loc_it->second.semantic_type) {
                        free_func_type_subs[generic.name] = loc_it->second.semantic_type;
                    }
                }
            }
        }
    }

    // Fallback: if free_func_type_subs is still empty but we have current_type_subs_,
    // use those. This handles calls inside monomorphized generic methods where the
    // called function's parameters use type params (T) from the enclosing impl block.
    // E.g., inside LockFreeStack[I32]::push(), calling StackNode::new(value: T)
    // needs T resolved to I32 from current_type_subs_.
    if (free_func_type_subs.empty() && !current_type_subs_.empty() && func_sig.has_value()) {
        // Check if any parameter uses a type that exists in current_type_subs_
        for (size_t i = 0; i < func_sig->params.size(); ++i) {
            auto param_type = func_sig->params[i];
            if (!param_type)
                continue;
            if (param_type->is<types::GenericType>()) {
                const auto& generic = param_type->as<types::GenericType>();
                auto it = current_type_subs_.find(generic.name);
                if (it != current_type_subs_.end()) {
                    free_func_type_subs[generic.name] = it->second;
                }
            }
        }
    }

    // Determine return type
    std::string ret_type = "i32"; // Default
    if (func_it != functions_.end()) {
        // Use return type from registered function (handles @extern correctly)
        ret_type = func_it->second.ret_type;
    } else if (func_sig.has_value()) {
        auto resolved_ret = func_sig->return_type;
        if (!free_func_type_subs.empty()) {
            resolved_ret = types::substitute_type(resolved_ret, free_func_type_subs);
        }
        ret_type = llvm_type_from_semantic(resolved_ret);
    }

    // Generate arguments with proper type conversion
    std::vector<std::pair<std::string, std::string>> arg_vals; // (value, type)
    for (size_t i = 0; i < call.args.size(); ++i) {
        // Check if parameter takes ownership (not a reference)
        bool param_takes_ownership = true;
        bool param_is_ref = false;
        if (func_sig.has_value() && i < func_sig->params.size()) {
            auto resolved_param = func_sig->params[i];
            if (!free_func_type_subs.empty()) {
                resolved_param = types::substitute_type(resolved_param, free_func_type_subs);
            }
            if (resolved_param->is<types::RefType>()) {
                param_takes_ownership = false;
                param_is_ref = true;
            }
            // Str is Copy (pointer copy, not move) — caller retains drop responsibility
            if (resolved_param->is<types::PrimitiveType>() &&
                resolved_param->as<types::PrimitiveType>().kind == types::PrimitiveKind::Str) {
                param_takes_ownership = false;
            }
        }

        std::string val;
        std::string actual_type;

        // For ref parameters with IdentExpr arguments, pass the address directly
        // instead of loading the value
        if (param_is_ref && call.args[i]->is<parser::IdentExpr>()) {
            const auto& ident = call.args[i]->as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                // Check if the local variable is already a ref (i.e., came from a ref parameter)
                // In that case, the alloca holds a POINTER to the data, not the data itself.
                // We need to LOAD the pointer value, not pass the alloca address.
                bool local_is_ref = false;
                if (it->second.semantic_type) {
                    local_is_ref = it->second.semantic_type->is<types::RefType>();
                }

                if (local_is_ref) {
                    // Local is already a ref - load the pointer value from the alloca
                    std::string loaded_ptr = fresh_reg();
                    emit_line("  " + loaded_ptr + " = load ptr, ptr " + it->second.reg);
                    val = loaded_ptr;

                    // For lowlevel/extern C calls: when a ref [T] (slice reference)
                    // is passed to a C function, extract the raw data pointer from the
                    // fat pointer { ptr, i64 }. C functions expect uint8_t*, not a
                    // pointer to the fat pointer struct.
                    bool callee_is_extern =
                        (func_it != functions_.end() && func_it->second.is_extern) ||
                        (func_sig.has_value() && (func_sig->is_extern() || func_sig->is_lowlevel));
                    if (callee_is_extern && it->second.semantic_type &&
                        it->second.semantic_type->is<types::RefType>()) {
                        const auto& inner_ref = it->second.semantic_type->as<types::RefType>();
                        if (inner_ref.inner && inner_ref.inner->is<types::SliceType>()) {
                            // loaded_ptr points to { ptr, i64 } — extract field 0 (data ptr)
                            std::string data_ptr = fresh_reg();
                            emit_line("  " + data_ptr + " = load ptr, ptr " + val);
                            val = data_ptr;
                        }
                    }
                } else {
                    // Check for array-to-slice coercion: when a fixed-size array [T; N]
                    // is passed to a parameter expecting ref [T] (slice), we need to
                    // create a fat pointer { ptr, i64 } containing the array pointer and length.
                    bool needs_slice_coercion = false;
                    size_t array_size = 0;
                    if (func_sig.has_value() && i < func_sig->params.size()) {
                        auto resolved_param = func_sig->params[i];
                        if (!free_func_type_subs.empty()) {
                            resolved_param =
                                types::substitute_type(resolved_param, free_func_type_subs);
                        }
                        if (resolved_param->is<types::RefType>()) {
                            const auto& ref_type = resolved_param->as<types::RefType>();
                            if (ref_type.inner && ref_type.inner->is<types::SliceType>()) {
                                // Parameter expects ref [T] (a slice)
                                // Check if the local is an array type
                                if (it->second.semantic_type &&
                                    it->second.semantic_type->is<types::ArrayType>()) {
                                    needs_slice_coercion = true;
                                    array_size =
                                        it->second.semantic_type->as<types::ArrayType>().size;
                                }
                            }
                        }
                    }

                    if (needs_slice_coercion) {
                        // Create a fat pointer { ptr, i64 } on the stack
                        // Field 0: pointer to the array data
                        // Field 1: array length (i64)
                        std::string fat_ptr_alloca = fresh_reg();
                        emit_line("  " + fat_ptr_alloca + " = alloca { ptr, i64 }");
                        std::string data_field = fresh_reg();
                        emit_line("  " + data_field +
                                  " = getelementptr inbounds { ptr, i64 }, ptr " + fat_ptr_alloca +
                                  ", i32 0, i32 0");
                        emit_line("  store ptr " + it->second.reg + ", ptr " + data_field);
                        std::string len_field = fresh_reg();
                        emit_line("  " + len_field +
                                  " = getelementptr inbounds { ptr, i64 }, ptr " + fat_ptr_alloca +
                                  ", i32 0, i32 1");
                        emit_line("  store i64 " + std::to_string(array_size) + ", ptr " +
                                  len_field);
                        val = fat_ptr_alloca;
                    } else {
                        // Local is a value - pass the alloca address directly (pointer to the
                        // value)
                        val = it->second.reg;
                    }
                }
                actual_type = "ptr";
            } else {
                // Fallback to normal gen_expr
                val = gen_expr(*call.args[i]);
                actual_type = last_expr_type_;
            }
        } else {
            val = gen_expr(*call.args[i]);
            actual_type = last_expr_type_;

            // If param is ref but arg is not an IdentExpr (e.g., temporary expression),
            // we need to store it in a temp alloca and pass the address
            if (param_is_ref && actual_type.starts_with("%struct.")) {
                std::string temp_alloca = fresh_reg();
                emit_line("  " + temp_alloca + " = alloca " + actual_type);
                emit_line("  store " + actual_type + " " + val + ", ptr " + temp_alloca);
                val = temp_alloca;
                actual_type = "ptr";
            }
        }

        std::string expected_type = actual_type; // Default to actual type from expression

        // If we have function signature from TypeEnv, use parameter type
        // (with generic type substitution applied)
        if (func_sig.has_value() && i < func_sig->params.size()) {
            auto resolved_param = func_sig->params[i];
            if (!free_func_type_subs.empty()) {
                resolved_param = types::substitute_type(resolved_param, free_func_type_subs);
            }
            expected_type = llvm_type_from_semantic(resolved_param);
            // Function-typed parameters use fat pointer { ptr, ptr }
            if (resolved_param && resolved_param->is<types::FuncType>()) {
                expected_type = "{ ptr, ptr }";
            }
        }
        // Otherwise, try to get parameter type from registered functions (codegen's FuncInfo)
        else if (func_it != functions_.end() && i < func_it->second.param_types.size()) {
            expected_type = func_it->second.param_types[i];
        }

        // Insert type conversion if needed
        if (actual_type != expected_type) {
            // i32 -> i64 conversion
            if (actual_type == "i32" && expected_type == "i64") {
                std::string converted = fresh_reg();
                emit_line("  " + converted + " = sext i32 " + val + " to i64");
                val = converted;
            }
            // i64 -> i32 conversion (truncate)
            else if (actual_type == "i64" && expected_type == "i32") {
                std::string converted = fresh_reg();
                emit_line("  " + converted + " = trunc i64 " + val + " to i32");
                val = converted;
            }
            // i1 -> i32 conversion (zero extend)
            else if (actual_type == "i1" && expected_type == "i32") {
                std::string converted = fresh_reg();
                emit_line("  " + converted + " = zext i1 " + val + " to i32");
                val = converted;
            }
            // i32 -> i1 conversion (compare ne 0)
            else if (actual_type == "i32" && expected_type == "i1") {
                std::string converted = fresh_reg();
                emit_line("  " + converted + " = icmp ne i32 " + val + ", 0");
                val = converted;
            }
            // %struct.* -> ptr conversion (extract inner pointer from wrapper struct)
            // Handles cases like List[T] -> TmlList* in lowlevel calls
            else if (actual_type.starts_with("%struct.") && expected_type == "ptr") {
                std::string converted = fresh_reg();
                emit_line("  " + converted + " = extractvalue " + actual_type + " " + val + ", 0");
                val = converted;
            }
            // { ptr, ptr } -> ptr conversion: extract fn_ptr from fat pointer closure
            // This happens when a closure is passed to a func(...) parameter
            else if (actual_type == "{ ptr, ptr }" && expected_type == "ptr") {
                std::string converted = fresh_reg();
                emit_line("  " + converted + " = extractvalue { ptr, ptr } " + val + ", 0");
                val = converted;
            }
            // ptr -> { ptr, ptr } conversion: wrap bare function pointer in fat pointer
            // This happens when a named function is passed to a func() parameter
            else if (actual_type == "ptr" && expected_type == "{ ptr, ptr }") {
                std::string fat1 = fresh_reg();
                std::string fat2 = fresh_reg();
                emit_line("  " + fat1 + " = insertvalue { ptr, ptr } undef, ptr " + val + ", 0");
                emit_line("  " + fat2 + " = insertvalue { ptr, ptr } " + fat1 + ", ptr null, 1");
                val = fat2;
            }
        }

        // Array-to-slice coercion: when parameter expects ref [T] (slice) but argument
        // is a ref to a fixed-size array [T; N], create a fat pointer { ptr, i64 }
        // containing the array data pointer and the array length.
        // This handles cases like: Type::method(ref array) where the argument is a
        // RefExpr that gen_expr produces as a thin pointer to the array data.
        if (actual_type == "ptr" && expected_type == "ptr" && func_sig.has_value() &&
            i < func_sig->params.size()) {
            auto resolved_param = func_sig->params[i];
            if (!free_func_type_subs.empty()) {
                resolved_param = types::substitute_type(resolved_param, free_func_type_subs);
            }
            if (resolved_param && resolved_param->is<types::RefType>()) {
                const auto& ref_type = resolved_param->as<types::RefType>();
                if (ref_type.inner && ref_type.inner->is<types::SliceType>()) {
                    auto arg_semantic = infer_expr_type(*call.args[i]);
                    size_t array_size = 0;
                    if (arg_semantic && arg_semantic->is<types::ArrayType>()) {
                        array_size = arg_semantic->as<types::ArrayType>().size;
                    } else if (arg_semantic && arg_semantic->is<types::RefType>()) {
                        const auto& arg_ref = arg_semantic->as<types::RefType>();
                        if (arg_ref.inner && arg_ref.inner->is<types::ArrayType>()) {
                            array_size = arg_ref.inner->as<types::ArrayType>().size;
                        }
                    }
                    if (array_size > 0) {
                        std::string fat_alloca = fresh_reg();
                        emit_line("  " + fat_alloca + " = alloca { ptr, i64 }");
                        std::string data_field = fresh_reg();
                        emit_line("  " + data_field +
                                  " = getelementptr inbounds { ptr, i64 }, ptr " + fat_alloca +
                                  ", i32 0, i32 0");
                        emit_line("  store ptr " + val + ", ptr " + data_field);
                        std::string len_field = fresh_reg();
                        emit_line("  " + len_field +
                                  " = getelementptr inbounds { ptr, i64 }, ptr " + fat_alloca +
                                  ", i32 0, i32 1");
                        emit_line("  store i64 " + std::to_string(array_size) + ", ptr " +
                                  len_field);
                        val = fat_alloca;
                    }
                }
            }
        }

        arg_vals.push_back({val, expected_type});

        // Mark variable/field as consumed if passed by value (ownership transfer)
        if (param_takes_ownership && call.args[i]->is<parser::IdentExpr>()) {
            const auto& ident = call.args[i]->as<parser::IdentExpr>();
            mark_var_consumed(ident.name);
        }
        // Handle partial moves: mark struct field as consumed when passed by value
        else if (param_takes_ownership && call.args[i]->is<parser::FieldExpr>()) {
            const auto& field = call.args[i]->as<parser::FieldExpr>();
            if (field.object->is<parser::IdentExpr>()) {
                const auto& base = field.object->as<parser::IdentExpr>();
                mark_field_consumed(base.name, field.field);
            }
        }
    }

    // Coverage tracking for @extern FFI function calls
    // This tracks which FFI functions are actually called by tests
    if (func_it != functions_.end() && func_it->second.is_extern) {
        emit_coverage(func_it->second.tml_name);
    }

    // Check if this extern function had Bool return promoted to i32 for C ABI
    bool needs_bool_trunc = func_it != functions_.end() && func_it->second.bool_ret_promoted;

    // Call - handle void vs non-void return types
    std::string dbg_suffix = get_debug_loc_suffix();
    if (ret_type == "void") {
        emit("  call void " + mangled + "(");
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0)
                emit(", ");
            emit(arg_vals[i].second + " " + arg_vals[i].first);
        }
        emit_line(")" + dbg_suffix);
        last_expr_type_ = "void";
        return "0";
    } else {
        std::string result = fresh_reg();
        emit("  " + result + " = call " + ret_type + " " + mangled + "(");
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0)
                emit(", ");
            emit(arg_vals[i].second + " " + arg_vals[i].first);
        }
        emit_line(")" + dbg_suffix);

        // For extern functions where Bool was promoted to i32 for C ABI,
        // truncate the i32 result back to i1 for TML code
        if (needs_bool_trunc) {
            std::string truncated = fresh_reg();
            emit_line("  " + truncated + " = trunc i32 " + result + " to i1");
            last_expr_type_ = "i1";
            return truncated;
        }

        last_expr_type_ = ret_type;
        return result;
    }
}

} // namespace tml::codegen
