TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Await Expression
//!
//! This file implements the `await` expression for async functions.
//!
//! ## Poll[T] Type
//!
//! Async functions return `Poll[T]`:
//! - `Ready(T)` - tag 0, value is available
//! - `Pending` - tag 1, would yield to scheduler
//!
//! ## Await Behavior
//!
//! 1. Call async function (returns Poll[T])
//! 2. Extract value from Poll.Ready
//!
//! ## Current Limitations
//!
//! Full async/await would require state machine transformation.
//! Current implementation assumes sync execution where async
//! functions always return Ready immediately.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_await(const parser::AwaitExpr& await_expr) -> std::string {
    // Generate the awaited expression (typically an async function call)
    // This will return a Poll[T] value for async functions
    std::string poll_value = gen_expr(*await_expr.expr);
    std::string poll_type = last_expr_type_;

    // Check if the result is actually a Poll type (starts with %struct.Poll)
    if (poll_type.find("%struct.Poll") == 0) {
        // Get the inner type by inferring from the expression
        types::TypePtr expr_type = infer_expr_type(*await_expr.expr);
        std::string inner_type = "i64"; // default

        if (expr_type && expr_type->is<types::NamedType>()) {
            const auto& named = expr_type->as<types::NamedType>();
            if (named.name == "Poll" && !named.type_args.empty()) {
                inner_type = llvm_type_from_semantic(named.type_args[0], true);
            }
        }

        // Extract the Ready value from Poll
        std::string result = extract_poll_ready(poll_value, poll_type, inner_type);
        last_expr_type_ = inner_type;
        return result;
    }

    // If not a Poll type, the expression might be from a non-async function
    // or already unwrapped - return as-is
    return poll_value;
}

// Wrap a value in Poll.Ready(value)
// Poll[T] = { i32 tag, [N x i8] payload } where tag 0 = Ready
// The payload is stored as a byte array for consistent enum layout
auto LLVMIRGen::wrap_in_poll_ready(const std::string& value, const std::string& value_type)
    -> std::string {
    // Use the correct inner type for the Poll struct (not the expression type)
    std::string inner_type =
        current_poll_inner_type_.empty() ? value_type : current_poll_inner_type_;
    std::string final_value = value;

    // If the value type doesn't match the inner type, we may need to extend/convert
    if (value_type != inner_type) {
        // Integer widening (e.g., i32 to i64)
        if ((value_type == "i32" || value_type == "i16" || value_type == "i8") &&
            (inner_type == "i64" || inner_type == "i32" || inner_type == "i16")) {
            std::string extended = fresh_reg();
            emit_line("  " + extended + " = sext " + value_type + " " + value + " to " +
                      inner_type);
            final_value = extended;
        }
    }

    // Create Poll struct using alloca + store (like other enum constructors)
    // This matches the { i32, [N x i8] } layout used for all enums
    std::string poll_alloca = fresh_reg();
    emit_line("  " + poll_alloca + " = alloca " + current_poll_type_ + ", align 8");

    // Set tag (field 0) to 0 (Ready)
    std::string tag_ptr = fresh_reg();
    emit_line("  " + tag_ptr + " = getelementptr inbounds " + current_poll_type_ + ", ptr " +
              poll_alloca + ", i32 0, i32 0");
    emit_line("  store i32 0, ptr " + tag_ptr);

    // Set payload (field 1) - cast and store the value
    std::string payload_ptr = fresh_reg();
    emit_line("  " + payload_ptr + " = getelementptr inbounds " + current_poll_type_ + ", ptr " +
              poll_alloca + ", i32 0, i32 1");
    emit_line("  store " + inner_type + " " + final_value + ", ptr " + payload_ptr);

    // Load the complete Poll value
    std::string result = fresh_reg();
    emit_line("  " + result + " = load " + current_poll_type_ + ", ptr " + poll_alloca);

    return result;
}

// Extract the value from Poll.Ready
// Assumes the Poll is Ready (tag = 0), which is always true in our synchronous model
auto LLVMIRGen::extract_poll_ready(const std::string& poll_value, const std::string& poll_type,
                                   const std::string& inner_type) -> std::string {
    // Poll[T] = { i32 tag, T data }
    // We need to extract element at index 1 (the data)

    // First, allocate space for the Poll value
    std::string alloca_reg = fresh_reg();
    emit_line("  " + alloca_reg + " = alloca " + poll_type);
    emit_line("  store " + poll_type + " " + poll_value + ", ptr " + alloca_reg);

    // Get pointer to the data field (index 1)
    std::string data_ptr = fresh_reg();
    emit_line("  " + data_ptr + " = getelementptr inbounds " + poll_type + ", ptr " + alloca_reg +
              ", i32 0, i32 1");

    // Load the value
    std::string result = fresh_reg();
    emit_line("  " + result + " = load " + inner_type + ", ptr " + data_ptr);

    return result;
}

} // namespace tml::codegen
