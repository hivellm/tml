// LLVM IR generator - Slice/MutSlice methods
// Handles: len, is_empty for Slice and MutSlice types
// Also handles SliceType [T] methods

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

// Handle SliceType [T] methods (the parser slice type, not the Slice named type)
// Returns empty optional if this isn't a slice type or method isn't recognized
auto LLVMIRGen::gen_slice_type_method(const parser::MethodCallExpr& call, const std::string& method)
    -> std::optional<std::string> {
    // Infer receiver type
    types::TypePtr receiver_semantic_type = infer_expr_type(*call.receiver);
    if (!receiver_semantic_type || !receiver_semantic_type->is<types::SliceType>()) {
        return std::nullopt;
    }

    const auto& slice_type = receiver_semantic_type->as<types::SliceType>();
    types::TypePtr elem_type = slice_type.element;

    std::string elem_llvm_type = llvm_type_from_semantic(elem_type, true);
    // SliceType is a fat pointer: { ptr, i64 }
    std::string slice_llvm_type = "{ ptr, i64 }";

    // Generate receiver
    std::string slice_receiver = gen_expr(*call.receiver);
    std::string receiver_type = last_expr_type_;
    std::string slice_ptr;

    // If receiver is already a pointer, use it directly; otherwise alloca and store
    if (receiver_type == "ptr") {
        slice_ptr = slice_receiver;
    } else {
        slice_ptr = fresh_reg();
        emit_line("  " + slice_ptr + " = alloca " + slice_llvm_type);
        emit_line("  store " + slice_llvm_type + " " + slice_receiver + ", ptr " + slice_ptr);
    }

    // len() returns the slice length
    if (method == "len" || method == "length") {
        std::string len_ptr = fresh_reg();
        emit_line("  " + len_ptr + " = getelementptr " + slice_llvm_type + ", ptr " + slice_ptr +
                  ", i32 0, i32 1");
        std::string len_val = fresh_reg();
        emit_line("  " + len_val + " = load i64, ptr " + len_ptr);
        last_expr_type_ = "i64";
        return len_val;
    }

    // is_empty() returns true if length is 0
    if (method == "is_empty" || method == "isEmpty") {
        std::string len_ptr = fresh_reg();
        emit_line("  " + len_ptr + " = getelementptr " + slice_llvm_type + ", ptr " + slice_ptr +
                  ", i32 0, i32 1");
        std::string len_val = fresh_reg();
        emit_line("  " + len_val + " = load i64, ptr " + len_ptr);
        std::string result = fresh_reg();
        emit_line("  " + result + " = icmp eq i64 " + len_val + ", 0");
        last_expr_type_ = "i1";
        return result;
    }

    // Not a recognized slice type method
    return std::nullopt;
}

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
