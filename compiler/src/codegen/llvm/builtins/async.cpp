TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Async Builtins
//!
//! This file implements async runtime intrinsics.
//!
//! ## block_on
//!
//! `block_on(async_fn()) -> T`
//!
//! Executes an async function synchronously. The async function
//! returns `Poll[T]`, and block_on extracts the value from `Poll.Ready`.
//!
//! ## Current Model
//!
//! In the synchronous execution model, async functions return
//! `Poll.Ready` immediately, so block_on simply extracts the payload.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_async(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // block_on(async_fn()) -> T
    // Executes an async function synchronously and extracts the result.
    // In the current synchronous model, async functions always return Poll.Ready immediately.
    // This function simply calls the async function and extracts the value from Poll.Ready.
    if (fn_name == "block_on") {
        if (!call.args.empty()) {
            // Generate the async function call (returns Poll[T])
            std::string poll_value = gen_expr(*call.args[0]);
            std::string poll_type = last_expr_type_;

            // Check if the result is a Poll type
            if (poll_type.find("%struct.Poll") == 0) {
                // Infer the inner type from the expression
                types::TypePtr expr_type = infer_expr_type(*call.args[0]);
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

            // If not a Poll type, just return the value as-is
            // (caller passed a non-async expression)
            return poll_value;
        }
        return "0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
