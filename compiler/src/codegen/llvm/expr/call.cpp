//! # LLVM IR Generator - Function Call Dispatcher
//!
//! This file implements the main function call dispatch logic.
//!
//! ## Call Resolution Order
//!
//! `gen_call()` resolves calls in this priority:
//!
//! 1. **Primitive static methods**: `I32::default()`, `Bool::default()`
//! 2. **Enum constructors**: `Maybe::Just(x)`, `Outcome::Ok(v)`
//! 3. **Builtin functions**: print, panic, assert, math, etc.
//! 4. **Generic functions**: Instantiate and call monomorphized version
//! 5. **User-defined functions**: Direct call to defined function
//! 6. **Indirect calls**: Call through function pointer
//!
//! ## Path Expressions
//!
//! Path expressions like `Type::method` or `Module::func` are resolved
//! by joining segments with `::` and looking up the mangled name.
//!
//! ## Generic Instantiation
//!
//! Generic calls trigger monomorphization - a specialized version of
//! the function is generated for the concrete type arguments.

#include "codegen/llvm/llvm_ir_gen.hpp"

#include <cctype>

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
    if (s.substr(0, 4) == "ptr_") {
        std::string inner_str = s.substr(4);
        auto inner = parse_mangled_type_string(inner_str);
        if (inner) {
            auto t = std::make_shared<types::Type>();
            t->kind = types::PtrType{.inner = inner};
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

auto LLVMIRGen::gen_call(const parser::CallExpr& call) -> std::string {
    // Clear expected literal type context - it should only apply within explicit type annotations
    // (like "let x: F64 = 5") and not leak into function call arguments
    expected_literal_type_.clear();
    expected_literal_is_unsigned_ = false;

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
    } else if (call.callee->is<parser::FieldExpr>()) {
        // Handle calling function pointers stored in struct fields: cb.action(21)
        // Function pointer fields are stored as fat pointers { fn_ptr, env_ptr }
        // to support both plain function pointers (env=null) and capturing closures
        std::string fat_ptr_val = gen_expr(*call.callee);
        std::string callee_type = last_expr_type_;

        // Infer the function type from the field
        types::TypePtr func_type = infer_expr_type(*call.callee);
        if (func_type && func_type->is<types::FuncType>()) {
            const auto& ft = func_type->as<types::FuncType>();

            // Extract function pointer and environment pointer from fat pointer
            std::string fn_ptr, env_ptr;
            if (callee_type == "{ ptr, ptr }") {
                fn_ptr = fresh_reg();
                emit_line("  " + fn_ptr + " = extractvalue { ptr, ptr } " + fat_ptr_val + ", 0");
                env_ptr = fresh_reg();
                emit_line("  " + env_ptr + " = extractvalue { ptr, ptr } " + fat_ptr_val + ", 1");
            } else {
                // Fallback: treat as thin pointer (backwards compat)
                fn_ptr = fat_ptr_val;
                env_ptr = "";
            }

            // Build user argument list
            std::vector<std::string> arg_vals;
            std::vector<std::string> arg_types;
            for (const auto& arg : call.args) {
                arg_vals.push_back(gen_expr(*arg));
                arg_types.push_back(last_expr_type_);
            }

            // Build call signature
            std::string ret_type =
                ft.return_type ? llvm_type_from_semantic(ft.return_type) : "void";

            if (!env_ptr.empty()) {
                // Fat pointer call: check if env is null to determine calling convention
                // Non-null env -> capturing closure: call fn(env, args...)
                // Null env -> plain function: call fn(args...)
                std::string is_null = fresh_reg();
                emit_line("  " + is_null + " = icmp eq ptr " + env_ptr + ", null");

                std::string label_thin = "fp_thin" + std::to_string(label_counter_);
                std::string label_fat = "fp_fat" + std::to_string(label_counter_);
                std::string label_merge = "fp_merge" + std::to_string(label_counter_);
                label_counter_++;

                emit_line("  br i1 " + is_null + ", label %" + label_thin + ", label %" +
                          label_fat);

                // Thin call (no env)
                emit_line(label_thin + ":");
                std::string args_str_thin;
                for (size_t i = 0; i < arg_vals.size(); ++i) {
                    if (i > 0)
                        args_str_thin += ", ";
                    args_str_thin += arg_types[i] + " " + arg_vals[i];
                }
                std::string thin_result;
                if (ret_type == "void") {
                    emit_line("  call void " + fn_ptr + "(" + args_str_thin + ")");
                } else {
                    thin_result = fresh_reg();
                    emit_line("  " + thin_result + " = call " + ret_type + " " + fn_ptr + "(" +
                              args_str_thin + ")");
                }
                emit_line("  br label %" + label_merge);

                // Fat call (with env as first arg)
                emit_line(label_fat + ":");
                std::string args_str_fat = "ptr " + env_ptr;
                for (size_t i = 0; i < arg_vals.size(); ++i) {
                    args_str_fat += ", ";
                    args_str_fat += arg_types[i] + " " + arg_vals[i];
                }
                std::string fat_result;
                if (ret_type == "void") {
                    emit_line("  call void " + fn_ptr + "(" + args_str_fat + ")");
                } else {
                    fat_result = fresh_reg();
                    emit_line("  " + fat_result + " = call " + ret_type + " " + fn_ptr + "(" +
                              args_str_fat + ")");
                }
                emit_line("  br label %" + label_merge);

                // Merge
                emit_line(label_merge + ":");
                if (ret_type == "void") {
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    std::string phi_result = fresh_reg();
                    emit_line("  " + phi_result + " = phi " + ret_type + " [ " + thin_result +
                              ", %" + label_thin + " ], [ " + fat_result + ", %" + label_fat +
                              " ]");
                    last_expr_type_ = ret_type;
                    return phi_result;
                }
            } else {
                // Thin pointer fallback
                std::string args_str;
                for (size_t i = 0; i < arg_vals.size(); ++i) {
                    if (i > 0)
                        args_str += ", ";
                    args_str += arg_types[i] + " " + arg_vals[i];
                }
                if (ret_type == "void") {
                    emit_line("  call void " + fn_ptr + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return "void";
                } else {
                    std::string result = fresh_reg();
                    emit_line("  " + result + " = call " + ret_type + " " + fn_ptr + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        }

        report_error("Cannot call non-function field", call.span, "C003");
        return "0";
    } else {
        report_error("Complex callee not supported", call.span, "C002");
        return "0";
    }

    // ============ PRIMITIVE TYPE STATIC METHODS ============
    // Handle Type::default() calls for primitive types
    if (call.callee->is<parser::PathExpr>()) {
        const auto& path = call.callee->as<parser::PathExpr>().path;
        if (path.segments.size() == 2) {
            std::string type_name = path.segments[0];
            const std::string& method = path.segments[1];

            // Substitute type parameter with concrete type (e.g., T -> I64)
            // This handles T::default() in generic contexts
            auto type_sub_it = current_type_subs_.find(type_name);
            if (type_sub_it != current_type_subs_.end()) {
                type_name = types::type_to_string(type_sub_it->second);
            }

            bool is_primitive_type =
                type_name == "I8" || type_name == "I16" || type_name == "I32" ||
                type_name == "I64" || type_name == "I128" || type_name == "U8" ||
                type_name == "U16" || type_name == "U32" || type_name == "U64" ||
                type_name == "U128" || type_name == "F32" || type_name == "F64" ||
                type_name == "Bool" || type_name == "Str";

            // Handle default(), zero(), one(), min_value(), max_value() for primitive types
            if (is_primitive_type && (method == "default" || method == "zero")) {
                // Track coverage
                emit_coverage(type_name + "::" + method);

                // Integer types: default/zero is 0
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
                // Float types: default/zero is 0.0
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

            // Handle one() for primitive types
            if (is_primitive_type && method == "one") {
                emit_coverage(type_name + "::one");

                // Integer types: one is 1
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
                    return "1";
                }
                // Float types: one is 1.0
                if (type_name == "F32") {
                    last_expr_type_ = "float";
                    return "1.0";
                }
                if (type_name == "F64") {
                    last_expr_type_ = "double";
                    return "1.0";
                }
            }

            // Handle min_value() for bounded types
            if (is_primitive_type && method == "min_value") {
                emit_coverage(type_name + "::min_value");

                if (type_name == "I8") {
                    last_expr_type_ = "i8";
                    return "-128";
                }
                if (type_name == "I16") {
                    last_expr_type_ = "i16";
                    return "-32768";
                }
                if (type_name == "I32") {
                    last_expr_type_ = "i32";
                    return "-2147483648";
                }
                if (type_name == "I64") {
                    last_expr_type_ = "i64";
                    return "-9223372036854775808";
                }
                if (type_name == "U8" || type_name == "U16" || type_name == "U32" ||
                    type_name == "U64" || type_name == "U128") {
                    std::string llvm_ty;
                    if (type_name == "U8")
                        llvm_ty = "i8";
                    else if (type_name == "U16")
                        llvm_ty = "i16";
                    else if (type_name == "U32")
                        llvm_ty = "i32";
                    else if (type_name == "U64")
                        llvm_ty = "i64";
                    else
                        llvm_ty = "i128";
                    last_expr_type_ = llvm_ty;
                    return "0";
                }
            }

            // Handle max_value() for bounded types
            if (is_primitive_type && method == "max_value") {
                emit_coverage(type_name + "::max_value");

                if (type_name == "I8") {
                    last_expr_type_ = "i8";
                    return "127";
                }
                if (type_name == "I16") {
                    last_expr_type_ = "i16";
                    return "32767";
                }
                if (type_name == "I32") {
                    last_expr_type_ = "i32";
                    return "2147483647";
                }
                if (type_name == "I64") {
                    last_expr_type_ = "i64";
                    return "9223372036854775807";
                }
                if (type_name == "U8") {
                    last_expr_type_ = "i8";
                    return "255";
                }
                if (type_name == "U16") {
                    last_expr_type_ = "i16";
                    return "65535";
                }
                if (type_name == "U32") {
                    last_expr_type_ = "i32";
                    return "4294967295";
                }
                if (type_name == "U64") {
                    last_expr_type_ = "i64";
                    return "18446744073709551615";
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
                    // Use last_expr_is_unsigned_ to determine si/ui conversion
                    if (last_expr_is_unsigned_) {
                        emit_line("  " + result + " = uitofp " + src_type + " " + src_val + " to " +
                                  target_ty);
                    } else {
                        emit_line("  " + result + " = sitofp " + src_type + " " + src_val + " to " +
                                  target_ty);
                    }
                } else {
                    // Int to int conversion
                    if (src_width < target_width) {
                        // Extension - use sext for signed, zext for unsigned
                        // Use last_expr_is_unsigned_ which was set by gen_expr for the source
                        // Special case: i1 (Bool) is always unsigned - use zext to get 0 or 1
                        if (last_expr_is_unsigned_ || src_type == "i1") {
                            emit_line("  " + result + " = zext " + src_type + " " + src_val +
                                      " to " + target_ty);
                        } else {
                            emit_line("  " + result + " = sext " + src_type + " " + src_val +
                                      " to " + target_ty);
                        }
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

    // Try intrinsics first (unreachable, assume, etc.)
    if (auto result = try_gen_intrinsic(fn_name, call)) {
        return *result;
    }

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

    if (auto result = try_gen_builtin_string(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_assert(fn_name, call)) {
        return *result;
    }

    if (auto result = try_gen_builtin_async(fn_name, call)) {
        return *result;
    }

    // ============ ENUM CONSTRUCTORS ============

    // Check if this is an enum constructor via PathExpr (e.g., Option::Some(42))
    if (call.callee->is<parser::PathExpr>()) {
        const auto& path_expr = call.callee->as<parser::PathExpr>();
        const auto& segments = path_expr.path.segments;
        if (segments.size() == 2) {
            const std::string& enum_name = segments[0];
            const std::string& variant_name = segments[1];

            // First check pending generic enums
            auto gen_enum_it = pending_generic_enums_.find(enum_name);
            if (gen_enum_it != pending_generic_enums_.end()) {
                const auto& gen_enum_decl = *gen_enum_it->second;
                for (size_t variant_idx = 0; variant_idx < gen_enum_decl.variants.size();
                     ++variant_idx) {
                    const auto& variant = gen_enum_decl.variants[variant_idx];
                    if (variant.name == variant_name) {
                        // Found generic enum constructor via PathExpr
                        std::string enum_type;

                        // Check if variant has payload
                        bool has_payload =
                            variant.tuple_fields.has_value() && !variant.tuple_fields->empty();

                        // If we have expected type from context, use it
                        if (!expected_enum_type_.empty()) {
                            enum_type = expected_enum_type_;
                        } else if (!current_ret_type_.empty() &&
                                   current_ret_type_.find("%struct." + enum_name + "__") == 0) {
                            enum_type = current_ret_type_;
                        } else if (!closure_return_type_.empty() &&
                                   closure_return_type_.find("%struct." + enum_name + "__") == 0) {
                            // Inside inline closure evaluation: use the closure's return type
                            enum_type = closure_return_type_;
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
                                require_enum_instantiation(enum_name, inferred_type_args);
                            enum_type = "%struct." + mangled_name;
                        }

                        std::string result = fresh_reg();
                        std::string enum_val = fresh_reg();

                        // Create enum value on stack
                        emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                        // Set tag (field 0)
                        std::string tag_ptr = fresh_reg();
                        emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type +
                                  ", ptr " + enum_val + ", i32 0, i32 0");
                        emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " +
                                  tag_ptr);

                        // Set payload if present
                        if (has_payload && !call.args.empty()) {
                            std::string payload = gen_expr(*call.args[0]);

                            // Skip store for Unit payload - "{}" is zero-sized
                            if (last_expr_type_ != "{}") {
                                std::string payload_ptr = fresh_reg();
                                emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                          enum_type + ", ptr " + enum_val + ", i32 0, i32 1");

                                std::string payload_typed_ptr = fresh_reg();
                                emit_line("  " + payload_typed_ptr + " = bitcast ptr " +
                                          payload_ptr + " to ptr");
                                emit_line("  store " + last_expr_type_ + " " + payload + ", ptr " +
                                          payload_typed_ptr);
                            }
                        }

                        // Load the complete enum value
                        emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                        last_expr_type_ = enum_type;
                        return result;
                    }
                }
            }

            // Then check non-generic enums (including from imported modules)
            // Helper lambda to generate path-based enum constructor
            auto gen_path_enum_constructor =
                [&](const std::string& enum_name,
                    const types::EnumDef& enum_def) -> std::optional<std::string> {
                for (size_t variant_idx = 0; variant_idx < enum_def.variants.size();
                     ++variant_idx) {
                    const auto& [vname, payload_types] = enum_def.variants[variant_idx];
                    if (vname == variant_name) {
                        std::string enum_type = "%struct." + enum_name;
                        std::string result = fresh_reg();
                        std::string enum_val = fresh_reg();

                        emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                        std::string tag_ptr = fresh_reg();
                        emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type +
                                  ", ptr " + enum_val + ", i32 0, i32 0");
                        emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " +
                                  tag_ptr);

                        if (!payload_types.empty() && !call.args.empty()) {
                            std::string payload = gen_expr(*call.args[0]);

                            // Skip store for Unit payload - "{}" is zero-sized
                            if (last_expr_type_ != "{}") {
                                std::string payload_ptr = fresh_reg();
                                emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                          enum_type + ", ptr " + enum_val + ", i32 0, i32 1");

                                std::string payload_typed_ptr = fresh_reg();
                                emit_line("  " + payload_typed_ptr + " = bitcast ptr " +
                                          payload_ptr + " to ptr");
                                emit_line("  store " + last_expr_type_ + " " + payload + ", ptr " +
                                          payload_typed_ptr);
                            }
                        }

                        emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                        last_expr_type_ = enum_type;
                        return result;
                    }
                }
                return std::nullopt;
            };

            // First try lookup_enum (handles local and imported enums)
            auto enum_opt = env_.lookup_enum(enum_name);
            if (enum_opt) {
                if (auto result = gen_path_enum_constructor(enum_name, *enum_opt)) {
                    return *result;
                }
            }

            // If not found via lookup_enum, search all modules
            // This handles cases where we're generating code for a module's functions
            // but the enum is defined in that module (not imported to main file)
            for (const auto& [mod_path, mod] : env_.get_all_modules()) {
                auto enum_it = mod.enums.find(enum_name);
                if (enum_it != mod.enums.end()) {
                    if (auto result = gen_path_enum_constructor(enum_name, enum_it->second)) {
                        return *result;
                    }
                }
            }
        }
    }

    // Check if this is an enum constructor via bare IdentExpr (e.g., Some(42))
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
                    } else if (!closure_return_type_.empty() &&
                               closure_return_type_.find("%struct." + gen_enum_name + "__") == 0) {
                        // Inside inline closure evaluation (e.g., Outcome::and_then closure):
                        // use the closure's return type to resolve the full generic enum type
                        enum_type = closure_return_type_;
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
                        // For nested generics like Maybe[Maybe[I32]], we need to compute
                        // the inner type for the payload before generating the inner expression.
                        // expected_enum_type_ might be %struct.Maybe__Maybe__I32, but the
                        // inner Just(42) needs %struct.Maybe__I32 as its expected type.
                        std::string saved_expected = expected_enum_type_;
                        if (!enum_type.empty() && enum_type.starts_with("%struct.")) {
                            // Extract type args from the mangled name
                            std::string mangled = enum_type.substr(8); // Skip "%struct."
                            auto sep = mangled.find("__");
                            if (sep != std::string::npos) {
                                std::string base = mangled.substr(0, sep);
                                std::string type_arg_str = mangled.substr(sep + 2);
                                // For single-type-param generics, the payload is the type arg
                                // Check if this is a single-type-param enum
                                size_t num_type_params = gen_enum_decl->generics.size();
                                if (num_type_params == 1 &&
                                    type_arg_str.find("__") != std::string::npos) {
                                    // The type arg itself is a generic - set expected type for
                                    // inner
                                    expected_enum_type_ = "%struct." + type_arg_str;
                                }
                            }
                        }

                        std::string payload = gen_expr(*call.args[0]);
                        expected_enum_type_ = saved_expected;

                        // Skip store for Unit payload - "{}" is zero-sized
                        if (last_expr_type_ != "{}") {
                            // Get pointer to payload field ([N x i8])
                            std::string payload_ptr = fresh_reg();
                            emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                      enum_type + ", ptr " + enum_val + ", i32 0, i32 1");

                            // Cast payload to bytes and store
                            std::string payload_typed_ptr = fresh_reg();
                            emit_line("  " + payload_typed_ptr + " = bitcast ptr " + payload_ptr +
                                      " to ptr");
                            emit_line("  store " + last_expr_type_ + " " + payload + ", ptr " +
                                      payload_typed_ptr);
                        }
                    }

                    // Load the complete enum value
                    emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                    last_expr_type_ = enum_type;
                    return result;
                }
            }
        }

        // Then check non-generic enums (including from imported modules)
        // Helper lambda to generate enum constructor
        auto gen_enum_constructor =
            [&](const std::string& enum_name,
                const types::EnumDef& enum_def) -> std::optional<std::string> {
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

                        // Skip store for Unit payload - "{}" is zero-sized
                        if (last_expr_type_ != "{}") {
                            // Get pointer to payload field ([N x i8])
                            std::string payload_ptr = fresh_reg();
                            emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                      enum_type + ", ptr " + enum_val + ", i32 0, i32 1");

                            // Cast payload to bytes and store
                            std::string payload_typed_ptr = fresh_reg();
                            emit_line("  " + payload_typed_ptr + " = bitcast ptr " + payload_ptr +
                                      " to ptr");
                            emit_line("  store " + last_expr_type_ + " " + payload + ", ptr " +
                                      payload_typed_ptr);
                        }
                    }

                    // Load the complete enum value
                    emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                    last_expr_type_ = enum_type;
                    return result;
                }
            }
            return std::nullopt;
        };

        // Check local enums first
        for (const auto& [enum_name, enum_def] : env_.all_enums()) {
            if (auto result = gen_enum_constructor(enum_name, enum_def)) {
                return *result;
            }
        }

        // Check enums from imported modules
        for (const auto& [mod_path, mod] : env_.get_all_modules()) {
            for (const auto& [enum_name, enum_def] : mod.enums) {
                if (auto result = gen_enum_constructor(enum_name, enum_def)) {
                    return *result;
                }
            }
        }
    }

    // ============ INDIRECT FUNCTION POINTER CALLS ============

    // Check if this is an indirect call through a function pointer variable
    auto local_it = locals_.find(fn_name);

    // Fat pointer closure call: variable type is "{ ptr, ptr }" (fn_ptr + env_ptr)
    if (local_it != locals_.end() && local_it->second.type == "{ ptr, ptr }") {
        bool is_capturing = local_it->second.is_capturing_closure;

        // Load the fat pointer from the alloca
        std::string fat_ptr = fresh_reg();
        emit_line("  " + fat_ptr + " = load { ptr, ptr }, ptr " + local_it->second.reg);

        // Extract fn_ptr and env_ptr
        std::string fn_ptr = fresh_reg();
        emit_line("  " + fn_ptr + " = extractvalue { ptr, ptr } " + fat_ptr + ", 0");
        std::string env_ptr = fresh_reg();
        emit_line("  " + env_ptr + " = extractvalue { ptr, ptr } " + fat_ptr + ", 1");

        // Generate user arguments
        std::vector<std::pair<std::string, std::string>> user_args;
        for (size_t i = 0; i < call.args.size(); ++i) {
            std::string val = gen_expr(*call.args[i]);
            user_args.push_back({val, last_expr_type_});
        }

        // Determine return type from semantic type if available
        std::string ret_type = "i32";
        if (local_it->second.semantic_type) {
            if (local_it->second.semantic_type->is<types::FuncType>()) {
                const auto& func_type = local_it->second.semantic_type->as<types::FuncType>();
                ret_type = llvm_type_from_semantic(func_type.return_type);
            } else if (local_it->second.semantic_type->is<types::ClosureType>()) {
                const auto& closure_type = local_it->second.semantic_type->as<types::ClosureType>();
                ret_type = llvm_type_from_semantic(closure_type.return_type);
            }
        }

        if (is_capturing) {
            // Known capturing closure: call fn(env, args...)
            std::vector<std::pair<std::string, std::string>> arg_vals;
            arg_vals.push_back({env_ptr, "ptr"});
            arg_vals.insert(arg_vals.end(), user_args.begin(), user_args.end());

            std::string func_type_sig = ret_type + " (";
            for (size_t i = 0; i < arg_vals.size(); ++i) {
                if (i > 0)
                    func_type_sig += ", ";
                func_type_sig += arg_vals[i].second;
            }
            func_type_sig += ")";

            if (ret_type == "void") {
                emit("  call " + func_type_sig + " " + fn_ptr + "(");
                for (size_t i = 0; i < arg_vals.size(); ++i) {
                    if (i > 0)
                        emit(", ");
                    emit(arg_vals[i].second + " " + arg_vals[i].first);
                }
                emit_line(")");
                last_expr_type_ = "void";
                return "0";
            }

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
        } else {
            // Unknown or non-capturing: runtime null-check on env_ptr
            // Non-null env -> capturing closure: call fn(env, args...)
            // Null env -> plain function: call fn(args...)
            std::string is_null = fresh_reg();
            emit_line("  " + is_null + " = icmp eq ptr " + env_ptr + ", null");

            std::string label_thin = "fp_thin" + std::to_string(label_counter_);
            std::string label_fat = "fp_fat" + std::to_string(label_counter_);
            std::string label_merge = "fp_merge" + std::to_string(label_counter_);
            label_counter_++;

            emit_line("  br i1 " + is_null + ", label %" + label_thin + ", label %" + label_fat);

            // Thin call (no env)
            emit_line(label_thin + ":");
            std::string args_str_thin;
            for (size_t i = 0; i < user_args.size(); ++i) {
                if (i > 0)
                    args_str_thin += ", ";
                args_str_thin += user_args[i].second + " " + user_args[i].first;
            }
            std::string thin_result;
            if (ret_type == "void") {
                emit_line("  call void " + fn_ptr + "(" + args_str_thin + ")");
            } else {
                thin_result = fresh_reg();
                emit_line("  " + thin_result + " = call " + ret_type + " " + fn_ptr + "(" +
                          args_str_thin + ")");
            }
            emit_line("  br label %" + label_merge);

            // Fat call (with env as first arg)
            emit_line(label_fat + ":");
            std::string args_str_fat = "ptr " + env_ptr;
            for (size_t i = 0; i < user_args.size(); ++i) {
                args_str_fat += ", ";
                args_str_fat += user_args[i].second + " " + user_args[i].first;
            }
            std::string fat_result;
            if (ret_type == "void") {
                emit_line("  call void " + fn_ptr + "(" + args_str_fat + ")");
            } else {
                fat_result = fresh_reg();
                emit_line("  " + fat_result + " = call " + ret_type + " " + fn_ptr + "(" +
                          args_str_fat + ")");
            }
            emit_line("  br label %" + label_merge);

            // Merge
            emit_line(label_merge + ":");
            if (ret_type == "void") {
                last_expr_type_ = "void";
                return "0";
            } else {
                std::string phi_result = fresh_reg();
                emit_line("  " + phi_result + " = phi " + ret_type + " [ " + thin_result + ", %" +
                          label_thin + " ], [ " + fat_result + ", %" + label_fat + " ]");
                last_expr_type_ = ret_type;
                return phi_result;
            }
        }
    }

    // Thin function pointer call: variable type is "ptr" (plain func pointer, no env)
    if (local_it != locals_.end() && local_it->second.type == "ptr") {
        // This is a plain function pointer variable - generate indirect call
        std::string fn_ptr;
        if (local_it->second.reg[0] == '@') {
            fn_ptr = local_it->second.reg;
        } else {
            fn_ptr = fresh_reg();
            emit_line("  " + fn_ptr + " = load ptr, ptr " + local_it->second.reg);
        }

        // Generate arguments (no env pointer for thin function pointers)
        std::vector<std::pair<std::string, std::string>> arg_vals;

        // Legacy: prepend captured variables if present (backward compat)
        if (local_it->second.closure_captures.has_value()) {
            const auto& captures = local_it->second.closure_captures.value();
            for (size_t i = 0; i < captures.captured_names.size(); ++i) {
                const std::string& cap_name = captures.captured_names[i];
                const std::string& cap_type = captures.captured_types[i];
                auto cap_it = locals_.find(cap_name);
                if (cap_it != locals_.end()) {
                    std::string cap_val = fresh_reg();
                    emit_line("  " + cap_val + " = load " + cap_type + ", ptr " +
                              cap_it->second.reg);
                    arg_vals.push_back({cap_val, cap_type});
                } else {
                    arg_vals.push_back({"0", cap_type});
                }
            }
        }

        for (size_t i = 0; i < call.args.size(); ++i) {
            std::string val = gen_expr(*call.args[i]);
            arg_vals.push_back({val, last_expr_type_});
        }

        std::string ret_type = "i32";
        if (local_it->second.semantic_type) {
            if (local_it->second.semantic_type->is<types::FuncType>()) {
                const auto& func_type = local_it->second.semantic_type->as<types::FuncType>();
                ret_type = llvm_type_from_semantic(func_type.return_type);
            } else if (local_it->second.semantic_type->is<types::ClosureType>()) {
                const auto& closure_type = local_it->second.semantic_type->as<types::ClosureType>();
                ret_type = llvm_type_from_semantic(closure_type.return_type);
            }
        }

        std::string func_type_sig = ret_type + " (";
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0)
                func_type_sig += ", ";
            func_type_sig += arg_vals[i].second;
        }
        func_type_sig += ")";

        if (ret_type == "void") {
            emit("  call " + func_type_sig + " " + fn_ptr + "(");
            for (size_t i = 0; i < arg_vals.size(); ++i) {
                if (i > 0)
                    emit(", ");
                emit(arg_vals[i].second + " " + arg_vals[i].first);
            }
            emit_line(")");
            last_expr_type_ = "void";
            return "0";
        }

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
    // For module-qualified calls like "mem::forget", also try the bare name "forget"
    // since module functions are registered by bare name during gen_func_decl
    // BUT: skip this for Type::method patterns (e.g., RawMutPtr::from_addr)
    // where the first segment is a type name (starts with uppercase).
    // Those are struct static methods, not module-qualified standalone functions.
    if (pending_func_it == pending_generic_funcs_.end() &&
        fn_name.find("::") != std::string::npos) {
        size_t last_sep = fn_name.rfind("::");
        std::string prefix = fn_name.substr(0, last_sep);
        std::string bare_name = fn_name.substr(last_sep + 2);
        // Only do bare name fallback for module-qualified calls (lowercase prefix)
        // Skip for Type::method patterns where prefix is a type name (uppercase)
        bool is_type_static_method =
            !prefix.empty() && std::isupper(prefix[0]) && prefix.find("::") == std::string::npos;
        if (!is_type_static_method) {
            pending_func_it = pending_generic_funcs_.find(bare_name);
        }
    }
    if (pending_func_it != pending_generic_funcs_.end()) {
        const auto& gen_func = *pending_func_it->second;

        // Build set of generic parameter names for unification
        std::unordered_set<std::string> generic_names;
        for (const auto& g : gen_func.generics) {
            generic_names.insert(g.name);
        }

        // First, check for explicit type arguments in the callee
        // e.g., get_from_container[IntBox](ref box, 0) has explicit type arg IntBox
        std::unordered_map<std::string, types::TypePtr> bindings;
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics.has_value() && !path_expr.generics->args.empty()) {
                // Map explicit type args to generic parameters
                for (size_t i = 0;
                     i < path_expr.generics->args.size() && i < gen_func.generics.size(); ++i) {
                    const auto& arg = path_expr.generics->args[i];
                    if (arg.is_type()) {
                        // Convert parser type to semantic type
                        std::unordered_map<std::string, types::TypePtr> empty_subs;
                        types::TypePtr explicit_type =
                            resolve_parser_type_with_subs(*arg.as_type(), empty_subs);
                        bindings[gen_func.generics[i].name] = explicit_type;
                        TML_DEBUG_LN(
                            "[GENERIC CALL] explicit type arg: "
                            << gen_func.generics[i].name << " -> "
                            << (explicit_type->is<types::NamedType>() ? "NamedType" : "other"));
                    }
                }
            }
        }

        // Infer any remaining type arguments using unification
        // For each argument, unify the parameter type pattern with the argument type
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
        // Use the key from pending_generic_funcs_ (bare name like "forget")
        // rather than fn_name ("mem::forget") so generate_pending_instantiations can find it
        std::string base_name = pending_func_it->first;
        std::string mangled_name = require_func_instantiation(base_name, inferred_type_args);

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
            bool param_takes_ownership = true; // Default to taking ownership
            if (i < gen_func.params.size()) {
                types::TypePtr param_type =
                    resolve_parser_type_with_subs(*gen_func.params[i].type, subs);
                std::string llvm_param_type = llvm_type_from_semantic(param_type);
                // Set expected type context for generic enum constructors like Nothing
                if (llvm_param_type.find("%struct.") == 0 &&
                    llvm_param_type.find("__") != std::string::npos) {
                    expected_enum_type_ = llvm_param_type;
                }
                // Check if parameter is a reference type (doesn't take ownership)
                if (param_type->is<types::RefType>()) {
                    param_takes_ownership = false;
                }
            }
            std::string val = gen_expr(*call.args[i]);
            expected_enum_type_.clear(); // Clear after generating argument
            // Generic function params with FuncType now accept { ptr, ptr } (fat pointer)
            // so no coercion needed — pass the full fat pointer through
            std::string arg_type = last_expr_type_;
            arg_vals.push_back({val, arg_type});

            // CRITICAL: Mark variable as consumed if passed by value (ownership transfer)
            // This prevents double-drop at function return for moved variables
            if (param_takes_ownership && call.args[i]->is<parser::IdentExpr>()) {
                const auto& ident = call.args[i]->as<parser::IdentExpr>();
                mark_var_consumed(ident.name);
            }
            // Handle partial moves: mark struct field as consumed when passed by value
            else if (param_takes_ownership && call.args[i]->is<parser::FieldExpr>()) {
                const auto& field = call.args[i]->as<parser::FieldExpr>();
                // Get the base variable name
                if (field.object->is<parser::IdentExpr>()) {
                    const auto& base = field.object->as<parser::IdentExpr>();
                    mark_field_consumed(base.name, field.field);
                }
            }
        }

        // Call the instantiated function
        // Generic function instantiations don't use suite prefix - they're typically library
        // functions and should be shared across all test files in a suite
        std::string func_name = "@tml_" + mangled_name;
        std::string dbg_suffix = get_debug_loc_suffix();
        if (ret_type == "void") {
            emit("  call void " + func_name + "(");
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
            emit("  " + result + " = call " + ret_type + " " + func_name + "(");
            for (size_t i = 0; i < arg_vals.size(); ++i) {
                if (i > 0)
                    emit(", ");
                emit(arg_vals[i].second + " " + arg_vals[i].first);
            }
            emit_line(")" + dbg_suffix);
            last_expr_type_ = ret_type;
            return result;
        }
    }

    // ============ CLASS CONSTRUCTOR CALLS ============
    // Handle calls like Counter::new(10) where Counter is a class
    if (call.callee->is<parser::PathExpr>()) {
        const auto& path = call.callee->as<parser::PathExpr>().path;
        if (path.segments.size() == 2) {
            const std::string& type_name = path.segments[0];
            const std::string& method = path.segments[1];

            // Check if this is a class constructor call
            if (method == "new") {
                auto class_def = env_.lookup_class(type_name);
                bool is_generic_class =
                    pending_generic_classes_.find(type_name) != pending_generic_classes_.end();

                if (class_def.has_value() || is_generic_class) {
                    std::string class_name = type_name;
                    std::string class_type;

                    // Handle generic class instantiation
                    if (is_generic_class && !expected_enum_type_.empty()) {
                        // Check if expected type is a class type like "%class.Box__I32"
                        std::string expected_prefix = "%class." + type_name + "__";
                        if (expected_enum_type_.find(expected_prefix) == 0) {
                            // Extract mangled name
                            std::string mangled = expected_enum_type_.substr(7); // Remove "%class."
                            std::string type_arg_str = mangled.substr(type_name.length() + 2);

                            // Infer type arguments from mangled name
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
                            else if (type_arg_str == "Bool")
                                type_arg = types::make_bool();
                            else {
                                // For struct types, use parse_mangled_type_string for proper
                                // handling
                                type_arg = parse_mangled_type_string(type_arg_str);
                            }

                            if (type_arg) {
                                std::vector<types::TypePtr> type_args = {type_arg};
                                class_name = require_class_instantiation(type_name, type_args);
                            }
                        }
                    }

                    if (class_type.empty()) {
                        class_type = "%class." + class_name;
                    }

                    // Generate arguments and track types for overload resolution
                    std::vector<std::string> args;
                    std::vector<std::string> arg_types;

                    for (const auto& arg : call.args) {
                        args.push_back(gen_expr(*arg));
                        arg_types.push_back(last_expr_type_.empty() ? "i64" : last_expr_type_);
                    }

                    // Build constructor lookup key based on argument types (for overload
                    // resolution)
                    std::string ctor_key = class_name + "_new";
                    if (!arg_types.empty()) {
                        for (const auto& at : arg_types) {
                            ctor_key += "_" + at;
                        }
                    }

                    // Look up the constructor in functions_ map to get mangled name and return type
                    std::string ctor_name;
                    std::string ctor_ret_type = "ptr"; // Default: pointer return (opaque ptr)
                    auto func_it = functions_.find(ctor_key);
                    if (func_it != functions_.end()) {
                        ctor_name = func_it->second.llvm_name;
                        // Use the registered return type (value classes return struct, not ptr)
                        if (!func_it->second.ret_type.empty()) {
                            ctor_ret_type = func_it->second.ret_type;
                        }
                    } else {
                        // Fallback: try without overload suffix for default constructor
                        auto default_it = functions_.find(class_name + "_new");
                        if (default_it != functions_.end()) {
                            ctor_name = default_it->second.llvm_name;
                            if (!default_it->second.ret_type.empty()) {
                                ctor_ret_type = default_it->second.ret_type;
                            }
                        } else {
                            // Last resort: generate name with parameter type suffixes
                            // (must match gen_class_constructor_instantiation naming)
                            ctor_name = "@tml_" + get_suite_prefix() + class_name + "_new";
                            if (!arg_types.empty()) {
                                for (const auto& at : arg_types) {
                                    std::string type_suffix = at;
                                    if (type_suffix == "i8")
                                        type_suffix = "I8";
                                    else if (type_suffix == "i16")
                                        type_suffix = "I16";
                                    else if (type_suffix == "i32")
                                        type_suffix = "I32";
                                    else if (type_suffix == "i64")
                                        type_suffix = "I64";
                                    else if (type_suffix == "i128")
                                        type_suffix = "I128";
                                    else if (type_suffix == "float")
                                        type_suffix = "F32";
                                    else if (type_suffix == "double")
                                        type_suffix = "F64";
                                    else if (type_suffix == "i1")
                                        type_suffix = "Bool";
                                    else if (type_suffix.find("ptr") != std::string::npos ||
                                             type_suffix.find("%") != std::string::npos)
                                        type_suffix = "ptr";
                                    ctor_name += "_" + type_suffix;
                                }
                            }
                        }
                    }

                    // Generate call using the correct return type
                    std::string result = fresh_reg();
                    std::string call_str =
                        "  " + result + " = call " + ctor_ret_type + " " + ctor_name + "(";
                    for (size_t i = 0; i < args.size(); ++i) {
                        if (i > 0)
                            call_str += ", ";
                        call_str += arg_types[i] + " " + args[i];
                    }
                    call_str += ")";
                    emit_line(call_str);

                    last_expr_type_ = ctor_ret_type;
                    return result;
                }
            }
        }
    }

    // ============ GENERIC CLASS STATIC METHODS ============
    // Handle calls like Utils::identity[I32](42) where identity is a generic static method
    if (call.callee->is<parser::PathExpr>()) {
        const auto& path_expr = call.callee->as<parser::PathExpr>();
        const auto& path = path_expr.path;
        if (path.segments.size() == 2 && path_expr.generics.has_value()) {
            const std::string& class_name = path.segments[0];
            const std::string& method_name = path.segments[1];
            const auto& gen_args = path_expr.generics->args;

            // Check if this is a generic class static method
            std::string method_key = class_name + "::" + method_name;
            auto pending_it = pending_generic_class_methods_.find(method_key);
            if (pending_it != pending_generic_class_methods_.end()) {
                const auto& pending = pending_it->second;
                const auto& method = pending.class_decl->methods[pending.method_index];

                // Build type substitutions from generic arguments
                // IMPORTANT: Use current_type_subs_ to resolve type parameters like T -> I32
                std::unordered_map<std::string, types::TypePtr> type_subs;
                for (size_t i = 0; i < method.generics.size() && i < gen_args.size(); ++i) {
                    if (!method.generics[i].is_const && gen_args[i].is_type()) {
                        type_subs[method.generics[i].name] = resolve_parser_type_with_subs(
                            *gen_args[i].as_type(), current_type_subs_);
                    }
                }

                // Build mangled name suffix (e.g., "_I32" for identity[I32])
                std::vector<types::TypePtr> method_type_args;
                for (const auto& arg : gen_args) {
                    if (arg.is_type()) {
                        auto sem_type =
                            resolve_parser_type_with_subs(*arg.as_type(), current_type_subs_);
                        method_type_args.push_back(sem_type);
                    }
                }
                std::string type_suffix =
                    method_type_args.empty() ? "" : "__" + mangle_type_args(method_type_args);

                // Generate mangled function name
                std::string mangled_func =
                    "@tml_" + get_suite_prefix() + class_name + "_" + method_name + type_suffix;

                // Queue the instantiation for later (after current function)
                if (generated_functions_.find(mangled_func) == generated_functions_.end()) {
                    pending_generic_class_method_insts_.push_back(PendingGenericClassMethodInst{
                        pending.class_decl, &method, type_suffix, type_subs});
                    generated_functions_.insert(mangled_func);
                }

                // Generate arguments
                std::vector<std::string> args;
                std::vector<std::string> arg_types;
                for (const auto& arg : call.args) {
                    args.push_back(gen_expr(*arg));
                    arg_types.push_back(last_expr_type_);
                }

                // Determine return type with substitution
                std::string ret_type = "void";
                if (method.return_type) {
                    auto sem_ret =
                        resolve_parser_type_with_subs(*method.return_type.value(), type_subs);
                    ret_type = llvm_type_from_semantic(sem_ret);
                }

                // Generate call
                std::string result = fresh_reg();
                std::string call_str =
                    "  " + result + " = call " + ret_type + " " + mangled_func + "(";
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

    // ============ GENERIC STRUCT STATIC METHODS ============
    // Delegated to call_generic_struct.cpp
    if (auto result = gen_call_generic_struct_method(call, fn_name)) {
        return *result;
    }

    // ============ USER-DEFINED FUNCTIONS ============
    // Delegated to call_user.cpp
    return gen_call_user_function(call, fn_name);
}

} // namespace tml::codegen
