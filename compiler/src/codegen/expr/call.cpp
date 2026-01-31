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

#include "codegen/llvm_ir_gen.hpp"

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
        // Handle calling function pointers stored in struct fields: (this.init)()
        // Get the function pointer from the field
        std::string func_ptr = gen_expr(*call.callee);

        // Infer the function type from the field
        types::TypePtr func_type = infer_expr_type(*call.callee);
        if (func_type && func_type->is<types::FuncType>()) {
            const auto& ft = func_type->as<types::FuncType>();

            // Build argument list
            std::vector<std::string> arg_vals;
            std::vector<std::string> arg_types;
            for (const auto& arg : call.args) {
                arg_vals.push_back(gen_expr(*arg));
                arg_types.push_back(last_expr_type_);
            }

            // Build call signature
            std::string ret_type =
                ft.return_type ? llvm_type_from_semantic(ft.return_type) : "void";

            std::string args_str;
            for (size_t i = 0; i < arg_vals.size(); ++i) {
                if (i > 0)
                    args_str += ", ";
                args_str += arg_types[i] + " " + arg_vals[i];
            }

            // Generate indirect call
            if (ret_type == "void") {
                emit_line("  call void " + func_ptr + "(" + args_str + ")");
                last_expr_type_ = "void";
                return "void";
            } else {
                std::string result = fresh_reg();
                emit_line("  " + result + " = call " + ret_type + " " + func_ptr + "(" + args_str +
                          ")");
                last_expr_type_ = ret_type;
                return result;
            }
        }

        report_error("Cannot call non-function field", call.span);
        return "0";
    } else {
        report_error("Complex callee not supported", call.span);
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

    if (auto result = try_gen_builtin_collections(fn_name, call)) {
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

                            std::string payload_ptr = fresh_reg();
                            emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                      enum_type + ", ptr " + enum_val + ", i32 0, i32 1");

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

                            std::string payload_ptr = fresh_reg();
                            emit_line("  " + payload_ptr + " = getelementptr inbounds " +
                                      enum_type + ", ptr " + enum_val + ", i32 0, i32 1");

                            std::string payload_typed_ptr = fresh_reg();
                            emit_line("  " + payload_typed_ptr + " = bitcast ptr " + payload_ptr +
                                      " to ptr");
                            emit_line("  store " + last_expr_type_ + " " + payload + ", ptr " +
                                      payload_typed_ptr);
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
                                if (num_type_params == 1 && type_arg_str.find("__") != std::string::npos) {
                                    // The type arg itself is a generic - set expected type for inner
                                    expected_enum_type_ = "%struct." + type_arg_str;
                                }
                            }
                        }

                        std::string payload = gen_expr(*call.args[0]);
                        expected_enum_type_ = saved_expected;

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

        // Handle void return type - don't assign result
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
            std::string arg_type = last_expr_type_;
            arg_vals.push_back({val, arg_type});

            // CRITICAL: Mark variable as consumed if passed by value (ownership transfer)
            // This prevents double-drop at function return for moved variables
            if (param_takes_ownership && call.args[i]->is<parser::IdentExpr>()) {
                const auto& ident = call.args[i]->as<parser::IdentExpr>();
                mark_var_consumed(ident.name);
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
                    bool is_local_type =
                        !is_library_internal && (pending_generic_structs_.count(type_name) > 0 ||
                                                 pending_generic_impls_.count(type_name) > 0);
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

                            pending_impl_method_instantiations_.push_back(PendingImplMethod{
                                mangled_type_name, method, actual_type_subs, type_name, "",
                                /*is_library_type=*/false});
                            generated_impl_methods_.insert(mangled_method);
                        }

                        // Generate arguments
                        std::vector<std::pair<std::string, std::string>> typed_args;
                        for (size_t i = 0; i < call.args.size(); ++i) {
                            std::string val = gen_expr(*call.args[i]);
                            std::string arg_type = last_expr_type_;
                            if (i < func_sig->params.size()) {
                                auto param_type =
                                    types::substitute_type(func_sig->params[i], type_subs);
                                arg_type = llvm_type_from_semantic(param_type);
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
                for (const auto& [mod_name, mod] : all_modules) {
                    // Check structs
                    auto struct_it = mod.structs.find(type_name);
                    if (struct_it != mod.structs.end() && !struct_it->second.type_params.empty()) {
                        imported_type_params = struct_it->second.type_params;
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
                        for (size_t i = 0; i < call.args.size() && i < func_sig->params.size(); ++i) {
                            auto arg_type = infer_expr_type(*call.args[i]);
                            const auto& param_type = func_sig->params[i];

                            // If arg is NamedType[X] and param is NamedType[T], map T -> X
                            if (arg_type && arg_type->is<types::NamedType>() &&
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
                                    // Update mangled type name
                                    if (!type_subs.empty()) {
                                        std::vector<types::TypePtr> type_args;
                                        for (const auto& gname : generic_names) {
                                            auto it = type_subs.find(gname);
                                            if (it != type_subs.end()) {
                                                type_args.push_back(it->second);
                                            }
                                        }
                                        if (!type_args.empty()) {
                                            mangled_type_name =
                                                type_name + "__" + mangle_type_args(type_args);
                                        }
                                    }
                                    break;
                                }
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

                            std::string arg_type = last_expr_type_;
                            if (param_semantic_type) {
                                arg_type = llvm_type_from_semantic(param_semantic_type);
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

    std::string mangled;
    if (func_it != functions_.end()) {
        // Use the registered LLVM name (handles @extern functions correctly)
        mangled = func_it->second.llvm_name;
    } else {
        // In suite mode, add suite prefix for test-local functions (forward references)
        // This handles mutual recursion where called function isn't yet in functions_ map
        // BUT: Don't add suite prefix for library functions (they don't have prefixes)
        bool is_library_function = false;
        if (env_.module_registry()) {
            const auto& all_modules = env_.module_registry()->get_all_modules();
            for (const auto& [mod_name, mod] : all_modules) {
                if (mod.functions.find(fn_name) != mod.functions.end() ||
                    mod.functions.find(sanitized_name) != mod.functions.end()) {
                    is_library_function = true;

                    // Queue instantiation for non-generic library static methods (e.g., Text::from)
                    // Only for Type::method calls (sanitized_name has no ::)
                    size_t sep_pos = fn_name.find("::");
                    if (sep_pos != std::string::npos) {
                        std::string type_name = fn_name.substr(0, sep_pos);
                        std::string method_name = fn_name.substr(sep_pos + 2);

                        // Check if this type exists in the module
                        bool is_type =
                            mod.structs.count(type_name) > 0 || mod.enums.count(type_name) > 0;
                        if (is_type) {
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
        mangled = "@tml_" + prefix + sanitized_name;
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

        // If we have function signature from TypeEnv, use parameter type
        if (func_sig.has_value() && i < func_sig->params.size()) {
            expected_type = llvm_type_from_semantic(func_sig->params[i]);
        }
        // Otherwise, try to get parameter type from registered functions (codegen's FuncInfo)
        else if (func_it != functions_.end() && i < func_it->second.param_types.size()) {
            expected_type = func_it->second.param_types[i];
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

    // V8-style inline optimization for hot runtime functions
    // text_push is called millions of times in tight loops
    if (mangled == "@tml_text_push" && arg_vals.size() == 2) {
        std::string receiver = arg_vals[0].first; // ptr to Text
        std::string byte_val = arg_vals[1].first; // i32 byte

        std::string id = std::to_string(temp_counter_++);

        // Load flags and check if heap mode (flags == 0)
        emit_line("  %flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 24");
        emit_line("  %flags." + id + " = load i8, ptr %flags_ptr." + id);
        emit_line("  %is_heap." + id + " = icmp eq i8 %flags." + id + ", 0");
        emit_line("  br i1 %is_heap." + id + ", label %push_heap." + id + ", label %push_slow." +
                  id);

        // Heap fast path
        emit_line("push_heap." + id + ":");
        emit_line("  %data_ptr_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 0");
        emit_line("  %data_ptr." + id + " = load ptr, ptr %data_ptr_ptr." + id);
        emit_line("  %len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 8");
        emit_line("  %len." + id + " = load i64, ptr %len_ptr." + id);
        emit_line("  %cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 16");
        emit_line("  %cap." + id + " = load i64, ptr %cap_ptr." + id);
        emit_line("  %has_space." + id + " = icmp ult i64 %len." + id + ", %cap." + id);
        emit_line("  br i1 %has_space." + id + ", label %push_fast." + id + ", label %push_slow." +
                  id);

        // Fast store path
        emit_line("push_fast." + id + ":");
        emit_line("  %byte_i8." + id + " = trunc i32 " + byte_val + " to i8");
        emit_line("  %store_ptr." + id + " = getelementptr i8, ptr %data_ptr." + id +
                  ", i64 %len." + id);
        emit_line("  store i8 %byte_i8." + id + ", ptr %store_ptr." + id);
        emit_line("  %new_len." + id + " = add i64 %len." + id + ", 1");
        emit_line("  store i64 %new_len." + id + ", ptr %len_ptr." + id);
        emit_line("  br label %push_done." + id);

        // Slow path - call FFI
        emit_line("push_slow." + id + ":");
        emit_line("  call void @tml_text_push(ptr " + receiver + ", i32 " + byte_val + ")");
        emit_line("  br label %push_done." + id);

        emit_line("push_done." + id + ":");
        last_expr_type_ = "void";
        return "0";
    }

    // V8-style inline optimization for text_push_str_len
    // This is the hot path for all string building operations
    if (mangled == "@tml_text_push_str_len" && arg_vals.size() == 3) {
        std::string receiver = arg_vals[0].first; // ptr to Text
        std::string str_ptr = arg_vals[1].first;  // ptr to string data
        std::string str_len = arg_vals[2].first;  // i64 length

        std::string id = std::to_string(temp_counter_++);

        // Load flags and check if heap mode (flags == 0)
        emit_line("  %psl_flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 24");
        emit_line("  %psl_flags." + id + " = load i8, ptr %psl_flags_ptr." + id);
        emit_line("  %psl_is_heap." + id + " = icmp eq i8 %psl_flags." + id + ", 0");
        emit_line("  br i1 %psl_is_heap." + id + ", label %psl_heap." + id + ", label %psl_slow." +
                  id);

        // Heap mode - check capacity
        emit_line("psl_heap." + id + ":");
        emit_line("  %psl_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 8");
        emit_line("  %psl_len." + id + " = load i64, ptr %psl_len_ptr." + id);
        emit_line("  %psl_cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 16");
        emit_line("  %psl_cap." + id + " = load i64, ptr %psl_cap_ptr." + id);
        emit_line("  %psl_new_len." + id + " = add i64 %psl_len." + id + ", " + str_len);
        emit_line("  %psl_has_space." + id + " = icmp ule i64 %psl_new_len." + id + ", %psl_cap." +
                  id);
        emit_line("  br i1 %psl_has_space." + id + ", label %psl_fast." + id +
                  ", label %psl_slow." + id);

        // Fast path - direct memcpy
        emit_line("psl_fast." + id + ":");
        emit_line("  %psl_data_ptr." + id + " = load ptr, ptr " + receiver);
        emit_line("  %psl_dst." + id + " = getelementptr i8, ptr %psl_data_ptr." + id +
                  ", i64 %psl_len." + id);
        // Use llvm.memcpy intrinsic for efficient copying
        emit_line("  call void @llvm.memcpy.p0.p0.i64(ptr %psl_dst." + id + ", ptr " + str_ptr +
                  ", i64 " + str_len + ", i1 false)");
        // Update length
        emit_line("  store i64 %psl_new_len." + id + ", ptr %psl_len_ptr." + id);
        // Null terminate
        emit_line("  %psl_null_ptr." + id + " = getelementptr i8, ptr %psl_data_ptr." + id +
                  ", i64 %psl_new_len." + id);
        emit_line("  store i8 0, ptr %psl_null_ptr." + id);
        emit_line("  br label %psl_done." + id);

        // Slow path - call FFI
        emit_line("psl_slow." + id + ":");
        emit_line("  call void @tml_text_push_str_len(ptr " + receiver + ", ptr " + str_ptr +
                  ", i64 " + str_len + ")");
        emit_line("  br label %psl_done." + id);

        emit_line("psl_done." + id + ":");
        last_expr_type_ = "void";
        return "0";
    }

    // V8-style inline optimization for text_push_formatted
    // Combines prefix + int + suffix in one optimized sequence
    if (mangled == "@tml_text_push_formatted" && arg_vals.size() == 6) {
        std::string receiver = arg_vals[0].first;   // ptr to Text
        std::string prefix = arg_vals[1].first;     // ptr to prefix string
        std::string prefix_len = arg_vals[2].first; // i64 prefix length
        std::string int_val = arg_vals[3].first;    // i64 integer value
        std::string suffix = arg_vals[4].first;     // ptr to suffix string
        std::string suffix_len = arg_vals[5].first; // i64 suffix length

        std::string id = std::to_string(temp_counter_++);

        // Check if heap mode (flags == 0)
        emit_line("  %pf_flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 24");
        emit_line("  %pf_flags." + id + " = load i8, ptr %pf_flags_ptr." + id);
        emit_line("  %pf_is_heap." + id + " = icmp eq i8 %pf_flags." + id + ", 0");
        emit_line("  br i1 %pf_is_heap." + id + ", label %pf_heap." + id + ", label %pf_slow." +
                  id);

        // Heap mode - check capacity (need prefix_len + 21 + suffix_len)
        emit_line("pf_heap." + id + ":");
        emit_line("  %pf_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 8");
        emit_line("  %pf_len." + id + " = load i64, ptr %pf_len_ptr." + id);
        emit_line("  %pf_cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 16");
        emit_line("  %pf_cap." + id + " = load i64, ptr %pf_cap_ptr." + id);
        // Max space needed: prefix + 21 (max int) + suffix
        emit_line("  %pf_need1." + id + " = add i64 " + prefix_len + ", 21");
        emit_line("  %pf_need2." + id + " = add i64 %pf_need1." + id + ", " + suffix_len);
        emit_line("  %pf_new_max." + id + " = add i64 %pf_len." + id + ", %pf_need2." + id);
        emit_line("  %pf_has_space." + id + " = icmp ule i64 %pf_new_max." + id + ", %pf_cap." +
                  id);
        emit_line("  br i1 %pf_has_space." + id + ", label %pf_fast." + id + ", label %pf_slow." +
                  id);

        // Fast path - inline prefix, call push_i64_unsafe, inline suffix
        emit_line("pf_fast." + id + ":");
        emit_line("  %pf_data_ptr." + id + " = load ptr, ptr " + receiver);

        // Copy prefix directly to buffer
        emit_line("  %pf_dst1." + id + " = getelementptr i8, ptr %pf_data_ptr." + id +
                  ", i64 %pf_len." + id);
        emit_line("  call void @llvm.memcpy.p0.p0.i64(ptr %pf_dst1." + id + ", ptr " + prefix +
                  ", i64 " + prefix_len + ", i1 false)");
        emit_line("  %pf_len2." + id + " = add i64 %pf_len." + id + ", " + prefix_len);
        emit_line("  store i64 %pf_len2." + id + ", ptr %pf_len_ptr." + id);

        // Call push_i64_unsafe - skips checks since we've already verified heap mode and capacity
        emit_line("  %pf_int_len." + id + " = call i64 @tml_text_push_i64_unsafe(ptr " + receiver +
                  ", i64 " + int_val + ")");

        // Reload length after push_i64_unsafe (it modified it)
        emit_line("  %pf_len3." + id + " = load i64, ptr %pf_len_ptr." + id);

        // Copy suffix directly to buffer
        emit_line("  %pf_data_ptr2." + id + " = load ptr, ptr " + receiver);
        emit_line("  %pf_dst2." + id + " = getelementptr i8, ptr %pf_data_ptr2." + id +
                  ", i64 %pf_len3." + id);
        emit_line("  call void @llvm.memcpy.p0.p0.i64(ptr %pf_dst2." + id + ", ptr " + suffix +
                  ", i64 " + suffix_len + ", i1 false)");
        emit_line("  %pf_new_len." + id + " = add i64 %pf_len3." + id + ", " + suffix_len);
        emit_line("  store i64 %pf_new_len." + id + ", ptr %pf_len_ptr." + id);

        // Null terminate
        emit_line("  %pf_null_ptr." + id + " = getelementptr i8, ptr %pf_data_ptr2." + id +
                  ", i64 %pf_new_len." + id);
        emit_line("  store i8 0, ptr %pf_null_ptr." + id);
        emit_line("  br label %pf_done." + id);

        // Slow path - call FFI
        emit_line("pf_slow." + id + ":");
        emit_line("  call void @tml_text_push_formatted(ptr " + receiver + ", ptr " + prefix +
                  ", i64 " + prefix_len + ", i64 " + int_val + ", ptr " + suffix + ", i64 " +
                  suffix_len + ")");
        emit_line("  br label %pf_done." + id);

        emit_line("pf_done." + id + ":");
        last_expr_type_ = "void";
        return "0";
    }

    // V8-style inline optimization for text_push_log
    // Pattern: s1 + n1 + s2 + n2 + s3 + n3 + s4
    if (mangled == "@tml_text_push_log" && arg_vals.size() == 12) {
        std::string receiver = arg_vals[0].first;
        std::string s1 = arg_vals[1].first;
        std::string s1_len = arg_vals[2].first;
        std::string n1 = arg_vals[3].first;
        std::string s2 = arg_vals[4].first;
        std::string s2_len = arg_vals[5].first;
        std::string n2 = arg_vals[6].first;
        std::string s3 = arg_vals[7].first;
        std::string s3_len = arg_vals[8].first;
        std::string n3 = arg_vals[9].first;
        std::string s4 = arg_vals[10].first;
        std::string s4_len = arg_vals[11].first;

        std::string id = std::to_string(temp_counter_++);

        // Check if heap mode
        emit_line("  %pl_flags_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 24");
        emit_line("  %pl_flags." + id + " = load i8, ptr %pl_flags_ptr." + id);
        emit_line("  %pl_is_heap." + id + " = icmp eq i8 %pl_flags." + id + ", 0");
        emit_line("  br i1 %pl_is_heap." + id + ", label %pl_heap." + id + ", label %pl_slow." +
                  id);

        // Check capacity
        emit_line("pl_heap." + id + ":");
        emit_line("  %pl_len_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 8");
        emit_line("  %pl_len." + id + " = load i64, ptr %pl_len_ptr." + id);
        emit_line("  %pl_cap_ptr." + id + " = getelementptr i8, ptr " + receiver + ", i32 16");
        emit_line("  %pl_cap." + id + " = load i64, ptr %pl_cap_ptr." + id);
        // Need: s1_len + s2_len + s3_len + s4_len + 63 (3 ints * 21)
        emit_line("  %pl_str_total." + id + " = add i64 " + s1_len + ", " + s2_len);
        emit_line("  %pl_str_total2." + id + " = add i64 %pl_str_total." + id + ", " + s3_len);
        emit_line("  %pl_str_total3." + id + " = add i64 %pl_str_total2." + id + ", " + s4_len);
        emit_line("  %pl_need." + id + " = add i64 %pl_str_total3." + id + ", 63");
        emit_line("  %pl_new_max." + id + " = add i64 %pl_len." + id + ", %pl_need." + id);
        emit_line("  %pl_has_space." + id + " = icmp ule i64 %pl_new_max." + id + ", %pl_cap." +
                  id);
        emit_line("  br i1 %pl_has_space." + id + ", label %pl_fast." + id + ", label %pl_slow." +
                  id);

        // Fast path
        emit_line("pl_fast." + id + ":");
        emit_line("  %pl_data." + id + " = load ptr, ptr " + receiver);

        // s1
        emit_line("  %pl_dst1." + id + " = getelementptr i8, ptr %pl_data." + id +
                  ", i64 %pl_len." + id);
        emit_line("  call void @llvm.memcpy.p0.p0.i64(ptr %pl_dst1." + id + ", ptr " + s1 +
                  ", i64 " + s1_len + ", i1 false)");
        emit_line("  %pl_len1." + id + " = add i64 %pl_len." + id + ", " + s1_len);
        emit_line("  store i64 %pl_len1." + id + ", ptr %pl_len_ptr." + id);

        // n1
        emit_line("  call i64 @tml_text_push_i64_unsafe(ptr " + receiver + ", i64 " + n1 + ")");
        emit_line("  %pl_len2." + id + " = load i64, ptr %pl_len_ptr." + id);

        // s2
        emit_line("  %pl_data2." + id + " = load ptr, ptr " + receiver);
        emit_line("  %pl_dst2." + id + " = getelementptr i8, ptr %pl_data2." + id +
                  ", i64 %pl_len2." + id);
        emit_line("  call void @llvm.memcpy.p0.p0.i64(ptr %pl_dst2." + id + ", ptr " + s2 +
                  ", i64 " + s2_len + ", i1 false)");
        emit_line("  %pl_len3." + id + " = add i64 %pl_len2." + id + ", " + s2_len);
        emit_line("  store i64 %pl_len3." + id + ", ptr %pl_len_ptr." + id);

        // n2
        emit_line("  call i64 @tml_text_push_i64_unsafe(ptr " + receiver + ", i64 " + n2 + ")");
        emit_line("  %pl_len4." + id + " = load i64, ptr %pl_len_ptr." + id);

        // s3
        emit_line("  %pl_data3." + id + " = load ptr, ptr " + receiver);
        emit_line("  %pl_dst3." + id + " = getelementptr i8, ptr %pl_data3." + id +
                  ", i64 %pl_len4." + id);
        emit_line("  call void @llvm.memcpy.p0.p0.i64(ptr %pl_dst3." + id + ", ptr " + s3 +
                  ", i64 " + s3_len + ", i1 false)");
        emit_line("  %pl_len5." + id + " = add i64 %pl_len4." + id + ", " + s3_len);
        emit_line("  store i64 %pl_len5." + id + ", ptr %pl_len_ptr." + id);

        // n3
        emit_line("  call i64 @tml_text_push_i64_unsafe(ptr " + receiver + ", i64 " + n3 + ")");
        emit_line("  %pl_len6." + id + " = load i64, ptr %pl_len_ptr." + id);

        // s4
        emit_line("  %pl_data4." + id + " = load ptr, ptr " + receiver);
        emit_line("  %pl_dst4." + id + " = getelementptr i8, ptr %pl_data4." + id +
                  ", i64 %pl_len6." + id);
        emit_line("  call void @llvm.memcpy.p0.p0.i64(ptr %pl_dst4." + id + ", ptr " + s4 +
                  ", i64 " + s4_len + ", i1 false)");
        emit_line("  %pl_new_len." + id + " = add i64 %pl_len6." + id + ", " + s4_len);
        emit_line("  store i64 %pl_new_len." + id + ", ptr %pl_len_ptr." + id);

        // Null terminate
        emit_line("  %pl_null_ptr." + id + " = getelementptr i8, ptr %pl_data4." + id +
                  ", i64 %pl_new_len." + id);
        emit_line("  store i8 0, ptr %pl_null_ptr." + id);
        emit_line("  br label %pl_done." + id);

        // Slow path
        emit_line("pl_slow." + id + ":");
        emit_line("  call void @tml_text_push_log(ptr " + receiver + ", ptr " + s1 + ", i64 " +
                  s1_len + ", i64 " + n1 + ", ptr " + s2 + ", i64 " + s2_len + ", i64 " + n2 +
                  ", ptr " + s3 + ", i64 " + s3_len + ", i64 " + n3 + ", ptr " + s4 + ", i64 " +
                  s4_len + ")");
        emit_line("  br label %pl_done." + id);

        emit_line("pl_done." + id + ":");
        last_expr_type_ = "void";
        return "0";
    }

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
        last_expr_type_ = ret_type;
        return result;
    }
}

} // namespace tml::codegen
