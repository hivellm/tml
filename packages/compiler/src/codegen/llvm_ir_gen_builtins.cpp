// LLVM IR generator - Builtin function call dispatch
// Main entry point for gen_call that delegates to specialized builtin handlers
// Handles: enum constructors, indirect calls, generic functions, user-defined functions

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_call(const parser::CallExpr& call) -> std::string {
    // Get function name
    std::string fn_name;
    if (call.callee->is<parser::IdentExpr>()) {
        fn_name = call.callee->as<parser::IdentExpr>().name;
    } else if (call.callee->is<parser::PathExpr>()) {
        // Handle path expressions like Instant::now, Duration::as_millis_f64
        const auto& path = call.callee->as<parser::PathExpr>().path;
        // Join segments with ::
        for (size_t i = 0; i < path.segments.size(); ++i) {
            if (i > 0)
                fn_name += "::";
            fn_name += path.segments[i];
        }
    } else {
        report_error("Complex callee not supported", call.span);
        return "0";
    }

    // ============ PRIMITIVE TYPE STATIC METHODS ============
    // Handle Type::default() calls for primitive types
    if (call.callee->is<parser::PathExpr>()) {
        const auto& path = call.callee->as<parser::PathExpr>().path;
        if (path.segments.size() == 2) {
            const std::string& type_name = path.segments[0];
            const std::string& method = path.segments[1];

            bool is_primitive_type =
                type_name == "I8" || type_name == "I16" || type_name == "I32" ||
                type_name == "I64" || type_name == "I128" || type_name == "U8" ||
                type_name == "U16" || type_name == "U32" || type_name == "U64" ||
                type_name == "U128" || type_name == "F32" || type_name == "F64" ||
                type_name == "Bool" || type_name == "Str";

            if (is_primitive_type && method == "default") {
                // Integer types: default is 0
                if (type_name == "I8" || type_name == "I16" || type_name == "I32" ||
                    type_name == "I64" || type_name == "I128" || type_name == "U8" ||
                    type_name == "U16" || type_name == "U32" || type_name == "U64" ||
                    type_name == "U128") {
                    std::string llvm_ty;
                    if (type_name == "I8" || type_name == "U8")
                        llvm_ty = "i8";
                    else if (type_name == "I16" || type_name == "U16")
                        llvm_ty = "i16";
                    else if (type_name == "I32" || type_name == "U32")
                        llvm_ty = "i32";
                    else if (type_name == "I64" || type_name == "U64")
                        llvm_ty = "i64";
                    else
                        llvm_ty = "i128";
                    last_expr_type_ = llvm_ty;
                    return "0";
                }
                // Float types: default is 0.0
                if (type_name == "F32") {
                    last_expr_type_ = "float";
                    return "0.0";
                }
                if (type_name == "F64") {
                    last_expr_type_ = "double";
                    return "0.0";
                }
                // Bool: default is false
                if (type_name == "Bool") {
                    last_expr_type_ = "i1";
                    return "false";
                }
                // Str: default is empty string
                if (type_name == "Str") {
                    std::string empty_str = add_string_literal("");
                    last_expr_type_ = "ptr";
                    return empty_str;
                }
            }

            // Handle Type::from(value) calls for type conversion
            if (is_primitive_type && method == "from" && !call.args.empty()) {
                // Generate the source value
                std::string src_val = gen_expr(*call.args[0]);
                std::string src_type = last_expr_type_;

                // Determine target LLVM type
                std::string target_ty;
                bool target_is_float = false;
                bool target_is_signed = true;
                if (type_name == "I8")
                    target_ty = "i8";
                else if (type_name == "I16")
                    target_ty = "i16";
                else if (type_name == "I32")
                    target_ty = "i32";
                else if (type_name == "I64")
                    target_ty = "i64";
                else if (type_name == "I128")
                    target_ty = "i128";
                else if (type_name == "U8") {
                    target_ty = "i8";
                    target_is_signed = false;
                } else if (type_name == "U16") {
                    target_ty = "i16";
                    target_is_signed = false;
                } else if (type_name == "U32") {
                    target_ty = "i32";
                    target_is_signed = false;
                } else if (type_name == "U64") {
                    target_ty = "i64";
                    target_is_signed = false;
                } else if (type_name == "U128") {
                    target_ty = "i128";
                    target_is_signed = false;
                } else if (type_name == "F32") {
                    target_ty = "float";
                    target_is_float = true;
                } else if (type_name == "F64") {
                    target_ty = "double";
                    target_is_float = true;
                }

                // Determine source type properties
                bool src_is_float = (src_type == "float" || src_type == "double");

                // If types are the same, just return the value
                if (src_type == target_ty) {
                    last_expr_type_ = target_ty;
                    return src_val;
                }

                std::string result = fresh_reg();

                // Get bit widths for integer types
                auto get_bit_width = [](const std::string& ty) -> int {
                    if (ty == "i8")
                        return 8;
                    if (ty == "i16")
                        return 16;
                    if (ty == "i32")
                        return 32;
                    if (ty == "i64")
                        return 64;
                    if (ty == "i128")
                        return 128;
                    if (ty == "float")
                        return 32;
                    if (ty == "double")
                        return 64;
                    return 0;
                };

                int src_width = get_bit_width(src_type);
                int target_width = get_bit_width(target_ty);

                if (src_is_float && target_is_float) {
                    // Float to float conversion
                    if (src_width < target_width) {
                        emit_line("  " + result + " = fpext " + src_type + " " + src_val + " to " +
                                  target_ty);
                    } else {
                        emit_line("  " + result + " = fptrunc " + src_type + " " + src_val +
                                  " to " + target_ty);
                    }
                } else if (src_is_float && !target_is_float) {
                    // Float to int conversion
                    if (target_is_signed) {
                        emit_line("  " + result + " = fptosi " + src_type + " " + src_val + " to " +
                                  target_ty);
                    } else {
                        emit_line("  " + result + " = fptoui " + src_type + " " + src_val + " to " +
                                  target_ty);
                    }
                } else if (!src_is_float && target_is_float) {
                    // Int to float conversion
                    // Assume source is signed for now (most common case for From trait)
                    emit_line("  " + result + " = sitofp " + src_type + " " + src_val + " to " +
                              target_ty);
                } else {
                    // Int to int conversion
                    if (src_width < target_width) {
                        // Extension - use sext for signed, zext for unsigned
                        // For From trait, we typically extend signed types
                        emit_line("  " + result + " = sext " + src_type + " " + src_val + " to " +
                                  target_ty);
                    } else if (src_width > target_width) {
                        // Truncation
                        emit_line("  " + result + " = trunc " + src_type + " " + src_val + " to " +
                                  target_ty);
                    } else {
                        // Same width, just a bitcast (e.g., I32 to U32)
                        last_expr_type_ = target_ty;
                        return src_val;
                    }
                }

                last_expr_type_ = target_ty;
                return result;
            }
        }
    }

    // ============ BUILTIN HANDLERS ============
    // Try each category of builtins. If any handler returns a value, use it.

    if (auto result = try_gen_builtin_io(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_mem(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_atomic(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_sync(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_time(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_math(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_collections(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_string(fn_name, call)) {
        return *result;
    }

    // ============ ENUM CONSTRUCTORS ============

    // Check if this is an enum constructor
    if (call.callee->is<parser::IdentExpr>()) {
        const auto& ident = call.callee->as<parser::IdentExpr>();

        // First check pending generic enums
        for (const auto& [gen_enum_name, gen_enum_decl] : pending_generic_enums_) {
            for (size_t variant_idx = 0; variant_idx < gen_enum_decl->variants.size();
                 ++variant_idx) {
                const auto& variant = gen_enum_decl->variants[variant_idx];
                if (variant.name == ident.name) {
                    // Found generic enum constructor
                    std::string enum_type;

                    // Check if variant has payload (tuple_fields for tuple variants like Just(T))
                    bool has_payload =
                        variant.tuple_fields.has_value() && !variant.tuple_fields->empty();

                    // If we have expected type from context, use it (for multi-param generics)
                    if (!expected_enum_type_.empty()) {
                        enum_type = expected_enum_type_;
                    } else if (!current_ret_type_.empty() &&
                               current_ret_type_.find("%struct." + gen_enum_name + "__") == 0) {
                        // Function returns this generic enum type - use the return type directly
                        // This handles multi-param generics like Outcome[T, E] where we can only
                        // infer T from Ok(value) but need E from context
                        enum_type = current_ret_type_;
                    } else {
                        // Infer type from arguments
                        std::vector<types::TypePtr> inferred_type_args;
                        if (has_payload && !call.args.empty()) {
                            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
                            inferred_type_args.push_back(arg_type);
                        } else {
                            // No payload to infer from - default to I32
                            inferred_type_args.push_back(types::make_i32());
                        }
                        std::string mangled_name =
                            require_enum_instantiation(gen_enum_name, inferred_type_args);
                        enum_type = "%struct." + mangled_name;
                    }

                    std::string result = fresh_reg();
                    std::string enum_val = fresh_reg();

                    // Create enum value on stack
                    emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                    // Set tag (field 0)
                    std::string tag_ptr = fresh_reg();
                    emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " +
                              enum_val + ", i32 0, i32 0");
                    emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " + tag_ptr);

                    // Set payload if present (stored in field 1, the [N x i8] array)
                    if (has_payload && !call.args.empty()) {
                        std::string payload = gen_expr(*call.args[0]);

                        // Get pointer to payload field ([N x i8])
                        std::string payload_ptr = fresh_reg();
                        emit_line("  " + payload_ptr + " = getelementptr inbounds " + enum_type +
                                  ", ptr " + enum_val + ", i32 0, i32 1");

                        // Cast payload to bytes and store
                        std::string payload_typed_ptr = fresh_reg();
                        emit_line("  " + payload_typed_ptr + " = bitcast ptr " + payload_ptr +
                                  " to ptr");
                        emit_line("  store " + last_expr_type_ + " " + payload + ", ptr " +
                                  payload_typed_ptr);
                    }

                    // Load the complete enum value
                    emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                    last_expr_type_ = enum_type;
                    return result;
                }
            }
        }

        // Then check non-generic enums
        for (const auto& [enum_name, enum_def] : env_.all_enums()) {
            for (size_t variant_idx = 0; variant_idx < enum_def.variants.size(); ++variant_idx) {
                const auto& [variant_name, payload_types] = enum_def.variants[variant_idx];

                if (variant_name == ident.name) {
                    // Found enum constructor
                    std::string enum_type = "%struct." + enum_name;
                    std::string result = fresh_reg();
                    std::string enum_val = fresh_reg();

                    // Create enum value on stack
                    emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                    // Set tag (field 0)
                    std::string tag_ptr = fresh_reg();
                    emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " +
                              enum_val + ", i32 0, i32 0");
                    emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " + tag_ptr);

                    // Set payload if present (stored in field 1, the [N x i8] array)
                    if (!payload_types.empty() && !call.args.empty()) {
                        std::string payload = gen_expr(*call.args[0]);

                        // Get pointer to payload field ([N x i8])
                        std::string payload_ptr = fresh_reg();
                        emit_line("  " + payload_ptr + " = getelementptr inbounds " + enum_type +
                                  ", ptr " + enum_val + ", i32 0, i32 1");

                        // Cast payload to bytes and store
                        // For simplicity, bitcast the i8 array pointer to the payload type pointer
                        std::string payload_typed_ptr = fresh_reg();
                        emit_line("  " + payload_typed_ptr + " = bitcast ptr " + payload_ptr +
                                  " to ptr");
                        emit_line("  store " + last_expr_type_ + " " + payload + ", ptr " +
                                  payload_typed_ptr);
                    }

                    // Load the complete enum value
                    emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                    last_expr_type_ = enum_type;
                    return result;
                }
            }
        }
    }

    // ============ INDIRECT FUNCTION POINTER CALLS ============

    // Check if this is an indirect call through a function pointer variable
    auto local_it = locals_.find(fn_name);
    if (local_it != locals_.end() && local_it->second.type == "ptr") {
        // This is a function pointer variable - generate indirect call
        std::string fn_ptr;
        if (local_it->second.reg[0] == '@') {
            // Direct function reference (closure stored as @tml_closure_0)
            fn_ptr = local_it->second.reg;
        } else {
            // Load the function pointer from the alloca
            fn_ptr = fresh_reg();
            emit_line("  " + fn_ptr + " = load ptr, ptr " + local_it->second.reg);
        }

        // Generate arguments - first add captured variables if this is a closure with captures
        std::vector<std::pair<std::string, std::string>> arg_vals;

        // Prepend captured variables if present
        if (local_it->second.closure_captures.has_value()) {
            const auto& captures = local_it->second.closure_captures.value();
            for (size_t i = 0; i < captures.captured_names.size(); ++i) {
                const std::string& cap_name = captures.captured_names[i];
                const std::string& cap_type = captures.captured_types[i];

                // Look up the captured variable and load its value
                auto cap_it = locals_.find(cap_name);
                if (cap_it != locals_.end()) {
                    std::string cap_val = fresh_reg();
                    emit_line("  " + cap_val + " = load " + cap_type + ", ptr " +
                              cap_it->second.reg);
                    arg_vals.push_back({cap_val, cap_type});
                } else {
                    // Captured variable not found - this shouldn't happen but handle gracefully
                    arg_vals.push_back({"0", cap_type});
                }
            }
        }

        // Add regular call arguments
        for (size_t i = 0; i < call.args.size(); ++i) {
            std::string val = gen_expr(*call.args[i]);
            arg_vals.push_back({val, last_expr_type_});
        }

        // Determine return type from semantic type if available
        std::string ret_type = "i32"; // Default fallback
        if (local_it->second.semantic_type) {
            if (local_it->second.semantic_type->is<types::FuncType>()) {
                const auto& func_type = local_it->second.semantic_type->as<types::FuncType>();
                ret_type = llvm_type_from_semantic(func_type.return_type);
            } else if (local_it->second.semantic_type->is<types::ClosureType>()) {
                const auto& closure_type = local_it->second.semantic_type->as<types::ClosureType>();
                ret_type = llvm_type_from_semantic(closure_type.return_type);
            }
        }

        // Build function type signature for indirect call using argument types
        // Use the types of the arguments being passed, not the semantic type params
        std::string func_type_sig = ret_type + " (";
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0)
                func_type_sig += ", ";
            func_type_sig += arg_vals[i].second; // Use the type from arg_vals
        }
        func_type_sig += ")";

        std::string result = fresh_reg();
        emit("  " + result + " = call " + func_type_sig + " " + fn_ptr + "(");
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0)
                emit(", ");
            emit(arg_vals[i].second + " " + arg_vals[i].first);
        }
        emit_line(")");
        last_expr_type_ = ret_type;
        return result;
    }

    // ============ GENERIC FUNCTION CALLS ============

    // Check if this is a generic function call
    auto pending_func_it = pending_generic_funcs_.find(fn_name);
    if (pending_func_it != pending_generic_funcs_.end()) {
        const auto& gen_func = *pending_func_it->second;

        // Build set of generic parameter names for unification
        std::unordered_set<std::string> generic_names;
        for (const auto& g : gen_func.generics) {
            generic_names.insert(g.name);
        }

        // Infer type arguments using unification
        // For each argument, unify the parameter type pattern with the argument type
        std::unordered_map<std::string, types::TypePtr> bindings;
        for (size_t i = 0; i < call.args.size() && i < gen_func.params.size(); ++i) {
            types::TypePtr arg_type = infer_expr_type(*call.args[i]);

            unify_types(*gen_func.params[i].type, arg_type, generic_names, bindings);
        }

        // Extract inferred type args in the order of generic parameters
        std::vector<types::TypePtr> inferred_type_args;
        for (const auto& g : gen_func.generics) {
            auto it = bindings.find(g.name);
            if (it != bindings.end()) {
                inferred_type_args.push_back(it->second);
            } else {
                // Generic not inferred - use Unit as fallback
                inferred_type_args.push_back(types::make_unit());
            }
        }

        // Register and get mangled name
        std::string mangled_name = require_func_instantiation(fn_name, inferred_type_args);

        // Use bindings as substitution map for return type
        std::unordered_map<std::string, types::TypePtr>& subs = bindings;

        // Get substituted return type
        std::string ret_type = "void";
        if (gen_func.return_type.has_value()) {
            types::TypePtr subbed_ret = resolve_parser_type_with_subs(**gen_func.return_type, subs);
            ret_type = llvm_type_from_semantic(subbed_ret);
        }

        // Generate arguments with expected type context for generic enum constructors
        std::vector<std::pair<std::string, std::string>> arg_vals;
        for (size_t i = 0; i < call.args.size(); ++i) {
            // Set expected enum type for this argument based on parameter type with substitutions
            if (i < gen_func.params.size()) {
                types::TypePtr param_type =
                    resolve_parser_type_with_subs(*gen_func.params[i].type, subs);
                std::string llvm_param_type = llvm_type_from_semantic(param_type);
                // Set expected type context for generic enum constructors like Nothing
                if (llvm_param_type.find("%struct.") == 0 &&
                    llvm_param_type.find("__") != std::string::npos) {
                    expected_enum_type_ = llvm_param_type;
                }
            }
            std::string val = gen_expr(*call.args[i]);
            expected_enum_type_.clear(); // Clear after generating argument
            std::string arg_type = last_expr_type_;
            arg_vals.push_back({val, arg_type});
        }

        // Call the instantiated function
        std::string func_name = "@tml_" + mangled_name;
        if (ret_type == "void") {
            emit("  call void " + func_name + "(");
            for (size_t i = 0; i < arg_vals.size(); ++i) {
                if (i > 0)
                    emit(", ");
                emit(arg_vals[i].second + " " + arg_vals[i].first);
            }
            emit_line(")");
            last_expr_type_ = "void";
            return "0";
        } else {
            std::string result = fresh_reg();
            emit("  " + result + " = call " + ret_type + " " + func_name + "(");
            for (size_t i = 0; i < arg_vals.size(); ++i) {
                if (i > 0)
                    emit(", ");
                emit(arg_vals[i].second + " " + arg_vals[i].first);
            }
            emit_line(")");
            last_expr_type_ = ret_type;
            return result;
        }
    }

    // ============ USER-DEFINED FUNCTIONS ============

    // Try to look up the function signature first
    auto func_sig = env_.lookup_func(fn_name);

    // Check if this function was registered (includes @extern functions)
    auto func_it = functions_.find(fn_name);

    // If not found directly, check if it's a qualified FFI call (e.g., "SDL2::init")
    // In this case, the function is registered with just the bare name ("init")
    if (func_it == functions_.end() && func_sig.has_value() && func_sig->has_ffi_module()) {
        // Look up with just the function name (without module prefix)
        func_it = functions_.find(func_sig->name);
    }

    std::string mangled;
    if (func_it != functions_.end()) {
        // Use the registered LLVM name (handles @extern functions correctly)
        mangled = func_it->second.llvm_name;
    } else {
        // Default: user-defined TML function with tml_ prefix
        // Replace :: with _ for valid LLVM IR identifiers (matches impl method naming convention)
        std::string sanitized_name = fn_name;
        size_t pos = 0;
        while ((pos = sanitized_name.find("::", pos)) != std::string::npos) {
            sanitized_name.replace(pos, 2, "_");
            pos += 1;
        }
        mangled = "@tml_" + sanitized_name;
    }

    // Determine return type
    std::string ret_type = "i32"; // Default
    if (func_it != functions_.end()) {
        // Use return type from registered function (handles @extern correctly)
        ret_type = func_it->second.ret_type;
    } else if (func_sig.has_value()) {
        ret_type = llvm_type_from_semantic(func_sig->return_type);
    }

    // Generate arguments with proper type conversion
    std::vector<std::pair<std::string, std::string>> arg_vals; // (value, type)
    for (size_t i = 0; i < call.args.size(); ++i) {
        std::string val = gen_expr(*call.args[i]);
        std::string actual_type = last_expr_type_; // Type of the generated value
        std::string expected_type = "i32";         // Default

        // If we have function signature, use parameter type
        if (func_sig.has_value() && i < func_sig->params.size()) {
            expected_type = llvm_type_from_semantic(func_sig->params[i]);
        } else {
            // Fallback to inference
            // Check if it's a string constant
            if (val.starts_with("@.str.")) {
                expected_type = "ptr";
            } else if (call.args[i]->is<parser::LiteralExpr>()) {
                const auto& lit = call.args[i]->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                    expected_type = "ptr";
                } else if (lit.token.kind == lexer::TokenKind::BoolLiteral) {
                    expected_type = "i1";
                }
            }
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
        }

        arg_vals.push_back({val, expected_type});
    }

    // Call - handle void vs non-void return types
    if (ret_type == "void") {
        emit("  call void " + mangled + "(");
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0)
                emit(", ");
            emit(arg_vals[i].second + " " + arg_vals[i].first);
        }
        emit_line(")");
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
        emit_line(")");
        last_expr_type_ = ret_type;
        return result;
    }
}

} // namespace tml::codegen
