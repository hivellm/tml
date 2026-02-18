//! # LLVM IR Generator - Static Method Calls
//!
//! This file implements `Type::method()` static method calls.
//!
//! ## Supported Types
//!
//! | Type    | Static Methods            |
//! |---------|---------------------------|
//! | I32, etc| `default()`, `max()`, `min()`|
//!
//! Note: List, HashMap, Buffer, File, Path static methods removed — now pure TML
//! (see lib/std/src/collections/, lib/std/src/file/)

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_static_method_call(const parser::MethodCallExpr& call,
                                       const std::string& type_name) -> std::optional<std::string> {
    const std::string& method = call.method;

    // Helper lambda to resolve type parameter names to concrete types
    auto resolve_type_arg_name = [this](const std::string& name) -> std::string {
        // Check if this is a type parameter that needs resolution
        auto it = current_type_subs_.find(name);
        if (it != current_type_subs_.end()) {
            // Resolve the type parameter to its concrete type
            return mangle_type(it->second);
        }
        return name;
    };

    // Note: List, HashMap, Buffer, File, Path static methods removed — now pure TML

    // Primitive type static methods (default)
    if (method == "default") {
        // Integer types: default is 0
        if (type_name == "I8" || type_name == "I16" || type_name == "I32" || type_name == "I64" ||
            type_name == "I128" || type_name == "U8" || type_name == "U16" || type_name == "U32" ||
            type_name == "U64" || type_name == "U128") {
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

    // Primitive type static methods: zero() from Zero behavior
    if (method == "zero") {
        // Integer types: zero is 0
        if (type_name == "I8" || type_name == "I16" || type_name == "I32" || type_name == "I64" ||
            type_name == "I128" || type_name == "U8" || type_name == "U16" || type_name == "U32" ||
            type_name == "U64" || type_name == "U128") {
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
        // Float types: zero is 0.0
        if (type_name == "F32") {
            last_expr_type_ = "float";
            return "0.0";
        }
        if (type_name == "F64") {
            last_expr_type_ = "double";
            return "0.0";
        }
    }

    // Primitive type static methods: one() from One behavior
    if (method == "one") {
        // Integer types: one is 1
        if (type_name == "I8" || type_name == "I16" || type_name == "I32" || type_name == "I64" ||
            type_name == "I128" || type_name == "U8" || type_name == "U16" || type_name == "U32" ||
            type_name == "U64" || type_name == "U128") {
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

    // Primitive type static methods: min_value() from Bounded behavior
    if (method == "min_value") {
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
        if (type_name == "U8") {
            last_expr_type_ = "i8";
            return "0";
        }
        if (type_name == "U16") {
            last_expr_type_ = "i16";
            return "0";
        }
        if (type_name == "U32") {
            last_expr_type_ = "i32";
            return "0";
        }
        if (type_name == "U64") {
            last_expr_type_ = "i64";
            return "0";
        }
    }

    // Primitive type static methods: max_value() from Bounded behavior
    if (method == "max_value") {
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

    // Primitive type static methods (from) - Type::from(value) conversions
    // Implements the From behavior for primitive type widening/narrowing
    if (method == "from" && !call.args.empty()) {
        // Helper to get LLVM type and bit width for TML primitive types
        auto get_type_info =
            [](const std::string& tml_type) -> std::tuple<std::string, int, bool, bool> {
            // Returns: (llvm_type, bit_width, is_signed, is_float)
            if (tml_type == "I8")
                return {"i8", 8, true, false};
            if (tml_type == "I16")
                return {"i16", 16, true, false};
            if (tml_type == "I32")
                return {"i32", 32, true, false};
            if (tml_type == "I64")
                return {"i64", 64, true, false};
            if (tml_type == "I128")
                return {"i128", 128, true, false};
            if (tml_type == "U8")
                return {"i8", 8, false, false};
            if (tml_type == "U16")
                return {"i16", 16, false, false};
            if (tml_type == "U32")
                return {"i32", 32, false, false};
            if (tml_type == "U64")
                return {"i64", 64, false, false};
            if (tml_type == "U128")
                return {"i128", 128, false, false};
            if (tml_type == "F32")
                return {"float", 32, true, true};
            if (tml_type == "F64")
                return {"double", 64, true, true};
            if (tml_type == "Bool")
                return {"i1", 1, false, false};
            return {"", 0, false, false};
        };

        auto [target_llvm, target_bits, target_signed, target_float] = get_type_info(type_name);

        if (!target_llvm.empty()) {
            // Generate the source value
            std::string src_val = gen_expr(*call.args[0]);
            std::string src_llvm = last_expr_type_;
            // Use last_expr_is_unsigned_ which is set by gen_expr
            bool src_signed = !last_expr_is_unsigned_;
            bool src_float = (src_llvm == "float" || src_llvm == "double");
            int src_bits = 0;

            if (src_llvm == "i1")
                src_bits = 1;
            else if (src_llvm == "i8")
                src_bits = 8;
            else if (src_llvm == "i16")
                src_bits = 16;
            else if (src_llvm == "i32")
                src_bits = 32;
            else if (src_llvm == "i64")
                src_bits = 64;
            else if (src_llvm == "i128")
                src_bits = 128;
            else if (src_llvm == "float")
                src_bits = 32;
            else if (src_llvm == "double")
                src_bits = 64;

            // Same type - identity conversion
            if (src_llvm == target_llvm) {
                last_expr_type_ = target_llvm;
                return src_val;
            }

            std::string result = fresh_reg();

            // Float to float conversion
            if (src_float && target_float) {
                if (src_bits < target_bits) {
                    emit_line("  " + result + " = fpext " + src_llvm + " " + src_val + " to " +
                              target_llvm);
                } else {
                    emit_line("  " + result + " = fptrunc " + src_llvm + " " + src_val + " to " +
                              target_llvm);
                }
                last_expr_type_ = target_llvm;
                return result;
            }

            // Int to float conversion
            if (!src_float && target_float) {
                if (src_signed) {
                    emit_line("  " + result + " = sitofp " + src_llvm + " " + src_val + " to " +
                              target_llvm);
                } else {
                    emit_line("  " + result + " = uitofp " + src_llvm + " " + src_val + " to " +
                              target_llvm);
                }
                last_expr_type_ = target_llvm;
                return result;
            }

            // Float to int conversion
            if (src_float && !target_float) {
                if (target_signed) {
                    emit_line("  " + result + " = fptosi " + src_llvm + " " + src_val + " to " +
                              target_llvm);
                } else {
                    emit_line("  " + result + " = fptoui " + src_llvm + " " + src_val + " to " +
                              target_llvm);
                }
                last_expr_type_ = target_llvm;
                return result;
            }

            // Int to int conversion
            if (src_bits < target_bits) {
                // Widening conversion
                if (src_signed) {
                    emit_line("  " + result + " = sext " + src_llvm + " " + src_val + " to " +
                              target_llvm);
                } else {
                    emit_line("  " + result + " = zext " + src_llvm + " " + src_val + " to " +
                              target_llvm);
                }
            } else if (src_bits > target_bits) {
                // Narrowing conversion (truncation)
                emit_line("  " + result + " = trunc " + src_llvm + " " + src_val + " to " +
                          target_llvm);
            } else {
                // Same bit width, just return the value (e.g., I32 to U32)
                last_expr_type_ = target_llvm;
                return src_val;
            }

            last_expr_type_ = target_llvm;
            return result;
        }
    }

    // Handle static methods from imported structs (like FormatSpec::new(), Text::from())
    if (env_.module_registry()) {
        std::string qualified_name = type_name + "::" + method;
        const auto& all_modules = env_.module_registry()->get_all_modules();
        for (const auto& [mod_name, mod] : all_modules) {
            auto func_it = mod.functions.find(qualified_name);
            if (func_it != mod.functions.end()) {
                const auto& func_sig = func_it->second;

                // Check if this is a generic type - if so, let the generic handling code
                // in method.cpp deal with it (via expected_enum_type_ context)
                auto struct_it = mod.structs.find(type_name);
                if (struct_it != mod.structs.end() && !struct_it->second.type_params.empty()) {
                    // This is a generic type like Range[T] - skip here and let method.cpp handle it
                    // The caller will have set expected_enum_type_ with the instantiated type
                    return std::nullopt;
                }
                auto enum_it = mod.enums.find(type_name);
                if (enum_it != mod.enums.end() && !enum_it->second.type_params.empty()) {
                    // This is a generic enum - skip here and let method.cpp handle it
                    return std::nullopt;
                }

                // Get return type - ensure struct type is defined
                std::string ret_type = llvm_type_from_semantic(func_sig.return_type);

                // For library types, use no suite prefix; for local test types use suite prefix
                // Primitive types (I8, I16, I32, etc.) are also library types - their impls are in
                // core
                auto is_primitive_type = [](const std::string& name) {
                    return name == "I8" || name == "I16" || name == "I32" || name == "I64" ||
                           name == "I128" || name == "U8" || name == "U16" || name == "U32" ||
                           name == "U64" || name == "U128" || name == "F32" || name == "F64" ||
                           name == "Bool";
                };
                bool is_library_type = mod.structs.count(type_name) > 0 ||
                                       mod.enums.count(type_name) > 0 ||
                                       is_primitive_type(type_name);

                // Generate arguments FIRST to determine their types
                // This is needed for behavior method overload resolution (e.g., TryFrom[I64])
                std::vector<std::pair<std::string, std::string>> typed_args;
                std::vector<std::string> arg_tml_types; // TML type names for behavior param lookup
                for (size_t i = 0; i < call.args.size(); ++i) {
                    std::string val = gen_expr(*call.args[i]);
                    std::string arg_type = last_expr_type_;
                    // Convert LLVM type to TML type name for behavior param lookup
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
                        tml_type_name = "Str"; // or pointer type
                    arg_tml_types.push_back(tml_type_name);
                    typed_args.push_back({arg_type, val});
                }

                // Build function name with behavior type parameter suffix for overloaded methods
                // Only add suffix for PRIMITIVE types that have multiple TryFrom/From overloads
                // e.g., I32::try_from(I64) -> I32_try_from_I64
                // Custom types like Celsius::from(Fahrenheit) stay as Celsius_from
                std::string behavior_suffix = "";
                auto is_primitive = [](const std::string& name) {
                    return name == "I8" || name == "I16" || name == "I32" || name == "I64" ||
                           name == "I128" || name == "U8" || name == "U16" || name == "U32" ||
                           name == "U64" || name == "U128" || name == "F32" || name == "F64" ||
                           name == "Bool";
                };
                if ((method == "try_from" || method == "from") && is_primitive(type_name) &&
                    !arg_tml_types.empty() && !arg_tml_types[0].empty()) {
                    // Use double underscore to match call.cpp's convention for behavior suffix
                    behavior_suffix = "__" + arg_tml_types[0];
                }

                std::string fn_name = "@tml_" + (is_library_type ? "" : get_suite_prefix()) +
                                      type_name + "_" + method + behavior_suffix;

                // Queue method instantiation for library types
                if (is_library_type) {
                    std::string mangled_method_name =
                        "tml_" + type_name + "_" + method + behavior_suffix;
                    if (generated_impl_methods_.find(mangled_method_name) ==
                        generated_impl_methods_.end()) {
                        // For TryFrom/From, pass the argument type as method_type_suffix
                        // so generic.cpp can find the correct impl block
                        std::string method_type_suffix_for_queue =
                            (!arg_tml_types.empty() && !arg_tml_types[0].empty()) ? arg_tml_types[0]
                                                                                  : "";
                        pending_impl_method_instantiations_.push_back(
                            PendingImplMethod{type_name,
                                              method,
                                              {},
                                              type_name,
                                              method_type_suffix_for_queue,
                                              /*is_library_type=*/true});
                        generated_impl_methods_.insert(mangled_method_name);
                    }
                }

                // Use registered function's return type if available (handles value class by-value
                // returns)
                std::string method_key = type_name + "_" + method + behavior_suffix;
                auto fn_info_it = functions_.find(method_key);
                if (fn_info_it != functions_.end() && !fn_info_it->second.ret_type.empty()) {
                    ret_type = fn_info_it->second.ret_type;
                }

                std::string args_str;
                for (size_t i = 0; i < typed_args.size(); ++i) {
                    if (i > 0)
                        args_str += ", ";
                    args_str += typed_args[i].first + " " + typed_args[i].second;
                }

                // Coverage instrumentation at call site for library static methods
                emit_coverage(qualified_name);

                std::string result = fresh_reg();
                if (ret_type == "void") {
                    emit_line("  call void " + fn_name + "(" + args_str + ")");
                    last_expr_type_ = "void";
                    return std::string("void");
                } else {
                    emit_line("  " + result + " = call " + ret_type + " " + fn_name + "(" +
                              args_str + ")");
                    last_expr_type_ = ret_type;
                    return result;
                }
            }
        }
    }

    return std::nullopt;
}

} // namespace tml::codegen
