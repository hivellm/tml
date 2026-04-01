TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Dynamic Function Pointer Call Intrinsics
//!
//! This file implements intrinsics for calling arbitrary function pointers
//! obtained from vtables or other sources at runtime.
//! Extracted from intrinsics_extended.cpp for maintainability.
//!
//! ## Sections in this file
//!
//! - Dynamic Function Pointer Calls (call_fn_ptr_i64, call_fn_ptr_ptr, call_fn_ptr_void)

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

/// Handles dynamic function pointer call intrinsics.
/// Called from try_gen_intrinsic_extended() after all other sections.
auto LLVMIRGen::try_gen_intrinsic_extended_dyncall(const std::string& intrinsic_name,
                                                   const std::string& fn_name,
                                                   const parser::CallExpr& call)
    -> std::optional<std::string> {

    (void)fn_name; // unused — present for consistency with other helper signatures

    // ========================================================================
    // Dynamic Function Pointer Calls
    // ========================================================================
    //
    // These intrinsics allow calling arbitrary function pointers obtained from
    // vtables or other sources at runtime. Essential for dynamic dispatch reflection.

    // call_fn_ptr_i64(fn_addr: I64, obj_ptr: I64) -> I64
    // Calls fn_addr as a function (ptr) -> i64, passing obj_ptr as first arg
    if (intrinsic_name == "call_fn_ptr_i64") {
        if (call.args.size() >= 2) {
            std::string fn_val = gen_expr(*call.args[0]);
            std::string arg_val = gen_expr(*call.args[1]);
            // Convert i64 fn address to function pointer
            std::string fn_ptr = fresh_reg();
            emit_line("  " + fn_ptr + " = inttoptr i64 " + fn_val + " to ptr");
            // Convert i64 arg to ptr
            std::string arg_ptr = fresh_reg();
            emit_line("  " + arg_ptr + " = inttoptr i64 " + arg_val + " to ptr");
            // Call the function pointer: i64 (ptr)
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 " + fn_ptr + "(ptr " + arg_ptr + ")");
            last_expr_type_ = "i64";
            return result;
        }
        last_expr_type_ = "i64";
        return "0";
    }

    // call_fn_ptr_ptr(fn_addr: I64, obj_ptr: I64) -> I64
    // Calls fn_addr as a function (ptr) -> ptr, returns result as I64
    if (intrinsic_name == "call_fn_ptr_ptr") {
        if (call.args.size() >= 2) {
            std::string fn_val = gen_expr(*call.args[0]);
            std::string arg_val = gen_expr(*call.args[1]);
            std::string fn_ptr = fresh_reg();
            emit_line("  " + fn_ptr + " = inttoptr i64 " + fn_val + " to ptr");
            std::string arg_ptr = fresh_reg();
            emit_line("  " + arg_ptr + " = inttoptr i64 " + arg_val + " to ptr");
            std::string raw_result = fresh_reg();
            emit_line("  " + raw_result + " = call ptr " + fn_ptr + "(ptr " + arg_ptr + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = ptrtoint ptr " + raw_result + " to i64");
            last_expr_type_ = "i64";
            return result;
        }
        last_expr_type_ = "i64";
        return "0";
    }

    // call_fn_ptr_void(fn_addr: I64, obj_ptr: I64)
    // Calls fn_addr as a function (ptr) -> void
    if (intrinsic_name == "call_fn_ptr_void") {
        if (call.args.size() >= 2) {
            std::string fn_val = gen_expr(*call.args[0]);
            std::string arg_val = gen_expr(*call.args[1]);
            std::string fn_ptr = fresh_reg();
            emit_line("  " + fn_ptr + " = inttoptr i64 " + fn_val + " to ptr");
            std::string arg_ptr = fresh_reg();
            emit_line("  " + arg_ptr + " = inttoptr i64 " + arg_val + " to ptr");
            emit_line("  call void " + fn_ptr + "(ptr " + arg_ptr + ")");
            last_expr_type_ = "void";
            return "void";
        }
        last_expr_type_ = "void";
        return "void";
    }

    return std::nullopt;
}

} // namespace tml::codegen
