// LLVM IR generator - Slice/MutSlice methods
// Handles: len, is_empty for Slice and MutSlice types

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_slice_method(const parser::MethodCallExpr& call, const std::string& receiver,
                                 const std::string& receiver_type_name,
                                 types::TypePtr receiver_type) -> std::optional<std::string> {
    const std::string& method = call.method;

    // Only handle Slice and MutSlice
    if (receiver_type_name != "Slice" && receiver_type_name != "MutSlice") {
        return std::nullopt;
    }

    // Get the mangled struct type
    std::string struct_type = "%struct." + receiver_type_name;
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        if (!named.type_args.empty()) {
            struct_type = "%struct." + named.name;
            for (const auto& arg : named.type_args) {
                struct_type += "__" + mangle_type(arg);
            }
        }
    }

    // len() -> I64
    if (method == "len" || method == "length") {
        // Store the struct to access its field
        std::string tmp = fresh_reg();
        emit_line("  " + tmp + " = alloca " + struct_type);
        emit_line("  store " + struct_type + " " + receiver + ", ptr " + tmp);
        // GEP to the len field (index 1)
        std::string len_ptr = fresh_reg();
        emit_line("  " + len_ptr + " = getelementptr inbounds " + struct_type + ", ptr " + tmp +
                  ", i32 0, i32 1");
        std::string result = fresh_reg();
        emit_line("  " + result + " = load i64, ptr " + len_ptr);
        last_expr_type_ = "i64";
        return result;
    }

    // is_empty() -> Bool
    if (method == "is_empty" || method == "isEmpty") {
        // Store the struct to access its field
        std::string tmp = fresh_reg();
        emit_line("  " + tmp + " = alloca " + struct_type);
        emit_line("  store " + struct_type + " " + receiver + ", ptr " + tmp);
        // GEP to the len field (index 1)
        std::string len_ptr = fresh_reg();
        emit_line("  " + len_ptr + " = getelementptr inbounds " + struct_type + ", ptr " + tmp +
                  ", i32 0, i32 1");
        std::string len_val = fresh_reg();
        emit_line("  " + len_val + " = load i64, ptr " + len_ptr);
        std::string result = fresh_reg();
        emit_line("  " + result + " = icmp eq i64 " + len_val + ", 0");
        last_expr_type_ = "i1";
        return result;
    }

    return std::nullopt;
}

} // namespace tml::codegen
