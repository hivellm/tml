TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Class Constructor Calls
//!
//! This file handles class constructor calls like `Counter::new(10)`.
//! It supports:
//!
//! - Non-generic class constructors with overload resolution by argument types
//! - Generic class instantiation (e.g., `Box::new(42)` with expected type `Box[I32]`)
//! - ABI fixups for struct/enum arguments that need ptr coercion
//! - Constructor return type detection (value classes return struct, heap classes return ptr)

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

// Forward declaration of parse_mangled_type_string (defined in call_generic_func.cpp).
// Needed for generic class constructor type argument inference from mangled names.
// This is a file-local re-declaration to avoid header pollution — the function is
// small and used only in the generic class instantiation path below.
static types::TypePtr parse_mangled_type_string_local(const std::string& s) {
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
        auto inner = parse_mangled_type_string_local(inner_str);
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
        auto inner = parse_mangled_type_string_local(arg_str);
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

auto LLVMIRGen::gen_call_class_constructor(const parser::CallExpr& call, const std::string& fn_name)
    -> std::optional<std::string> {
    // Handle calls like Counter::new(10) where Counter is a class
    if (!call.callee->is<parser::PathExpr>()) {
        return std::nullopt;
    }

    const auto& path = call.callee->as<parser::PathExpr>().path;
    if (path.segments.size() != 2) {
        return std::nullopt;
    }

    const std::string& type_name = path.segments[0];
    const std::string& method = path.segments[1];

    // Check if this is a class constructor call
    if (method != "new") {
        return std::nullopt;
    }

    auto class_def = env_.lookup_class(type_name);
    bool is_generic_class =
        pending_generic_classes_.find(type_name) != pending_generic_classes_.end();

    if (!class_def.has_value() && !is_generic_class) {
        return std::nullopt;
    }

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
                // For struct types, use parse_mangled_type_string for proper handling
                type_arg = parse_mangled_type_string_local(type_arg_str);
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

    // Build constructor lookup key based on argument types (for overload resolution)
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
            std::string method_name = "new";
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
                    method_name += "_" + type_suffix;
                }
            }
            ctor_name = "@" + mangle_impl_method(class_name, method_name);
        }
    }

    // struct/enum -> ptr ABI fix for constructor args (see impl.cpp:282)
    // Definition side converts first non-self struct/enum param to ptr.
    // Look up the registered function info to get expected param types.
    {
        auto abi_it = functions_.find(ctor_key);
        if (abi_it == functions_.end())
            abi_it = functions_.find(class_name + "_new");
        if (abi_it != functions_.end()) {
            const auto& expected_params = abi_it->second.param_types;
            for (size_t i = 0; i < args.size() && i < expected_params.size(); ++i) {
                if (expected_params[i] == "ptr" &&
                    (arg_types[i].find("%struct.") == 0 || arg_types[i].find("%enum.") == 0)) {
                    std::string tmp = fresh_reg();
                    emit_line("  " + tmp + " = alloca " + arg_types[i]);
                    emit_line("  store " + arg_types[i] + " " + args[i] + ", ptr " + tmp);
                    args[i] = tmp;
                    arg_types[i] = "ptr";
                }
            }
        }
    }

    // Generate call using the correct return type
    std::string result = fresh_reg();
    std::string call_str = "  " + result + " = call " + ctor_ret_type + " " + ctor_name + "(";
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

} // namespace tml::codegen
